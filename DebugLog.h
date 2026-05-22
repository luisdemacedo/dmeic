#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#ifdef DEBUG
#define DLOG(stream, ...) fprintf(stream, __VA_ARGS__)
#else
#define DLOG(stream, ...)                                                      \
  do {                                                                         \
  } while (0)
#endif

#endif // DEBUGLOG_H
