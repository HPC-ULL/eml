/*
 * Copyright (c) 2014 Universidad de La Laguna <cap@pcg.ull.es>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

//feature test macro for addrinfo funcs
#define _POSIX_C_SOURCE 1

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <confuse.h>
#include <netdb.h>
#include <openssl/rc4.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "data.h"
#include "debug.h"
#include "driver.h"
#include "error.h"
#include "timer.h"

//Schleifenbauer PDU power readings seem to be updated every ~1s
#define SB_DEFAULT_SAMPLING_INTERVAL 1000000000L
#define SB_DEFAULT_HOST "192.168.1.200"
#define SB_DEFAULT_PORT 7783
#define SB_DEFAULT_RC4KEY "0000000000000000"
#define RC4KEY_MAXLEN 16

#define PDUS_MAX 10
#define DEVICES_MAX 100

#define PACKET_MAXLEN 512
static const size_t SCAN_PACKET_LEN = 13;

#define CHECK_LEN 4
static const char TAG[] = {'S','A','P','I'};
static const size_t SIZE_LEN = 2;
static const size_t CHECKSUM_LEN = 4;
static const size_t COMMAND_LEN = 2;
static const size_t CRC_LEN = 2;
static const size_t MEASURE_REG_LEN = 2;
static const size_t NCHANNELS = 27;
static const unsigned long long MEASURE_TTL = 2000000000;

static const char ETX = 3;
#define TCP_PORT_STR_MAXLEN sizeof("65535")

struct emlDriver sb_pdu_driver;

struct pdustate {
  int sockfd;
  pthread_mutex_t msglock;
  size_t noutlets;
  unsigned char keydata[RC4KEY_MAXLEN];
  unsigned char sendbuf[PACKET_MAXLEN];
  unsigned char recvbuf[PACKET_MAXLEN];
  size_t recvoff;
  size_t recvremaining;
  unsigned char lastblk[PACKET_MAXLEN];
  unsigned long long lastts;
};

struct devstate {
  size_t pdu;
  size_t outlet;
};

//local state
static size_t npdus;
static struct pdustate* restrict pdustate[PDUS_MAX];
static struct devstate devstate[DEVICES_MAX];
static unsigned char measurecmdbuf[PACKET_MAXLEN];
static size_t measurecmdlen;

static uint32_t chksum(const unsigned char* src, const size_t len) {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++)
    sum += (uint32_t) src[i];
  return sum;
}

static ssize_t pduwrite(struct pdustate* const st, const void* const src, const size_t len) {
  const size_t paylen = CHECK_LEN + len + CHECKSUM_LEN;
  const size_t totlen = sizeof(TAG) + SIZE_LEN + paylen;

  unsigned char* p = st->sendbuf + sizeof(TAG);

  //message length field
  *p++ = (paylen >> 8) & 0xff;
  *p++ = paylen & 0xff;

  //start of payload
  unsigned char* const paystart = p;

  //"check" field (first four bytes of the RC4 key)
  memcpy(p, st->keydata, CHECK_LEN);
  p += CHECK_LEN;

  //message contents
  memcpy(p, src, len);
  p += len;

  //checksum of check+message fields
  uint32_t sum = chksum(paystart, paylen - CHECKSUM_LEN);
  *p++ = (sum >> 24) & 0xff;
  *p++ = (sum >> 16) & 0xff;
  *p++ = (sum >> 8) & 0xff;
  *p++ = sum & 0xff;

  //encrypt the payload
  static RC4_KEY key;
  RC4_set_key(&key, sizeof(st->keydata), st->keydata);
  RC4(&key, paylen, paystart, paystart);

  ssize_t sent = send(st->sockfd, st->sendbuf, totlen, 0);
  if (sent < 0) {
    dbglog_warn("sending message: %s", strerror(errno));
    return sent;
  }
  assert(sent == (ssize_t) totlen);

  return len;
}

static ssize_t pduread(struct pdustate* const st, void* const dst, size_t len) {
  if (st->recvremaining == 0) {
    st->recvoff = 0;

    ssize_t rcvd = recv(st->sockfd, st->recvbuf, PACKET_MAXLEN, 0);
    if (rcvd < 0) {
      dbglog_warn("recving message: %s", strerror(errno));
      return rcvd;
    }

    unsigned char* p = st->recvbuf;

    //check tag
    if (memcmp(p, TAG, sizeof(TAG))) {
      dbglog_warn("malformed packet: wrong message tag");
      errno = EBADMSG;
      return -1;
    }
    p += sizeof(TAG);

    //check length
    size_t paylen = (p[0] << 8) + p[1];
    if (paylen == 0 || paylen > PACKET_MAXLEN) {
      dbglog_warn("malformed packet: invalid length");
      errno = EBADMSG;
      return -1;
    }
    p += SIZE_LEN;

    //decrypt the payload
    unsigned char* const paystart = p;
    RC4_KEY key;
    RC4_set_key(&key, sizeof(st->keydata), st->keydata);
    RC4(&key, paylen, paystart, paystart);

    //check "check" field
    if (memcmp(p, st->keydata, CHECK_LEN)) {
      dbglog_warn("malformed packet: wrong check field");
      errno = EBADMSG;
      return -1;
    }

    //check checksum
    uint32_t expected = chksum(paystart, paylen - CHECKSUM_LEN);
    p += paylen - CHECKSUM_LEN;
    uint32_t sum = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
    if (sum != expected) {
      dbglog_warn("malformed packet: wrong checksum");
      errno = EBADMSG;
      return -1;
    }

    st->recvremaining = paylen - CHECKSUM_LEN - CHECK_LEN;
    st->recvoff = sizeof(TAG) + SIZE_LEN + CHECK_LEN;
  }

  if (len > st->recvremaining)
    len = st->recvremaining;

  //deliver message contents
  memcpy(dst, &st->recvbuf[st->recvoff], len);
  st->recvremaining -= len;
  st->recvoff += len;

  return len;
}

static uint16_t crc16(const unsigned char* src, const size_t len) {
  uint32_t crc = 0xffff;
  for (size_t i = 0; i < len; i++) {
    crc ^= src[i] << 8;
    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) {
        crc <<= 1;
        crc ^= 0x1021;
      }
      else
        crc <<= 1;
    }
  }
  return (crc & 0xffff);
}

enum pdu_command {
  SB_CMD_NOP = 0x0000,
  SB_CMD_READ = 0x0201,
  SB_CMD_READ_EXTENDED = 0x0202,
  SB_CMD_WRITE = 0x0210,
  SB_CMD_WRITE_EXTENDED = 0x0211,
  SB_CMD_SET_ADDRESS = 0x0220,
  SB_CMD_DIAGNOSTIC_TEST = 0x0240,
  SB_CMD_BCAST_DISPLAY_ON = 0x0280,
  SB_CMD_BCAST_DISPLAY_OFF = 0x0281,
  SB_CMD_BCAST_DIAGNOSTIC_TEST = 0x0282,
  SB_CMD_BCAST_IDENTIFY = 0x0290,
  SB_CMD_BCAST_STATUS = 0x0291,
  SB_CMD_BCAST_WRITE_REGISTERS = 0x02a0,
  SB_CMD_BCAST_OTHER_MASTERS = 0x02f0,
  //ACKs
  SB_ACK_READ = 0x0601,
  SB_ACK_READ_EXTENDED = 0x0602,
  SB_ACK_WRITE = 0x0610,
  SB_ACK_WRITE_EXTENDED = 0x0611,
  SB_ACK_SET_ADDRESS = 0x0620,
  SB_ACK_IDENTIFY = 0x0690,
  SB_ACK_STATUS = 0x0691,
  //NAKs
  SB_NAK_READ = 0x0f01,
  SB_NAK_WRITE = 0x0f10,
  SB_NAK_WRITE_EXTENDED = 0x0f20,
};

enum pdu_error {
  //codes for errors detected on our end
  SB_ERR_NONE = 0,
  SB_ERR_NO_STX = 1,
  SB_ERR_NO_ETX = 2,
  SB_ERR_INVALID_CRC = 3,
  SB_ERR_UNKNOWN_COMMAND = 4,
  //error codes returned by the PDU
  SB_ERR_REPLY_NO_STX = 0xff,
  SB_ERR_REPLY_NO_ETX = 0xfe,
  SB_ERR_REPLY_INVALID_CRC = 0xfd,
  SB_ERR_REPLY_UNKNOWN_COMMAND = 0xfc,
  SB_ERR_REPLY_NAK = 0xfb,
  SB_ERR_REPLY_BUS_LOCKED = 0xfa,
  SB_ERR_REPLY_ZEROS_RECEIVED = 0xf9,
  SB_ERR_REPLY_TRANSACTION_ID_MISMATCH = 0xf8,
  SB_ERR_REPLY_NO_REPLY_TIMEOUT = 0xf7,
  SB_ERR_REPLY_UNEXPECTED_LENGTH = 0xf6,
};

//SPDM
enum pdu_register {
  //dummy
  SB_REG_NULL   = 0,     //dummy                  len   1, ch  1, int
  //identification
  SB_REG_IDSPDM = 100,   //SP DM version          len   2, ch  1, int
  SB_REG_IDFWVS = 102,   //firmware version       len   2, ch  1, int
  SB_REG_IDONBR = 104,   //sales order number     len  16, ch  1, ascii
  SB_REG_IDPART = 120,   //product id             len  16, ch  1, ascii
  SB_REG_IDSNBR = 136,   //serial number          len  16, ch  1, ascii
  SB_REG_IDCHIP = 152,   //hardware address       len   2, ch  3, int
  SB_REG_IDADDR = 158,   //unit address           len   2, ch  1, int
  //configuration
  SB_REG_CFNRPH = 200,   //n.o. phases            len   1, ch  1, int
  SB_REG_CFNRNO = 201,   //n.o. outlets total     len   1, ch  1, int
  SB_REG_CFNRSO = 202,   //n.o. switched outl     len   1, ch  1, int
  SB_REG_CFNRMO = 203,   //n.o. outlets meas.     len   1, ch  1, int
  SB_REG_CFAMPS = 204,   //maximum load           len   1, ch  1, int
  SB_REG_CFNRTE = 205,   //n.o. temp. sensors     len   1, ch  1, int
  SB_REG_CFNRES = 206,   //n.o. env. sensors      len   1, ch  1, int
  //system_status
  SB_REG_SSSTAT = 300,   //device status code     len   1, ch  1, int
  SB_REG_SSTTRI = 301,   //temperature alert      len   1, ch  1, int
  SB_REG_SSITRI = 302,   //input current alert    len   1, ch  1, int
  SB_REG_SSOTRI = 303,   //output current alert   len   1, ch  1, int
  SB_REG_SSVTRI = 304,   //input voltage alert    len   1, ch  1, int
  SB_REG_SSFTRI = 305,   //oCurrentDropAlert      len   1, ch  1, int
  SB_REG_SSICDA = 306,   //iCurrentDropAlert      len   1, ch  1, int
  //reset
  SB_REG_RSBOOT = 400,   //reboot device          len   1, ch  1, int
  SB_REG_RSALRT = 401,   //reset alerts           len   1, ch  1, int
  SB_REG_RSIMKS = 402,   //zero input kWh subt    len   1, ch  1, int
  SB_REG_RSOMKS = 403,   //zero out kWh subtot    len   1, ch 48, int
  SB_REG_RSPVAL = 430,   //resetPeakValues        len   1, ch  1, int
  //settings
  SB_REG_STDVNM = 1000,  //device name            len  16, ch  1, ascii
  SB_REG_STDVLC = 1016,  //device location        len  16, ch  1, ascii
  SB_REG_STUSER = 1032,  //vanity tag             len  20, ch  1, ascii
  SB_REG_STPKDR = 1052,  //peak duration          len   2, ch  1, int
  SB_REG_STRSAL = 1054,  //local reset            len   1, ch  1, int
  SB_REG_STFODL = 1056,  //fixed outlet delay     len   2, ch  1, int
  SB_REG_STPSAV = 1058,  //power saver mode       len   1, ch  1, int
  SB_REG_STOPOM = 1059,  //outlet powerup mode    len   1, ch  1, int
  SB_REG_STMAXT = 1060,  //maximum temperature    len   1, ch  1, int
  SB_REG_STDISO = 1061,  //display orientation    len   1, ch  1, int
  SB_REG_STIMCM = 1062,  //max inlet amps         len   2, ch  3, fd
  SB_REG_STOMCM = 1068,  //max outlet amps        len   2, ch 48, fd
  SB_REG_STOMCT = 1122,  //output CT ratio        len   1, ch 27, int
  SB_REG_STIMCT = 1149,  //input CT ratio         len   1, ch  3, int
  SB_REG_STINNM = 1152,  //input name             len   8, ch  3, ascii
  SB_REG_STOLNM = 1176,  //outlet name            len   8, ch 48, ascii
  SB_REG_STIODL = 1392,  //indiv. outlet delay    len   2, ch 48, int
  SB_REG_STCDDT = 1446,  //currentDropDetection   len   1, ch  1, int
  //switched_outlets
  SB_REG_SWOCST = 2000,  //current state          len   1, ch 48, int
  SB_REG_SWOSCH = 2027,  //scheduled              len   1, ch 48, int
  SB_REG_SWOREB = 2054,  //[reboot]               len   1, ch 48, int
  SB_REG_SWOUNL = 2081,  //unlock                 len   1, ch 48, int
  //input_measures
  SB_REG_IMKWHT = 3000,  //kWh total              len   3, ch  3, int
  SB_REG_IMKWHS = 3009,  //kWh subtotal           len   3, ch  3, int
  SB_REG_IMPFAC = 3018,  //power factor           len   2, ch  3, fd
  SB_REG_IMCRAC = 3024,  //actual current         len   2, ch  3, fd
  SB_REG_IMCRPK = 3030,  //peak current           len   2, ch  3, fd
  SB_REG_IMVOAC = 3036,  //actual voltage         len   2, ch  3, fd
  SB_REG_IMVODP = 3042,  //min voltage            len   2, ch  3, fd
  SB_REG_IMKWHF = 3048,  //WhSubtotal fraction    len   4, ch  3, int
  //output_measures
  SB_REG_OMKWHT = 4000,  //kWh total              len   3, ch 48, int
  SB_REG_OMKWHS = 4081,  //kWh subtotal           len   3, ch 48, int
  SB_REG_OMPFAC = 4162,  //power factor           len   2, ch 48, fd
  SB_REG_OMCRAC = 4216,  //actual current         len   2, ch 48, fd
  SB_REG_OMCRPK = 4270,  //peak current           len   2, ch 48, fd
  SB_REG_OMVOAC = 4324,  //actual voltage         len   2, ch 48, fd
  SB_REG_OMUWHS = 4378,  //outlets uWhSubtotal    len   4, ch  1, int
  //pdu_measures
  SB_REG_PDITEM = 5000,  //pdu int temperature    len   2, ch  1, fd
  SB_REG_PDETEM = 5002,  //pdu ext temperature    len   2, ch  1, fd
  SB_REG_PDINPK = 5004,  //pdu int peak temp      len   2, ch  1, fd
  SB_REG_PDEXPK = 5006,  //pdu ext peak temp      len   2, ch  1, fd
  SB_REG_SNSTYP = 5008,  //sensor type            len   1, ch 16, ascii
  SB_REG_SNSVAL = 5024,  //sensor value           len   2, ch 16, fd
  SB_REG_SNSNME = 5056,  //sensor name            len   6, ch 16, ascii
  //upload_info
  SB_REG_UPVERS = 10000, //version                len   2, ch  1, int
  SB_REG_UPCSUM = 10002, //checksum               len   4, ch  1, int
  SB_REG_UPLCRC = 10006, //crc                    len   2, ch  1, int
  SB_REG_UPBLKS = 10008, //numberOfBlocks         len   2, ch  1, int
  SB_REG_UPSIZE = 10010, //size                   len   2, ch  1, int
  SB_REG_UPCKOK = 10012, //firmwareIsValid        len   1, ch  1, int
  SB_REG_UPBLNR = 10100, //dataBlockNumber        len   2, ch  1, int
  SB_REG_UPDATA = 10102, //dataBlock              len 256, ch  1, ascii
  //calibration data
  SB_REG_CBRSTS = 20000, //CALIBRATION STATUS     len   1, ch  1, int
  SB_REG_CBRAMF = 20001, //IRMS_CONVERSION_FACT   len   4, ch  1, int
  SB_REG_CBRAMO = 20005, //IRMS_OFFSET_CORRECT    len   4, ch  1, int
  SB_REG_CBRVOF = 20009, //VRMS_CONVERSION_FACT   len   4, ch  1, int
  SB_REG_CBRVOO = 20013, //VRMS_OFFSET_CORRECTI   len   4, ch  1, int
  SB_REG_CBRVAF = 20017, //VAHR_CONVERSION_FACT   len   4, ch  1, int
  SB_REG_CBRWHF = 20021, //WATTHR_CONVERSION_FA   len   4, ch  1, int
  SB_REG_CBRWHO = 20025, //WATTHR_OFFSET_CORREC   len   4, ch  1, int
  SB_REG_CBRLCK = 20030, //CALIBRATION LOCK       len   1, ch  1, int
};

static size_t pduwritecmd(void* const msg, enum pdu_command cmd, ...) {
  const unsigned char* const start = msg;
  unsigned char* p = msg;

  //transaction ID
  static uint32_t transid = 1;

  //write command
  uint16_t cmdval = cmd;
  *p++ = (cmdval >> 8) & 0xff;
  *p++ = cmdval & 0xff;

  //write arguments (only scan and register reads are supported)
  va_list args;
  va_start(args, cmd);
  switch (cmd) {
    case SB_CMD_BCAST_IDENTIFY:
      break;
    case SB_CMD_READ:
      //address, transaction (not passed in args), register, register length
      for (size_t i = 0; i < 4; i++) {
        uint32_t arg = (i == 1) ? transid : va_arg(args, uint32_t);
        *p++ = arg & 0xff;
        *p++ = (arg >> 8) & 0xff;
        if (i == 1)
          transid++;

      }
      break;
    default:
      return SB_ERR_UNKNOWN_COMMAND;
  }
  va_end(args);

  //write CRC16
  uint16_t crc = crc16(msg, p - start);
  *p++ = crc & 0xff;
  *p++ = (crc >> 8) & 0xff;

  //write terminator byte
  *p++ = ETX;

  return p - start;
}

static enum pdu_error pdureadcmd(const void* const msg, const size_t len, enum pdu_command* const cmd) {
  static const size_t VALID_PAYLOAD_MINLEN = 9;
  const unsigned char* const data = msg;

  if (len < VALID_PAYLOAD_MINLEN) {
    const enum pdu_error error_reply = data[0];
    return error_reply;
  }

  if (data[len - 1] != ETX)
    return SB_ERR_NO_ETX;

  const size_t crcoff = len - sizeof(ETX) - CRC_LEN;
  uint16_t expected_crc = crc16(data, len - CRC_LEN - sizeof(ETX));
  uint16_t given_crc = (data[crcoff + 1] << 8) + data[crcoff];
  if (given_crc != expected_crc)
    return SB_ERR_INVALID_CRC;

  uint16_t incmd = (data[0] << 8) + data[1];
  switch (incmd) {
    case SB_CMD_NOP:
    case SB_CMD_READ:
    case SB_CMD_READ_EXTENDED:
    case SB_CMD_WRITE:
    case SB_CMD_WRITE_EXTENDED:
    case SB_CMD_SET_ADDRESS:
    case SB_CMD_DIAGNOSTIC_TEST:
    case SB_CMD_BCAST_DISPLAY_ON:
    case SB_CMD_BCAST_DISPLAY_OFF:
    case SB_CMD_BCAST_DIAGNOSTIC_TEST:
    case SB_CMD_BCAST_IDENTIFY:
    case SB_CMD_BCAST_STATUS:
    case SB_CMD_BCAST_WRITE_REGISTERS:
    case SB_CMD_BCAST_OTHER_MASTERS:
    case SB_ACK_READ:
    case SB_ACK_READ_EXTENDED:
    case SB_ACK_WRITE:
    case SB_ACK_WRITE_EXTENDED:
    case SB_ACK_SET_ADDRESS:
    case SB_ACK_IDENTIFY:
    case SB_ACK_STATUS:
    case SB_NAK_READ:
    case SB_NAK_WRITE:
    case SB_NAK_WRITE_EXTENDED:
      *cmd = incmd;
      return SB_ERR_NONE;
    default:
      return SB_ERR_UNKNOWN_COMMAND;
  }
}

static enum emlError pdureadvalidcmd(const void* const msg, const size_t len, const enum pdu_command expected_cmd) {
  enum pdu_command pducmd = SB_CMD_NOP;
  enum pdu_error pduerr = pdureadcmd(msg, len, &pducmd);
  if (pduerr != SB_ERR_NONE) {
    dbglog_warn("received a malformed response from PDU "
        "(error %x)", pduerr);
    return EML_NETWORK_ERROR;
  }
  if (pducmd != expected_cmd) {
    dbglog_error("received unexpected response from PDU "
        "(command %x, expected %x)", pducmd, expected_cmd);
    return EML_NETWORK_ERROR;
  }
  return EML_SUCCESS;
}

static int pdu_init(cfg_t* const pducfg) {
  dbglog_info("Initializing PDU %s (%s:%ld)",
      cfg_title(pducfg),
      cfg_getstr(pducfg, "host"), cfg_getint(pducfg, "port"));

  struct addrinfo* info;
  struct addrinfo hints = {
    .ai_family = AF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = IPPROTO_TCP,
    .ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG
  };

  char portbuf[TCP_PORT_STR_MAXLEN];
  unsigned short port = cfg_getint(pducfg, "port");
  snprintf(portbuf, sizeof(portbuf), "%hu", port);

  int err;
  char* errmsg = NULL;

  err = getaddrinfo(cfg_getstr(pducfg, "host"), portbuf, &hints, &info);
  if (err) {
    dbglog_error("%s: %s", cfg_title(pducfg), gai_strerror(err));
    return -1;
  }

  struct pdustate* const restrict st = malloc(sizeof(*st));
  struct addrinfo* ai;
  for (ai = info; ai != NULL; ai = ai->ai_next) {
    st->sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (st->sockfd < 0) continue;

    int reuse = 1;
    err = setsockopt(st->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (err < 0) goto close_and_next;

    static const struct timeval delay = {
      .tv_sec = 5,
      .tv_usec = 0
    };
    err = setsockopt(st->sockfd, SOL_SOCKET, SO_SNDTIMEO, &delay, sizeof(delay));
    if (err < 0) goto close_and_next;
    err = setsockopt(st->sockfd, SOL_SOCKET, SO_RCVTIMEO, &delay, sizeof(delay));
    if (err < 0) goto close_and_next;

    err = connect(st->sockfd, ai->ai_addr, sizeof(*ai->ai_addr));
    if (!err) break;
    dbglog_info("Connecting %s (ai %p): %s", cfg_title(pducfg), (void*) ai, strerror(errno));

close_and_next:
    close(st->sockfd);
  }

  if (!ai) {
    errmsg = "Could not connect";
    goto err_free;
  }

  pthread_mutex_init(&st->msglock, NULL);

  //prepare key and buffers before calling comm functions
  memcpy(&st->sendbuf, TAG, sizeof(TAG));
  const char* rc4keydata = cfg_getptr(pducfg, "rc4key");
  memcpy(&st->keydata, rc4keydata, RC4KEY_MAXLEN);
  st->recvoff = 0;
  st->recvremaining = 0;
  st->lastblk[0] = '\0';

  unsigned char cmdbuf[PACKET_MAXLEN];

  size_t cmdsize = pduwritecmd(cmdbuf, SB_CMD_BCAST_IDENTIFY);
  assert(cmdsize <= sizeof(cmdbuf));
  ssize_t sent = pduwrite(st, cmdbuf, cmdsize);
  if (sent < 0) {
    dbglog_error("pduwrite returned %zd", sent);
    goto err_close_socket;
  }

  ssize_t read;
  while ((read = pduread(st, cmdbuf, sizeof(cmdbuf))) > 0) {
    if (read != (ssize_t) SCAN_PACKET_LEN) {
      dbglog_warn("received a malformed response from PDU "
          "(unexpected length %zd)", read);
      goto err_close_socket_badmsg;
    }

    enum emlError err;

    err = pdureadvalidcmd(cmdbuf, read, SB_ACK_IDENTIFY);
    if (err != EML_SUCCESS) goto err_close_socket_badmsg;

    //query available number of measured outlets
    const uint16_t address = 1;
    const uint16_t reg = SB_REG_CFNRMO;
    const uint16_t reglen = 1;
    size_t cmdlen = pduwritecmd(cmdbuf, SB_CMD_READ, address, reg, reglen);
    sent = pduwrite(st, cmdbuf, cmdlen);
    if (sent < 0) {
      dbglog_error("pduwrite returned %zd", sent);
      goto err_close_socket;
    }

    read = pduread(st, cmdbuf, sizeof(cmdbuf));
    if (read < 0) {
      dbglog_error("pduread returned %zd", read);
      goto err_close_socket;
    }

    err = pdureadvalidcmd(cmdbuf, read, SB_ACK_READ);
    if (err != EML_SUCCESS) goto err_close_socket_badmsg;
    st->noutlets = cmdbuf[10];

    sb_pdu_driver.devices = realloc(sb_pdu_driver.devices,
        (sb_pdu_driver.ndevices + st->noutlets) * sizeof(*sb_pdu_driver.devices));
    assert(sb_pdu_driver.devices);

    for (size_t i = 0; i < st->noutlets; i++) {
      struct emlDevice devinit = {
        .driver = &sb_pdu_driver,
        .index = sb_pdu_driver.ndevices,
      };
      snprintf(devinit.name, sizeof(devinit.name), "%s%zu_outlet%zu",
          sb_pdu_driver.name, npdus, i);

      struct emlDevice* const dev = &sb_pdu_driver.devices[devinit.index];
      memcpy(dev, &devinit, sizeof(*dev));

      devstate[sb_pdu_driver.ndevices].pdu = npdus;
      devstate[sb_pdu_driver.ndevices].outlet = i;
      sb_pdu_driver.ndevices++;
    }

    pdustate[npdus] = st;
    npdus++;
  }

  if (!npdus) {
    errmsg = "Did not receive any PDU responses";
    goto err_close_socket;
  }

  return 0;

err_close_socket_badmsg:
  errno = EBADMSG;

err_close_socket:
  if (!errmsg) errmsg = strerror(errno);
  err = close(st->sockfd);
  if (err < 0) {
    dbglog_error("Closing socket for %s: %s", cfg_title(pducfg), strerror(errno));
  }

err_free:
  if (!errmsg) errmsg = strerror(errno);
  freeaddrinfo(info);
  dbglog_warn("%s: %s", cfg_title(pducfg), errmsg);
  return -1;
}

static int pdu_shutdown(const size_t pduno) {
  int err;
  err = shutdown(pdustate[pduno]->sockfd, SHUT_RDWR);
  if (err) {
    dbglog_info("shutting down socket: %s", strerror(errno));
  }
  err = close(pdustate[pduno]->sockfd);
  if (err) {
    dbglog_info("closing socket: %s", strerror(errno));
  }

  pthread_mutex_destroy(&pdustate[pduno]->msglock);

  return 0;
}

static enum emlError init(cfg_t* config) {
  assert(!sb_pdu_driver.initialized);
  assert(config);
  sb_pdu_driver.config = config;

  sb_pdu_driver.ndevices = 0;
  sb_pdu_driver.devices = malloc(0);

  cfg_t* pducfg;
  npdus = 0;
  for (size_t i = 0; (pducfg = cfg_getnsec(config, "device", i)); i++)
    pdu_init(pducfg);

  const uint16_t address = 1;
  //we want to contiguously read OMCRAC/OMCRPK/OMVOAC for all channels 1..27
  const uint16_t reg = SB_REG_OMCRAC;
  const uint16_t blklen = MEASURE_REG_LEN * NCHANNELS * 3;

  measurecmdlen = pduwritecmd(measurecmdbuf, SB_CMD_READ,
      address, reg, blklen);

  sb_pdu_driver.initialized = 1;
  return EML_SUCCESS;
}

static enum emlError shutdowndrv() {
  assert(sb_pdu_driver.initialized);

  sb_pdu_driver.initialized = 0;

  for (size_t i = 0; i < npdus; i++)
    pdu_shutdown(i);

  free(sb_pdu_driver.devices);

  return EML_SUCCESS;
}

static enum emlError measure(size_t devno, unsigned long long* values) {
  assert(sb_pdu_driver.initialized);
  assert(devno < sb_pdu_driver.ndevices);


  const size_t pduno = devstate[devno].pdu;
  const size_t outlet = devstate[devno].outlet;

  ssize_t sent;
  ssize_t read;
  enum emlError err;

  pthread_mutex_lock(&pdustate[pduno]->msglock);

  //only actually query the pdu if there is no fresh block
  unsigned long long now = nanotimestamp();
  if (pdustate[pduno]->lastblk[0] == '\0' || (now - pdustate[pduno]->lastts) > MEASURE_TTL)
  {
    sent = pduwrite(pdustate[pduno], measurecmdbuf, measurecmdlen);
    if (sent < 0) {
      dbglog_error("pduwrite returned %zd", sent);
      goto err_unlock_mutex;
    }

    read = pduread(pdustate[pduno], pdustate[pduno]->lastblk, sizeof(pdustate[pduno]->lastblk));
    if (read < 0) {
      dbglog_error("pduread returned %zd", sent);
      goto err_unlock_mutex;
    }

    err = pdureadvalidcmd(pdustate[pduno]->lastblk, read, SB_ACK_READ);
    if (err != EML_SUCCESS) goto err_unlock_mutex;

    pdustate[pduno]->lastts = nanotimestamp();
  }

  values[0] = pdustate[pduno]->lastts;

  //current RMS in centiampères, < 0.5% deviation
  const size_t currentpos = 10 + MEASURE_REG_LEN * outlet;
  const uint16_t current = (pdustate[pduno]->lastblk[currentpos + 1] << 8) + pdustate[pduno]->lastblk[currentpos];

  //voltage RMS in centivolts, < 0.5% deviation
  const size_t voltagepos = currentpos + MEASURE_REG_LEN * NCHANNELS * 2;
  const uint16_t voltage = (pdustate[pduno]->lastblk[voltagepos + 1] << 8) + pdustate[pduno]->lastblk[voltagepos];

  pthread_mutex_unlock(&pdustate[pduno]->msglock);

  //apparent power in volt-ampères * 1e4
  const unsigned long long power = (unsigned long long) voltage * current;

  values[sb_pdu_driver.default_props->inst_power_field * DATABLOCK_SIZE] = power;

  return EML_SUCCESS;

err_unlock_mutex:
  pthread_mutex_unlock(&pdustate[pduno]->msglock);
  return EML_NETWORK_ERROR;
}

// default measurement properties for this driver
static const struct emlDataProperties default_props = {
  .time_factor = EML_SI_NANO,
  //PDU power calculations up to 1e-4 precision
  .energy_factor = -10000,
  .power_factor = -10000,
  .inst_energy_field = 0,
  .inst_power_field = 1,
};

static char hexval(char c) {
  return (c&15)+(c>>6)*9;
}

static int cfg_rc4key_parsecb(cfg_t* cfg, cfg_opt_t* opt, const char* value, void* result) {
  size_t len = strlen(value);

  bool hex_mode = false;
  if (len == RC4KEY_MAXLEN * 2) {
    hex_mode = true;
    len /= 2;
  }

  if (len > RC4KEY_MAXLEN) {
    dbglog_info("rc4key is %zu chars long", len);
    cfg_error(cfg, "\"%s\" invalid size "
        "(should be either up to %d ASCII characters, or exactly %d hex digits)",
        opt->name, RC4KEY_MAXLEN, RC4KEY_MAXLEN * 2);
    return 1;
  }

  char* keybuf = malloc(RC4KEY_MAXLEN);
  if (!hex_mode)
    memcpy(keybuf, value, len);
  else {
    for (size_t i = 0; i < len; i++) {
      char c1 = value[i*2];
      char c2 = value[i*2+1];
      if (!isxdigit(c1) || !isxdigit(c2)) {
        cfg_error(cfg, "\"%s\" invalid hex "
            "(must contain %d hex digits)",
            opt->name, RC4KEY_MAXLEN * 2);
        return 1;
      }

      keybuf[i] = (hexval(c1) << 4) + hexval(c2);
    }
  }

  //right-pad the user-provided ASCII keys with '0' like spapi.pm does
  memset(keybuf + len, '0', RC4KEY_MAXLEN - len);

  *(void**) result = (void*) keybuf;
  return 0;
}

static cfg_opt_t deviceopts[] = {
  CFG_STR("host", SB_DEFAULT_HOST, CFGF_NONE),
  CFG_INT("port", SB_DEFAULT_PORT, CFGF_NONE),
  CFG_PTR_CB("rc4key", SB_DEFAULT_RC4KEY, CFGF_NONE,
      &cfg_rc4key_parsecb, &free),
  CFG_END()
};

static cfg_opt_t cfgopts[] = {
  CFG_BOOL("disabled", cfg_false, CFGF_NONE),
  CFG_INT("sampling_interval", SB_DEFAULT_SAMPLING_INTERVAL, CFGF_NONE),

  CFG_SEC("device", deviceopts, CFGF_MULTI | CFGF_TITLE | CFGF_NO_TITLE_DUPES),

  CFG_END()
};

//public driver state and interface
struct emlDriver sb_pdu_driver = {
  .name = "sb_pdu",
  .type = EML_DEV_SB_PDU,
  .failed_reason = "",
  .default_props = &default_props,
  .cfgopts = cfgopts,

  .init = &init,
  .shutdown = &shutdowndrv,
  .measure = &measure,
};
