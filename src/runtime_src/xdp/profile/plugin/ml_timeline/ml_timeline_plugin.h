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

#ifndef XDP_ML_TIMELINE_PLUGIN_H
#define XDP_ML_TIMELINE_PLUGIN_H

#include "xdp/profile/plugin/ml_timeline/ml_timeline_impl.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"
#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"
#include "xdp/profile/plugin/ml_timeline/ml_timeline_debug.h"


namespace xdp {

  class MLTimelinePlugin : public XDPPlugin
  {
    public:

    MLTimelinePlugin();
    ~MLTimelinePlugin();

    void updateDevice(void* hwCtxImpl);
    void finishflushDevice(void* hwCtxImpl);
    void writeAll(bool openNewFiles);

    virtual void broadcast(VPDatabase::MessageType, void*);

    static bool alive();

    private:
    static bool live;

    struct DeviceData {
      bool valid;
      std::unique_ptr<MLTimelineImpl> implementation;
    } DeviceDataEntry;

    void* mHwCtxImpl = nullptr;
    uint32_t mBufSz  = 0x20000;

    std::shared_ptr<MLTimelineDebug> mDebugger;

    boost::property_tree::ptree aie_meta;
    std::shared_ptr<aie::BaseFiletypeImpl> metadataReader;

  };

} // end namespace xdp

#endif
