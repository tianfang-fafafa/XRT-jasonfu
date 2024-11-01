#ifndef XDP_PLUGIN_ML_TIMELINE_DEBUG_H
#define XDP_PLUGIN_ML_TIMELINE_DEBUG_H

#include "core/common/message.h"
#include "xdp/profile/plugin/aie_profile/aie_profile_metadata.h"

extern "C" {
#include <xaiengine.h>
#include <xaiengine/xaiegbl_params.h>
}

namespace xdp {
  class MLTimelineDebug
  {
    public:
      MLTimelineDebug()
      {
        std::cout << "-x- This is MLTimelineDebug() Creation --" << std::endl;
      }
      int cnt;
      int stacktrace();

      XAie_DevInst aieDevInst = {0};
      int timestamp_debug();

      xdp::aie::driver_config meta_config;
  };
} // namespace xdp
#endif // XDP_PLUGIN_ML_TIMELINE_DEBUG_H
