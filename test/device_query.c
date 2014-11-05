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

#include <eml.h>

void check_error(emlError_t ret) {
  if (ret != EML_SUCCESS) {
    fprintf(stderr, "error: %s\n", emlErrorMessage(ret));
    exit(1);
  }
}

const char* support_repr(emlDeviceType_t type) {
  emlDeviceTypeStatus_t status;
  check_error(emlDeviceGetTypeStatus(type, &status));

  switch(status) {
    case EML_SUPPORT_AVAILABLE:
      return "available";
    case EML_SUPPORT_NOT_COMPILED:
      return "not compiled";
    case EML_SUPPORT_NOT_RUNTIME:
      return "compiled but not available";
    default:
      fprintf(stderr, "error: invalid type status %d", status);
      exit(1);
  }
}

int main() {
  check_error(emlInit());

  printf("Available drivers:\n");
  printf("    [ NVML] %s\n", support_repr(EML_DEV_NVML));
  printf("    [ RAPL] %s\n", support_repr(EML_DEV_RAPL));
  printf("    [  MIC] %s\n", support_repr(EML_DEV_MIC));
  printf("    [SBPDU] %s\n", support_repr(EML_DEV_SB_PDU));

  size_t count;
  check_error(emlDeviceGetCount(&count));
  printf("Found %zu devices.\n", count);

  if (count) {
    printf("Device list:\n");
    emlDevice_t* dev;
    for (size_t i = 0; i < count; i++) {
      check_error(emlDeviceByIndex(i, &dev));
      const char* devname;
      check_error(emlDeviceGetName(dev, &devname));
      printf("    %s\n", devname);
    }
  }

  check_error(emlShutdown());
  return 0;
}
