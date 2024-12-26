/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#include <fstream>
#include <sstream>

#include "client_transaction.h"
#include "core/common/message.h"

#include "transactions/op_buf.hpp"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

// ***************************************************************
// Anonymous namespace for helper functions local to this file
// ***************************************************************
namespace xdp::aie {
    using severity_level = xrt_core::message::severity_level;

    constexpr std::uint64_t CONFIGURE_OPCODE = std::uint64_t{2};

    bool 
    ClientTransaction::initializeKernel(std::string kernelName) 
    {
      try {
        kernel = xrt::kernel(context, kernelName);  
      } catch (std::exception &e){
        std::stringstream msg;
        msg << "Unable to find " << kernelName << " kernel from hardware context. Failed to configure " << transactionName << ". " << e.what();
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        return false;
      }

      return true;
    }

#if 1
    std::string getUsecSinceEpoch()
    {
      //auto timeSinceEpoch = (std::chrono::system_clock::now()).time_since_epoch();
      auto timeSinceEpoch = (std::chrono::steady_clock::now()).time_since_epoch();
      auto value = std::chrono::duration_cast<std::chrono::microseconds>(timeSinceEpoch);
      return std::to_string(value.count());
    }
#endif

    bool 
    ClientTransaction::submitTransaction(uint8_t* txn_ptr) {
      op_buf instr_buf;
      instr_buf.addOP(transaction_op(txn_ptr));
      xrt::bo instr_bo;

      // Configuration bo
      try {
        instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, kernel.group_id(1));
      } catch (std::exception &e){
        std::stringstream msg;
        msg << "Unable to create instruction buffer for " << transactionName << " transaction. Unable to configure " << transactionName<< ". " << e.what() << std::endl;
        xrt_core::message::send(severity_level::warning, "XRT", msg.str());
        return false;
      }

      instr_bo.write(instr_buf.ibuf_.data());
      instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
      auto run = kernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0);

      // mtf: not use both sleep function and output string function before the
      // kernel run.wait2().
      //std::this_thread::sleep_for(std::chrono::seconds(2));//mtf//introduce error of timeline
      auto usec_before2 = getUsecSinceEpoch();
      run.wait2();
      auto usec_after = getUsecSinceEpoch();
      {
#if 0
        auto delta2 = std::stoull(usec_after) - std::stoull(usec_before2);
        std::stringstream ssmsg;
        ssmsg << std::endl;
        ssmsg << "==========-x-" << __func__ << "():" << __LINE__ << ", usec before2(before running) since epoch: " << usec_before2 << std::endl;
        ssmsg << "==========-x-" << __func__ << "():" << __LINE__ << ", usec   after since epoch: " << usec_after << std::endl;
        ssmsg << "==========-x-" << __func__ << "():" << __LINE__ << ", delta2: " << std::to_string(delta2) << std::endl;
        xrt_core::message::send(severity_level::info, "XRT", ssmsg.str());
#endif
        std::ofstream fOut;
        fOut.open("tianfang_run_done_ts.json");
        fOut << (std::stoull(usec_before2) + std::stoull(usec_after)) / 2;
        fOut << "\n";
        fOut << usec_before2;
        fOut << "\n";
        fOut << usec_after;
        fOut.close();
      }
      
      xrt_core::message::send(severity_level::info, "XRT","Successfully scheduled " + transactionName + " instruction buffer.");
      return true;
    }

} // namespace xdp::aie
