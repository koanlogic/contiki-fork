/*
 * Copyright (c) 2012, KoanLogic Srl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */
#include "rest-z1.h"

/** Convert a colour string ("r", "g", or "b") to enumeration. */
static uint8_t __col2enum (const char *s_col, size_t len)
{
    if (strncmp(s_col, "r", len) == 0)
        return LEDS_RED;
    else if (strncmp(s_col, "g", len) == 0)
        return LEDS_GREEN;
    else if (strncmp(s_col, "b", len) == 0)
        return LEDS_BLUE;
    else
        return -1;
}

/** Convert from string ("0", or "1") to boolean. */
static uint8_t __str2bool (const char *s_on, size_t len)
{
    if (strncmp(s_on, "1", len) == 0)
        return 1;
    else if (strncmp(s_on, "0", len) == 0)
        return 0;
    else
        return -1;
}

/** Fetch a string representation of the current temperature from sensor. */
static int __calc_tmp (char *s, int s_sz, int *t_sz)
{
    int16_t raw;
    int16_t intg;
    int16_t sign = 1;
    uint16_t frac;
    uint16_t absraw;

    raw = tmp102_read_temp_raw();
    absraw = raw;

    if(raw < 0)   /* perform 2C's if sensor returned negative data */
    {
      absraw = (raw ^ 0xFFFF) + 1;
      sign = -1;
    }
    intg = (absraw >> 8) * sign;
    frac = ((absraw >> 4) % 16) * 625;  /* info in 1/10000 of degree */

    if ((intg == 0) && (sign == -1))
        *t_sz = snprintf(s, s_sz, "-%d.%04d", intg, frac);
    else
        *t_sz = snprintf(s, s_sz, "%d.%04d", intg, frac);

    ERR_IF (*t_sz <= 0);

    return 0;
err:
    return ~0;
}

/**
 * Fetch a string representation of the the x, y and z axis values from
 * accelerometer.
 */
static int __calc_acc (char *s, int s_sz, int *t_sz)
{
    int16_t x, y, z;

    x = accm_read_axis(X_AXIS);
    y = accm_read_axis(Y_AXIS);
    z = accm_read_axis(Z_AXIS);

    *t_sz = snprintf(s, s_sz, "%hd,%hd,%hd", x, y, z);
    ERR_IF (*t_sz <= 0);

    return 0;
err:
    return ~0;
}

/**
 * PUT or POST to change led values.
 *
 * url = '/leds?col=COL&on=BOOL',
 *
 * where COL=   "r"|"g"|"b" and
 *       BOOL=  "0"|"1"
 */
RESOURCE(leds, METHOD_PUT | METHOD_POST, "leds", "title=\"LED controls\";rt=\"Text\"");
void leds_handler (void* request, void* response, uint8_t *buffer, uint16_t
        preferred_size, int32_t *offset)
{
    uint8_t col = LEDS_RED;       /* default to red */
    uint8_t on = 1;               /* default to on */
    const char *s_col = NULL;
    const char *s_on = NULL;
    size_t len;

    if ((len = REST.get_query_variable(request, "col", &s_col)))
        ERR_IF ((col = __col2enum(s_col, len)) == -1);

    if ((len = REST.get_query_variable(request, "on", &s_on)))
        ERR_IF ((on = __str2bool(s_on, len)) == -1);

    if (on)
        leds_on(col);
    else
        leds_off(col);

    return;
err:
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    return;
}

/**
 * GET the temperature.
 *
 * url = '/tmp',
 *
 * which returns something like '26.1234' (4dp).
 */
RESOURCE(tmp, METHOD_GET, "tmp", "title=\"Temperature\";rt=\"Text\"");
void tmp_handler (void* request, void* response, uint8_t *buffer, uint16_t
        preferred_size, int32_t *offset)
{
    char s_tmp[20];
    int length;

    ERR_IF (__calc_tmp(s_tmp, sizeof(s_tmp), &length));
    memcpy(buffer, s_tmp, length);

    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, buffer, length);

    return;
err:
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    return;
}

/**
 * GET axis values from accelerometer.
 *
 * url = '/acc',
 *
 * which returns something like: '100,100,200'.
 */
RESOURCE(acc, METHOD_GET, "acc", "title=\"Accelerometer\";rt=\"Text\"");
void acc_handler (void* request, void* response, uint8_t *buffer, uint16_t
        preferred_size, int32_t *offset)
{
    char s_acc[20];
    int length;

    ERR_IF (__calc_acc(s_acc, sizeof(s_acc), &length));

    memcpy(buffer, s_acc, length);

    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, buffer, length);

    return;
err:
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    return;
}

/**
 * GET temperature and axis values from accelerometer.
 *
 * url = '/acctmp',
 *
 * which returns something like '26.1234:100,100,200'.
 */
RESOURCE(acctmp, METHOD_GET, "acctmp", "title=\"Temperature and"\
        "Accelerometer\";rt=\"Text\"");
void acctmp_handler (void* request, void* response, uint8_t *buffer, uint16_t
        preferred_size, int32_t *offset)
{
    char s[30], st[10], *sp = s;
    int len, length, s_left = sizeof(s);

    ERR_IF (__calc_acc(st, sizeof(st), &len));
    BUF_APPEND(sp, s_left, st, len);

    BUF_APPEND(sp, s_left, ":", sizeof(char));

    ERR_IF (__calc_tmp(st, sizeof(st), &len));
    BUF_APPEND(sp, s_left, st, len);

    length = sizeof(s) - s_left;
    memcpy(buffer, s, length);

    REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
    REST.set_response_payload(response, buffer, length);

    return;
err:
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    return;
}

PROCESS(rest_server_example, "Z1 REST example");
AUTOSTART_PROCESSES(&rest_server_example);

/**
 * Main process
 *
 * Performs initialisations, activates resources and waits for events.
 */
PROCESS_THREAD (rest_server_example, ev, data)
{
    PROCESS_BEGIN();

    PRINTF("RF channel: %u\n", RF_CHANNEL);
    PRINTF("uIP buffer: %u\n", UIP_BUFSIZE);
    PRINTF("LL header: %u\n", UIP_LLH_LEN);
    PRINTF("IP+UDP header: %u\n", UIP_IPUDPH_LEN);
    PRINTF("REST max chunk: %u\n", REST_MAX_CHUNK_SIZE);
    PRINTF("XXX 0\n");

    /* initialize sensors */
    tmp102_init();
    accm_init();

    /* initialize REST framework */
    rest_init_framework();

    /* activate application-specific resources */
    rest_activate_resource(&resource_leds);
    rest_activate_resource(&resource_tmp);
    rest_activate_resource(&resource_acc);
    rest_activate_resource(&resource_acctmp);

    /* define application-specific events */
    while (1)
    {
        PROCESS_WAIT_EVENT();
    }

    PROCESS_END();
}
