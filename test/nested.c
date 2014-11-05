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

#ifndef TEST_SECONDS
#define TEST_SECONDS 10
#endif

#ifndef TEST_ITERATIONS
#define TEST_ITERATIONS 3
#endif

void check_error(emlError_t ret) {
  if (ret != EML_SUCCESS) {
    fprintf(stderr, "error: %s\n", emlErrorMessage(ret));
    exit(1);
  }
}

void print_and_free_data(emlData_t** data, size_t count) {
  for (size_t i = 0; i < count; i++) {
    double consumed, elapsed;
    check_error(emlDataGetConsumed(data[i], &consumed));
    check_error(emlDataGetElapsed(data[i], &elapsed));
    check_error(emlDataFree(data[i]));

    emlDevice_t* dev;
    check_error(emlDeviceByIndex(i, &dev));
    const char* devname;
    check_error(emlDeviceGetName(dev, &devname));
    printf("%s: %gJ in %gs\n", devname, consumed, elapsed);
  }
}

int main() {
  //initialize EML
  check_error(emlInit());

  //get total device count
  size_t count;
  check_error(emlDeviceGetCount(&count));
  emlData_t* outer_data[count];
  emlData_t* inner_data[TEST_ITERATIONS][count];

  //start measuring outside the loop
  check_error(emlStart());

  for (int i = 0; i < TEST_ITERATIONS; i++) {
    //measure one iteration
    check_error(emlStart());
    sleep(TEST_SECONDS);
    check_error(emlStop(inner_data[i]));
  }

  //stop measuring and gather data
  check_error(emlStop(outer_data));

  //print data for every iteration
  for (int i = 0; i < TEST_ITERATIONS; i++) {
    printf("iteration %d:\n", i);
    print_and_free_data(inner_data[i], count);
  }

  //print global data
  printf("total:\n");
  print_and_free_data(outer_data, count);

  check_error(emlShutdown());
  return 0;
}
