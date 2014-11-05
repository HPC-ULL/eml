/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include "error.h"

const char* emlErrorMessage(enum emlError err) {
  switch (err) {
    case EML_SUCCESS:
      return "success";
    case EML_NOT_INITIALIZED:
      return "not initialized";
    case EML_ALREADY_INITIALIZED:
      return "already initialized";
    case EML_LIBRARY_UNAVAILABLE:
      return "couldn't load dynamic library";
    case EML_SYMBOL_UNAVAILABLE:
      return "dynamic library missing required symbol";
    case EML_INVALID_PARAMETER:
      return "invalid parameter";
    case EML_NO_MEMORY:
      return "memory allocation failed";
    case EML_UNSUPPORTED_HARDWARE:
      return "hardware model not supported";
    case EML_NO_PERMISSION:
      return "insufficient user permissions";
    case EML_NOT_IMPLEMENTED:
      return "not implemented";
    case EML_PARSING_ERROR:
      return "parsing error";
    case EML_UNSUPPORTED:
      return "unsupported";
    case EML_NOT_STARTED:
      return "not started";
    case EML_ALREADY_STARTED:
      return "already started";
    case EML_MEASUREMENT_STACK_FULL:
      return "simultaneous measurement limit exceeded";
    case EML_BAD_CONFIG:
      return "malformed configuration file";
    default:
      return "unknown error";
  }
}
