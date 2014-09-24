/*****************************************************************************
 * isdbt_cdt.c: cdt decoder/generator
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
#include "isdbt_cdt.h"

typedef struct dvbpsi_isdbt_cdt_decoder_s
{
    DVBPSI_DECODER_COMMON

    dvbpsi_isdbt_cdt_callback pf_cdt_callback;
    void * p_cb_data;

    dvbpsi_isdbt_cdt_t current_cdt;
    dvbpsi_isdbt_cdt_t * p_building_cdt;

} dvbpsi_isdbt_cdt_decoder_t;

static void dvbpsi_isdbt_cdt_sections_gather(dvbpsi_t *p_dvbpsi,
        dvbpsi_decoder_t *p_private_decoder, dvbpsi_psi_section_t * p_section);

static void dvbpsi_isdbt_cdt_sections_decode(dvbpsi_isdbt_cdt_t* p_cdt,
        dvbpsi_psi_section_t* p_section);

/*****************************************************************************
 * dvbpsi_isdbt_cdt_attach
 *****************************************************************************
 * Initialize a cdt subtable decoder.
 *****************************************************************************/
bool dvbpsi_isdbt_cdt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension, dvbpsi_isdbt_cdt_callback pf_callback,
        void* p_cb_data)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    dvbpsi_demux_t* p_demux = (dvbpsi_demux_t*) p_dvbpsi->p_decoder;

    if (dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension))
    {
        dvbpsi_error(p_dvbpsi, "cdt decoder",
                "Already a decoder for (table_id == 0x%02x,"
                        "extension == 0x%02x)", i_table_id, i_extension);
        return false;
    }

    dvbpsi_isdbt_cdt_decoder_t* p_cdt_decoder;

    p_cdt_decoder = (dvbpsi_isdbt_cdt_decoder_t*) dvbpsi_decoder_new(NULL, 0,
            true, sizeof(dvbpsi_isdbt_cdt_decoder_t));
    if (p_cdt_decoder == NULL)
        return false;

    /* subtable decoder configuration */
    dvbpsi_demux_subdec_t* p_subdec;
    p_subdec = dvbpsi_NewDemuxSubDecoder(i_table_id, i_extension,
            dvbpsi_isdbt_cdt_detach, dvbpsi_isdbt_cdt_sections_gather,
            DVBPSI_DECODER(p_cdt_decoder));
    if (p_subdec == NULL)
    {
        dvbpsi_decoder_delete(DVBPSI_DECODER(p_cdt_decoder));
        return false;
    }

    /* Attach the subtable decoder to the demux */
    dvbpsi_AttachDemuxSubDecoder(p_demux, p_subdec);

    /* cdt decoder information */
    p_cdt_decoder->pf_cdt_callback = pf_callback;
    p_cdt_decoder->p_cb_data = p_cb_data;
    p_cdt_decoder->p_building_cdt = NULL;
    return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_detach
 *****************************************************************************
 * Close a cdt decoder.
 *****************************************************************************/
void dvbpsi_isdbt_cdt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *) p_dvbpsi->p_decoder;
    dvbpsi_demux_subdec_t* p_subdec;
    p_subdec = dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension);
    if (p_subdec == NULL)
    {
        dvbpsi_error(p_dvbpsi, "cdt Decoder",
                "No such cdt decoder (table_id == 0x%02x,"
                        "extension == 0x%02x)", i_table_id, i_extension);
        return;
    }
    assert(p_subdec->p_decoder);
    dvbpsi_isdbt_cdt_decoder_t* p_cdt_decoder;
    p_cdt_decoder = (dvbpsi_isdbt_cdt_decoder_t*) p_subdec->p_decoder;
    if (p_cdt_decoder->p_building_cdt)
        dvbpsi_isdbt_cdt_delete(p_cdt_decoder->p_building_cdt);
    p_cdt_decoder->p_building_cdt = NULL;

    /* Free sub table decoder */
    dvbpsi_DetachDemuxSubDecoder(p_demux, p_subdec);
    dvbpsi_DeleteDemuxSubDecoder(p_subdec);
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_init
 *****************************************************************************
 * Initialize a pre-allocated dvbpsi_isdbt_cdt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_cdt_init(dvbpsi_isdbt_cdt_t* p_cdt, uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_original_network_id, uint8_t i_data_type)
{
    assert(p_cdt);

    p_cdt->i_table_id = i_table_id;
    p_cdt->i_extension = i_extension;

    p_cdt->i_version = i_version;
    p_cdt->b_current_next = b_current_next;
    p_cdt->i_original_network_id = i_original_network_id;
    p_cdt->i_data_type = i_data_type;
    p_cdt->p_first_descriptor = NULL;
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_new
 *****************************************************************************
 * Allocate and Initialize a new dvbpsi_isdbt_cdt_t structure.
 *****************************************************************************/
dvbpsi_isdbt_cdt_t *dvbpsi_isdbt_cdt_new(uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version,
        bool b_current_next, uint16_t i_original_network_id,
        uint8_t i_data_type)
{
    dvbpsi_isdbt_cdt_t *p_cdt = (dvbpsi_isdbt_cdt_t*) malloc(
            sizeof(dvbpsi_isdbt_cdt_t));
    if (p_cdt != NULL)
        dvbpsi_isdbt_cdt_init(p_cdt, i_table_id, i_extension, i_version,
                b_current_next, i_original_network_id, i_data_type);
    return p_cdt;
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_empty
 *****************************************************************************
 * Clean a dvbpsi_isdbt_cdt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_cdt_empty(dvbpsi_isdbt_cdt_t* p_cdt)
{
    dvbpsi_DeleteDescriptors(p_cdt->p_first_descriptor);
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_delete
 *****************************************************************************
 * Clean and Delete dvbpsi_isdbt_cdt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_cdt_delete(dvbpsi_isdbt_cdt_t *p_cdt)
{
    if (p_cdt)
        dvbpsi_isdbt_cdt_empty(p_cdt);
    free(p_cdt);
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_descriptor_add
 *****************************************************************************
 * Add a descriptor in the cdt descriptor loop.
 *****************************************************************************/
dvbpsi_descriptor_t *dvbpsi_isdbt_cdt_descriptor_add(dvbpsi_isdbt_cdt_t* p_cdt,
        uint8_t i_tag, uint8_t i_length, uint8_t *p_data)
{
    dvbpsi_descriptor_t * p_descriptor;
    p_descriptor = dvbpsi_NewDescriptor(i_tag, i_length, p_data);
    if (p_descriptor == NULL)
        return NULL;

    p_cdt->p_first_descriptor = dvbpsi_AddDescriptor(p_cdt->p_first_descriptor,
            p_descriptor);
    assert(p_cdt->p_first_descriptor);
    if (p_cdt->p_first_descriptor == NULL)
        return NULL;

    return p_descriptor;
}

static void dvbpsi_ReInitCDT(dvbpsi_isdbt_cdt_decoder_t* p_decoder,
        const bool b_force)
{
    assert(p_decoder);

    dvbpsi_decoder_reset(DVBPSI_DECODER(p_decoder), b_force);

    /* Force redecoding */
    if (b_force)
    {
        /* Free structures */
        if (p_decoder->p_building_cdt)
            dvbpsi_isdbt_cdt_delete(p_decoder->p_building_cdt);
    }
    p_decoder->p_building_cdt = NULL;
}

void reset_isdbt_cdt(dvbpsi_decoder_t* p_decoder)
{
    assert(p_decoder);
    dvbpsi_isdbt_cdt_decoder_t* p_cdt_decoder =
            (dvbpsi_isdbt_cdt_decoder_t*) p_decoder;
    dvbpsi_decoder_reset(DVBPSI_DECODER(p_cdt_decoder), true);
}

static bool dvbpsi_CheckCDT(dvbpsi_t *p_dvbpsi,
        dvbpsi_isdbt_cdt_decoder_t *p_cdt_decoder,
        dvbpsi_psi_section_t *p_section)
{
    bool b_reinit = false;
    assert(p_dvbpsi);
    assert(p_cdt_decoder);

    if (p_cdt_decoder->p_building_cdt->i_extension != p_section->i_extension)
    {
        /* transport_stream_id */
        dvbpsi_error(p_dvbpsi, "cdt decoder", "'transport_stream_id' differs"
                " whereas no TS discontinuity has occured");
        b_reinit = true;
    } else if (p_cdt_decoder->p_building_cdt->i_version != p_section->i_version)
    {
        /* version_number */
        dvbpsi_error(p_dvbpsi, "cdt decoder", "'version_number' differs"
                " whereas no discontinuity has occured");
        b_reinit = true;
    } else if (p_cdt_decoder->i_last_section_number != p_section->i_last_number)
    {
        /* last_section_number */
        dvbpsi_error(p_dvbpsi, "cdt decoder", "'last_section_number' differs"
                " whereas no discontinuity has occured");
        b_reinit = true;
    }

    return b_reinit;
}

static bool dvbpsi_AddSectionCDT(dvbpsi_t *p_dvbpsi,
        dvbpsi_isdbt_cdt_decoder_t *p_cdt_decoder,
        dvbpsi_psi_section_t* p_section)
{

    assert(p_dvbpsi);
    assert(p_cdt_decoder);
    assert(p_section);

    /* Initialize the structures if it's the first section received */
    if (p_cdt_decoder->p_building_cdt == NULL)
    {
        uint16_t i_original_network_id = ((uint16_t)(p_section->p_payload_start[0]) << 8) | p_section->p_payload_start[1];
        uint8_t i_data_type = p_section->p_payload_start[2];
        p_cdt_decoder->p_building_cdt = dvbpsi_isdbt_cdt_new(p_section->i_table_id,
                                                       p_section->i_extension,
                                                       p_section->i_version,
                                                       p_section->b_current_next,
                                                       i_original_network_id,
                                                       i_data_type );

        if (p_cdt_decoder->p_building_cdt == NULL)
            return false;
        p_cdt_decoder->i_last_section_number = p_section->i_last_number;
    }

    /* Add to linked list of sections */
    if (dvbpsi_decoder_psi_section_add(DVBPSI_DECODER(p_cdt_decoder), p_section))
        dvbpsi_debug(p_dvbpsi, "CDT decoder", "overwrite section number %d", p_section->i_number);
    return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_sections_gather
 *****************************************************************************
 * Callback for the subtable demultiplexor.
 *****************************************************************************/
void dvbpsi_isdbt_cdt_sections_gather(dvbpsi_t *p_dvbpsi, dvbpsi_decoder_t *p_private_decoder, dvbpsi_psi_section_t * p_section)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    if (!dvbpsi_CheckPSISection(p_dvbpsi, p_section, 0xC8, "cdt decoder"))
    {
        dvbpsi_DeletePSISections2(p_section);
        return;
    }

    /* We have a valid cdt section */
    dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *) p_dvbpsi->p_decoder;
    dvbpsi_isdbt_cdt_decoder_t *p_cdt_decoder =
            (dvbpsi_isdbt_cdt_decoder_t*) p_private_decoder;
    /* TS discontinuity check */
    if (p_demux->b_discontinuity)
    {
        dvbpsi_ReInitCDT(p_cdt_decoder, true);
        p_cdt_decoder->b_discontinuity = false;
        p_demux->b_discontinuity = false;
    } else
    {
        /* Perform a few sanity checks */
        if (p_cdt_decoder->p_building_cdt)
        {
            if (dvbpsi_CheckCDT(p_dvbpsi, p_cdt_decoder, p_section))
            {
                dvbpsi_ReInitCDT(p_cdt_decoder, true);
            }
        }
    }

    /* Add section to cdt */
    if (!dvbpsi_AddSectionCDT(p_dvbpsi, p_cdt_decoder, p_section))
    {
        dvbpsi_error(p_dvbpsi, "cdt decoder", "failed decoding section %d",
                p_section->i_number);
        dvbpsi_DeletePSISections2(p_section);
        return;
    }
    /* Check if we have all the sections */
    if (dvbpsi_decoder_psi_sections_completed(DVBPSI_DECODER(p_cdt_decoder)))
    {
        assert(p_cdt_decoder->pf_cdt_callback);
        p_cdt_decoder->b_current_valid = true;
        p_dvbpsi->p_decoder->b_current_valid = true;
        /* Decode the sections */
        dvbpsi_isdbt_cdt_sections_decode(p_cdt_decoder->p_building_cdt,
                p_cdt_decoder->p_sections);
        dvbpsi_psi_section_t* p_mysection = p_cdt_decoder->p_sections;
        bool cdtEquals = true;
        for (int i = 0; i < 6; i++)
        {
            if (p_mysection)
            {
                p_cdt_decoder->p_building_cdt->i_crcs[i] =
                        ((*(p_mysection->p_payload_end)) << 24)
                                + ((*(p_mysection->p_payload_end + 1)) << 16)
                                + ((*(p_mysection->p_payload_end + 2)) << 8)
                                + ((*(p_mysection->p_payload_end + 3)));
                p_mysection = p_mysection->p_next;
            } else
            {
                p_cdt_decoder->p_building_cdt->i_crcs[i] = 0;
            }
            if (p_cdt_decoder->current_cdt.i_crcs[i]
                    != p_cdt_decoder->p_building_cdt->i_crcs[i])
            {
                cdtEquals = false;
            }
        }
        /* Save the current information */
        p_cdt_decoder->current_cdt = *p_cdt_decoder->p_building_cdt;
        /* Delete the sections */
        dvbpsi_DeletePSISections(p_cdt_decoder->p_sections);
        p_cdt_decoder->p_sections = NULL;

        /* signal the new cdt */
        if (!cdtEquals)
            p_cdt_decoder->pf_cdt_callback(p_cdt_decoder->p_cb_data,
                    p_cdt_decoder->p_building_cdt);
        /* Reinitialize the structures */
        dvbpsi_ReInitCDT(p_cdt_decoder, true);
    }
}

/*****************************************************************************
 * dvbpsi_isdbt_cdt_sections_decode
 *****************************************************************************
 * cdt decoder.
 *****************************************************************************/
void dvbpsi_isdbt_cdt_sections_decode(dvbpsi_isdbt_cdt_t* p_cdt,
        dvbpsi_psi_section_t* p_section)
{
    uint8_t* p_byte, *p_end;

    while (p_section)
    {

        //uint16_t original_network_id = (((uint16_t)(p_section->p_payload_start[0] & 0xff ) << 8) | p_section->p_payload_start[1]);
        //uint8_t data_type = p_section->p_payload_start[2];

        uint16_t descriptors_loop_length =
                (((uint16_t) (p_section->p_payload_start[3] & 0x0f) << 8)
                        | p_section->p_payload_start[4]);
        p_byte = p_section->p_payload_start + 5;
        p_end = p_byte + descriptors_loop_length;
        if (p_end > p_section->p_payload_end)
            p_end = p_section->p_payload_end;
        /* - loop descriptors */
        while (p_byte + 2 <= p_end)
        {
            uint8_t i_tag = p_byte[0];
            uint8_t i_length = p_byte[1];
            if (i_length + 2 <= p_end - p_byte)
                dvbpsi_isdbt_cdt_descriptor_add(p_cdt, i_tag, i_length, p_byte + 2);
            p_byte += 2 + i_length;
        }
        uint16_t length = p_section->p_payload_end - p_byte;

//        uint8_t data_module_byte[length];
//        memcpy(data_module_byte, p_byte, length);
//        for (uint16_t var = 0; var < length; ++var) {
//            printf("********** %02x ", data_module_byte[var]);
//        }

        memcpy(p_cdt->i_data_module_byte, p_byte, length);
        p_cdt->i_data_module_byte_length = length;
        p_section = p_section->p_next;
    }
}
