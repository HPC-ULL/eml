/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <confuse.h>
#include <dlfcn.h>
#include <miclib.h>

#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

struct emlDriver mic_driver;

//MIC power readings are updated every 50ms
#define MIC_DEFAULT_SAMPLING_INTERVAL 50000000L

//local state
static void* handle;
static struct mic_devices_list* micdeviceslist;
static struct mic_device** micdevices;

//imported funcs
static int (*dl_mic_get_devices)(struct mic_devices_list**);
static int (*dl_mic_free_devices)(struct mic_devices_list*);
static int (*dl_mic_get_ndevices)(struct mic_devices_list*, int*);
static int (*dl_mic_get_device_at_index)(struct mic_devices_list*, int, int*);
static int (*dl_mic_open_device)(struct mic_device**, uint32_t);
static int (*dl_mic_close_device)(struct mic_device*);
static const char* (*dl_mic_get_error_string)();
static int (*dl_mic_get_power_utilization_info)(struct mic_device*, struct mic_power_util_info**);
static int (*dl_mic_free_power_utilization_info)(struct mic_power_util_info*);
static int (*dl_mic_get_inst_power_readings)(struct mic_power_util_info*, uint32_t*);

static enum emlError link_miclib() {
  //clear any existing error
  dlerror();

  handle = dlopen("libmicmgmt.so", RTLD_LAZY);
  if (dlerror()) {
    strncpy(mic_driver.failed_reason, dlerror(), sizeof(mic_driver.failed_reason));
    return EML_LIBRARY_UNAVAILABLE;
  }

  *(void **) (&dl_mic_get_devices) = dlsym(handle, "mic_get_devices");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_free_devices) = dlsym(handle, "mic_free_devices");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_get_ndevices) = dlsym(handle, "mic_get_ndevices");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_get_device_at_index) = dlsym(handle, "mic_get_device_at_index");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_open_device) = dlsym(handle, "mic_open_device");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_close_device) = dlsym(handle, "mic_close_device");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_get_error_string) = dlsym(handle, "mic_get_error_string");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_get_power_utilization_info) = dlsym(handle, "mic_get_power_utilization_info");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_free_power_utilization_info) = dlsym(handle, "mic_free_power_utilization_info");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_mic_get_inst_power_readings) = dlsym(handle, "mic_get_inst_power_readings");
  if (dlerror()) goto err_unlink;

  return EML_SUCCESS;

err_unlink:
  strncpy(mic_driver.failed_reason, dlerror(), sizeof(mic_driver.failed_reason));
  dlclose(handle);

  return EML_SYMBOL_UNAVAILABLE;
}

static enum emlError init(cfg_t* const config) {
  assert(!mic_driver.initialized);
  assert(config);
  mic_driver.config = config;

  enum emlError err;

  //dynamically link miclibmgmt
  err = link_miclib();
  if (err != EML_SUCCESS)
    goto error;

  int ret;

  ret = dl_mic_get_devices(&micdeviceslist);
  if (ret != E_MIC_SUCCESS) {
    snprintf(mic_driver.failed_reason, sizeof(mic_driver.failed_reason),
        "%s %s",
        "mic_get_devices:",
        dl_mic_get_error_string());

    err = EML_UNKNOWN;
    goto err_unlink;
  }

  int ndevices;
  ret = dl_mic_get_ndevices(micdeviceslist, &ndevices);
  if (ret != E_MIC_SUCCESS) {
    snprintf(mic_driver.failed_reason, sizeof(mic_driver.failed_reason),
        "%s %s",
        "mic_get_ndevices:",
        dl_mic_get_error_string());

    err = EML_UNKNOWN;
    goto err_free_unlink;
  }

  if (ndevices < 0) {
    snprintf(mic_driver.failed_reason, sizeof(mic_driver.failed_reason),
        "mic_get_ndevices returned %d",
        ndevices);
    err = EML_UNKNOWN;
    goto err_free_unlink;
  }

  if (ndevices > 0) {
    micdevices = malloc(ndevices * sizeof(*micdevices));
    if (!micdevices) {
      err = EML_NO_MEMORY;
      goto err_free_unlink;
    }
  }

  mic_driver.ndevices = 0;
  for (size_t i = 0; i < (size_t) ndevices; i++) {
    int devno;
    ret = dl_mic_get_device_at_index(micdeviceslist, i, &devno);
    if (ret != E_MIC_SUCCESS) {
      err = EML_NO_MEMORY;
      goto err_close_free_unlink;
    }

    ret = dl_mic_open_device(&micdevices[i], devno);
    if (ret != E_MIC_SUCCESS) {
      err = EML_NO_MEMORY;
      goto err_close_free_unlink;
    }

    mic_driver.ndevices++;
  }

  mic_driver.devices = malloc(mic_driver.ndevices * sizeof(*mic_driver.devices));
  for (size_t i = 0; i < mic_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &mic_driver,
      .index = i,
    };
    snprintf(devinit.name, sizeof(devinit.name), "%s%zu", mic_driver.name, i);

    struct emlDevice* const dev = &mic_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  mic_driver.initialized = 1;
  return EML_SUCCESS;

err_close_free_unlink:
  //close already opened MIC devices before free
  while (mic_driver.ndevices--) {
    if (dl_mic_close_device(micdevices[mic_driver.ndevices]) != E_MIC_SUCCESS) {
      dbglog_warn("mic_close_device(%zu): %s", mic_driver.ndevices, dl_mic_get_error_string());
    }
  }
  free(micdevices);
  mic_driver.ndevices = 0;

err_free_unlink:
  if (dl_mic_free_devices(micdeviceslist) != E_MIC_SUCCESS) {
    dbglog_warn("mic_free_devices: %s", dl_mic_get_error_string());
  }

err_unlink:
  dlclose(handle);

error:
  if (mic_driver.failed_reason[0] == '\0')
    strncpy(mic_driver.failed_reason, emlErrorMessage(err), sizeof(mic_driver.failed_reason));
  return err;
}

static enum emlError shutdown() {
  assert(mic_driver.initialized);

  mic_driver.initialized = 0;

  if (mic_driver.ndevices) {
    for (size_t i = 0; i < mic_driver.ndevices; i++) {
      int ret = dl_mic_close_device(micdevices[i]);
      if (ret != E_MIC_SUCCESS) {
        dbglog_warn("mic_close_device failed: %s", dl_mic_get_error_string());
      }
    }
    free(micdevices);

    int ret = dl_mic_free_devices(micdeviceslist);
    if (ret != E_MIC_SUCCESS) {
      dbglog_warn("mic_free_devices failed: %s", dl_mic_get_error_string());
    }
  }
  free(mic_driver.devices);

  dlclose(handle);

  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(mic_driver.initialized);
  assert(devno < mic_driver.ndevices);

  values[0] = nanotimestamp();

  int ret;
  struct mic_device* dev = micdevices[devno];
  struct mic_power_util_info* power_info;
  ret = dl_mic_get_power_utilization_info(dev, &power_info);
  if (ret != E_MIC_SUCCESS) {
    dbglog_error("mic_get_power_utilization_info: %s", dl_mic_get_error_string());
    return EML_UNKNOWN;
  }

  uint32_t power;
  dl_mic_get_inst_power_readings(power_info, &power);
  if (ret != E_MIC_SUCCESS) {
    dbglog_error("mic_get_inst_power_readings: %s", dl_mic_get_error_string());
    return EML_UNKNOWN;
  }
  values[mic_driver.default_props->inst_power_field * DATABLOCK_SIZE] = power;

  dl_mic_free_power_utilization_info(power_info);
  if (ret != E_MIC_SUCCESS) {
    dbglog_warn("mic_free_power_utilization_info: %s", dl_mic_get_error_string());
  }

  return EML_SUCCESS;
}

// default measurement properties for this driver
static const struct emlDataProperties default_props = {
  .time_factor = EML_SI_NANO,
  .energy_factor = EML_SI_MICRO,
  .power_factor = EML_SI_MICRO,
  .inst_energy_field = 0,
  .inst_power_field = 1,
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL("disabled", cfg_false, CFGF_NONE),
  CFG_INT("sampling_interval", MIC_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
  CFG_END()
};

//public driver state and interface
struct emlDriver mic_driver = {
  .name = "mic",
  .type = EML_DEV_MIC,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,
  .config = NULL,

  .init = &init,
  .shutdown = &shutdown,
  .measure = &measure,
};
