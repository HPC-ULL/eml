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
#define MAX_NAME_LEN 20

struct emlMonitor;

/** Holds information about a measurable device */
struct emlDevice {
  /** Device type */
  const enum emlDeviceType type;

  /** Device index within its type */
  const size_t index;

  /** Driver associated to this device */
  const struct emlDriver* const driver;

  /** Internal device name.
   *
   * For example, "rapl-0" or "nvml-2".
   */
  char name[MAX_NAME_LEN];

  /** Device monitor */
  struct emlMonitor* monitor;
};

#endif /*EML_DEVICE_H*/
