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

#ifndef XDP_PLUGIN_ML_TIMELINE_CLIENTDEV_IMPL_H
#define XDP_PLUGIN_ML_TIMELINE_CLIENTDEV_IMPL_H

#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_util.h"
#include "xdp/profile/database/static_info/filetypes/base_filetype_impl.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_impl.h"

namespace xdp {

  class ResultBOContainer;
  class MLTimelineClientDevImpl : public MLTimelineImpl
  {
    ResultBOContainer* mResultBOHolder;
    boost::property_tree::ptree aie_meta;
    std::shared_ptr<aie::BaseFiletypeImpl> metadataReader;
    void sendTimestampReadingCmd();
    std::vector<uint64_t> getTimestampReadingResult(uint32_t ts_num);
    public :
      MLTimelineClientDevImpl(VPDatabase* dB);

      ~MLTimelineClientDevImpl() = default;

      virtual void updateDevice(void* hwCtxImpl);
      virtual void finishflushDevice(void* hwCtxImpl);
      uint64_t readTimestamp() {
        sendTimestampReadingCmd();
        std::vector<uint64_t> ts = getTimestampReadingResult(1);
        return ts[0];
      };
  };

}

#endif

