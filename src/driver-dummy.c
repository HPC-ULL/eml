/*
 * Copyright (c) 2017 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

//feature test macro for pread() in unistd.h
#define _XOPEN_SOURCE 500

#define DUMMY_DEFAULT_SAMPLING_INTERVAL 100000000L // ~100ms

#include <assert.h>
//#include <ctype.h>
//#include <errno.h>
//#include <stddef.h>
//#include <stdint.h>
//#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <dirent.h>
//
//#include <confuse.h>
//#include <fcntl.h>
//#include <sys/stat.h>
//#include <unistd.h>
//
#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

struct emlDriver dummy_driver;


static enum emlError init(cfg_t* const config) {
  assert(!dummy_driver.initialized);
  assert(config);
  dummy_driver.config = config;

  printf("OMG pre\n");
  dummy_driver.ndevices = 1;
  dummy_driver.devices = malloc(dummy_driver.ndevices * sizeof(*dummy_driver.devices));
  for (size_t i = 0; i < dummy_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &dummy_driver,
      .index = i,
    };
    sprintf(devinit.name, "%s%zu", dummy_driver.name, i);

    struct emlDevice* const dev = &dummy_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  printf("OMG\n");
  dummy_driver.initialized = 1;
  return EML_SUCCESS;
}


static enum emlError shutdown() {
  assert(dummy_driver.initialized);

  dummy_driver.initialized = 0;

  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(dummy_driver.initialized);
  assert(devno < dummy_driver.ndevices); // Shouldn't be more than one anyways

  values[0] = millitimestamp();
  values[dummy_driver.default_props->inst_power_field * DATABLOCK_SIZE] = values[0]; 
  return EML_SUCCESS;
}

// default measurement properties for this driver
static struct emlDataProperties default_props = {
  .time_factor = EML_SI_MILLI,
  .energy_factor = EML_SI_MILLI, // Measurement is in W, but to store on a long, it is multiplied by EML_SI_MEGA
//  .power_factor = EML_SI_MICRO,  
  .inst_energy_field = 0,
  .inst_power_field = 1,
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL("disabled", cfg_true, CFGF_NONE),
  CFG_INT("sampling_interval", DUMMY_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
  CFG_END()
};

//public driver state and interface
struct emlDriver dummy_driver = {
  .name = "dummy",
  .type = EML_DEV_DUMMY,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,
  .config = NULL,

  .init = &init,
  .shutdown = &shutdown,
  .measure = &measure,
};
