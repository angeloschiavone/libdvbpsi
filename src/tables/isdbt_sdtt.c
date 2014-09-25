/*****************************************************************************
 * isdbt_sdtt.c: sdtt decoder/generator
 *----------------------------------------------------------------------------
 *
 * Author: Angelo Schiavone
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#elif defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include <assert.h>

#include "../dvbpsi.h"
#include "../dvbpsi_private.h"
#include "../psi.h"
#include "../descriptor.h"
#include "../demux.h"
#include "isdbt_sdtt.h"

typedef struct dvbpsi_isdbt_sdtt_decoder_s
{
    DVBPSI_DECODER_COMMON

    dvbpsi_isdbt_sdtt_callback pf_sdtt_callback;
    void * p_cb_data;

    dvbpsi_isdbt_sdtt_t current_sdtt;
    dvbpsi_isdbt_sdtt_t * p_building_sdtt;

} dvbpsi_isdbt_sdtt_decoder_t;

static void dvbpsi_isdbt_sdtt_sections_gather(dvbpsi_t *p_dvbpsi,
        dvbpsi_decoder_t *p_private_decoder, dvbpsi_psi_section_t * p_section);

static void dvbpsi_isdbt_sdtt_sections_decode(dvbpsi_isdbt_sdtt_t* p_sdtt,
        dvbpsi_psi_section_t* p_section);

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_attach
 *****************************************************************************
 * Initialize a sdtt subtable decoder.
 *****************************************************************************/
bool dvbpsi_isdbt_sdtt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension, dvbpsi_isdbt_sdtt_callback pf_callback,
        void* p_cb_data)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    dvbpsi_demux_t* p_demux = (dvbpsi_demux_t*) p_dvbpsi->p_decoder;

    if (dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension))
    {
        dvbpsi_error(p_dvbpsi, "sdtt decoder",
                "Already a decoder for (table_id == 0x%02x,"
                        "extension == 0x%02x)", i_table_id, i_extension);
        return false;
    }

    dvbpsi_isdbt_sdtt_decoder_t* p_sdtt_decoder;

    p_sdtt_decoder = (dvbpsi_isdbt_sdtt_decoder_t*) dvbpsi_decoder_new(NULL, 0,
    true, sizeof(dvbpsi_isdbt_sdtt_decoder_t));
    if (p_sdtt_decoder == NULL)
        return false;

    /* subtable decoder configuration */
    dvbpsi_demux_subdec_t* p_subdec;
    p_subdec = dvbpsi_NewDemuxSubDecoder(i_table_id, i_extension,
            dvbpsi_isdbt_sdtt_detach, dvbpsi_isdbt_sdtt_sections_gather,
            DVBPSI_DECODER(p_sdtt_decoder));
    if (p_subdec == NULL)
    {
        dvbpsi_decoder_delete(DVBPSI_DECODER(p_sdtt_decoder));
        return false;
    }

    /* Attach the subtable decoder to the demux */
    dvbpsi_AttachDemuxSubDecoder(p_demux, p_subdec);

    /* sdtt decoder information */
    p_sdtt_decoder->pf_sdtt_callback = pf_callback;
    p_sdtt_decoder->p_cb_data = p_cb_data;
    p_sdtt_decoder->p_building_sdtt = NULL;
    return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_detach
 *****************************************************************************
 * Close a sdtt decoder.
 *****************************************************************************/
void dvbpsi_isdbt_sdtt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *) p_dvbpsi->p_decoder;
    dvbpsi_demux_subdec_t* p_subdec;
    p_subdec = dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension);
    if (p_subdec == NULL)
    {
        dvbpsi_error(p_dvbpsi, "sdtt Decoder",
                "No such sdtt decoder (table_id == 0x%02x,"
                        "extension == 0x%02x)", i_table_id, i_extension);
        return;
    }
    assert(p_subdec->p_decoder);
    dvbpsi_isdbt_sdtt_decoder_t* p_sdtt_decoder;
    p_sdtt_decoder = (dvbpsi_isdbt_sdtt_decoder_t*) p_subdec->p_decoder;
    if (p_sdtt_decoder->p_building_sdtt)
        dvbpsi_isdbt_sdtt_delete(p_sdtt_decoder->p_building_sdtt);
    p_sdtt_decoder->p_building_sdtt = NULL;

    /* Free sub table decoder */
    dvbpsi_DetachDemuxSubDecoder(p_demux, p_subdec);
    dvbpsi_DeleteDemuxSubDecoder(p_subdec);
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_init
 *****************************************************************************
 * Initialize a pre-allocated dvbpsi_isdbt_sdtt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_sdtt_init(dvbpsi_isdbt_sdtt_t* p_sdtt, uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_transport_stream_id, uint16_t i_original_network_id,
        uint16_t i_service_id)
{
    assert(p_sdtt);

    p_sdtt->i_table_id = i_table_id;
    p_sdtt->i_extension = i_extension;
    p_sdtt->i_maker_id = (i_extension >> 8) & 0xff;
    p_sdtt->i_model_id = i_extension & 0xff;
    p_sdtt->i_version = i_version;
    p_sdtt->b_current_next = b_current_next;
    p_sdtt->i_original_network_id = i_original_network_id;
    p_sdtt->i_transport_stream_id = i_transport_stream_id;
    p_sdtt->i_original_network_id = i_original_network_id;
    p_sdtt->i_service_id = i_service_id;
    p_sdtt->i_num_of_contents = 0;
    p_sdtt->p_first_content = NULL;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_new
 *****************************************************************************
 * Allocate and Initialize a new dvbpsi_isdbt_sdtt_t structure.
 *****************************************************************************/
dvbpsi_isdbt_sdtt_t *dvbpsi_isdbt_sdtt_new(uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version,
        bool b_current_next, uint16_t i_transport_stream_id,
        uint16_t i_original_network_id, uint16_t i_service_id)
{
    dvbpsi_isdbt_sdtt_t *p_sdtt = (dvbpsi_isdbt_sdtt_t*) malloc(
            sizeof(dvbpsi_isdbt_sdtt_t));
    if (p_sdtt != NULL)
        dvbpsi_isdbt_sdtt_init(p_sdtt, i_table_id, i_extension, i_version,
                b_current_next, i_transport_stream_id, i_original_network_id,
                i_service_id);
    return p_sdtt;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_empty
 *****************************************************************************
 * Clean a dvbpsi_isdbt_sdtt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_sdtt_empty(dvbpsi_isdbt_sdtt_t* p_sdtt)
{
    dvbpsi_isdbt_sdtt_content_t * p_content = p_sdtt->p_first_content;

    while (p_content != NULL)
    {
        dvbpsi_isdbt_sdtt_content_t* p_content_tmp = p_content->p_next;
        dvbpsi_isdbt_sdtt_schedule_t * p_schedule = p_content->p_first_schedule;
        while (p_schedule != NULL)
        {
            dvbpsi_isdbt_sdtt_schedule_t* p_schedule_tmp = p_schedule->p_next;
            free(p_schedule);
            p_schedule = p_schedule_tmp;
        }
        p_content->p_first_schedule = NULL;
        dvbpsi_DeleteDescriptors(p_content->p_first_descriptor);
        free(p_content);
        p_content = p_content_tmp;
    }
    p_sdtt->i_num_of_contents = 0;
    p_sdtt->p_first_content = NULL;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_delete
 *****************************************************************************
 * Clean and Delete dvbpsi_isdbt_sdtt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_sdtt_delete(dvbpsi_isdbt_sdtt_t *p_sdtt)
{
    if (p_sdtt)
        dvbpsi_isdbt_sdtt_empty(p_sdtt);
    free(p_sdtt);
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_add
 *****************************************************************************
 * Add a content at the end of the SDTT.
 *****************************************************************************/
dvbpsi_isdbt_sdtt_content_t *dvbpsi_isdbt_sdtt_content_add(
        dvbpsi_isdbt_sdtt_t* p_sdtt, uint8_t i_group, uint16_t i_target_version,
        uint16_t i_new_version, uint8_t i_download_level,
        uint8_t i_version_indicator, uint8_t i_schedule_timeshift_information)
{
    dvbpsi_isdbt_sdtt_content_t * p_content;
    p_content = (dvbpsi_isdbt_sdtt_content_t*) calloc(1,
            sizeof(dvbpsi_isdbt_sdtt_content_t));
    if (p_content == NULL)
        return NULL;

    p_content->i_group = i_group;
    p_content->i_target_version = i_target_version;
    p_content->i_new_version = i_new_version;
    p_content->i_download_level = i_download_level;
    p_content->i_version_indicator = i_version_indicator;
    p_content->i_schedule_timeshift_information = i_schedule_timeshift_information;
    p_content->p_next = NULL;
    p_content->p_first_descriptor = NULL;
    p_content->p_first_schedule = NULL;
    p_sdtt->i_num_of_contents++;
    if (p_sdtt->p_first_content == NULL)
        p_sdtt->p_first_content = p_content;
    else
    {
        dvbpsi_isdbt_sdtt_content_t * p_last_content = p_sdtt->p_first_content;
        while (p_last_content->p_next != NULL)
            p_last_content = p_last_content->p_next;
        p_last_content->p_next = p_content;
    }
    return p_content;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_schedule_add
 *****************************************************************************
 * Add a schedule at the end of the schedule loop of the passed content.
 *****************************************************************************/
dvbpsi_isdbt_sdtt_schedule_t *dvbpsi_isdbt_sdtt_content_schedule_add(
        dvbpsi_isdbt_sdtt_content_t* p_sdtt_content, uint64_t i_start_time,
        uint32_t i_duration)
{
    dvbpsi_isdbt_sdtt_schedule_t * p_schedule;
    p_schedule = (dvbpsi_isdbt_sdtt_schedule_t*) calloc(1,
            sizeof(dvbpsi_isdbt_sdtt_schedule_t));
    if (p_schedule == NULL)
        return NULL;
    p_schedule->i_start_time = i_start_time;
    p_schedule->i_duration = i_duration;
    p_schedule->p_next = NULL;
    if (p_sdtt_content->p_first_schedule == NULL)
        p_sdtt_content->p_first_schedule = p_schedule;
    else
    {
        dvbpsi_isdbt_sdtt_schedule_t * p_last_schedule =
                p_sdtt_content->p_first_schedule;
        while (p_last_schedule->p_next != NULL)
            p_last_schedule = p_last_schedule->p_next;
        p_last_schedule->p_next = p_schedule;
    }
    return p_schedule;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_descriptor_add
 *****************************************************************************
 * Add a descriptor to the descriptor loop of passed sdtt content.
 *****************************************************************************/
dvbpsi_descriptor_t *dvbpsi_isdbt_sdtt_content_descriptor_add(
        dvbpsi_isdbt_sdtt_content_t* p_sdtt_content, uint8_t i_tag,
        uint8_t i_length, uint8_t *p_data)
{
    dvbpsi_descriptor_t * p_descriptor;
    p_descriptor = dvbpsi_NewDescriptor(i_tag, i_length, p_data);
    if (p_descriptor == NULL)
        return NULL;

    p_sdtt_content->p_first_descriptor = dvbpsi_AddDescriptor(
            p_sdtt_content->p_first_descriptor, p_descriptor);
    assert(p_sdtt_content->p_first_descriptor);
    if (p_sdtt_content->p_first_descriptor == NULL)
        return NULL;

    return p_descriptor;
}

static void dvbpsi_ReInitSDTT(dvbpsi_isdbt_sdtt_decoder_t* p_decoder,
        const bool b_force)
{
    assert(p_decoder);

    dvbpsi_decoder_reset(DVBPSI_DECODER(p_decoder), b_force);

    /* Force redecoding */
    if (b_force)
    {
        /* Free structures */
        if (p_decoder->p_building_sdtt)
            dvbpsi_isdbt_sdtt_delete(p_decoder->p_building_sdtt);
    }
    p_decoder->p_building_sdtt = NULL;
}

void reset_isdbt_sdtt(dvbpsi_decoder_t* p_decoder)
{
    assert(p_decoder);
    dvbpsi_isdbt_sdtt_decoder_t* p_sdtt_decoder =
            (dvbpsi_isdbt_sdtt_decoder_t*) p_decoder;
    dvbpsi_decoder_reset(DVBPSI_DECODER(p_sdtt_decoder), true);
}

static bool dvbpsi_CheckSDTT(dvbpsi_t *p_dvbpsi,
        dvbpsi_isdbt_sdtt_decoder_t *p_sdtt_decoder,
        dvbpsi_psi_section_t *p_section)
{
    bool b_reinit = false;
    assert(p_dvbpsi);
    assert(p_sdtt_decoder);

    if (p_sdtt_decoder->p_building_sdtt->i_extension != p_section->i_extension)
    {
        /* transport_stream_id */
        dvbpsi_error(p_dvbpsi, "sdtt decoder", "'transport_stream_id' differs"
                " whereas no TS discontinuity has occured");
        b_reinit = true;
    } else if (p_sdtt_decoder->p_building_sdtt->i_version
            != p_section->i_version)
    {
        /* version_number */
        dvbpsi_error(p_dvbpsi, "sdtt decoder", "'version_number' differs"
                " whereas no discontinuity has occured");
        b_reinit = true;
    } else if (p_sdtt_decoder->i_last_section_number
            != p_section->i_last_number)
    {
        /* last_section_number */
        dvbpsi_error(p_dvbpsi, "sdtt decoder", "'last_section_number' differs"
                " whereas no discontinuity has occured");
        b_reinit = true;
    }

    return b_reinit;
}

static bool dvbpsi_AddSectionSDTT(dvbpsi_t *p_dvbpsi,
        dvbpsi_isdbt_sdtt_decoder_t *p_sdtt_decoder,
        dvbpsi_psi_section_t* p_section)
{

    assert(p_dvbpsi);
    assert(p_sdtt_decoder);
    assert(p_section);

    /* Initialize the structures if it's the first section received */
    if (p_sdtt_decoder->p_building_sdtt == NULL)
    {
        uint16_t i_transport_stream_id =
                ((uint16_t) (p_section->p_payload_start[0]) << 8)
                        | p_section->p_payload_start[1];
        uint16_t i_original_network_id =
                ((uint16_t) (p_section->p_payload_start[2]) << 8)
                        | p_section->p_payload_start[3];
        uint16_t i_service_id =
                ((uint16_t) (p_section->p_payload_start[4]) << 8)
                        | p_section->p_payload_start[5];

        p_sdtt_decoder->p_building_sdtt = dvbpsi_isdbt_sdtt_new(
                p_section->i_table_id, p_section->i_extension,
                p_section->i_version, p_section->b_current_next,
                i_transport_stream_id, i_original_network_id, i_service_id);

        if (p_sdtt_decoder->p_building_sdtt == NULL)
            return false;
        p_sdtt_decoder->i_last_section_number = p_section->i_last_number;
    }

    /* Add to linked list of sections */
    if (dvbpsi_decoder_psi_section_add(DVBPSI_DECODER(p_sdtt_decoder),
            p_section))
        dvbpsi_debug(p_dvbpsi, "SDTT decoder", "overwrite section number %d",
                p_section->i_number);
    return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_sections_gather
 *****************************************************************************
 * Callback for the subtable demultiplexor.
 *****************************************************************************/
void dvbpsi_isdbt_sdtt_sections_gather(dvbpsi_t *p_dvbpsi,
        dvbpsi_decoder_t *p_private_decoder, dvbpsi_psi_section_t * p_section)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    if (!dvbpsi_CheckPSISection(p_dvbpsi, p_section, 0xC3, "sdtt decoder"))
    {
        dvbpsi_DeletePSISections2(p_section);
        return;
    }

    /* We have a valid sdtt section */
    dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *) p_dvbpsi->p_decoder;
    dvbpsi_isdbt_sdtt_decoder_t *p_sdtt_decoder =
            (dvbpsi_isdbt_sdtt_decoder_t*) p_private_decoder;
    /* TS discontinuity check */
    if (p_demux->b_discontinuity)
    {
        dvbpsi_ReInitSDTT(p_sdtt_decoder, true);
        p_sdtt_decoder->b_discontinuity = false;
        p_demux->b_discontinuity = false;
    } else
    {
        /* Perform a few sanity checks */
        if (p_sdtt_decoder->p_building_sdtt)
        {
            if (dvbpsi_CheckSDTT(p_dvbpsi, p_sdtt_decoder, p_section))
            {
                dvbpsi_ReInitSDTT(p_sdtt_decoder, true);
            }
        }
    }

    /* Add section to sdtt */
    if (!dvbpsi_AddSectionSDTT(p_dvbpsi, p_sdtt_decoder, p_section))
    {
        dvbpsi_error(p_dvbpsi, "sdtt decoder", "failed decoding section %d",
                p_section->i_number);
        dvbpsi_DeletePSISections2(p_section);
        return;
    }
    /* Check if we have all the sections */
    if (dvbpsi_decoder_psi_sections_completed(DVBPSI_DECODER(p_sdtt_decoder)))
    {
        assert(p_sdtt_decoder->pf_sdtt_callback);
        p_sdtt_decoder->b_current_valid = true;
        p_dvbpsi->p_decoder->b_current_valid = true;
        /* Decode the sections */
        dvbpsi_isdbt_sdtt_sections_decode(p_sdtt_decoder->p_building_sdtt,
                p_sdtt_decoder->p_sections);
        dvbpsi_psi_section_t* p_mysection = p_sdtt_decoder->p_sections;
        bool sdttEquals = true;
        for (int i = 0; i < 6; i++)
        {
            if (p_mysection)
            {
                p_sdtt_decoder->p_building_sdtt->i_crcs[i] =
                        ((*(p_mysection->p_payload_end)) << 24)
                                + ((*(p_mysection->p_payload_end + 1)) << 16)
                                + ((*(p_mysection->p_payload_end + 2)) << 8)
                                + ((*(p_mysection->p_payload_end + 3)));
                p_mysection = p_mysection->p_next;
            } else
            {
                p_sdtt_decoder->p_building_sdtt->i_crcs[i] = 0;
            }
            if (p_sdtt_decoder->current_sdtt.i_crcs[i]
                    != p_sdtt_decoder->p_building_sdtt->i_crcs[i])
            {
                sdttEquals = false;
            }
        }
        /* Save the current information */
        p_sdtt_decoder->current_sdtt = *p_sdtt_decoder->p_building_sdtt;
        /* Delete the sections */
        dvbpsi_DeletePSISections(p_sdtt_decoder->p_sections);
        p_sdtt_decoder->p_sections = NULL;

        /* signal the new sdtt */
        if (!sdttEquals)
            p_sdtt_decoder->pf_sdtt_callback(p_sdtt_decoder->p_cb_data,
                    p_sdtt_decoder->p_building_sdtt);
        /* Reinitialize the structures */
        dvbpsi_ReInitSDTT(p_sdtt_decoder, true);
    }
}

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_sections_decode
 *****************************************************************************
 * sdtt decoder.
 *****************************************************************************/
void dvbpsi_isdbt_sdtt_sections_decode(dvbpsi_isdbt_sdtt_t* p_sdtt,
        dvbpsi_psi_section_t* p_section)
{
    uint8_t* p_byte, *p_end;

    while (p_section)
    {
        uint8_t num_of_contents = p_section->p_payload_start[6];
        p_byte = p_section->p_payload_start + 7;
        for (uint8_t i = 0; i < num_of_contents; i++)
        {
            uint8_t i_group = (uint8_t) (p_byte[0]) >> 4;
            uint16_t i_target_version = ((uint16_t) (p_byte[0] & 0xf) << 8)
                    | p_byte[1];
            uint16_t i_new_version = ((uint16_t) (p_byte[2]) << 4)
                    | (p_byte[3] >> 4);
            uint8_t i_download_level = ((p_byte[3] & 0xC) >> 2);
            uint8_t i_version_indicator = (p_byte[3] & 0x3);
            uint16_t i_content_descriptor_length = ((uint16_t) (p_byte[4]) << 4)
                    | (p_byte[5] >> 4);
            uint16_t i_schedule_descriptor_length =
                    ((uint16_t) (p_byte[6]) << 4) | (p_byte[7] >> 4);
            uint8_t i_schedule_timeshift_info = (p_byte[7] & 0xf);
            p_byte += 8;
            p_end = p_byte + i_content_descriptor_length;
            dvbpsi_isdbt_sdtt_content_t * p_content;
            p_content = dvbpsi_isdbt_sdtt_content_add(p_sdtt, i_group,
                    i_target_version, i_new_version, i_download_level,
                    i_version_indicator, i_schedule_timeshift_info);

            for (uint16_t j = 0; (j + 8) <= i_schedule_descriptor_length; j += 8)
            {
                uint64_t i_start_time = (((uint64_t) p_byte[j]) << 32)
                        | (((uint64_t) p_byte[j + 1]) << 24)
                        | (((uint64_t) p_byte[j + 2]) << 16)
                        | (((uint64_t) p_byte[j + 3]) << 8)
                        | ((uint64_t) p_byte[j + 4]);
                uint32_t i_duration = (((uint32_t) p_byte[j + 5]) << 16)
                        | (((uint32_t) p_byte[j + 6]) << 8)
                        | ((uint32_t) p_byte[j + 7]);
                dvbpsi_isdbt_sdtt_content_schedule_add(p_content, i_start_time, i_duration);
            }
            p_byte += i_schedule_descriptor_length;
            for (uint16_t j = i_schedule_descriptor_length;
                    (j + 2) <= i_content_descriptor_length;)
            {
                uint8_t i_tag = p_byte[0];
                uint8_t i_length = p_byte[1];
                if (i_length + 2 <= p_end - p_byte)
                    dvbpsi_isdbt_sdtt_content_descriptor_add(p_content, i_tag,
                            i_length, p_byte + 2);
                p_byte += 2 + i_length;
                j += 2 + i_length;
            }
        }
        p_section = p_section->p_next;
    }
}
