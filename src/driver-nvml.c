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

#include <dlfcn.h>
#include <nvml.h>

#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

struct emlDriver nvml_driver;

//local state
static void* handle;
static nvmlDevice_t* nvmldevices;

//imported funcs
static nvmlReturn_t (*dl_nvmlInit)();
static nvmlReturn_t (*dl_nvmlDeviceGetCount)(unsigned int*);
static nvmlReturn_t (*dl_nvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
static nvmlReturn_t (*dl_nvmlDeviceGetPowerManagementMode)(nvmlDevice_t, nvmlEnableState_t*);
static nvmlReturn_t (*dl_nvmlDeviceGetPowerUsage)(nvmlDevice_t, unsigned int*);
static const char* (*dl_nvmlErrorString)(nvmlReturn_t);
static nvmlReturn_t (*dl_nvmlShutdown)();

static enum emlError link_nvml() {
  //clear any existing error
  dlerror();

  handle = dlopen("libnvidia-ml.so", RTLD_LAZY);
  if (dlerror()) {
    strncpy(nvml_driver.failed_reason, dlerror(), sizeof(nvml_driver.failed_reason));
    return EML_LIBRARY_UNAVAILABLE;
  }

  *(void **) (&dl_nvmlInit) = dlsym(handle, "nvmlInit");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_nvmlDeviceGetCount) = dlsym(handle, "nvmlDeviceGetCount");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_nvmlDeviceGetHandleByIndex) = dlsym(handle, "nvmlDeviceGetHandleByIndex");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_nvmlDeviceGetPowerUsage) = dlsym(handle, "nvmlDeviceGetPowerUsage");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_nvmlDeviceGetPowerManagementMode) = dlsym(handle, "nvmlDeviceGetPowerManagementMode");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_nvmlErrorString) = dlsym(handle, "nvmlErrorString");
  if (dlerror()) goto err_unlink;

  *(void **) (&dl_nvmlShutdown) = dlsym(handle, "nvmlShutdown");
  if (dlerror()) goto err_unlink;

  return EML_SUCCESS;

err_unlink:
  strncpy(nvml_driver.failed_reason, dlerror(), sizeof(nvml_driver.failed_reason));
  dlclose(handle);

  return EML_SYMBOL_UNAVAILABLE;
}

static enum emlError init() {
  assert(!nvml_driver.initialized);

  enum emlError err;

  err = link_nvml();
  if (err != EML_SUCCESS)
    goto error;

  nvmlReturn_t ret;
  ret = dl_nvmlInit();
  if (ret != NVML_SUCCESS) {
    snprintf(nvml_driver.failed_reason, sizeof(nvml_driver.failed_reason),
        "%s %s",
        "nvmlInit:",
        dl_nvmlErrorString(ret));

    err = EML_UNKNOWN;
    goto err_shutdown;
  }

  unsigned int ndevices;
  ret = dl_nvmlDeviceGetCount(&ndevices);
  assert(ret != NVML_ERROR_UNINITIALIZED);
  assert(ret != NVML_ERROR_INVALID_ARGUMENT);
  if (ret != NVML_SUCCESS) {
    snprintf(nvml_driver.failed_reason, sizeof(nvml_driver.failed_reason),
        "%s %s",
        "nvmlDeviceGetCount:",
        dl_nvmlErrorString(ret));

    err = EML_UNKNOWN;
    goto err_shutdown;
  }

  if (ndevices > 0) {
    nvmldevices = malloc(ndevices * sizeof(*nvmldevices));
    if (!nvmldevices) {
      err = EML_NO_MEMORY;
      goto err_shutdown;
    }
  }

  //store device handles (for nvmlDeviceGetPowerUsage supported devices only)
  nvml_driver.ndevices = 0;
  for (size_t i = 0; i < ndevices; i++) {
    size_t last = nvml_driver.ndevices;
    ret = dl_nvmlDeviceGetHandleByIndex(i, &nvmldevices[last]);
    if (ret != NVML_SUCCESS) {
      assert(ret != NVML_ERROR_INVALID_ARGUMENT);
      snprintf(nvml_driver.failed_reason, sizeof(nvml_driver.failed_reason),
          "%s %s",
          "nvmlDeviceGetHandleByIndex:",
          dl_nvmlErrorString(ret));

      err = EML_UNKNOWN;
      goto err_free_shutdown;
    }

    nvmlEnableState_t mode;
    ret = dl_nvmlDeviceGetPowerManagementMode(nvmldevices[last], &mode);
    if (ret != NVML_SUCCESS) {
      assert(ret != NVML_ERROR_INVALID_ARGUMENT);
      snprintf(nvml_driver.failed_reason, sizeof(nvml_driver.failed_reason),
          "%s %s",
          "nvmlDeviceGetPowerManagementMode:",
          dl_nvmlErrorString(ret));

      err = EML_UNKNOWN;
      goto err_free_shutdown;
    }

    if (mode == NVML_FEATURE_ENABLED) {
      nvml_driver.ndevices++;
    }
    else {
      dbglog_info("NVML device %zu does not support power usage readings", i);
    }
  }

  //free device handle memory for unsupported devices
  if (nvml_driver.ndevices < ndevices) {
    nvmlDevice_t* resized = malloc(nvml_driver.ndevices * sizeof(*resized));
    if (resized)
      nvmldevices = resized;
  }

  nvml_driver.devices = malloc(nvml_driver.ndevices * sizeof(*nvml_driver.devices));
  for (size_t i = 0; i < nvml_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &nvml_driver,
      .index = i,
    };
    snprintf(devinit.name, sizeof(devinit.name), "%s%zu", nvml_driver.name, i);

    struct emlDevice* const dev = &nvml_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  nvml_driver.initialized = 1;
  return EML_SUCCESS;

err_free_shutdown:
  free(nvmldevices);

err_shutdown:
  ret = dl_nvmlShutdown();
  if (ret != NVML_SUCCESS) {
    dbglog_warn("nvmlShutdown: %s", dl_nvmlErrorString(ret));
  }
  dlclose(handle);

error:
  if (nvml_driver.failed_reason[0] == '\0')
    strncpy(nvml_driver.failed_reason, emlErrorMessage(err), sizeof(nvml_driver.failed_reason));
  return err;
}

static enum emlError shutdown() {
  assert(nvml_driver.initialized);

  nvml_driver.initialized = 0;

  nvmlReturn_t ret;

  ret = dl_nvmlShutdown();
  if (ret != NVML_SUCCESS) {
    dbglog_warn("nvmlShutdown: %s", dl_nvmlErrorString(ret));
  }

  if (nvml_driver.ndevices) {
    free(nvmldevices);
  }
  free(nvml_driver.devices);

  dlclose(handle);

  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(nvml_driver.initialized);
  assert(devno < nvml_driver.ndevices);

  values[0] = nanotimestamp();

  nvmlReturn_t ret;
  unsigned int power;
  ret = dl_nvmlDeviceGetPowerUsage(nvmldevices[devno], &power);
  if (ret != NVML_SUCCESS) {
    dbglog_error("nvmlDeviceGetPowerUsage %s", dl_nvmlErrorString(ret));
    return EML_UNKNOWN;
  }
  else if (!power) {
    dbglog_warn("nvmlDeviceGetPowerUsage returned 0, no error code");
  }
  values[nvml_driver.default_props->inst_power_field * DATABLOCK_SIZE] = power;

  return EML_SUCCESS;
}

// default measurement properties for this driver
static const struct emlDataProperties default_props = {
  .time_factor = EML_SI_NANO,
  .energy_factor = EML_SI_MILLI,
  .power_factor = EML_SI_MILLI,
  .inst_energy_field = 0,
  .inst_power_field = 1,
  //Fermi GPU power readings are updated every ~16ms
  .sampling_nanos = 16000000L,
};


//public driver state and interface
struct emlDriver nvml_driver = {
  .name = "nvml",
  .type = EML_DEV_NVML,
  .failed_reason = "",
  .default_props = &default_props,

  .init = &init,
  .shutdown = &shutdown,
  .measure = &measure,
};
