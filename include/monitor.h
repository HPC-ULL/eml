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
 * Internal functions dealing with device monitoring
 * @ingroup internalapi
 */

#ifndef EML_MONITOR_H
#define EML_MONITOR_H

#include "error.h"

struct emlData;
struct emlDevice;

/**
 * Initialize a device monitor
 *
 * @param[in] device Device whose monitor is to be initialized
 *
 * @retval EML_SUCCESS The device monitor was initialized
 */
enum emlError emlDeviceMonitorInit(struct emlDevice* device);

/**
 * Shut down a device monitor
 *
 * @param[in] device Device whose monitor is to be shut down
 *
 * @retval EML_SUCCESS The device monitor was shut down
 */
enum emlError emlDeviceMonitorShutdown(struct emlDevice* device);

/**
 * Start a monitored section on a device monitor
 *
 * @param[in] device Device whose monitor is to be started
 *
 * @retval EML_SUCCESS The monitored section was started
 */
enum emlError emlDeviceMonitorStart(const struct emlDevice* device);

/**
 * Stop a monitored section on a device monitor and return results
 *
 * @param[in] device Device whose monitor is to be started
 * @param[out] result Address where a handle to the data will be copied
 *
 * @retval EML_SUCCESS The monitored section was stopped
 */
enum emlError emlDeviceMonitorStop(const struct emlDevice* device, struct emlData** result);

#endif /*EML_MONITOR_H*/
