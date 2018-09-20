/*!
 * Copyright (c) 2018 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * \author Alberto Cabrera Perez <Alberto.Cabrera@ull.edu.es>
 * \date   jun-2018
 * \brief  Driver implementation for the Labee module using curl and xml
 */

#define _XOPEN_SOURCE 500

#include <assert.h>
#include <confuse.h>
#include <curl/curl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
//#include <errno.h>
//#include <stddef.h>
//#include <stdint.h>

//#include <dirent.h>
//

//#include <fcntl.h>
//#include <sys/stat.h>
//#include <unistd.h>
//
#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

// CURL Options
#define CURLOPT_DEFAULT_TIMEOUT_MS 500L

// Confuse CFG options
#define LABEE_HOSTNAME_CFG "hostname"
#define LABEE_NODELIST_FILENAME_CFG "nodelist_file"
#define LABEE_DEFAULT_NODELIST_FILENAME "./nodelist"
#define LABEE_STATUS_CFG "disabled"
#define LABEE_DEFAULT_STATUS cfg_false
#define LABEE_SAMPLING_INTERVAL_CFG "sampling_interval"
#define LABEE_DEFAULT_SAMPLING_INTERVAL 150000000L // 150ms
#define LABEE_API_URL_CFG "api_url"
#define LABEE_API_USER_CFG "user"
#define LABEE_API_PASSWD_CFG "password"
#define LABEE_DEFAULT_API_URL "http://10.11.12.242/REST/node"

#define LABEE_NODE_ID "id"
#define LABEE_DEFAULT_POWER_ATTR "actualPowerUsage"
#define LABEE_POWER_ATTR_CFG "power_attribute"

#define LABEE_NODELIST_DELIM ","

struct emlDriver labee_driver;

struct xml_content {
  char * content;
  size_t length;
};


char *trimwhitespace(char *str) {
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}


static size_t store_partial_content(void * content, size_t size,
                                    size_t nmemb, struct xml_content* s) {
    size_t new_length = s->length + size * nmemb;
    s->content = realloc(s->content, new_length + 1);
    memcpy(s->content + s->length, content, size*nmemb);
    s->length = new_length;
    s->content[s->length] = '\0';
    return size * nmemb;
}


static enum emlError get_xml(struct xml_content * xc) {
  enum emlError err = EML_SUCCESS;
  char * labee_api_url = cfg_getstr(labee_driver.config, LABEE_API_URL_CFG);
  char * api_user = cfg_getstr(labee_driver.config, LABEE_API_USER_CFG);
  char * api_passwd = cfg_getstr(labee_driver.config, LABEE_API_PASSWD_CFG);
  CURLcode res;

  CURL * curl = curl_easy_init();

  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, labee_api_url);
    curl_easy_setopt(curl, CURLOPT_USERNAME, api_user);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, api_passwd);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, CURLOPT_DEFAULT_TIMEOUT_MS);

    struct curl_slist * list = 0;
    list = curl_slist_append(list, "Content-type: application/xml");
             
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)xc); 
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, store_partial_content);
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
       snprintf(labee_driver.failed_reason, sizeof(labee_driver.failed_reason),
              "obtain_xml(): %s, %s ", labee_api_url, curl_easy_strerror(res));
       err = EML_NETWORK_ERROR;
    }

    curl_easy_cleanup(curl);
  }
  return err;
}


void init_xml(struct xml_content ** xc) {
  (*xc) = malloc(sizeof(struct xml_content));
  (*xc)->content = malloc(1); 
  (*xc)->content[0] = '\0';
  (*xc)->length = 0;
}


void free_xml(struct xml_content * xc) {
  if (xc) {
    free(xc->content);
    xc->length = 0;
    free(xc);
  }
}


static enum emlError init(cfg_t* const config) {
  assert(!labee_driver.initialized);
  assert(config);
  labee_driver.config = config;

  enum emlError err;
  // Check that the API is working
  struct xml_content * xc;
  init_xml(&xc);
  err = get_xml(xc); 
  free_xml(xc);
  
  // Function that generates error code
  if (err != EML_SUCCESS)
    goto err_free;
  
  labee_driver.ndevices = 1;
  labee_driver.devices = malloc(labee_driver.ndevices * sizeof(*labee_driver.devices));
  for (size_t i = 0; i < labee_driver.ndevices; i++) {
    struct emlDevice devinit = {
      .driver = &labee_driver,
      .index = i,
    };
    sprintf(devinit.name, "%s%zu", labee_driver.name, i);

    struct emlDevice* const dev = &labee_driver.devices[i];
    memcpy(dev, &devinit, sizeof(*dev));
  }

  labee_driver.initialized = 1;
  return EML_SUCCESS;

err_free:
  if (labee_driver.failed_reason[0] == '\0')
    strncpy(labee_driver.failed_reason, emlErrorMessage(err), sizeof(labee_driver.failed_reason) - 1);
  return err;
}


// We were using shutdown of the sake of consistency between all drivers
// but it produces a conflict with a shutdown function used in curl.
// It has been changed as it does not affect any behaviour.
static enum emlError labee_shutdown() {
  assert(labee_driver.initialized);

  labee_driver.initialized = 0;

  return EML_SUCCESS;
}


static enum emlError obtain_hostname_rest_reference(char ** ref) {
  char * nodelist_filename = cfg_getstr(labee_driver.config, LABEE_NODELIST_FILENAME_CFG);
  char * hostname = cfg_getstr(labee_driver.config, LABEE_HOSTNAME_CFG);
  char buffer[BUFSIZ];
  enum emlError err = EML_BAD_CONFIG;

  FILE * fp = fopen(nodelist_filename, "r");
  while(fscanf(fp, "%s", buffer) != EOF) {
    char * tok1 = strtok(buffer, LABEE_NODELIST_DELIM); 
    char * tok2 = strtok(0, LABEE_NODELIST_DELIM); 
    tok2 = trimwhitespace(tok2);

    if (!strcmp(tok2, hostname)) {
      err = EML_SUCCESS;
      (*ref) = malloc(sizeof(tok1));
      strcpy(*ref, tok1); 
      break;
    }
  }
  fclose(fp);
  return err;
}


enum emlError get_power_from_xml(long long unsigned * power, 
                                 struct xml_content * xc) { 
  xmlDocPtr doc; 
  xmlChar * id;
  xmlChar * chr_power;
  char * ref;
  enum emlError err;
  const xmlChar * label_id = xmlCharStrdup(LABEE_NODE_ID);
  const xmlChar * label_powerusage = xmlCharStrdup(
        cfg_getstr(labee_driver.config, LABEE_POWER_ATTR_CFG)
      );

  err = obtain_hostname_rest_reference(&ref);
  if (err != EML_SUCCESS)
    return err;

  // The document being in memory, it have no base per RFC 2396,
  // and the "noname.xml" argument will serve as its base.
  doc = xmlReadMemory(xc->content, xc->length, "noname.xml", NULL, 0);
  if (!doc)
    return EML_SENSOR_MEASUREMENT_ERROR; 

  xmlNodePtr cur = xmlDocGetRootElement(doc);
  cur = cur->children;
  while (cur != NULL) {
    id = xmlGetProp(cur, label_id);
    if (!strcmp((char *) id, ref)) {
      xmlFree(id);
      chr_power = xmlGetProp(cur, label_powerusage);
      break;
    }
    xmlFree(id);
    cur = cur->next;
  }

  // EML_SI_MEGA is applied since the measurement offers enough precision
  (*power) = atof((char *) chr_power) * EML_SI_MEGA; 

  xmlFreeDoc(doc); 
  free(ref);
  return EML_SUCCESS;
}


static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(labee_driver.initialized);
  assert(devno < labee_driver.ndevices); // Shouldn't be more than one anyways

  enum emlError err = EML_SUCCESS;
  unsigned long long power;

  struct xml_content * xc;
  init_xml(&xc);
  err = get_xml(xc); 
  if (err != EML_SUCCESS)
    goto err_free;

  err = get_power_from_xml(&power, xc);
  if (err != EML_SUCCESS)
    goto err_free;

  values[0] = nanotimestamp();
  values[labee_driver.default_props->inst_power_field * DATABLOCK_SIZE] = power; 

  free_xml(xc);
  return EML_SUCCESS;

err_free:
  if (labee_driver.failed_reason[0] == '\0')
    strncpy(labee_driver.failed_reason, emlErrorMessage(err),
            sizeof(labee_driver.failed_reason) - 1);
  free_xml(xc);
  return err;
}

// default measurement properties for this driver
static struct emlDataProperties default_props = {
  .time_factor = EML_SI_NANO,
  .energy_factor = EML_SI_MICRO, 
  .power_factor = EML_SI_MICRO, // The unit is in W, so it is saved multiplied by EML_SI_MEGA.
  .inst_energy_field = 0,
  .inst_power_field = 1,
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL(LABEE_STATUS_CFG, LABEE_DEFAULT_STATUS, CFGF_NONE),
  CFG_INT(LABEE_SAMPLING_INTERVAL_CFG, LABEE_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
  CFG_STR(LABEE_API_URL_CFG, LABEE_DEFAULT_API_URL, CFGF_NONE),
  CFG_STR(LABEE_HOSTNAME_CFG, "", CFGF_NONE),
  CFG_STR(LABEE_NODELIST_FILENAME_CFG, "./nodelist", CFGF_NONE),
  CFG_STR(LABEE_API_USER_CFG, "", CFGF_NONE),
  CFG_STR(LABEE_API_PASSWD_CFG, "", CFGF_NONE),
  CFG_STR(LABEE_POWER_ATTR_CFG, LABEE_DEFAULT_POWER_ATTR, CFGF_NONE),
  CFG_END()
};

//public driver state and interface
struct emlDriver labee_driver = {
  .name = "labee",
  .type = EML_DEV_LABEE,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,
  .config = NULL,

  .init = &init,
  .shutdown = &labee_shutdown,
  .measure = &measure,
};
