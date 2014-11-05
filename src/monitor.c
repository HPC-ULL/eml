/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

//feature test macro for clock_nanosleep(), etc
#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <sys/queue.h>

#include <confuse.h>

#include "data.h"
#include "debug.h"
#include "device.h"
#include "driver.h"
#include "monitor.h"

#ifndef MEASUREMENT_STACK_SIZE
/// Maximum level of measurement nesting
#define MEASUREMENT_STACK_SIZE 10
#endif

/// Contains monitoring state for a single device
struct emlMonitor {
  /// Thread that measures data periodically
  pthread_t measuring_thread;
  /// Gathered measurement run data
  struct emlDataRun* run;
  /// Measurement nesting level we are currently at
  size_t level;
  /// Stack containing start point/block for nested measurements
  struct emlDataBlock* firstblock[MEASUREMENT_STACK_SIZE];
  size_t firstpoint[MEASUREMENT_STACK_SIZE];

  /// Total gathered data points
  size_t npoints;
  /// Current data block
  struct emlDataBlock* curblk;
  /// Mutex for current point/block between monitor and main threads
  pthread_mutex_t pointlock;
};

static void* monitor_thread(void* arg) {
  const struct emlDevice* dev = arg;
  const long delay_ns = cfg_getint(dev->driver->config, "sampling_interval");

  static const long NS_PER_SEC = 1000000000L;
  const struct timespec delay = {
    .tv_sec = delay_ns / NS_PER_SEC,
    .tv_nsec = delay_ns % NS_PER_SEC,
  };
  struct emlMonitor* mon = dev->monitor;

  size_t nfields = 1;
  if (mon->run->props->inst_energy_field) nfields++;
  if (mon->run->props->inst_power_field) nfields++;

  //as long as there is at least one ongoing measurement on this device:
  while (mon->level) {
    //allocate and insert a new block if current is full
    struct emlDataBlock* thisblk = mon->curblk;
    const size_t i = mon->npoints % DATABLOCK_SIZE;
    const int needblock = !i && mon->npoints;
    if (needblock) {
      thisblk = malloc(sizeof(*thisblk));
      if (!thisblk) goto mem_err;
      thisblk->fields = malloc(nfields * DATABLOCK_SIZE * sizeof(*thisblk->fields));
      if (!thisblk->fields) goto mem_err;
      SLIST_INSERT_AFTER(mon->curblk, thisblk, entries);
    }

    //get a new datapoint and wait
    dev->driver->measure(dev->index, &thisblk->fields[i]);

    pthread_mutex_lock(&mon->pointlock);
    mon->npoints++;
    mon->curblk = thisblk;
    pthread_mutex_unlock(&mon->pointlock);

    int err = clock_nanosleep(CLOCK_MONOTONIC, 0, &delay, NULL);
    assert(err != EINVAL);
    assert(err != EFAULT);
  }

  return NULL;

mem_err:
  dbglog_error("out of memory");
  return NULL;
}

enum emlError emlDeviceMonitorInit(struct emlDevice* const device) {
  device->monitor = malloc(sizeof(*device->monitor));
  device->monitor->level = 0;
  pthread_mutex_init(&device->monitor->pointlock, NULL);
  return EML_SUCCESS;
}

enum emlError emlDeviceMonitorShutdown(struct emlDevice* const device) {
  struct emlMonitor* mon = device->monitor;

  if (mon->level) {
    //skip to last measurement level (forces the thread to be stopped)
    device->monitor->level = 1;
    struct emlData* discarded;
    emlDeviceMonitorStop(device, &discarded);
    emlDataFree(discarded);
  }

  pthread_mutex_destroy(&device->monitor->pointlock);
  free(device->monitor);
  return EML_SUCCESS;
}

enum emlError emlDeviceMonitorStart(const struct emlDevice* const device) {
  struct emlMonitor* mon = device->monitor;

  //increase measurement nesting level unless the stack is full
  if (mon->level == MEASUREMENT_STACK_SIZE)
    return EML_MEASUREMENT_STACK_FULL;
  mon->level++;

  //if we weren't measuring before, start now
  if (mon->level == 1) {
    //init measurement run data
    mon->run = malloc(sizeof(*mon->run));
    if (!mon->run)
      return EML_NO_MEMORY;
    mon->run->refcount = 0;
    mon->run->device = device;
    mon->run->props = device->driver->default_props;

    size_t nfields = 1;
    if (mon->run->props->inst_energy_field) nfields++;
    if (mon->run->props->inst_power_field) nfields++;

    //init blocklist and allocate first block
    SLIST_INIT(&mon->run->blocks);
    mon->curblk = malloc(sizeof(*mon->curblk));
    if (!mon->curblk) {
      free(mon->run);
      return EML_NO_MEMORY;
    }
    mon->curblk->fields = malloc(nfields * DATABLOCK_SIZE * sizeof(*mon->curblk->fields));
    if (!mon->curblk->fields) {
      free(mon->run);
      free(mon->curblk);
      return EML_NO_MEMORY;
    }
    SLIST_INSERT_HEAD(&mon->run->blocks, mon->curblk, entries);

    mon->npoints = 0;
    mon->firstblock[0] = mon->curblk;
    mon->firstpoint[0] = 0;

    //launch measuring thread
    int err = pthread_create(&mon->measuring_thread,
        NULL,
        &monitor_thread,
        (void*) device);

    if (err) {
      dbglog_error("pthread_create returned %d", err);
      return EML_UNKNOWN;
    }
  }
  //if we were measuring, record start block
  else {
    pthread_mutex_lock(&mon->pointlock);
    mon->firstblock[mon->level - 1] = mon->curblk;
    mon->firstpoint[mon->level - 1] = mon->npoints;
    pthread_mutex_unlock(&mon->pointlock);
  }

  mon->run->refcount++;

  return EML_SUCCESS;
}

enum emlError emlDeviceMonitorStop(
    const struct emlDevice* const device,
    struct emlData** const result)
{
  struct emlMonitor* mon = device->monitor;

  if (!mon->level)
    return EML_NOT_STARTED;

  pthread_mutex_lock(&mon->pointlock);
  const size_t endnpoints = mon->npoints;
  pthread_mutex_unlock(&mon->pointlock);

  //decrease level and stop measuring if 0
  mon->level--;
  if (!mon->level) {
    int err = pthread_join(mon->measuring_thread, NULL);
    if (err) {
      emlDataRunRelease(mon->run);
      return EML_UNKNOWN;
    }
  }

  //return interval data
  struct emlData* d = *result;
  d->run = mon->run;
  d->firstblock = mon->firstblock[mon->level];
  d->firstpoint = mon->firstpoint[mon->level];
  d->npoints = endnpoints - d->firstpoint;

  return EML_SUCCESS;
}
