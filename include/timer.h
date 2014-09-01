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
 * Internal functions dealing with time
 * @ingroup internalapi
 */

#ifndef EML_TIMER_H
#define EML_TIMER_H

/**
 * Returns nanoseconds since some unspecified starting point.
 *
 * Uses the highest precision POSIX timer available.
 * Requires POSIX timer support.
 *
 * @return Timestamp.
 */
unsigned long long nanotimestamp();

#endif /*EML_TIMER_H*/
