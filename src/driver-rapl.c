/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
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

#include <confuse.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

struct emlDriver rapl_driver;

//MSR_PKG_ENERGY_STATUS is updated every ~1ms
#define RAPL_DEFAULT_SAMPLING_INTERVAL 1000000L

static const size_t MSR_SIZE = 8;
static const unsigned long long WRAP_VALUE = 1ULL << 32;

struct msr_config {
  //Units register
  off_t MSR_RAPL_POWER_UNIT;
  //Package RAPL domain
  off_t MSR_PKG_RAPL_POWER_LIMIT;
  off_t MSR_PKG_ENERGY_STATUS;
  off_t MSR_PKG_PERF_STATUS;
  off_t MSR_PKG_POWER_INFO;
  //Power plane PP0 RAPL domain
  off_t MSR_PP0_POWER_LIMIT;
  off_t MSR_PP0_ENERGY_STATUS;
  off_t MSR_PP0_POLICY;
  off_t MSR_PP0_PERF_STATUS;
  //Power plane PP1 RAPL domain
  off_t MSR_PP1_POWER_LIMIT;
  off_t MSR_PP1_ENERGY_STATUS;
  off_t MSR_PP1_POLICY;
  //DRAM RAPL domain
  off_t MSR_DRAM_POWER_LIMIT;
  off_t MSR_DRAM_ENERGY_STATUS;
  off_t MSR_DRAM_PERF_STATUS;
  off_t MSR_DRAM_POWER_INFO;

  //MSR_RAPL_POWER_UNIT fields
  off_t POWER_UNIT_OFFSET;
  off_t POWER_UNIT_MASK;
  off_t ENERGY_UNIT_OFFSET;
  off_t ENERGY_UNIT_MASK;
  off_t TIME_UNIT_OFFSET;
  off_t TIME_UNIT_MASK;
};

static const struct msr_config default_msr_config = {
  .MSR_RAPL_POWER_UNIT = 0x606,
  .MSR_PKG_RAPL_POWER_LIMIT = 0x610,
  .MSR_PKG_ENERGY_STATUS = 0x611,
  .MSR_PKG_PERF_STATUS = 0x613,
  .MSR_PKG_POWER_INFO = 0x614,
  .MSR_PP0_POWER_LIMIT = 0x638,
  .MSR_PP0_ENERGY_STATUS = 0x639,
  .MSR_PP0_POLICY = 0x63A,
  .MSR_PP0_PERF_STATUS = 0x63B,
  .MSR_PP1_POWER_LIMIT = 0x640,
  .MSR_PP1_ENERGY_STATUS = 0x641,
  .MSR_PP1_POLICY = 0x642,
  .MSR_DRAM_POWER_LIMIT = 0x618,
  .MSR_DRAM_ENERGY_STATUS = 0x619,
  .MSR_DRAM_PERF_STATUS = 0x61B,
  .MSR_DRAM_POWER_INFO = 0x61C,
  .POWER_UNIT_OFFSET = 0x0,
  .POWER_UNIT_MASK = 0x0F,
  .ENERGY_UNIT_OFFSET = 0x08,
  .ENERGY_UNIT_MASK = 0x1F00,
  .TIME_UNIT_OFFSET = 0x10,
  .TIME_UNIT_MASK = 0xF000,
};

//local state
static int* msrfd;
static size_t npackages;
static size_t ncores;
static size_t* package_for_core;
static size_t* core_from_package;
static unsigned long long* prev_energy;
static const struct msr_config* cfg = &default_msr_config;

//unit scaling divisors from MSR_RAPL_POWER_UNIT
//initialized to default values from:
//  Intel dev manual volume 3, 14.9.1 RAPL Interfaces
static unsigned int power_divisor = 1 << 0x3;
static unsigned int energy_divisor = 1 << 0x10;
static unsigned int time_divisor = 1 << 0xA;

static struct emlDataProperties default_props;

static enum emlError open_msr(size_t core) {
  char filename[BUFSIZ];
  snprintf(filename, sizeof(filename), "/dev/cpu/%zu/msr", core);

  msrfd[core] = open(filename, O_RDONLY);
  if (msrfd[core] < 0) {
    if (errno == ENXIO || errno == EIO)
      return EML_UNSUPPORTED;
    else if (errno == EACCES)
      return EML_NO_PERMISSION;
    else
      return EML_UNKNOWN;
  }

  return EML_SUCCESS;
}

static int read_msr(size_t core, off_t offset, unsigned long long* value) {
  return (pread(msrfd[core], value, MSR_SIZE, offset) != (ssize_t) MSR_SIZE);
}

static int parse_size(const char* from, size_t* to) {
  char* endptr;
  const int base = 10;
  errno = 0;
  *to = strtoul(from, &endptr, base);
  if (errno == ERANGE || *to > SIZE_MAX)
    return 1;
  //we only accept a trailing \n, for convenience
  if (*endptr != '\0' && *endptr != '\n')
    return 1;
  return 0;
}

static int is_cpu_model_supported(int model) {
  enum cpu_model {
    CPU_SANDYBRIDGE = 42,
    CPU_SANDYBRIDGE_EP = 45,
    CPU_IVYBRIDGE = 58,
    CPU_IVYBRIDGE_EP = 62,
    CPU_HASWELL_1 = 60,
    CPU_HASWELL_2 = 69,
    CPU_HASWELL_3 = 70,
    CPU_HASWELL_EP = 63,
    CPU_BROADWELL_1 = 61,
    CPU_BROADWELL_2 = 71,
    CPU_BROADWELL_EP = 79,
    CPU_BROADWELL_DE = 86,
    CPU_SKYLAKE_1 = 78,
    CPU_SKYLAKE_2 = 94,
    CPU_SKYLAKE_3 = 85,
    CPU_KABYLAKE_1 = 142,
    CPU_KABYLAKE_2 = 158,
  };

  switch(model) {
    case CPU_SANDYBRIDGE:
    case CPU_SANDYBRIDGE_EP:
    case CPU_IVYBRIDGE:
    case CPU_IVYBRIDGE_EP:
    case CPU_HASWELL_1:
    case CPU_HASWELL_2:
    case CPU_HASWELL_3:
    case CPU_HASWELL_EP:
    case CPU_BROADWELL_1:
    case CPU_BROADWELL_2:
    case CPU_BROADWELL_EP:
    case CPU_BROADWELL_DE:
    case CPU_SKYLAKE_1:
    case CPU_SKYLAKE_2:
    case CPU_SKYLAKE_3:
    case CPU_KABYLAKE_1:
    case CPU_KABYLAKE_2:
      return 1;
    default:
      return 0;
  }
}

static enum emlError find_supported_cpu() {
  const unsigned int supported_family = 6;
  const char supported_vendor[] = "GenuineIntel";
  const char vendorfld[] = "vendor_id";
  const char familyfld[] = "cpu family";
  const char modelfld[] = "model";

  char buffer[BUFSIZ];
  enum emlError err;

  const char* filename = "/proc/cpuinfo";
  FILE* file = fopen(filename, "r");
  if (!file) {
    err = EML_UNSUPPORTED;
    snprintf(rapl_driver.failed_reason, sizeof(rapl_driver.failed_reason),
        "%s: %s", emlErrorMessage(err), filename);
    return err;
  }

  const char* line;
  err = EML_SUCCESS;
  while ((line = fgets(buffer, sizeof(buffer), file))) {
    size_t fldlen = 0;

    //find separator and key length
    const char* p;
    for (p = line; *p && *p != ':'; p++) {
      if (!isspace(*p))
        fldlen = p - line;
    }

    //skip this line if no key or no separator
    if (!fldlen || !*p) continue;

    //skip whitespace after separator
    p++;
    while (isspace(*p)) p++;

    //skip this line if no value
    if (!*p) continue;

    if (!strncmp(line, vendorfld, fldlen))
      if (strncmp(p, supported_vendor, sizeof(supported_vendor)-1)) {
        err = EML_UNSUPPORTED_HARDWARE;
        break;
      }

    if (!strncmp(line, familyfld, fldlen)) {
      size_t family;
      int fmterr = parse_size(p, &family);
      if (fmterr || (family != supported_family)) {
        err = EML_UNSUPPORTED_HARDWARE;
        break;
      }
    }

    if (!strncmp(line, modelfld, fldlen)) {
      size_t model;
      int fmterr = parse_size(p, &model);
      if (fmterr || !is_cpu_model_supported(model)) {
        err = EML_UNSUPPORTED_HARDWARE;
        break;
      }
    }
  }

  fclose(file);
  return err;
}

static enum emlError get_cpu_topology() {
  enum emlError err;
  char buffer[BUFSIZ];
  FILE* file;
  const char* pos;

  const char* filename = "/sys/devices/system/cpu/present";
  file = fopen(filename, "r");
  if (!file) {
    err = EML_UNSUPPORTED;
    snprintf(rapl_driver.failed_reason, sizeof(rapl_driver.failed_reason),
        "%s: %s", emlErrorMessage(err), filename);
    return err;
  }

  pos = fgets(buffer, sizeof(buffer), file);
  fclose(file);
  if (!pos)
    return EML_PARSING_ERROR;

  //skip to start of last present CPU
  pos = strchr(pos, '-');
  if (!pos)
    return EML_PARSING_ERROR;
  pos++;

  int fmterr = parse_size(pos, &ncores);
  if (fmterr || !ncores)
    return EML_PARSING_ERROR;

  int package_found[ncores];
  for (size_t i = 0; i < ncores; i++)
    package_found[i] = 0;

  msrfd = malloc(ncores * sizeof(*msrfd));
  package_for_core = malloc(ncores * sizeof(*package_for_core));
  core_from_package = malloc(ncores * sizeof(*core_from_package));
  if (!msrfd || !package_for_core || !core_from_package) {
    err = EML_NO_MEMORY;
    goto err_free;
  }

  npackages = 0;
  for (size_t i = 0; i < ncores; i++) {
    snprintf(buffer, sizeof(buffer),
        "/sys/devices/system/cpu/cpu%zu/topology/physical_package_id", i);
    file = fopen(buffer, "r");
    if (!file) {
      err = EML_UNSUPPORTED;
      snprintf(rapl_driver.failed_reason, sizeof(rapl_driver.failed_reason),
          "%s: %s", emlErrorMessage(err), buffer);
      goto err_free;
    }

    pos = fgets(buffer, sizeof(buffer), file);
    fclose(file);
    if (!pos) {
      err = EML_PARSING_ERROR;
      goto err_free;
    }

    size_t pkg;
    int fmterr = parse_size(pos, &pkg);
    if (fmterr) {
      err = EML_PARSING_ERROR;
      goto err_free;
    }

    if (!package_found[pkg]) {
      package_for_core[i] = pkg;
      core_from_package[pkg] = i;
      package_found[pkg] = 1;
      npackages++;
    }
  }

  prev_energy = malloc(npackages * sizeof(*prev_energy));
  if (!prev_energy) {
    err = EML_NO_MEMORY;
    goto err_free_2;
  }
  for (size_t i = 0; i < npackages; i++)
    prev_energy[i] = WRAP_VALUE;

  return EML_SUCCESS;

err_free_2:
  free(prev_energy);

err_free:
  free(msrfd);
  free(package_for_core);
  free(core_from_package);

  return err;
}

static enum emlError init(cfg_t* const config) {
  assert(!rapl_driver.initialized);
  assert(config);
  rapl_driver.config = config;

  enum emlError err;

  err = find_supported_cpu();
  if (err != EML_SUCCESS)
    goto error;

  err = get_cpu_topology();
  if (err != EML_SUCCESS)
    goto error;

  for (size_t i = 0; i < ncores; i++) {
    err = open_msr(i);
    if (err != EML_SUCCESS) {
      snprintf(rapl_driver.failed_reason, sizeof(rapl_driver.failed_reason),
          "open_msr(%zu): %s", i, strerror(errno));
      goto err_free;
    }
  }

  unsigned long long units;
  int read_error = read_msr(0, cfg->MSR_RAPL_POWER_UNIT, &units);
  if (read_error) {
    snprintf(rapl_driver.failed_reason, sizeof(rapl_driver.failed_reason),
        "read_msr(0, MSR_RAPL_POWER_UNIT): %s", strerror(errno));
    goto err_free;
  }

  power_divisor = 1 << ((units & cfg->POWER_UNIT_MASK) >> cfg->POWER_UNIT_OFFSET);
  energy_divisor = 1 << ((units & cfg->ENERGY_UNIT_MASK) >> cfg->ENERGY_UNIT_OFFSET);
  time_divisor = 1 << ((units & cfg->TIME_UNIT_MASK) >> cfg->TIME_UNIT_OFFSET);

  default_props.energy_factor = -energy_divisor;

  rapl_driver.ndevices = npackages;

  rapl_driver.devices = malloc(rapl_driver.ndevices * sizeof(*rapl_driver.devices));
  for (size_t i = 0; i < rapl_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &rapl_driver,
      .index = i,
    };
    snprintf(devinit.name, sizeof(devinit.name), "%s%zu", rapl_driver.name, i);

    struct emlDevice* const dev = &rapl_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  rapl_driver.initialized = 1;

  return EML_SUCCESS;

err_free:
  free(prev_energy);
  free(msrfd);
  free(package_for_core);
  free(core_from_package);

error:
  if (rapl_driver.failed_reason[0] == '\0')
    strncpy(rapl_driver.failed_reason, emlErrorMessage(err), sizeof(rapl_driver.failed_reason) - 1);
  return err;
}

static enum emlError shutdown() {
  assert(rapl_driver.initialized);

  rapl_driver.initialized = 0;

  for (size_t i = 0; i < ncores; i++) {
    close(msrfd[i]);
  }

  free(prev_energy);
  free(msrfd);
  free(package_for_core);
  free(core_from_package);

  free(rapl_driver.devices);

  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(rapl_driver.initialized);
  assert(devno < rapl_driver.ndevices);

  values[0] = nanotimestamp() / 1000000;

  unsigned long long energy;
  size_t core = core_from_package[devno];
  int read_error = read_msr(core, cfg->MSR_PKG_ENERGY_STATUS, &energy);
  if (read_error)
    return EML_UNKNOWN;

  //detect overflow
  unsigned long long* energyvalue = &values[rapl_driver.default_props->inst_energy_field * DATABLOCK_SIZE];
  if (prev_energy[devno] == WRAP_VALUE)
    *energyvalue = 0;
  else if (energy < prev_energy[devno])
    *energyvalue = energy + (WRAP_VALUE - prev_energy[devno]);
  else
    *energyvalue = energy - prev_energy[devno];
  prev_energy[devno] = energy;

  return EML_SUCCESS;
}

// default measurement properties for this driver
static struct emlDataProperties default_props = {
  //ENERGY_STATUS is updated every ~1ms
  .time_factor = EML_SI_MILLI,
  //.energy_factor set by init()
  //.power_factor is unused
  .inst_energy_field = 1,
  .inst_power_field = 0,
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL("disabled", cfg_false, CFGF_NONE),
  CFG_INT("sampling_interval", RAPL_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
  CFG_END()
};

//public driver state and interface
struct emlDriver rapl_driver = {
  .name = "rapl",
  .type = EML_DEV_RAPL,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,
  .config = NULL,

  .init = &init,
  .shutdown = &shutdown,
  .measure = &measure,
};
