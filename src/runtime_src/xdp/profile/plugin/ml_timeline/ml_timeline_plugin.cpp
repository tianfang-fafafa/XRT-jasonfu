/**
 * Copyright (C) 2023-2024 Advanced Micro Devices, Inc. - All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#define XDP_PLUGIN_SOURCE

#include<regex>
#include<string>

#include "core/common/device.h"
#include "core/common/message.h"
#include "core/common/api/hw_context_int.h"

#include "xdp/profile/plugin/ml_timeline/ml_timeline_plugin.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#ifdef XDP_CLIENT_BUILD
#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#endif

#include "xdp/profile/device/common/client_transaction.h"

namespace xdp {

  bool MLTimelinePlugin::live = false;

  uint32_t ParseMLTimelineBufferSizeConfig()
  {
    uint32_t bufSz = 0x20000;
    std::string szCfgStr = xrt_core::config::get_ml_timeline_buffer_size();
    std::smatch subStr;

    const std::regex validSzRegEx("\\s*([0-9]+)\\s*(K|k|M|m|)\\s*");
    if (std::regex_match(szCfgStr, subStr, validSzRegEx)) {
      try {
        if ("K" == subStr[2] || "k" == subStr[2]) {
          bufSz = (uint32_t)std::stoull(subStr[1]) * uint_constants::one_kb;
        } else if ("M" == subStr[2] || "m" == subStr[2]) {
          bufSz = (uint32_t)std::stoull(subStr[1]) * uint_constants::one_mb;
        }

      } catch (const std::exception &e) {
        std::stringstream msg;
        msg << "Invalid string specified for ML Timeline Buffer Size. "
            << e.what() << std::endl;
        xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      }

    } else {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
                "Invalid string specified for ML Timeline Buffer Size");
    }
    return bufSz;
  }

  MLTimelinePlugin::MLTimelinePlugin()
    : XDPPlugin()
  {
    MLTimelinePlugin::live = true;

    db->registerPlugin(this);
    db->registerInfo(info::ml_timeline);

    mBufSz = ParseMLTimelineBufferSizeConfig();
    mDebugger = std::shared_ptr<MLTimelineDebug>();
    //mDebugger->stacktrace();
  }

  MLTimelinePlugin::~MLTimelinePlugin()
  {
    if (VPDatabase::alive()) {
      try {
        writeAll(false);
      }
      catch (...) {
      }
      db->unregisterPlugin(this);
    }

    MLTimelinePlugin::live = false;
  }

  bool MLTimelinePlugin::alive()
  {
    return MLTimelinePlugin::live;
  }

  void MLTimelinePlugin::updateDevice(void* hwCtxImpl)
  {
#ifdef XDP_CLIENT_BUILD
    if (mHwCtxImpl) {
      // For client device flow, only 1 device and xclbin is supported now.
      return;
    }
    mHwCtxImpl = hwCtxImpl;

    xrt::hw_context hwContext = xrt_core::hw_context_int::create_hw_context_from_implementation(mHwCtxImpl);
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::hw_context_int::get_core_device(hwContext);
    if (coreDevice == nullptr) {
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "Failed to get device when try to parsing AIE Profile Metadata.");
      return;
    }

    // Only one device for Client Device flow
    uint64_t deviceId = db->addDevice("win_device");
    (db->getStaticInfo()).updateDeviceClient(deviceId, coreDevice, false);
    (db->getStaticInfo()).setDeviceName(deviceId, "win_device");

    auto data = coreDevice->get_axlf_section(AIE_METADATA);
    metadataReader = aie::readAIEMetadata(data.first, data.second, aie_meta);
    if (!metadataReader) {
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "Error parsing AIE Profiling Metadata.");
      return;
    }
    xdp::aie::driver_config meta_config = metadataReader->getDriverConfig();
    { 
      std::stringstream msg;
      msg << "-x- " << __func__ << "(): " << __LINE__
          << ", hw_gen: " << std::to_string(meta_config.hw_gen);
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    }
    /* 
    mDebugger->meta_config = meta_config;
    msg << "-x- " << __func__ << "(): " << __LINE__
        << ", hw_gen: " << std::to_string(mDebugger->meta_config.hw_gen);
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    */

    {
      std::stringstream msg;
      msg << "-x- " << __func__ << "(): " << __LINE__
        << ", hw_gen: " << std::to_string(meta_config.hw_gen);
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    }
    XAie_Config cfg {
      meta_config.hw_gen,
      meta_config.base_address,
      meta_config.column_shift,
      meta_config.row_shift,
      meta_config.num_rows,
      meta_config.num_columns,
      meta_config.shim_row,
      meta_config.mem_row_start,
      meta_config.mem_num_rows,
      meta_config.aie_tile_row_start,
      meta_config.aie_tile_num_rows,
      {0} // PartProp
    };
    XAie_DevInst aieDevInst = {0};
    auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
    if (RC != XAIE_OK) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
      return;
    }

    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    const std::map<module_type, std::vector<uint64_t>> regValues {
      {module_type::core, {0x31520,0x31524,0x31528,0x3152C}},
      {module_type::dma, {0x11020,0x11024}},
      {module_type::shim, {0x31020, 0x31024}},
      {module_type::mem_tile, {0x91020,0x91024,0x91028,0x9102C}},
    };


    read_register_op_t* op;
    std::size_t op_size;

    int counterId = 2;

    int col = 0;
    int row = 2;
    std::vector<register_data_t> op_profile_data;

    std::vector<uint64_t> Regs = regValues.at(module_type::core);
    // 25 is column offset and 20 is row offset for IPU
    op_profile_data.emplace_back(register_data_t{Regs[0] + (col << 25) + (row << 20)});

    op_size = sizeof(read_register_op_t) + sizeof(register_data_t) * (counterId - 1);
    op = (read_register_op_t*)malloc(op_size);
    op->count = counterId;
    for (int i = 0; i < op_profile_data.size(); i++) {
      op->data[i] = op_profile_data[i];
    }

    {
      std::stringstream msg;
      msg << "-x- " << __func__ << "(): " << __LINE__
        << ", op_size: " << std::to_string(op_size);
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
    }
    //XAie_AddCustomTxnOp(&aieDevInst, XAIE_IO_CUSTOM_OP_READ_REGS, (void*)op, op_size);
    XAie_AddCustomTxnOp(&aieDevInst, XAIE_IO_CUSTOM_OP_RECORD_TIMER, (void*)op, 4);
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);

    std::unique_ptr<aie::ClientTransaction> transactionHandler;
    transactionHandler = std::make_unique<aie::ClientTransaction>(hwContext, "AIE Profile Setup");

    if (!transactionHandler->initializeKernel("XDP_KERNEL")) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "initializeKernel(\"XDP_KERNEL\") Failed.");
      return;
    }

    if (!transactionHandler->submitTransaction(txn_ptr)) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", "submitTransaction(txn_ptr) Failed.");
      return;
    }

    DeviceDataEntry.valid = true;
    DeviceDataEntry.implementation = std::make_unique<MLTimelineClientDevImpl>(db);
    DeviceDataEntry.implementation->mDebugger = mDebugger;
    DeviceDataEntry.implementation->setHwContext(hwContext);
    DeviceDataEntry.implementation->setBufSize(mBufSz);
    DeviceDataEntry.implementation->updateDevice(mHwCtxImpl);
#endif
  }

  void MLTimelinePlugin::finishflushDevice(void* hwCtxImpl)
  {
#ifdef XDP_CLIENT_BUILD
    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }

    if (hwCtxImpl != mHwCtxImpl) {
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
          "Cannot retrieve ML Timeline data as a new HW Context Implementation is passed.");
      return;
    } 
    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

  void MLTimelinePlugin::writeAll(bool /*openNewFiles*/)
  {
#ifdef XDP_CLIENT_BUILD
    if (!mHwCtxImpl || !DeviceDataEntry.valid) {
      return;
    }
    DeviceDataEntry.valid = false;
    DeviceDataEntry.implementation->finishflushDevice(mHwCtxImpl);
#endif
  }

  void MLTimelinePlugin::broadcast(VPDatabase::MessageType msgType, void* /*blob*/)
  {
    switch(msgType)
    {
      case VPDatabase::READ_RECORD_TIMESTAMPS:
      {
        writeAll(false);
        break;
      }
      default:
        break;
    }
  }
}
