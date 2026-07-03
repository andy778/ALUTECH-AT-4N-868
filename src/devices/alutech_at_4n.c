/** @file
    Alutech AT-4N-868 garage door / gate remote (rolling code).

    Copyright (C) 2026 Andy B

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
/**
Alutech AT-4N-868 garage door / gate remote (rolling code).

The transmitter is built around a general-purpose Arm Cortex-M0+ MCU
(PUYA PY32F002A) running a proprietary rolling-code firmware. The protocol
was reverse engineered by the Flipper Zero project
(lib/subghz/protocols/alutech_at_4n.c).

Modulation: ASK/OOK, PWM.
Frequency:  868.35 MHz (some units 433.92 MHz).

A keypress sends a 12-bit preamble (twelve short "1" pulses) followed by a
~4150 us gap, then a 72-bit payload, repeated several times.

    short pulse ~370-400 us = 1
    long  pulse ~740-800 us = 0

Frame, 72 bits = 9 bytes (as received, MSB first):

    | 0 .. 63 | encrypted payload (64 bit) |
    | 64 .. 71 | CRC-8                      |

The CRC and the encrypted payload can be recovered without any secret. The
inner fields (serial, rolling counter, button) are obfuscated with a per-
manufacturer "rainbow table" (six 32-bit magic constants) that is NOT public,
so this decoder validates the frame and emits the raw encrypted code only.

CRC details (verified against captured frames): each received byte is bit-
reversed, then a CRC-8 with polynomial 0x31 and init 0xff is computed over the
eight payload bytes and compared against the bit-reversed CRC byte.

    crc = crc8(poly=0x31, init=0xff, reflected I/O)

Verified against g012_868M_1000k.cu8 and g025_868M_1000k.cu8 (868.35 MHz).
The ~4150 us gap splits each burst into a 12-bit preamble row and a 72-bit
data row, so a flex test must keep gap_limit below it:

    rtl_433 -r g012_868M_1000k.cu8 -R 0 \
        -X 'n=alutech,m=OOK_PWM,s=400,l=800,r=4400,g=1500,t=160'

To build into rtl_433: add this file to src/devices/, declare
`DECL(alutech_at_4n)` in include/rtl_433_devices.h, and list it in
src/CMakeLists.txt / Makefile.am.
*/

#include "decoder.h"
#include <stdlib.h>

/*
Optional decryption of the inner fields (serial, counter, button).

The 64-bit payload is encrypted with an XTEA-style cipher keyed by six 32-bit
per-manufacturer constants (the "rainbow table"). These constants are the same
for the whole AT-4N product line, but are NOT shipped with rtl_433 - the
upstream project does not embed manufacturer keys.

To enable serial/button/counter output, pass the six values at runtime as a
decoder parameter (extract them from your own device, e.g. a Flipper Zero's
decrypted alutech_at_4n keystore):

    rtl_433 -R 321:9E3779B9,01234567,89ABCDEF,FEDCBA98,11223344,55667788

Values are 32-bit hex, in keystore order, separated by any non-hex character
(comma/colon/space). Without the parameter the decoder emits encrypted-only
output. Algorithm and field layout ported from the Flipper Zero project
(lib/subghz/protocols/alutech_at_4n.c).
*/
typedef struct {
    int have_key;
    uint32_t magic[6];
} alutech_ctx_t;

static int alutech_is_hex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/// Parse up to 6 hex constants from arg into magic[6]; returns the count parsed.
static int alutech_parse_key(char const *arg, uint32_t magic[6])
{
    int n = 0;
    while (n < 6 && arg && *arg) {
        while (*arg && !alutech_is_hex(*arg))
            arg++; // skip separators
        if (!*arg)
            break;
        char *end  = NULL;
        magic[n++] = (uint32_t)strtoul(arg, &end, 16);
        arg        = end;
    }
    return n;
}

static uint8_t alutech_decrypt_data_crc(uint8_t b)
{
    uint8_t crc = b;
    for (int i = 0; i < 8; ++i)
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    return (uint8_t)~crc;
}

/// Decrypt the 8 payload bytes p[8] into out[8] using the rainbow table magic[6].
static void alutech_decrypt(uint32_t const magic[6], uint8_t const *p, uint8_t *out)
{
    uint32_t d1 = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    uint32_t d2 = ((uint32_t)p[4] << 24) | ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 8) | p[7];
    uint32_t d3;
    uint32_t sum   = magic[0];
    unsigned guard = 0;

    do {
        d2 -= (magic[1] + (d1 << 4)) ^ (magic[2] + (d1 >> 5)) ^ (d1 + sum);
        d3 = d2 + sum;
        sum += magic[3];
        d1 -= (magic[4] + (d2 << 4)) ^ (magic[5] + (d2 >> 5)) ^ d3;
    } while (sum != 0 && ++guard < (1u << 20));

    out[0] = (uint8_t)(d1 >> 24);
    out[1] = (uint8_t)(d1 >> 16);
    out[2] = (uint8_t)(d1 >> 8);
    out[3] = (uint8_t)d1;
    out[4] = (uint8_t)(d2 >> 24);
    out[5] = (uint8_t)(d2 >> 16);
    out[6] = (uint8_t)(d2 >> 8);
    out[7] = (uint8_t)d2;
}

static int alutech_at_4n_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    // The ~4150 us gap after the preamble splits the transmission into two
    // rows: a 12-bit preamble (0xfff) and the 72-bit data row.
    int ret = DECODE_ABORT_LENGTH;

    for (unsigned row = 0; row < bitbuffer->num_rows; ++row) {
        if (bitbuffer->bits_per_row[row] != 72)
            continue;

        uint8_t *b = bitbuffer->bb[row];

        // Reject an all-ones payload (no signal / saturated capture).
        if (b[0] == 0xff && b[1] == 0xff && b[2] == 0xff && b[3] == 0xff)
            return DECODE_FAIL_SANITY;

        // Each on-air byte is transmitted LSB first; reflect to get the
        // payload bytes the CRC is computed over.
        uint8_t p[8];
        for (int i = 0; i < 8; ++i)
            p[i] = reverse8(b[i]);
        uint8_t recv_crc = reverse8(b[8]);

        uint8_t crc = crc8(p, 8, 0x31, 0xff);
        if (crc != recv_crc) {
            decoder_log(decoder, 2, __func__, "CRC mismatch");
            ret = DECODE_FAIL_MIC;
            continue;
        }

        // Encrypted 64-bit payload, MSB first for display.
        char code_str[17];
        snprintf(code_str, sizeof(code_str), "%02X%02X%02X%02X%02X%02X%02X%02X",
                b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);

        // Optional decryption (requires the rainbow table above). The decrypted
        // block carries its own check byte, so we only trust the fields when it
        // verifies - a wrong key simply falls back to encrypted-only output.
        alutech_ctx_t *ctx = decoder_user_data(decoder);
        uint8_t dec[8];
        int decoded = 0;
        if (ctx && ctx->have_key) {
            alutech_decrypt(ctx->magic, p, dec);
            decoded = dec[7] == alutech_decrypt_data_crc(dec[1]);
        }
        uint32_t serial  = 0;
        uint16_t counter = 0;
        int button       = 0;
        if (decoded) {
            serial  = (uint32_t)dec[3] | ((uint32_t)dec[4] << 8) | ((uint32_t)dec[5] << 16) | ((uint32_t)dec[6] << 24);
            counter = (uint16_t)(dec[1] | (dec[2] << 8));
            switch (dec[0]) {
            case 0xff: button = 1; break;
            case 0x11: button = 2; break;
            case 0x22: button = 3; break;
            case 0x33: button = 4; break;
            case 0x44: button = 5; break;
            default: button = 0; break;
            }
        }

        /* clang-format off */
        data_t *data = data_make(
                "model",        "",             DATA_STRING, "Alutech-AT4N",
                "id",           "Serial",       DATA_COND,   decoded, DATA_FORMAT, "%08X", DATA_INT, serial,
                "code",         "Encrypted",    DATA_STRING, code_str,
                "button",       "Button",       DATA_COND,   decoded, DATA_INT, button,
                "counter",      "Counter",      DATA_COND,   decoded, DATA_INT, counter,
                "mic",          "Integrity",    DATA_STRING, "CRC",
                NULL);
        /* clang-format on */

        decoder_output_data(decoder, data);
        return 1;
    }

    return ret;
}

static char const *const output_fields[] = {
        "model",
        "id",
        "code",
        "button",
        "counter",
        "mic",
        NULL,
};

r_device const alutech_at_4n;

static r_device *alutech_create(char *arg)
{
    r_device *r_dev = decoder_create(&alutech_at_4n, sizeof(alutech_ctx_t));
    if (!r_dev)
        return NULL; // NOTE: returns NULL on alloc failure to suppress protocol

    alutech_ctx_t *ctx = decoder_user_data(r_dev);
    if (alutech_parse_key(arg, ctx->magic) == 6)
        ctx->have_key = 1;

    return r_dev;
}

r_device const alutech_at_4n = {
        .name        = "Alutech AT-4N-868 garage/gate remote (rolling code)",
        .modulation  = OOK_PULSE_PWM,
        .short_width = 400,
        .long_width  = 800,
        .gap_limit   = 1500,
        .reset_limit = 4400,
        .tolerance   = 160, // us
        .decode_fn   = &alutech_at_4n_decode,
        .create_fn   = &alutech_create,
        .fields      = output_fields,
};
