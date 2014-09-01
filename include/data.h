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
 * @ingroup internalapi
 * Internal definitions for measurement datasets
 */

#ifndef EML_DATA_H
#define EML_DATA_H

#include <sys/queue.h>

#include <eml/data.h>

struct emlDevice;

/** Measurement properties.
 *
 * Information on measurement conditions for all datapoints in a dataset.
 */
struct emlDataProperties {
  /** Unit factor to convert time values to seconds.
   *
   * if positive, values are based on (time_factor)
   * if negative, values are based on 1/abs(time_factor)
   */
  int time_factor;

  /** Unit factor to convert energy values to joules.
   *
   * if positive, values are based on (energy_factor)
   * if negative, values are based on 1/abs(energy_factor)
   */
  int energy_factor;

  /** Unit factor to convert power values to watts.
   *
   * if positive, values are based on (power_factor)
   * if negative, values are based on 1/abs(power_factor)
   */
  int power_factor;

  /** Field number for consumed energy counter readings.
   *
   * 0 if energy counters are not available on this device.
   */
  size_t inst_energy_field;

  /** Field number for instant power counter readings.
   *
   * 0 if power counters are not available on this device.
   */
  size_t inst_power_field;

  /** Sampling interval in nanoseconds */
  long sampling_nanos;
};

/** Singly-linked list of datapoint blocks. */
struct emlDataList;

/** Block of datapoints */
struct emlDataBlock {
  /** Link to the next block */
  SLIST_ENTRY(emlDataBlock) entries;
  /** Buffer holding all field values for the datapoints in this block */
  unsigned long long* fields;
};

#ifndef EML_DATABLOCK_SIZE
/** Number of datapoints in a block (compile-time option) */
#define EML_DATABLOCK_SIZE 10000
#endif

/** Number of datapoints in a block */
static const size_t DATABLOCK_SIZE = EML_DATABLOCK_SIZE;

/** Fixed field ID for timestamp values */
static const size_t timestamp_field = 0;

/** Linked list of data blocks representing a continuous measurement run.
 *
 * This data can back multiple datasets if nested measurements are used.
 * Reference-counted.
 */
struct emlDataRun {
  /** Block list head */
  SLIST_HEAD(emlDataBlockList, emlDataBlock) blocks;
  /** Reference count */
  size_t refcount;
  /** Device these measurements were taken from */
  const struct emlDevice* device;
  /** Properties for these measurements */
  const struct emlDataProperties* props;
};

/** Measurement dataset */
struct emlData {
  /** Measurement run holding data for this interval.
   *
   * This run may be shared by other intervals.
   */
  struct emlDataRun* run;
  /** First data block belonging to this interval */
  struct emlDataBlock* firstblock;
  /** First data point on the first block belonging to this interval */
  size_t firstpoint;
  /** Number of data points on this interval */
  size_t npoints;

  /** Total time elapsed */
  unsigned long long elapsed_time;
  /** Total energy consumed */
  unsigned long long consumed_energy;
};

/** SI unit factors */
enum emlSIFactor {
  EML_SI_NANO  = -1000000000,
  EML_SI_MICRO = -1000000,
  EML_SI_MILLI = -1000,
  EML_SI_NONE  = 1,
  EML_SI_KILO  = 1000,
  EML_SI_MEGA  = 1000000,
  EML_SI_GIGA  = 1000000000,
};

/**
 * Computes totals for a dataset from the datapoints.
 *
 * Fills in the @a elapsed_time and @a consumed_energy fields.
 *
 * @param[in,out] data Dataset to have totals updated
 *
 * @retval EML_SUCCESS The dataset totals were updated
 */
enum emlError emlDataUpdateTotals(struct emlData* data);

/**
 * Frees data for a measurement run.
 *
 * @param[in] run List of datapoint blocks to be freed
 *
 * @retval EML_SUCCESS The list was successfully freed
 */
enum emlError emlDataRunRelease(struct emlDataRun* run);

#endif /*EML_DATA_H*/
