#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <cstdio>

enum class LogCategory {
  General,
  SatCalls,
  Sampling,
  ClauseSharing,
  SolutionSharing
};

constexpr bool logEnabled(LogCategory category) {

  switch (category) {
  case LogCategory::General:
#ifdef DEBUG_GENERAL
    return true;
#else
    return false;
#endif
  case LogCategory::SatCalls:
#ifdef DEBUG_SATCALLS
    return true;
#else
    return false;
#endif
  case LogCategory::Sampling:
#ifdef DEBUG_SAMPLING
    return true;
#else
    return false;
#endif
  case LogCategory::ClauseSharing:
#ifdef DEBUG_CLAUSESHARING
    return true;
#else
    return false;
#endif
  case LogCategory::SolutionSharing:
#ifdef DEBUG_SOLUTIONSHARING
    return true;
#else
    return false;
#endif
  default:
    return false;
  }
}

#define DLOG(category, stream, ...)                                            \
  do {                                                                         \
    if constexpr (logEnabled(category))                                        \
      std::fprintf(stream, __VA_ARGS__);                                       \
  } while (false)

#endif // DEBUGLOG_H
