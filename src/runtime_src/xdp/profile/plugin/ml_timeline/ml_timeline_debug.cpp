#include <iostream>
#include <iomanip>
#include <boost/stacktrace.hpp>

#include "xdp/profile/plugin/ml_timeline/ml_timeline_debug.h"

namespace xdp {
  int MLTimelineDebug::stacktrace()
  {
    std::cout << "-x- This is MLTimelineDebug::stacktrace() -- " << std::endl;
    std::cout << boost::stacktrace::stacktrace();
    return 0;
  }

  int MLTimelineDebug::timestamp_debug()
  {
    std::cout << "-x- This is MLTimelineDebug::timestamp_debug() -- " << std::endl;
#if 0
    auto falModuleType =   XAIE_CORE_MOD;
    uint64_t timerValue = 0;
    XAie_LocType tileLocation = XAie_TileLoc(2 /*aie->column*/, 2 /* aie->row*/);// TODO
    XAie_ReadTimer(&aieDevInst, tileLocation, falModuleType, &timerValue);
    std::cout << "timerValue: " << timerValue 
              << "(0x" << std::setw(8) << std::setfill('0') << std::hex 
              << timerValue << ")"
              << std::endl;
#endif
#if 0
    std::stringstream msg;
    msg << "-x- " << __func__ << "(): " << __LINE__
        << ", hw_gen: " << std::to_string(meta_config.hw_gen);
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", msg.str());
#endif
    return 0;
  }
} // namespace xdp
