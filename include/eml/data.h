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
 * @copydoc externalapi_data
 */

#ifndef EMLAPI_DATA_H
#define EMLAPI_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include <eml/error.h>

/**
 * @defgroup externalapi_data Data
 * @ingroup externalapi
 * Definition of @ref emlData_t and related functions
 * @{
 */

/** Data obtained from an energy monitoring section for a single device */
typedef struct emlData emlData_t;

/**
 * Dumps the data as JSON to a file.
 *
 * @param[in] data Data to be dumped
 * @param[out] dumpfile File to dump the data to
 *
 * @retval EML_SUCCESS @a data has been dumped
 * @retval EML_INVALID_PARAMETER @a dumpfile is invalid or @a data is NULL
 */
emlError_t emlDataDumpJSON(const emlData_t* data, FILE* dumpfile);

/**
 * Frees resources associated with the data object.
 *
 * Should be called when consumption data is no longer needed.
 *
 * @param data Data to be freed
 *
 * @retval EML_SUCCESS @a data has been freed
 */
emlError_t emlDataFree(emlData_t* data);

/**
 * Retrieves the energy consumed by the device on a section, in Joules.
 *
 * @param[in] data Data returned for the monitoring section
 * @param[out] consumed Reference in which to return consumed energy
 *
 * @retval EML_SUCCESS @a consumed has been set
 */
emlError_t emlDataGetConsumed(const emlData_t* data, double* consumed);

/**
 * Retrieves the time elapsed on a section, in seconds.
 *
 * @param[in] data Data returned for the monitoring section
 * @param[out] elapsed Reference in which to return elapsed time
 *
 * @retval EML_SUCCESS @a elapsed has been set
 */
emlError_t emlDataGetElapsed(const emlData_t* data, double* elapsed);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /*EMLAPI_DATA_H*/
