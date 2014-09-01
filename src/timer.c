/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#define _POSIX_C_SOURCE 199309L
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if !defined (_POSIX_TIMERS) || (_POSIX_TIMERS <= 0)
#error "POSIX timers are not supported"
#endif

#include "debug.h"
#include "timer.h"

unsigned long long nanotimestamp() {
  struct timespec tms;
  const clockid_t clock =
#if defined(CLOCK_MONOTONIC_RAW)
    CLOCK_MONOTONIC_RAW;
#elif defined(CLOCK_PRECISE)
    CLOCK_PRECISE;
#elif defined(_POSIX_MONOTONIC_CLOCK)
    CLOCK_MONOTONIC;
#else
    CLOCK_REALTIME;
#endif

  int ret = clock_gettime(clock, &tms);
  if (ret) {
    dbglog_error("clock_gettime: %s", strerror(errno));
    return 0;
  }

  return tms.tv_sec * 1000000000U + tms.tv_nsec;
}
