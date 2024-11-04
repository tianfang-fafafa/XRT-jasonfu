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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <chrono>
#include <fstream>
#include <regex>

#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/common/device.h"
#include "core/common/message.h"

#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_kernel.h"

#include "xdp/profile/plugin/ml_timeline/clientDev/ml_timeline.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/device/common/client_transaction.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {

  class ResultBOContainer
  {
    public:
      xrt::bo  mBO;
      ResultBOContainer(xrt::hw_context hwCtx, uint32_t sz)
      {
        mBO = xrt_core::bo_int::create_debug_bo(hwCtx, sz);
      }
      ~ResultBOContainer() {}

      void 
      syncFromDevice()
      {
        mBO.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      }
      uint32_t*
      map()
      {
        return mBO.map<uint32_t*>();
      }
  };

  MLTimelineClientDevImpl::MLTimelineClientDevImpl(VPDatabase*dB)
    : MLTimelineImpl(dB),
      mResultBOHolder(nullptr)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Created ML Timeline Plugin for Client Device.");
  }

  void MLTimelineClientDevImpl::updateDevice(void* /*hwCtxImpl*/)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "In MLTimelineClientDevImpl::updateDevice");

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(xrt_core::message::severity_level::debug)) {
      /*
       * To make sure the debug buffer host_offset is zero, create_debug_bo()
       * must be called before trying to read timestamp. And create_debug_bo()
       * should not call before previous has been freed, otherwise the creation
       * would be ignored and the host_offset would not be zero.
       */
      mResultBOHolder = new ResultBOContainer(mHwContext, mBufSz);
      memset(mResultBOHolder->map(), 0, mBufSz);
      std::string msec_before = xdp::getMsecSinceEpoch();
      uint64_t ts = readTimestamp();
      std::string msec_after = xdp::getMsecSinceEpoch();
      std::stringstream ssmsg;
      ssmsg << "MsecSinceEpoch: " << msec_before
          << ", Timestamp: " << ts << "(0x" << std::hex << ts << ")"
          << ", MsecSinceEpoch: " << msec_after << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", ssmsg.str());
      delete mResultBOHolder;
      mResultBOHolder = nullptr;
    }

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(xrt_core::message::severity_level::debug)) {
      mResultBOHolder = new ResultBOContainer(mHwContext, mBufSz);
      memset(mResultBOHolder->map(), 0, mBufSz);

      std::stringstream ssmsg;
      std::string msec_before = xdp::getMsecSinceEpoch();
      ssmsg << std::endl << " MsecSinceEpoch[0]: " << msec_before << std::endl;
      sendTimestampReadingCmd();
      msec_before = xdp::getMsecSinceEpoch();
      ssmsg << " MsecSinceEpoch[1]: " << msec_before << std::endl;
      sendTimestampReadingCmd();
      msec_before = xdp::getMsecSinceEpoch();
      ssmsg << " MsecSinceEpoch[2]: " << msec_before << std::endl;
      sendTimestampReadingCmd();
      msec_before = xdp::getMsecSinceEpoch();
      ssmsg << " MsecSinceEpoch[3]: " << msec_before << std::endl;
      sendTimestampReadingCmd();
      msec_before = xdp::getMsecSinceEpoch();
      ssmsg << " MsecSinceEpoch[4]: " << msec_before << std::endl << "----------" << std::endl;
      sendTimestampReadingCmd();
      std::vector<uint64_t> ts_v;
      ts_v = getTimestampReadingResult(5);
      std::string msec_after = xdp::getMsecSinceEpoch();

      int index = 0;
      for (uint64_t ts: ts_v) {
        ssmsg << "  Timestamp[" << index++ << "]: " << std::dec << ts << "(0x" << std::hex << ts << ")" << std::endl;
      }
      ssmsg << ", MsecSinceEpoch: " << msec_after << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", ssmsg.str());
      delete mResultBOHolder;
      mResultBOHolder = nullptr;
    }

    try {

      /* Use a container for Debug BO for results to control its lifetime.
       * The result BO should be deleted after reading out recorded data in
       * finishFlushDevice so that AIE Profile/Debug Plugins, if enabled,
       * can use their own Debug BO to capture their data.
       */
      mResultBOHolder = new ResultBOContainer(mHwContext, mBufSz);
      memset(mResultBOHolder->map(), 0, mBufSz);

    } catch (std::exception& e) {
      std::stringstream msg;
      msg << "Unable to create/initialize result buffer of size "
          << std::hex << mBufSz << std::dec
          << " Bytes for Record Timer Values. Cannot get ML Timeline info. " 
          << e.what() << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
      return;
    }
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Allocated buffer In MLTimelineClientDevImpl::updateDevice");
  }

  void MLTimelineClientDevImpl::finishflushDevice(void* /*hwCtxImpl*/)
  {
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Using Allocated buffer In MLTimelineClientDevImpl::finishflushDevice");
              
    mResultBOHolder->syncFromDevice();    
    uint32_t* ptr = mResultBOHolder->map();
      
    boost::property_tree::ptree ptTop;
    boost::property_tree::ptree ptHeader;
    boost::property_tree::ptree ptRecordTimerTS;

    // Header for JSON 
    ptHeader.put("date", xdp::getCurrentDateTime());
    ptHeader.put("time_created", xdp::getMsecSinceEpoch());

    boost::property_tree::ptree ptSchema;
    ptSchema.put("major", "1");
    ptSchema.put("minor", "0");
    ptSchema.put("patch", "0");
    ptHeader.add_child("schema_version", ptSchema);
    ptHeader.put("device", "Client");
    ptHeader.put("clock_freq_MHz", 1000);
    ptTop.add_child("header", ptHeader);

    // Record Timer TS in JSON
    // Assuming correct Stub has been called and Write Buffer contains valid data
    
    uint32_t max_count = mBufSz / (3*sizeof(uint32_t));
    // Each record timer entry has 32bit ID and 32bit AIE High Timer + 32bit AIE Low Timer value.

    uint32_t numEntries = max_count;
    std::stringstream msg;
    msg << " A maximum of " << numEntries << " record can be accommodated in given buffer of bytes size "
        << std::hex << mBufSz << std::dec << std::endl;
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());

    if (numEntries <= max_count) {
      for (uint32_t i = 0 ; i < numEntries; i++) {
        boost::property_tree::ptree ptIdTS;
        uint32_t id = *ptr;
        ptIdTS.put("id", *ptr);
        ptr++;

        uint64_t ts64 = *ptr;
        ts64 = ts64 << 32;
        ptr++;
        ts64 |= (*ptr);
        if (0 == ts64 && 0 == id) {
          // Zero value for Timestamp in cycles (and id too) indicates end of recorded data
          std::string msgEntries = " Got " + std::to_string(i) + " records in buffer";
          xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msgEntries);
          break;
        }
        ptIdTS.put("cycle", ts64);
        ptr++;

        ptRecordTimerTS.push_back(std::make_pair("", ptIdTS));
      }
    }    

    if (ptRecordTimerTS.empty()) {
      boost::property_tree::ptree ptEmpty;
      ptRecordTimerTS.push_back(std::make_pair("", ptEmpty));
    }
    ptTop.add_child("record_timer_ts", ptRecordTimerTS);

    // Write output file
    std::ostringstream oss;
    boost::property_tree::write_json(oss, ptTop);

    // Remove quotes from value strings
    std::regex reg("\\\"((-?[0-9]+\\.{0,1}[0-9]*)|(null)|())\\\"(?!\\:)");
    std::string result = std::regex_replace(oss.str(), reg, "$1");

    std::ofstream fOut;
    fOut.open("record_timer_ts.json");
    fOut << result;
    fOut.close();

    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", 
              "Finished writing record_timer_ts.json in MLTimelineClientDevImpl::finishflushDevice");

    /* Delete the result BO so that AIE Profile/Debug Plugins, if enabled,
     * can use their own Debug BO to capture their data.
     */
    delete mResultBOHolder;
    mResultBOHolder = nullptr;

    if (xrt_core::config::get_verbosity() >= static_cast<uint32_t>(xrt_core::message::severity_level::debug)) {
      /*
       * To make sure the debug buffer host_offset is zero, create_debug_bo()
       * must be called before trying to read timestamp. And create_debug_bo()
       * should not call before previous has been freed, otherwise the creation
       * would be ignored and the host_offset would not be zero.
       */
      mResultBOHolder = new ResultBOContainer(mHwContext, mBufSz);
      memset(mResultBOHolder->map(), 0, mBufSz);

      std::string msec_before = xdp::getMsecSinceEpoch();
      uint64_t ts = readTimestamp();
      std::string msec_after = xdp::getMsecSinceEpoch();
      std::stringstream ssmsg;
      ssmsg << "MsecSinceEpoch: " << msec_before
          << ", Timestamp: " << ts << "(0x" << std::hex << ts << ")"
          << ", MsecSinceEpoch: " << msec_after << std::endl;
      xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", ssmsg.str());
      delete mResultBOHolder;
      mResultBOHolder = nullptr;
    }
    /* The purpose of this line is to make sure the firmware utl log has poped
     * before ipu power down */
    std::this_thread::sleep_for(std::chrono::microseconds(1000000));
  }

  void MLTimelineClientDevImpl::sendTimestampReadingCmd()
  {
    std::shared_ptr<xrt_core::device> coreDevice = xrt_core::hw_context_int::get_core_device(mHwContext);
    if (coreDevice == nullptr) {
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "Failed to get device when try to parsing metadata.");
      return;
    }
    auto data = coreDevice->get_axlf_section(AIE_METADATA);
    metadataReader = aie::readAIEMetadata(data.first, data.second, aie_meta);
    if (!metadataReader) {
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "Error parsing metadata.");
      return;
    }
    xdp::aie::driver_config meta_config = metadataReader->getDriverConfig();

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
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "AIE Driver Initialization Failed.");
      return;
    }
    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
    uint32_t pad = 0;
    XAie_AddCustomTxnOp(&aieDevInst, XAIE_IO_CUSTOM_OP_RECORD_TIMER, (void*)&pad, 4);
    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    std::unique_ptr<aie::ClientTransaction> transactionHandler;
    transactionHandler = std::make_unique<aie::ClientTransaction>(mHwContext, "AIE Profile Setup");
    if (!transactionHandler->initializeKernel("XDP_KERNEL")) {
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "initializeKernel(\"XDP_KERNEL\") Failed.");
      return;
    }
    if (!transactionHandler->submitTransaction(txn_ptr)) {
      xrt_core::message::send(xrt_core::message::severity_level::error,
          "XRT", "submitTransaction(txn_ptr) Failed.");
      return;
    }
    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);
  }

  std::vector<uint64_t> MLTimelineClientDevImpl::getTimestampReadingResult(uint32_t ts_num)
  {
    mResultBOHolder->syncFromDevice();
    std::vector<uint64_t> result;
    uint32_t* output = mResultBOHolder->map();
    for (uint32_t i = 0; i < ts_num; i++, output += 3) {
      uint64_t ts64 = *(output + 1);
      ts64 = (ts64 << 32) | *(output + 2);
      result.push_back(ts64);
    }
    return result;
  }
}

