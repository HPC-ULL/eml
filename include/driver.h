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
 * @brief Internal definitions for device drivers
 * @ingroup internalapi
 */

#ifndef EML_DRIVER_H
#define EML_DRIVER_H

#include "device.h"
#include "error.h"

struct emlData;
struct emlDataProperties;

/** Contains state, properties and methods for a device type */
struct emlDriver {
  /** Driver name */
  const char* name;

  /** Device type */
  const enum emlDeviceType type;

  /** Default measurement properties for this driver */
  const struct emlDataProperties* default_props;

  /** Whether the driver is initialized.
   *
   * 1 if it is initialized, 0 if it is not.
   */
  int initialized;

  /** Reason of initialization failure */
  char failed_reason[BUFSIZ];

  /** Device information structures */
  struct emlDevice* devices;

  /** Number of available devices */
  size_t ndevices;

  /**
   * Initializes the driver
   *
   * @retval EML_SUCCESS The driver was initialized
   */
  enum emlError (*init) ();

  /**
   * Shuts down the driver
   *
   * @retval EML_SUCCESS The driver was shut down
   */
  enum emlError (*shutdown) ();

  /**
   * Takes a measurement from a single device
   *
   * @param[in] devno ID of the device to measure
   * @param[out] values Where to write measurement values
   *
   * @retval EML_SUCCESS The measurement was taken
   */
  enum emlError (*measure) (size_t devno, unsigned long long* values);
};

#endif /*EML_DRIVER_H*/
