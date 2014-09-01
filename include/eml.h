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
 * @ingroup externalapi
 * Includes the entire Energy Measurement Library API
 */

#ifndef EMLAPI_H
#define EMLAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <eml/data.h>
#include <eml/device.h>
#include <eml/error.h>

/**
 * @defgroup externalapi_main Base
 * Basic high-level API calls
 * @ingroup externalapi
 * @{
 */

/**
 * Initializes EML.
 *
 * This function must be called before making any other EML call.
 *
 * @retval EML_SUCCESS The library has been properly initialized
 * @retval EML_ALREADY_INITIALIZED The library was already initialized
 * @retval EML_NO_MEMORY Insufficient memory for initialization
 * @retval EML_UNKNOWN Internal library error
 */
emlError_t emlInit();

/**
 * Shuts down EML.
 *
 * Stops all running measurements and releases all resources.
 *
 * @retval EML_SUCCESS The library has been properly shut down
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 * @retval EML_UNKNOWN Internal library error
 */
emlError_t emlShutdown();

/**
 * Begins an energy monitoring section on all available devices.
 *
 * @note Calls to emlStart can be nested. A single data collection thread will
 * run for each device for the duration of the outermost section.
 *
 * @warning EML is not yet thread-safe. Taking measurements from multiple
 * application threads simultaneously is not supported.
 *
 * @retval EML_SUCCESS Monitoring has been started
 * @retval EML_NO_MEMORY Insufficient memory for monitoring
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 * @retval EML_UNKNOWN Internal library error
 */
emlError_t emlStart();

/**
 * Ends the current monitoring section on all available devices and returns
 * consumption data.
 *
 * Ends the section marked by the last call to @ref emlStart, and stops data
 * collection threads if all sections have been closed.
 *
 * Writes an array of @ref emlData_t * (one per available device) in the memory
 * pointed by @c results.
 *
 * @note The number of devices, and thus the necessary size for @c results,
 * can be determined through @ref emlDeviceGetCount.
 *
 * @warning EML is not yet thread-safe. Taking measurements from multiple
 * application threads simultaneously is not supported.
 *
 * @param[out] results Address where data handles will be copied
 *
 * @retval EML_SUCCESS Monitoring has been stopped, and data returned
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 * @retval EML_UNKNOWN Internal library error
 */
emlError_t emlStop(emlData_t** results);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /*EMLAPI_H*/
