/*
 * Copyright (c) 2018 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * @author: Alberto Cabrera Perez <Alberto.Cabrera@ull.edu.es>
 */

#define _XOPEN_SOURCE 500

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
#include <confuse.h>
//#include <fcntl.h>
//#include <sys/stat.h>
//#include <unistd.h>
//
#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

#define LABEE_DEFAULT_SAMPLING_INTERVAL 100000000L // 100ms

struct emlDriver labee_driver;


static enum emlError init(cfg_t* const config) {
  assert(!labee_driver.initialized);
  assert(config);
  labee_driver.config = config;

  enum emlError err;

  // TODO Check if the API is up
  
  // FUnction that generates error code
  if (err != EML_SUCCESS)
    goto err_free;
  
  labee_driver.ndevices = 1;
  labee_driver.devices = malloc(labee_driver.ndevices * sizeof(*labee_driver.devices));
  for (size_t i = 0; i < labee_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &labee_driver,
      .index = i,
    };
    sprintf(devinit.name, "%s%zu", labee_driver.name, i);

    struct emlDevice* const dev = &labee_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  labee_driver.initialized = 1;
  return EML_SUCCESS;

err_free:
  if (labee_driver.failed_reason[0] == '\0')
    strncpy(labee_driver.failed_reason, emlErrorMessage(err), sizeof(labee_driver.failed_reason) - 1);
  return err;
}


static enum emlError shutdown() {
  assert(labee_driver.initialized);

  labee_driver.initialized = 0;

  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(labee_driver.initialized);
  assert(devno < labee_driver.ndevices); // Shouldn't be more than one anyways

  enum emlError err;

  values[0] = millitimestamp();
  const long sampling_interval = cfg_getint(labee_driver.config, "sampling_interval");
  unsigned long long power = 1 / sampling_interval;

  //TODO parse xml from rest api apply average proportionally to interval 
  values[labee_driver.default_props->inst_energy_field * DATABLOCK_SIZE] = power; 
  return EML_SUCCESS;
}

// default measurement properties for this driver
static struct emlDataProperties default_props = {
  .time_factor = EML_SI_MILLI,
  .energy_factor = EML_SI_MICRO, // Measurement is in J, but to store on a long, it is multiplied by EML_SI_MEGA
  .power_factor = EML_SI_MICRO,  // Measurement is in W, same principle, with EML_SI_MEGA 
  .inst_energy_field = 1,
  .inst_power_field = 2,
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL("disabled", cfg_false, CFGF_NONE),
  CFG_INT("sampling_interval", LABEE_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
  CFG_END()
};

//public driver state and interface
struct emlDriver labee_driver = {
  .name = "labee",
  .type = EML_DEV_LABEE,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,
  .config = NULL,

  .init = &init,
  .shutdown = &shutdown,
  .measure = &measure,
};
