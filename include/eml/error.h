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
 * @copydoc externalapi_error
 */

#ifndef EMLAPI_ERROR_H
#define EMLAPI_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup externalapi_error Error codes
 * Definition of @ref emlError_t and related functions
 * @ingroup externalapi
 * @{
 */

/** Error codes for EML API calls */
typedef enum emlError {
  /** The operation was successful */
  EML_SUCCESS = 0,
  /** The library had not been initialized */
  EML_NOT_INITIALIZED = 1,
  /** The library was already initialized */
  EML_ALREADY_INITIALIZED = 2,
  /** A necessary third-party library is unavailable */
  EML_LIBRARY_UNAVAILABLE = 3,
  /** A necessary symbol from a library is unavailable */
  EML_SYMBOL_UNAVAILABLE = 4,
  /** A supplied parameter is invalid */
  EML_INVALID_PARAMETER = 5,
  /** Insufficient memory for this operation */
  EML_NO_MEMORY = 6,
  /** This operation is not supported on this hardware */
  EML_UNSUPPORTED_HARDWARE = 7,
  /** Insufficient permissions for this operation */
  EML_NO_PERMISSION = 8,
  /** This operation is not implemented */
  EML_NOT_IMPLEMENTED = 9,
  /** An input file was malformed */
  EML_PARSING_ERROR = 10,
  /** This operation is not supported */
  EML_UNSUPPORTED = 11,
  /** Monitoring had not been started */
  EML_NOT_STARTED = 12,
  /** Monitoring was already started */
  EML_ALREADY_STARTED = 13,
  /** Nested section stack full */
  EML_MEASUREMENT_STACK_FULL = 14,
  /** Internal library error */
  EML_UNKNOWN = 999
} emlError_t;

/**
 * Returns the error message associated with an error code.
 *
 * @param err Error code
 *
 * @return Error message
 */
const char* emlErrorMessage(emlError_t err);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /*EMLAPI_ERROR_H*/
