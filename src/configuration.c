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
#include <string.h>

#include "configuration.h"
#include "debug.h"
#include "error.h"

char* emlConfigFind() {
  static const char* configname = "/eml/config";
  const char* xdg_config_home = getenv("XDG_CONFIG_HOME");

  if (xdg_config_home) {
    size_t dirlen = strlen(xdg_config_home);
    if (dirlen) {
      size_t len = dirlen + strlen(configname) + 1;
      char* path = malloc(len);
      snprintf(path, len, "%s%s", xdg_config_home, configname);
      FILE* file = fopen(path, "r");
      if (file) {
        fclose(file);
        return path;
      }
    }
  }

  const char* home = getenv("HOME");
  if (home) {
    static const char* configdir = "/.config";
    size_t dirlen = strlen(home);
    if (dirlen) {
      size_t len = dirlen + strlen(configdir) + strlen(configname) + 1;
      char* path = malloc(len);
      snprintf(path, len, "%s%s%s", home, configdir, configname);
      FILE* file = fopen(path, "r");
      if (file) {
        fclose(file);
        return path;
      }
    }
  }

  static const char* globaldir = "/etc";
  size_t len = strlen(globaldir) + strlen(configname) + 1;
  char* path = malloc(len);
  snprintf(path, len, "%s%s", globaldir, configname);
  FILE* file = fopen(path, "r");
  if (file) {
    fclose(file);
    return path;
  }
  else
    return NULL;
}

void emlConfigPrintError(cfg_t* cfg, const char* fmt, va_list ap) {
  dbglog_warn("Parsing %s:%d: ", cfg->filename, cfg->line);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
}
