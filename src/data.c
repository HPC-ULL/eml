/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "data.h"
#include "device.h"
#include "error.h"

void emlDataFactorDump(int factor, FILE* dumpfile) {
  if (factor >= 0) {
    fprintf(dumpfile, "      \"mult\":%d,\n", factor);
    fprintf(dumpfile, "      \"div\":1\n");
  }
  else {
    fprintf(dumpfile, "      \"mult\":1,\n");
    fprintf(dumpfile, "      \"div\":%d\n", -factor);
  }
}

void emlDataPropertiesDump(
    const struct emlDataProperties* props,
    FILE* dumpfile)
{
  fprintf(dumpfile, "  \"time_factor\": {\n");
  emlDataFactorDump(props->time_factor, dumpfile);
  fprintf(dumpfile, "   },\n");

  fprintf(dumpfile, "  \"energy_factor\": {\n");
  emlDataFactorDump(props->energy_factor, dumpfile);
  fprintf(dumpfile, "   },\n");

  fprintf(dumpfile, "  \"power_factor\": {\n");
  emlDataFactorDump(props->power_factor, dumpfile);
  fprintf(dumpfile, "   },\n");

  fprintf(dumpfile, "  \"header\": [\"timestamp\"");

  if (props->inst_energy_field)
    fprintf(dumpfile, ",\"inst_energy\"");

  if (props->inst_power_field)
    fprintf(dumpfile, ",\"inst_power\"");

  fprintf(dumpfile, "],\n");
}

/// Decreases the reference count for a data run, freeing if it becomes 0
enum emlError emlDataRunRelease(struct emlDataRun* run) {
  assert(run->refcount);
  run->refcount--;

  //free run memory if no data interval needs it now
  if (!run->refcount) {
    while (!SLIST_EMPTY(&run->blocks)) {
      struct emlDataBlock* first = SLIST_FIRST(&run->blocks);
      SLIST_REMOVE_HEAD(&run->blocks, entries);
      free(first->fields);
      free(first);
    }
  }

  return EML_SUCCESS;
}

enum emlError emlDataFree(struct emlData* data) {
  emlDataRunRelease(data->run);
  free(data);

  return EML_SUCCESS;
}

enum emlError emlDataUpdateTotals(struct emlData* data) {
  const struct emlDataProperties* props = data->run->props;

  data->elapsed_time = 0;
  data->consumed_energy = 0;

  if (!data->npoints)
    return EML_SUCCESS;

  size_t remaining = data->npoints;
  unsigned long long total_elapsed = 0;
  for (const struct emlDataBlock* bp = data->firstblock; bp != NULL && remaining; bp = SLIST_NEXT(bp, entries)) {
    //find current block size
    size_t blockstart = (bp == data->firstblock) ? (data->firstpoint % DATABLOCK_SIZE) : 0;
    size_t blocksize = DATABLOCK_SIZE - blockstart;
    int lastblock = 0;
    if (remaining < blocksize) {
      blocksize = remaining;
      lastblock = 1;
    }
    assert(blocksize);

    //compute total elapsed time from first and last timestamps
    const unsigned long long* ts = bp->fields + timestamp_field * DATABLOCK_SIZE;
    if (bp == data->firstblock)
      total_elapsed = ts[blockstart];
    if (lastblock)
      total_elapsed = ts[blockstart + blocksize - 1] - total_elapsed;

    //compute total consumed energy...
    //...from energy counter readings
    if (props->inst_energy_field) {
      const unsigned long long* energy = bp->fields + props->inst_energy_field * DATABLOCK_SIZE;
      for (size_t i = 1; i < blocksize; i++)
        data->consumed_energy += energy[i];
    }

    //...from instant power readings
    else if (props->inst_power_field) {
      const unsigned long long* power = bp->fields + props->inst_power_field * DATABLOCK_SIZE;
      for (size_t i = blockstart + 1; i < blockstart + blocksize; i++) {
        unsigned long long pwrdelta = power[i-1] * (ts[i] - ts[i-1]);
        if (props->time_factor >= 0)
          data->consumed_energy += pwrdelta * props->time_factor;
        else
          data->consumed_energy += pwrdelta / (-props->time_factor);
      }
    }
    if (remaining - blocksize > 0) {
      remaining -= blocksize;
    } else {
      remaining = 0;
    }
  }

  data->elapsed_time = total_elapsed;
  return EML_SUCCESS;
}

enum emlError emlDataDumpJSON(const struct emlData* data, FILE* dumpfile) {
  if (!data || !dumpfile)
    return EML_INVALID_PARAMETER;

  const char* devname;
  emlDeviceGetName(data->run->device, &devname);

  fprintf(dumpfile,
    "{\n"
    "  \"device\": \"%s\",\n",
    devname);

  fprintf(dumpfile,
    "  \"elapsed\": %llu,\n"
    "  \"consumed\": %llu,\n",
    data->elapsed_time,
    data->consumed_energy);

  emlDataPropertiesDump(data->run->props, dumpfile);

  fprintf(dumpfile, "  \"data\": [\n");
  char delim = ' ';
  size_t remaining = data->npoints;
  for (const struct emlDataBlock* bp = data->firstblock; bp != NULL && remaining; bp = SLIST_NEXT(bp, entries), remaining -= DATABLOCK_SIZE) {
    //find current block size
    size_t blockstart = (bp == data->firstblock) ? (data->firstpoint % DATABLOCK_SIZE) : 0;
    size_t blocksize = DATABLOCK_SIZE - blockstart;
    if (remaining < blocksize)
      blocksize = remaining;
    assert(blocksize);

    for (size_t i = blockstart; i < blockstart + blocksize; i++) {
      fprintf(dumpfile, "   %c[%llu", delim, bp->fields[i + timestamp_field * DATABLOCK_SIZE]);

      if (data->run->props->inst_energy_field)
        fprintf(dumpfile, ",%llu", bp->fields[i + data->run->props->inst_energy_field * DATABLOCK_SIZE]);

      if (data->run->props->inst_power_field)
        fprintf(dumpfile, ",%llu", bp->fields[i + data->run->props->inst_power_field * DATABLOCK_SIZE]);

      fprintf(dumpfile, "]\n");
      delim = ',';
    }
  }

  fprintf(dumpfile,
      "  ]\n"
      "}\n");

  return EML_SUCCESS;
}

enum emlError emlDataGetElapsed(
    const struct emlData* data,
    double* elapsed)
{
  const int factor = data->run->props->time_factor;
  if (factor >= 0)
    *elapsed = data->elapsed_time * factor;
  else
    *elapsed = (double) data->elapsed_time / (double) (-factor);

  return EML_SUCCESS;
}

enum emlError emlDataGetConsumed(
    const struct emlData* data,
    double* consumed)
{
  const int factor = data->run->props->energy_factor;
  if (factor >= 0)
    *consumed = data->consumed_energy * factor;
  else
    *consumed = (double) data->consumed_energy / (double) (-factor);

  return EML_SUCCESS;
}
