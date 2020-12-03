/*!
 * Copyright (c) 2020 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * \author Alberto Cabrera <Alberto.Cabrera@ull.edu.es>
 * \date   oct-2020
 * \brief  Driver implementation for the pmlib module using telnet
 */

// TODO Currently the driver only considers power readings to maximise compatibility with the current eml Implementation

//feature test macro for pread() in unistd.h
#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <arpa/inet.h>
#include <confuse.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

struct emlDriver pmlib_driver;

#define PMLIB_DEFAULT_SAMPLING_INTERVAL 50 * EML_TIME_MILLISECONDS  // Default set to 50ms
#define PMLIB_DEFAULT_TLS_INTERVAL 50 * EML_SI_NANO
#define PMLIB_DEFAULT_HOST "localhost"
#define PMLIB_DEFAULT_PORT 6526
#define PMLIB_DEFAULT_DEVICE "DummyDevice"
#define PMLIB_DEFAULT_OUTLETS 2
#define PMLIB_DEFAULT_TARGET_OUTLETS "{1, 2}"
#define PMLIB_DEFAULT_METRIC "power"


#define PACKET_MAXLEN 512
#define DEVICES_MAX 4
#define OUTLETS_MAX 24

#define TCP_PORT_STR_MAXLEN sizeof("65535")

enum Metric {
    voltage,
    shunt_voltage,
    current,
    power,
    temperature,
    def
};

enum pmlib_command {
    PMLIB_CREATE = 0,
    PMLIB_START = 1,
    PMLIB_CONTINUE = 2,
    PMLIB_STOP = 3,
    PMLIB_GET = 4,
    PMLIB_FINALIZE = 5,
    PMLIB_INFO_DEVICE = 6,
    PMLIB_LIST_DEVICES = 7,
    PMLIB_CMD_STATUS = 8,
    PMLIB_READ_DEVICE = 9,
    PMLIB_ERROR = -1,
};


struct pmlibconnection {
    int sockfd;
    pthread_mutex_t msglock;
    char recvbuf[PACKET_MAXLEN];
};


struct devstate {
    size_t pdu;
    size_t outlet;
};


struct pmlibstate {
    char * name;
    size_t n_outlets;
    size_t n_target_outlets;
    unsigned short * target_outlets;
    enum Metric metric;
    double last_measurement[OUTLETS_MAX];
    unsigned long long last_timestamp;
    struct pmlibconnection connection;
    struct devstate devstate[OUTLETS_MAX];
};


//local state
unsigned long long measurement_interval;
static struct pmlibstate* restrict pmlibstate[DEVICES_MAX];
static size_t pmlib_distinct_devices;
static struct devstate devstate[DEVICES_MAX * OUTLETS_MAX];

/* PMLIB connection auxiliary functions */

static enum Metric metricRead(char * metric) {
    if (!strcmp("voltage", metric)) {
        return voltage;
    }
    if (!strcmp("shunt_voltage", metric)) {
        return shunt_voltage;
    }
    if (!strcmp("current", metric)) {
        return current;
    }
    if (!strcmp("power", metric)) {
        return power;
    }
    if (!strcmp("temperature", metric)) {
        return temperature;
    }
    if (!strcmp("def", metric)) {
        return def;
    }
}

static void pmlib_send_command(int sockfd, int command) {
    send(sockfd, &command, sizeof(command), 0);
}

static void pmlib_send_device_name(int sockfd, char * devicename) {
    int devicelen = strlen(devicename);
    send(sockfd, &devicelen, sizeof(devicelen), 0);
    send(sockfd, devicename, strlen(devicename), 0);
}

static void pmlib_send_sampling_interval(int sockfd, long sampling_interval) {
    // PMLib receives number of samples per second
    // samplingInterval = x NanoSeconds
    // samplingIntervalInSeconds = EML_TIME_NANOSECONDS / sampling_interval
    // frequency = 1 / samplingIntervalInSeconds
    int frequency = EML_TIME_NANOSECONDS / sampling_interval;
    send(sockfd, &frequency, sizeof(frequency), 0);
}

static void pmlib_init_connection(struct pmlibstate* state, cfg_t* const pmlibcfg, int sampling_interval) {
    int sockfd = state->connection.sockfd;
    pmlib_send_command(sockfd, PMLIB_READ_DEVICE);
    pmlib_send_device_name(sockfd, cfg_getstr(pmlibcfg, "device_name"));
    pmlib_send_sampling_interval(sockfd, sampling_interval);
}

static int pmlib_read_int(struct pmlibstate* state) {
    int sockfd = state->connection.sockfd;
    void * buffer = state->connection.recvbuf;
    int bytes_read = read(sockfd, buffer, sizeof(int));
    if (bytes_read < 0) {
        return PMLIB_ERROR;
    }
    return ((int *) buffer)[0];
}

static double pmlib_read_double(struct pmlibstate* state) {
    int sockfd = state->connection.sockfd;
    void * buffer = state->connection.recvbuf;
    int bytes_read = read(sockfd , buffer, sizeof(double));
    if (bytes_read < 0) {
        return PMLIB_ERROR;
    }
    return ((double *) buffer)[0];
}

/* Initialization functions */

static int connect_socket(int devno, cfg_t* const pmlibcfg, struct pmlibstate* const state) {
    struct addrinfo* info;
    struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM,
            .ai_protocol = IPPROTO_TCP,
            .ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG
    };

    char portbuf[TCP_PORT_STR_MAXLEN];
    unsigned short port = cfg_getint(pmlibcfg, "port");
    snprintf(portbuf, sizeof(portbuf), "%hu", port);

    int err;
    char* errmsg = NULL;

    err = getaddrinfo(cfg_getstr(pmlibcfg, "host"), portbuf, &hints, &info);
    if (err) {
        dbglog_error("%s: %s", pmlibstate[devno]->name, gai_strerror(err));
        return -1;
    }

    struct addrinfo* ainfo;
    for (ainfo = info; ainfo != NULL; ainfo = ainfo->ai_next) {
        state->connection.sockfd = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
        if (state->connection.sockfd < 0) continue;

        int reuse = 1;
        err = setsockopt(state->connection.sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        if (err < 0) goto close_and_next;

        static const struct timeval delay = {
                .tv_sec = 5,
                .tv_usec = 0
        };
        err = setsockopt(state->connection.sockfd, SOL_SOCKET, SO_SNDTIMEO, &delay, sizeof(delay));
        if (err < 0) goto close_and_next;

        err = connect(state->connection.sockfd, ainfo->ai_addr, sizeof(*ainfo->ai_addr));
        if (!err) break;
        dbglog_info("Connecting %s (ainfo %p): %s", cfg_getstr(pmlibcfg, "device_name"), (void*) ainfo, strerror(errno));

        close_and_next:
        close(state->connection.sockfd);
    }


    if (!ainfo) {
        errmsg = "Could not connect";
        goto err_free;
    }

    return EML_SUCCESS;

    err_free:
        if (!errmsg) errmsg = strerror(errno);
        freeaddrinfo(info);
        dbglog_warn("%s: %s", cfg_getstr(pmlibcfg, "device_name"), errmsg);
        return -1;
}

static void pmlib_initialize_outlet_devices(struct pmlibstate* const state) {
    for (size_t i = 0; i < state->n_outlets; i++) {
        if (!state->target_outlets[i])
            continue;

        struct emlDevice devinit = {
                .driver = &pmlib_driver,
                .index = pmlib_driver.ndevices,
        };
        snprintf(devinit.name, sizeof(devinit.name), "%s%zu_outlet%zu",
                 pmlib_driver.name, pmlib_distinct_devices, i);

        struct emlDevice* const dev = &pmlib_driver.devices[devinit.index];
        memcpy(dev, &devinit, sizeof(*dev));

        state->devstate[i].pdu = pmlib_distinct_devices;
        state->devstate[i].outlet = i;
        devstate[pmlib_driver.ndevices] = state->devstate[i];
        pmlib_driver.ndevices++;
    }
}

static enum emlError init_device(int devno, cfg_t* const pmlibcfg) {
    dbglog_info("Initializing pmlib %s [type:%s, (%s:%ld)]",
                cfg_title(pmlibcfg),
                cfg_getstr(pmlibcfg, "device_name"),
                cfg_getstr(pmlibcfg, "host"),
                cfg_getint(pmlibcfg, "port"));

    int status = 0;
    char* errmsg = NULL;

    struct pmlibstate* const restrict state = malloc(sizeof(*state));

    // Read data from confuse configuration file
    state->name = cfg_getstr(pmlibcfg, "device_name");
    state->metric = metricRead(cfg_getstr(pmlibcfg, "metric"));
    state->n_outlets = cfg_getint(pmlibcfg, "n_outlets");
    state->n_target_outlets = cfg_size(pmlibcfg, "target_outlets");
    state->target_outlets = calloc(state->n_target_outlets, sizeof(state->target_outlets));
    for(int outlet = 0; outlet < state->n_target_outlets; outlet++) {
        int target_outlet = cfg_getnint(pmlibcfg, "target_outlets", outlet);
        state->target_outlets[target_outlet] = 1;
    }
    state->last_timestamp = 0;
    state->metric = metricRead(cfg_getstr(pmlibcfg, "metric"));

    // Start the device socket connection
    status = connect_socket(devno, pmlibcfg, state);

    if (status == -1) {
        return status;
    }
    pthread_mutex_init(&state->connection.msglock, NULL);

    // Check if the device specified is valid
    pmlib_init_connection(state, pmlibcfg, measurement_interval);
    status = pmlib_read_int(state);
    pthread_mutex_unlock(&state->connection.msglock);

    if (status == PMLIB_ERROR) {
        errmsg = "PMLIB connection error or wrong device specified.";
        goto err_close_socket;
    }

    // Create a device per target outlet
    pmlib_driver.devices = realloc(pmlib_driver.devices,
                                   (pmlib_driver.ndevices + state->n_target_outlets)
                                   * sizeof(*pmlib_driver.devices));

    assert(pmlib_driver.devices);

    pmlib_initialize_outlet_devices(state);

    pmlibstate[pmlib_distinct_devices] = state;
    pmlib_distinct_devices++;

    return EML_SUCCESS;

    err_close_socket:
    if (!errmsg) errmsg = strerror(errno);
    status = close(state->connection.sockfd);
    if (status < 0) {
        dbglog_error("Closing socket for %s: %s", cfg_getstr(pmlibcfg, "device_name"), strerror(errno));
    }
    return status;
}

/* Shutdown functions */

static enum emlError shutdown_device(int i) {
    int err;
    err = shutdown(pmlibstate[i]->connection.sockfd, SHUT_RDWR);
    if (err) {
        dbglog_info("shutting down socket: %s", strerror(errno));
    }
    err = close(pmlibstate[i]->connection.sockfd);
    if (err) {
        dbglog_info("closing socket: %s", strerror(errno));
    }

    pthread_mutex_destroy(&pmlibstate[i]->connection.msglock);

    free(pmlibstate[i]->target_outlets);

    return EML_SUCCESS;
}

/* Measurement functions */



/* Driver functions */

static enum emlError init(cfg_t *const config) {
    assert(!pmlib_driver.initialized);
    assert(config);
    pmlib_driver.config = config;

    pmlib_driver.ndevices = 0;
    pmlib_driver.devices = malloc(0);

    cfg_t *device_cfg;
    measurement_interval = cfg_getint(config, "sampling_interval");
    for (size_t i = 0; (device_cfg = cfg_getnsec(config, "device", i)); i++) {
        init_device(i, device_cfg);
    }

    pmlib_driver.initialized = 1;
    return EML_SUCCESS;
}


static enum emlError shutdown_pmlib() {
    assert(pmlib_driver.initialized);

    pmlib_driver.initialized = 0;

    enum emlError err;
    int flag = 0;
    for (size_t i = 0; i < pmlib_distinct_devices; i++) {
        err = shutdown_device(i);
        if (err != EML_SUCCESS)
            flag = 1;
    }

    free(pmlib_driver.devices);
    if (flag)
        dbglog_error("PMlib device shutdown has encountered an unknown error");

    return EML_SUCCESS;
}


static enum emlError measure(size_t devno, unsigned long long *values) {
    assert(pmlib_driver.initialized);
    assert(devno < pmlib_driver.ndevices); // Shouldn't be more than one anyways
    const size_t pduno = devstate[devno].pdu;
    const size_t outlet = devstate[devno].outlet;

    //only actually query the pdu if there is no fresh block
    pthread_mutex_lock(&pmlibstate[pduno]->connection.msglock);
    unsigned long long now = nanotimestamp();
    if (!pmlibstate[pduno]->last_timestamp || (now - pmlibstate[pduno]->last_timestamp) > measurement_interval) {
        pmlib_read_int(pmlibstate[pduno]); // Read num lines, required by the pmlib server every time
        for (int i = 0; i < pmlibstate[pduno]->n_outlets; i++) {
            pmlibstate[pduno]->last_measurement[i] = pmlib_read_double(pmlibstate[pduno]);
        }
        pmlibstate[pduno]->last_timestamp = nanotimestamp();
    }
    pthread_mutex_unlock(&pmlibstate[pduno]->connection.msglock);

    values[0] = pmlibstate[pduno]->last_timestamp;

    values[pmlib_driver.default_props->inst_power_field * DATABLOCK_SIZE] = pmlibstate[pduno]->last_measurement[outlet];

    return EML_SUCCESS;
}


// default measurement properties for this driver
static struct emlDataProperties default_props = {
        .time_factor = EML_SI_NANO,
        .energy_factor = EML_SI_MILLI,
        .power_factor = EML_SI_MILLI,
        .inst_energy_field = 0,
        .inst_power_field = 1,
};


static cfg_opt_t deviceopts[] = {
        CFG_STR("host", PMLIB_DEFAULT_HOST, CFGF_NONE),
        CFG_INT("port", PMLIB_DEFAULT_PORT, CFGF_NONE),
        CFG_STR("device_name", PMLIB_DEFAULT_DEVICE, CFGF_NONE),
        CFG_INT("n_outlets", PMLIB_DEFAULT_OUTLETS, CFGF_NONE),
        CFG_INT_LIST("target_outlets", PMLIB_DEFAULT_TARGET_OUTLETS, CFGF_NONE),
        CFG_STR("metric", PMLIB_DEFAULT_METRIC, CFGF_NONE),
        CFG_END()
};

static cfg_opt_t cfgopts[] = {
        CFG_BOOL("disabled", cfg_false, CFGF_NONE),
        CFG_INT("sampling_interval", PMLIB_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),
        CFG_SEC("device", deviceopts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),
        CFG_END()
};

//public driver state and interface
struct emlDriver pmlib_driver = {
        .name = "pmlib",
        .type = EML_DEV_PMLIB,
        .failed_reason = "",
        .default_props = &default_props,
        .cfgopts = cfgopts,
        .config = NULL,

        .init = &init,
        .shutdown = &shutdown_pmlib, // shutdown exists in socket.h
        .measure = &measure,
};
