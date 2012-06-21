/*****************************************************************************
 * dvbpsi.c: conversion from TS packets to PSI sections
 *----------------------------------------------------------------------------
 * Copyright (C) 2001-2011 VideoLAN
 * $Id$
 *
 * Authors: Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *----------------------------------------------------------------------------
 *
 *****************************************************************************/

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include <assert.h>

#include "dvbpsi.h"
#include "dvbpsi_private.h"
#include "psi.h"


/*****************************************************************************
 * dvbpsi_crc32_table
 *****************************************************************************
 * This table is used to compute a PSI CRC byte per byte instead of bit per
 * bit. It's been generated by 'gen_crc' in the 'misc' directory:
 *
 *   uint32_t table[256];
 *   uint32_t i, j, k;
 *
 *   for(i = 0; i < 256; i++)
 *   {
 *     k = 0;
 *     for (j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1)
 *       k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);
 *     table[i] = k;
 *   }
 *
 * A CRC is computed like this:
 *
 *   initialization
 *   --------------
 *   uint32_t i_crc = 0xffffffff;
 *
 *   for each data byte do
 *   ---------------------
 *   i_crc = (i_crc << 8) ^ dvbpsi_crc32_table[(i_crc >> 24) ^ (data_byte)];
 *****************************************************************************/
uint32_t dvbpsi_crc32_table[256] =
{
  0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
  0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
  0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
  0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
  0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
  0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
  0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
  0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
  0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
  0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
  0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
  0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
  0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
  0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
  0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
  0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
  0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
  0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
  0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
  0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
  0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
  0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
  0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
  0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
  0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
  0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
  0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
  0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
  0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
  0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
  0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
  0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
  0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
  0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
  0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
  0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
  0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
  0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
  0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
  0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
  0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
  0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
  0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
  0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
  0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
  0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
  0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
  0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
  0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
  0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
  0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
  0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
  0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
  0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
  0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
  0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
  0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
  0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
  0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
  0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
  0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
  0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
  0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
  0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

/*****************************************************************************
 * dvbpsi_NewHandle
 *****************************************************************************/
dvbpsi_t *dvbpsi_NewHandle(dvbpsi_message_cb callback, enum dvbpsi_msg_level level)
{
    dvbpsi_t *handle = calloc(1, sizeof(dvbpsi_t));
    if (handle != NULL)
    {
        handle->p_private  = NULL;
        handle->pf_message = callback;
        handle->i_msg_level = level;
    }
    return handle;
}

/*****************************************************************************
 * dvbpsi_DeleteHandle
 *****************************************************************************/
void dvbpsi_DeleteHandle(dvbpsi_t *handle)
{
    assert(handle->p_private == NULL);
    handle->pf_message = NULL;
    free(handle);
}

/*****************************************************************************
 * dvbpsi_NewDecoder
 *****************************************************************************/
#define DVBPSI_INVALID_CC (0xFF)
dvbpsi_decoder_t *dvbpsi_NewDecoder(dvbpsi_callback_gather_t pf_gather,
    const int i_section_max_size, const bool b_discontinuity, const size_t psi_size)
{
    assert(psi_size >= sizeof(dvbpsi_decoder_t));

    dvbpsi_decoder_t *p_decoder = (dvbpsi_decoder_t *) calloc(1, psi_size);
    if (p_decoder == NULL)
        return NULL;

    p_decoder->pf_gather = pf_gather;
    p_decoder->p_current_section = NULL;
    p_decoder->i_section_max_size = i_section_max_size;
    p_decoder->b_discontinuity = b_discontinuity;
    p_decoder->i_continuity_counter = DVBPSI_INVALID_CC;
    p_decoder->p_current_section = NULL;
    p_decoder->b_current_valid = false;

    p_decoder->i_last_section_number = 0;
    for (unsigned int i = 0; i <= 255; i++)
        p_decoder->ap_sections[i] = NULL;

    p_decoder->b_complete_header = false;

    return p_decoder;
}

/*****************************************************************************
 * dvbpsi_DeleteDecoder
 *****************************************************************************/
void dvbpsi_DeleteDecoder(dvbpsi_decoder_t *p_decoder)
{
    assert(p_decoder);

    for (unsigned int i = 0; i <= 255; i++)
    {
        if (p_decoder->ap_sections[i])
            dvbpsi_DeletePSISections(p_decoder->ap_sections[i]);
        p_decoder->ap_sections[i] = NULL;
    }

    dvbpsi_DeletePSISections(p_decoder->p_current_section);
    free(p_decoder);
}

/*****************************************************************************
 * dvbpsi_HasDecoder
 *****************************************************************************/
bool dvbpsi_HasDecoder(dvbpsi_t *p_dvbpsi)
{
    if (p_dvbpsi && p_dvbpsi->p_private)
        return true;
    else
        return false;
}

/*****************************************************************************
 * dvbpsi_PushPacket
 *****************************************************************************
 * Injection of a TS packet into a PSI decoder.
 *****************************************************************************/
bool dvbpsi_PushPacket(dvbpsi_t *handle, uint8_t* p_data)
{
    uint8_t i_expected_counter;           /* Expected continuity counter */
    dvbpsi_psi_section_t* p_section;      /* Current section */
    uint8_t* p_payload_pos;               /* Where in the TS packet */
    uint8_t* p_new_pos = NULL;            /* Beginning of the new section,
                                             updated to NULL when the new
                                             section is handled */
    int i_available;                      /* Byte count available in the
                                             packet */

    dvbpsi_decoder_t *p_decoder = (dvbpsi_decoder_t *)handle->p_private;
    assert(p_decoder);

    /* TS start code */
    if (p_data[0] != 0x47)
    {
        dvbpsi_error(handle, "PSI decoder", "not a TS packet");
        return false;
    }

    /* Continuity check */
    bool b_first = (p_decoder->i_continuity_counter == DVBPSI_INVALID_CC);
    if (b_first)
        p_decoder->i_continuity_counter = p_data[3] & 0xf;
    else
    {
        i_expected_counter = (p_decoder->i_continuity_counter + 1) & 0xf;
        p_decoder->i_continuity_counter = p_data[3] & 0xf;

        if (i_expected_counter == ((p_decoder->i_continuity_counter + 1) & 0xf)
            && !p_decoder->b_discontinuity)
        {
            dvbpsi_error(handle, "PSI decoder",
                     "TS duplicate (received %d, expected %d) for PID %d",
                     p_decoder->i_continuity_counter, i_expected_counter,
                     ((uint16_t)(p_data[1] & 0x1f) << 8) | p_data[2]);
            return false;
        }

        if (i_expected_counter != p_decoder->i_continuity_counter)
        {
            dvbpsi_error(handle, "PSI decoder",
                     "TS discontinuity (received %d, expected %d) for PID %d",
                     p_decoder->i_continuity_counter, i_expected_counter,
                     ((uint16_t)(p_data[1] & 0x1f) << 8) | p_data[2]);
            p_decoder->b_discontinuity = true;
            if (p_decoder->p_current_section)
            {
                dvbpsi_DeletePSISections(p_decoder->p_current_section);
                p_decoder->p_current_section = NULL;
            }
        }
    }

    /* Return if no payload in the TS packet */
    if (!(p_data[3] & 0x10))
        return false;

    /* Skip the adaptation_field if present */
    if (p_data[3] & 0x20)
        p_payload_pos = p_data + 5 + p_data[4];
    else
        p_payload_pos = p_data + 4;

    /* Unit start -> skip the pointer_field and a new section begins */
    if (p_data[1] & 0x40)
    {
        p_new_pos = p_payload_pos + *p_payload_pos + 1;
        p_payload_pos += 1;
    }

    p_section = p_decoder->p_current_section;

    /* If the psi decoder needs a beginning of a section and a new section
       begins in the packet then initialize the dvbpsi_psi_section_t structure */
    if (p_section == NULL)
    {
        if (p_new_pos)
        {
            /* Allocation of the structure */
            p_decoder->p_current_section
                        = p_section
                        = dvbpsi_NewPSISection(p_decoder->i_section_max_size);
            if (!p_section)
                return false;
            /* Update the position in the packet */
            p_payload_pos = p_new_pos;
            /* New section is being handled */
            p_new_pos = NULL;
            /* Just need the header to know how long is the section */
            p_decoder->i_need = 3;
            p_decoder->b_complete_header = false;
        }
        else
        {
            /* No new section => return */
            return false;
        }
    }

    /* Remaining bytes in the payload */
    i_available = 188 + p_data - p_payload_pos;

    while (i_available > 0)
    {
        if (i_available >= p_decoder->i_need)
        {
            /* There are enough bytes in this packet to complete the
               header/section */
            memcpy(p_section->p_payload_end, p_payload_pos, p_decoder->i_need);
            p_payload_pos += p_decoder->i_need;
            p_section->p_payload_end += p_decoder->i_need;
            i_available -= p_decoder->i_need;

            if (!p_decoder->b_complete_header)
            {
                /* Header is complete */
                p_decoder->b_complete_header = true;
                /* Compute p_section->i_length and update p_decoder->i_need */
                p_decoder->i_need = p_section->i_length
                                 =   ((uint16_t)(p_section->p_data[1] & 0xf)) << 8
                                       | p_section->p_data[2];
                /* Check that the section isn't too long */
                if (p_decoder->i_need > p_decoder->i_section_max_size - 3)
                {
                    dvbpsi_error(handle, "PSI decoder", "PSI section too long");
                    dvbpsi_DeletePSISections(p_section);
                    p_decoder->p_current_section = NULL;
                    /* If there is a new section not being handled then go forward
                       in the packet */
                    if (p_new_pos)
                    {
                        p_decoder->p_current_section
                                    = p_section
                                    = dvbpsi_NewPSISection(p_decoder->i_section_max_size);
                        if (!p_section)
                            return false;
                        p_payload_pos = p_new_pos;
                        p_new_pos = NULL;
                        p_decoder->i_need = 3;
                        p_decoder->b_complete_header = false;
                        i_available = 188 + p_data - p_payload_pos;
                    }
                    else
                    {
                        i_available = 0;
                    }
                }
            }
            else
            {
                /* PSI section is complete */
                p_section->b_syntax_indicator = p_section->p_data[1] & 0x80;
                p_section->b_private_indicator = p_section->p_data[1] & 0x40;
                /* Update the end of the payload if CRC_32 is present */
                if (p_section->b_syntax_indicator)
                    p_section->p_payload_end -= 4;

                if ((p_section->p_data[0] == 0x70) /* TDT (has no CRC 32) */ ||
                    (p_section->p_data[0] != 0x72 && dvbpsi_ValidPSISection(p_section)))
                {
                    /* PSI section is valid */
                    p_section->i_table_id = p_section->p_data[0];
                    if (p_section->b_syntax_indicator)
                    {
                        p_section->i_extension =   (p_section->p_data[3] << 8)
                                                 | p_section->p_data[4];
                        p_section->i_version = (p_section->p_data[5] & 0x3e) >> 1;
                        p_section->b_current_next = p_section->p_data[5] & 0x1;
                        p_section->i_number = p_section->p_data[6];
                        p_section->i_last_number = p_section->p_data[7];
                        p_section->p_payload_start = p_section->p_data + 8;
                    }
                    else
                    {
                        p_section->i_extension = 0;
                        p_section->i_version = 0;
                        p_section->b_current_next = 1;
                        p_section->i_number = 0;
                        p_section->i_last_number = 0;
                        p_section->p_payload_start = p_section->p_data + 3;
                    }
                    if (p_decoder->pf_gather)
                        p_decoder->pf_gather(handle, p_section);
                    p_decoder->p_current_section = NULL;
                }
                else
                {
                    if (!dvbpsi_ValidPSISection(p_section))
                        dvbpsi_error(handle, "misc PSI", "Bad CRC_32 table 0x%x !!!",
                                             p_section->p_data[0]);
                    else
                        dvbpsi_error(handle, "misc PSI", "table 0x%x", p_section->p_data[0]);

                    /* PSI section isn't valid => trash it */
                    dvbpsi_DeletePSISections(p_section);
                    p_decoder->p_current_section = NULL;
                }

                /* A TS packet may contain any number of sections, only the first
                 * new one is flagged by the pointer_field. If the next payload
                 * byte isn't 0xff then a new section starts. */
                if (p_new_pos == NULL && i_available && *p_payload_pos != 0xff)
                    p_new_pos = p_payload_pos;

                /* If there is a new section not being handled then go forward
                   in the packet */
                if (p_new_pos)
                {
                    p_decoder->p_current_section
                              = p_section
                              = dvbpsi_NewPSISection(p_decoder->i_section_max_size);
                    if (!p_section)
                        return false;
                    p_payload_pos = p_new_pos;
                    p_new_pos = NULL;
                    p_decoder->i_need = 3;
                    p_decoder->b_complete_header = false;
                    i_available = 188 + p_data - p_payload_pos;
                }
                else
                {
                    i_available = 0;
                }
            }
        }
        else
        {
            /* There aren't enough bytes in this packet to complete the
               header/section */
            memcpy(p_section->p_payload_end, p_payload_pos, i_available);
            p_section->p_payload_end += i_available;
            p_decoder->i_need -= i_available;
            i_available = 0;
        }
    }
    return true;
}
#undef DVBPSI_INVALID_CC

/*****************************************************************************
 * Message error level:
 * -1 is disabled,
 *  0 is errors only
 *  1 is warning and errors
 *  2 is debug, warning and errors
 *****************************************************************************/
#if !defined(_GNU_SOURCE)
#   define DVBPSI_MSG_SIZE 1024
#endif

#define DVBPSI_MSG_FORMAT "libdvbpsi (%s): %s"

#ifdef HAVE_VARIADIC_MACROS
void message(dvbpsi_t *dvbpsi, const dvbpsi_msg_level_t level, const char *fmt, ...)
{
    if ((dvbpsi->i_msg_level > DVBPSI_MSG_NONE) &&
        (level <= dvbpsi->i_msg_level))
    {
        va_list ap;
        va_start(ap, fmt);
        char *msg = NULL;
#if defined(_GNU_SOURCE)
        int err = vasprintf(&msg, fmt, ap);
#else
        msg = malloc(DVBPSI_MSG_SIZE);
        if (msg == NULL)
            return;
        if (snprintf(&msg, DVBPSI_MSG_SIZE, DVBPSI_MSG_FORMAT, ap) < 0) {
            free(msg);
            return;
        }
        int err = vsnprintf(&msg, DVBPSI_MSG_SIZE, fmt, ap);
#endif
        va_end(ap);
        if (err > DVBPSI_MSG_NONE) {
            if (dvbpsi->pf_message)
                dvbpsi->pf_message(dvbpsi, level, msg);
        }
        free(msg);
    }
}
#else

/* Common code for printing messages */
#   if defined(_GNU_SOURCE)
#       define DVBPSI_MSG_COMMON(level)                         \
    do {                                                        \
        va_list ap;                                             \
        va_start(ap, fmt);                                      \
        char *tmp = NULL;                                       \
        int err = vasprintf(&tmp, fmt, ap);                     \
        if (err < 0)                                            \
            return;                                             \
        char *msg = NULL;                                       \
        if (asprintf(&msg, DVBPSI_MSG_FORMAT, src, tmp) < 0) {  \
            free(tmp);                                          \
            return;                                             \
        }                                                       \
        free(tmp);                                              \
        va_end(ap);                                             \
        if (err > 0) {                                          \
            if (dvbpsi->pf_message)                             \
                dvbpsi->pf_message(dvbpsi, level, msg);         \
        }                                                       \
        free(msg);                                              \
    } while(0);
#   else
#       define DVBPSI_MSG_COMMON                                \
    do {                                                        \
        va_list ap;                                             \
        va_start(ap, fmt);                                      \
        char *msg = malloc(DVBPSI_MSG_SIZE);                    \
        if (msg == NULL)                                        \
            return;                                             \
        if (snprintf(&msg, DVBPSI_MSG_SIZE, DVBPSI_MSG_FORMAT, src) < 0) \
            return;                                             \
        int err = vsnprintf(&msg, DVBPSI_MSG_SIZE, fmt, ap);    \
        va_end(ap);                                             \
        if (err > 0) {                                          \
            if (dvbpsi->pf_message)                             \
                dvbpsi->pf_message(dvbpsi, level, msg);         \
        }                                                       \
        free(msg);                                              \
    } while(0);
#   endif

void dvbpsi_error(dvbpsi_t *dvbpsi, const char *src, const char *fmt, ...)
{
    if ((dvbpsi->i_msg_level > DVBPSI_MSG_NONE) &&
        (DVBPSI_MSG_ERROR <= dvbpsi->i_msg_level))
    {
        DVBPSI_MSG_COMMON(DVBPSI_MSG_ERROR)
    }
}

void dvbpsi_warning(dvbpsi_t *dvbpsi, const char *src, const char *fmt, ...)
{
    if ((dvbpsi->i_msg_level > DVBPSI_MSG_NONE) &&
        (DVBPSI_MSG_WARN <= dvbpsi->i_msg_level))
    {
        DVBPSI_MSG_COMMON(DVBPSI_MSG_WARN)
    }
}

void dvbpsi_debug(dvbpsi_t *dvbpsi, const char *src, const char *fmt, ...)
{
    if ((dvbpsi->i_msg_level > DVBPSI_MSG_NONE) &&
        (DVBPSI_MSG_DEBUG <= dvbpsi->i_msg_level))
    {
        DVBPSI_MSG_COMMON(DVBPSI_MSG_DEBUG)
    }
}
#endif
