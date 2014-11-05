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
 * Internal definitions for EML configuration
 */

#ifndef EML_CONFIGURATION_H
#define EML_CONFIGURATION_H

#include <stdarg.h>

#include <confuse.h>

/**
 * Returns the configuration filename.
 *
 * Looks for a configuration file in the following paths, in order:
 *
 *  1. $XDG_CONFIG_HOME/eml/config (if $XDG_CONFIG_HOME is set and not empty)
 *  2. $HOME/.config/eml/config
 *  3. /etc/eml/config
 *
 * Only the first existing file is returned, even if it is malformed.
 *
 * @note Configuration files are not required, and can be empty. Default values
 * will be used for most missing entries (except for values made necessary by
 * existing entries, such as the hostname for a network PDU that was declared in
 * the configuration).
 *
 * @returns The configuration filename
 * @retval NULL No configuration file exists
 */
char* emlConfigFind();

/**
 * Prints an EML warning for a configuration parsing error.
 *
 * @note This function is passed to cfg_set_error_function when in debug mode.
 *
 * @param cfg The configuration that caused the parsing error
 * @param fmt Format string for the error
 * @param ap Format string arguments
 */
void emlConfigPrintError(cfg_t* cfg, const char* fmt, va_list ap);

#endif /*EML_CONFIGURATION_H*/
