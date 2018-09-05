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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <confuse.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

struct emlDriver odroid_driver;

//local state
static int* odroidfd;
size_t sensor_amount; 

#define ODROID_BUFSIZ 8 // INA231 Sensor returns an 8 char float 
#define ODROID_MAX_SENSORS 10 

//MSR_PKG_ENERGY_STATUS is updated every 263808~us as defined by /sys/bus/i2c/drivers/INA231/*/update_period
#define ODROID_DEFAULT_SAMPLING_INTERVAL 263808000L

#define ODROID_SENSORS_PATH "/sys/bus/i2c/drivers/INA231"
#define ODROID_POWER_SENSOR "sensor_W"
#define ODROID_SENSOR_ENABLED "enable"
#define ODROID_UPDATE_INTERVAL_FILE "update_period" 


static enum emlError measurement_enabled(char *sensor, int *enabled) {
  char filename[BUFSIZ];
  snprintf(filename, sizeof(filename), ODROID_SENSORS_PATH"/%s/"ODROID_SENSOR_ENABLED, sensor);

  int enablefd = open(filename, O_RDONLY);
  char is_enabled[ODROID_BUFSIZ];

  int err = read(enablefd, is_enabled, ODROID_BUFSIZ);
  if (err < 0) {
    if (errno == ENXIO || errno == EIO)
      return EML_UNSUPPORTED;
    else if (errno == EACCES)
      return EML_NO_PERMISSION;
    else
      return err;
  }

  close(enablefd);
  (*enabled) = atoi(is_enabled);
  return EML_SUCCESS;
}


static enum emlError find_sensors(char **sensors, size_t* sensor_amount) {
  DIR *dp;
  struct dirent *ep;
  char path[BUFSIZ];
  int enabled;

  dp = opendir(ODROID_SENSORS_PATH);
  if (dp != NULL) {
    while ((ep = readdir(dp))) {
      if (ep->d_name[0] != '.') {
        struct stat path_stat;
        sprintf(path, ODROID_SENSORS_PATH"/%s", ep->d_name);
        lstat(path, &path_stat);
        if ((S_ISDIR(path_stat.st_mode) || S_ISLNK(path_stat.st_mode))
            && ep->d_name[1] == '-') {
            enabled = 0;
            measurement_enabled(ep->d_name, &enabled); 
            if (!enabled) {
              dbglog_warn("ODROID INA231 '%s' sensor was found, but is not enabled", ep->d_name);
              continue;
            }
            sensors[(*sensor_amount)] = malloc(BUFSIZ);
            strcpy(sensors[(*sensor_amount)], ep->d_name);
            (*sensor_amount)++;
        }
      }
    }
    closedir(dp);
  } else {
    return EML_INVALID_PARAMETER;
  }
  
  return EML_SUCCESS;
}

static enum emlError open_sensor(size_t sensor_index, char *sensor) {
  char filename[BUFSIZ];
  sprintf(filename, ODROID_SENSORS_PATH"/%s/"ODROID_POWER_SENSOR, sensor);

  odroidfd[sensor_index] = open(filename, O_RDONLY);
  if (odroidfd[sensor_index] < 0) {
    if (errno == ENXIO || errno == EIO)
      return EML_UNSUPPORTED;
    else if (errno == EACCES)
      return EML_NO_PERMISSION;
    else
      return EML_UNKNOWN;
  }

  return EML_SUCCESS;
}

static enum emlError read_sensor(size_t sensor_index, unsigned long long* value) {
  char aux[ODROID_BUFSIZ]; 
  int err = pread(odroidfd[sensor_index], &aux, ODROID_BUFSIZ, 0);
  if (err < 0) {
    snprintf(odroid_driver.failed_reason, sizeof(odroid_driver.failed_reason),
         "read_sensor(%zu, ...): %s", sensor_index, strerror(errno));
    if (errno == ENXIO || errno == EIO)
      return EML_UNSUPPORTED;
    else if (errno == EACCES)
      return EML_NO_PERMISSION;
    else
      return err;
  }

  (*value) = (unsigned long long)(atof(aux) * EML_SI_MEGA);
  return EML_SUCCESS;
}


static enum emlError init(cfg_t* const config) {
  assert(!odroid_driver.initialized);
  assert(config);
  odroid_driver.config = config;

  enum emlError err;
  char **sensors = malloc(ODROID_MAX_SENSORS * sizeof(char *));
  size_t sensor_index = 0; // If not initialized, the free operation could fail
  sensor_amount = 0;

  err = find_sensors(sensors, &sensor_amount); // Allocates sensors and odroidfd
                                               // Sensor_amount is a global var, but it is passed 
                                               // as parameter to improve code legibility 
  if (err != EML_SUCCESS)
    goto err_free;

  odroidfd = malloc(sizeof(odroidfd) * sensor_amount);
  for (sensor_index = 0; sensor_index < sensor_amount; sensor_index++) {
    err = open_sensor(sensor_index, sensors[sensor_index]);
    if (err != EML_SUCCESS) {
      snprintf(odroid_driver.failed_reason, sizeof(odroid_driver.failed_reason),
          "open_sensor(%zu): %s", sensor_index, strerror(errno));
      goto err_free_fd;
    }
    free(sensors[sensor_index]);
  }
  free(sensors);


  odroid_driver.ndevices = sensor_amount? 1 : 0;
  odroid_driver.devices = malloc(odroid_driver.ndevices * sizeof(*odroid_driver.devices));
  for (size_t i = 0; i < odroid_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &odroid_driver,
      .index = i,
    };
    sprintf(devinit.name, "%s%zu", odroid_driver.name, i);

    struct emlDevice* const dev = &odroid_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  odroid_driver.initialized = 1;
  return EML_SUCCESS;

err_free_fd:
  free(odroidfd);

err_free:
  while (sensor_index < sensor_amount) {
    free(sensors[sensor_index]);
    sensor_index++;
  }
  free(sensors);

  if (odroid_driver.failed_reason[0] == '\0')
    strncpy(odroid_driver.failed_reason, emlErrorMessage(err), sizeof(odroid_driver.failed_reason) - 1);
  return err;
}


static enum emlError shutdown() {
  assert(odroid_driver.initialized);

  odroid_driver.initialized = 0;

  for (size_t sensor_index = 0; sensor_index < sensor_amount; sensor_index++) {
    close(odroidfd[sensor_index]);
  }

  free(odroidfd);
  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(odroid_driver.initialized);
  assert(devno < odroid_driver.ndevices); // Shouldn't be more than one anyways

  enum emlError err;

  values[0] = millitimestamp();

  unsigned long long power = 0;
  unsigned long long aux = 0;

  for (size_t sensor_index = 0; sensor_index < sensor_amount; sensor_index++) {
    err = read_sensor(sensor_index, &aux);
    if (err != EML_SUCCESS)  
      return err; 
    power += aux;
  }
  values[odroid_driver.default_props->inst_power_field * DATABLOCK_SIZE] = power; 
  return EML_SUCCESS;
}

// default measurement properties for this driver
static struct emlDataProperties default_props = {
  .time_factor = EML_SI_MILLI,
  .energy_factor = EML_SI_MICRO, // Measurement is in W, but to store on a long, it is multiplied by EML_SI_MEGA
//  .power_factor = EML_SI_MICRO,  
  .inst_energy_field = 0,
  .inst_power_field = 1,
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL("disabled", cfg_false, CFGF_NONE),
  CFG_INT("sampling_interval", ODROID_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
  CFG_END()
};

//public driver state and interface
struct emlDriver odroid_driver = {
  .name = "odroid",
  .type = EML_DEV_ODROID,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,
  .config = NULL,

  .init = &init,
  .shutdown = &shutdown,
  .measure = &measure,
};
