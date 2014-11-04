/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

/**
 * @file
 * Internal definitions for devices
 * @ingroup internalapi
 */

#ifndef EML_DEVICE_H
#define EML_DEVICE_H

#include <eml/device.h>

/** Maximum internal name length */
#define EML_DEVNAME_MAXLEN 40

struct emlMonitor;

/** Holds information about a measurable device */
struct emlDevice {
  /** Driver associated to this device */
  const struct emlDriver* const driver;

  /** Device index within the driver */
  const size_t index;

  /** Internal device name.
   *
   * For example, "rapl0" or "nvml2".
   */
  char name[EML_DEVNAME_MAXLEN];

  /** Device monitoring state */
  struct emlMonitor* monitor;
};

#endif /*EML_DEVICE_H*/
