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
 * Internal debugging facilities
 * @ingroup internalapi
 */

#ifndef EML_DEBUG_H
#define EML_DEBUG_H

#ifndef NDEBUG

#include <stdio.h>

/**
 * Prints a debug message to @c stderr along with file and line information.
 * Takes a tag and parameters for @c printf.
 *
 * @param TAG Tag for this message
 * @param MSG Message format string
 * @param ... Parameters to the format string
 *
 * @return Number of characters written, excluding the terminating null char.
 */
#define DBGLOG(TAG, MSG, ...) fprintf(stderr, "[EML-" TAG "] (%s:%d) " MSG "%c", __FILE__, __LINE__, __VA_ARGS__)

#else

#define DBGLOG(...)

#endif /*NDEBUG*/

/**
 * Prints an error message to @c stderr along with file and line information.
 * Has the same semantics as @c printf.
 *
 * @return Number of characters written, excluding the terminating null char.
 */
#define dbglog_error(...)  DBGLOG("ERROR", __VA_ARGS__, '\n')

/**
 * Prints a warning message to @c stderr along with file and line information.
 * @copydetails dbglog_error
 */
#define dbglog_warn(...)  DBGLOG("WARN", __VA_ARGS__, '\n')

/**
 * Prints an info message to @c stderr along with file and line information.
 * @copydetails dbglog_error
 */
#define dbglog_info(...)  DBGLOG("INFO", __VA_ARGS__, '\n')

#endif /*EML_DEBUG_H*/
