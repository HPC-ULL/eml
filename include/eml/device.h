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
 * @copydoc externalapi_device
 */

#ifndef EMLAPI_DEVICE_H
#define EMLAPI_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <eml/data.h>
#include <eml/error.h>

/**
 * @defgroup externalapi_device Devices
 * Definition of @ref emlDevice_t and related functions
 * @ingroup externalapi
 * @{
 */

/** Represents a device that can report energy data */
typedef struct emlDevice emlDevice_t;

/** Known device types */
typedef enum emlDeviceType {
  /** Dummy measurement for testing algorithms */
  EML_DEV_DUMMY = 0,
  /** Nvidia cards supporting power readings through NVML */
  EML_DEV_NVML = 1,
  /** Intel CPUs supporting energy counters through RAPL */
  EML_DEV_RAPL = 2,
  /** Intel MICs (Xeon Phi) */
  EML_DEV_MIC = 3,
  /** Schleifenbauer PDUs */
  EML_DEV_SB_PDU = 4,
  /** Odroid with sensor support */
  EML_DEV_ODROID = 5,
  /** Labee(PSNN) Rest interface */
  EML_DEV_LABEE = 6,
  /** PMLib interface */
  EML_DEV_PMLIB = 7,
  /** Number of supported device types */
  EML_DEVICE_TYPE_COUNT
} emlDeviceType_t;

/** Device type support status */
typedef enum emlDeviceTypeStatus {
  /** This type is available for measurements */
  EML_SUPPORT_AVAILABLE = 0,
  /** Support for this type was disabled at EML compile time */
  EML_SUPPORT_NOT_COMPILED = 1,
  /** Runtime support for this type is missing */
  EML_SUPPORT_NOT_RUNTIME = 2,
} emlDeviceTypeStatus_t;

/**
 * Retrieves the number of supported devices detected.
 *
 * @param[out] count Reference in which to return device count
 *
 * @retval EML_SUCCESS @a count has been set
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 */
emlError_t emlDeviceGetCount(size_t* count);

/**
 * Provides a device handle from its index.
 *
 * Valid indexes go from 0 to the count returned by @ref emlDeviceGetCount.
 *
 * @param[in] index Device index
 * @param[out] device Reference in which to return device handle
 *
 * @retval EML_SUCCESS @a device has been set
 * @retval EML_INVALID_PARAMETER @a index is invalid
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 */
emlError_t emlDeviceByIndex(size_t index, emlDevice_t** device);

/**
 * Retrieves the internal name for a device.
 *
 * Internal names are assigned in the form [type]-[id], such as "rapl-0" or
 * "nvml-2".
 *
 * @param[in] device Target device
 * @param[out] name Reference in which to return the internal name
 *
 * @retval EML_SUCCESS @a name has been set
 * @retval EML_INVALID_PARAMETER @a device is invalid
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 */
emlError_t emlDeviceGetName(const emlDevice_t* device, const char** name);

/**
 * Retrieves the type of a device.
 *
 * @param[in] device Target device
 * @param[out] type Reference in which to return device type
 *
 * @retval EML_SUCCESS @a type has been set
 * @retval EML_INVALID_PARAMETER @a device is invalid
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 */
emlError_t emlDeviceGetType(const emlDevice_t* device, emlDeviceType_t* type);

/**
 * Retrieves support status for the device type.
 *
 * @param[in] type Target type
 * @param[out] status Reference in which to return type support status
 *
 * @retval EML_SUCCESS @a status has been set
 * @retval EML_INVALID_PARAMETER @a type is invalid
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 */
emlError_t emlDeviceGetTypeStatus(emlDeviceType_t type, emlDeviceTypeStatus_t* status);

/**
 * Begins an energy monitoring section on a specific device.
 *
 * @note Calls to emlDeviceStart can be nested. A single data collection thread
 * will run for each device for the duration of the outermost section.
 *
 * @warning EML is not yet thread-safe. Taking measurements from multiple
 * application threads simultaneously is not supported.
 *
 * @param[in] device Target device
 *
 * @retval EML_SUCCESS Monitoring has been started
 * @retval EML_NO_MEMORY Insufficient memory for monitoring
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 * @retval EML_UNKNOWN Internal library error
 */
emlError_t emlDeviceStart(const emlDevice_t* device);

/**
 * Ends an energy monitoring section on a specific device and returns
 * consumption data.
 *
 * Ends the section marked by the last call to emlStart, stopping data
 * collection if all sections have been closed.
 *
 * Writes an @a emlData_t * handle in the memory pointed by @a result. The
 * number of devices, and thus the necessary size for this buffer, can be
 * determined through @ref emlDeviceGetCount.
 *
 * @warning EML is not yet thread-safe. Taking measurements from multiple
 * application threads simultaneously is not supported.
 *
 * @param[in] device Target device
 * @param[out] result Address where data handles will be copied.
 *
 * @retval EML_SUCCESS Monitoring has been stopped, and data returned
 * @retval EML_NOT_INITIALIZED The library had not been initialized
 * @retval EML_UNKNOWN Internal library error
 */
emlError_t emlDeviceStop(const emlDevice_t* device, emlData_t** result);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /*EMLAPI_DEVICE_H*/
