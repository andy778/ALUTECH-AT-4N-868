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

This is NOT a Microchip HCS200/HCS301 (KeeLoq) device. The transmitter is
built around a general-purpose Arm Cortex-M0+ MCU (PUYA PY32F002A) running
a proprietary rolling-code firmware. The protocol was reverse engineered by
the Flipper Zero project (lib/subghz/protocols/alutech_at_4n.c).

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

        /* clang-format off */
        data_t *data = data_make(
                "model",        "",             DATA_STRING, "Alutech-AT4N",
                "code",         "Encrypted",    DATA_STRING, code_str,
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
        "code",
        "mic",
        NULL,
};

r_device const alutech_at_4n = {
        .name        = "Alutech AT-4N-868 garage/gate remote (rolling code)",
        .modulation  = OOK_PULSE_PWM,
        .short_width = 400,
        .long_width  = 800,
        .gap_limit   = 1500,
        .reset_limit = 4400,
        .tolerance   = 160, // us
        .decode_fn   = &alutech_at_4n_decode,
        .fields      = output_fields,
};
