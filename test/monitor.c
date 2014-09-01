/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <eml.h>

void check_error(emlError_t ret) {
  if (ret != EML_SUCCESS) {
    fprintf(stderr, "error: %s\n", emlErrorMessage(ret));
    exit(1);
  }
}

int main() {
  //initialize EML
  check_error(emlInit());

  //get total device count
  size_t count;
  check_error(emlDeviceGetCount(&count));
  emlData_t* data[count];

  //start measuring
  check_error(emlStart());

  sleep(1);

  //stop measuring and gather data
  check_error(emlStop(data));

  //dump and free results
  for (size_t i = 0; i < count; i++) {
    check_error(emlDataDumpJSON(data[i], stdout));
    check_error(emlDataFree(data[i]));
  }

  check_error(emlShutdown());
  return 0;
}
