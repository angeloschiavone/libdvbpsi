/*****************************************************************************
 * isdbt_ldt.c: LDT decoder/generator
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
#include "isdbt_ldt.h"


typedef struct dvbpsi_isdbt_ldt_decoder_s
{
    DVBPSI_DECODER_COMMON

    dvbpsi_isdbt_ldt_callback     pf_ldt_callback;
    void *                        p_cb_data;

    dvbpsi_isdbt_ldt_t            current_ldt;
    dvbpsi_isdbt_ldt_t *          p_building_ldt;

} dvbpsi_isdbt_ldt_decoder_t;

static void dvbpsi_isdbt_ldt_sections_gather(dvbpsi_t *p_dvbpsi,
                                dvbpsi_decoder_t *p_private_decoder,
                                dvbpsi_psi_section_t * p_section);

static void dvbpsi_isdbt_ldt_sections_decode(dvbpsi_isdbt_ldt_t* p_ldt,
                                dvbpsi_psi_section_t* p_section);

/*****************************************************************************
 * dvbpsi_isdbt_ldt_attach
 *****************************************************************************
 * Initialize a LDT subtable decoder.
 *****************************************************************************/
bool dvbpsi_isdbt_ldt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension,
                      dvbpsi_isdbt_ldt_callback pf_callback, void* p_cb_data)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    dvbpsi_demux_t* p_demux = (dvbpsi_demux_t*)p_dvbpsi->p_decoder;

    if (dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension))
    {
        dvbpsi_error(p_dvbpsi, "LDT decoder",
                     "Already a decoder for (table_id == 0x%02x,"
                     "extension == 0x%02x)",
                     i_table_id, i_extension);
        return false;
    }

    dvbpsi_isdbt_ldt_decoder_t*  p_ldt_decoder;

    p_ldt_decoder = (dvbpsi_isdbt_ldt_decoder_t*) dvbpsi_decoder_new(NULL,
                                             0, true, sizeof(dvbpsi_isdbt_ldt_decoder_t));
    if (p_ldt_decoder == NULL)
        return false;

    /* subtable decoder configuration */
    dvbpsi_demux_subdec_t* p_subdec;
    p_subdec = dvbpsi_NewDemuxSubDecoder(i_table_id, i_extension, dvbpsi_isdbt_ldt_detach,
                                         dvbpsi_isdbt_ldt_sections_gather, DVBPSI_DECODER(p_ldt_decoder));
    if (p_subdec == NULL)
    {
        dvbpsi_decoder_delete(DVBPSI_DECODER(p_ldt_decoder));
        return false;
    }

    /* Attach the subtable decoder to the demux */
    dvbpsi_AttachDemuxSubDecoder(p_demux, p_subdec);

    /* LDT decoder information */
    p_ldt_decoder->pf_ldt_callback = pf_callback;
    p_ldt_decoder->p_cb_data = p_cb_data;
    p_ldt_decoder->p_building_ldt = NULL;
    return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_ldt_detach
 *****************************************************************************
 * Close a LDT decoder.
 *****************************************************************************/
void dvbpsi_isdbt_ldt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *) p_dvbpsi->p_decoder;
    dvbpsi_demux_subdec_t* p_subdec;
    p_subdec = dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension);
    if (p_subdec == NULL)
    {
        dvbpsi_error(p_dvbpsi, "LDT Decoder",
                     "No such LDT decoder (table_id == 0x%02x,"
                     "extension == 0x%02x)",
                     i_table_id, i_extension);
        return;
    }
    assert(p_subdec->p_decoder);
    dvbpsi_isdbt_ldt_decoder_t* p_ldt_decoder;
    p_ldt_decoder = (dvbpsi_isdbt_ldt_decoder_t*)p_subdec->p_decoder;
    if (p_ldt_decoder->p_building_ldt)
        dvbpsi_isdbt_ldt_delete(p_ldt_decoder->p_building_ldt);
    p_ldt_decoder->p_building_ldt = NULL;

    /* Free sub table decoder */
    dvbpsi_DetachDemuxSubDecoder(p_demux, p_subdec);
    dvbpsi_DeleteDemuxSubDecoder(p_subdec);
}

/*****************************************************************************
 * dvbpsi_isdbt_ldt_init
 *****************************************************************************
 * Initialize a pre-allocated dvbpsi_isdbt_ldt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_ldt_init(dvbpsi_isdbt_ldt_t* p_ldt, uint8_t i_table_id, uint16_t i_extension,
                     uint8_t i_version, bool b_current_next, uint16_t i_transport_stream_id, uint16_t i_network_id)
{
    assert(p_ldt);

    p_ldt->i_table_id = i_table_id;
    p_ldt->i_extension = i_extension;

    p_ldt->i_version = i_version;
    p_ldt->b_current_next = b_current_next;
    p_ldt->i_transport_stream_id = i_transport_stream_id;
    p_ldt->i_network_id = i_network_id;
    p_ldt->p_first_description = NULL;
}




/*****************************************************************************
 * dvbpsi_isdbt_ldt_new
 *****************************************************************************
 * Allocate and Initialize a new dvbpsi_isdbt_ldt_t structure.
 *****************************************************************************/
dvbpsi_isdbt_ldt_t *dvbpsi_isdbt_ldt_new(uint8_t i_table_id, uint16_t i_extension, uint8_t i_version,
                             bool b_current_next, uint16_t i_transport_stream_id, uint16_t i_network_id)
{
    dvbpsi_isdbt_ldt_t *p_ldt = (dvbpsi_isdbt_ldt_t*)malloc(sizeof(dvbpsi_isdbt_ldt_t));
    if (p_ldt != NULL)
        dvbpsi_isdbt_ldt_init(p_ldt, i_table_id, i_extension, i_version,
                        b_current_next, i_transport_stream_id, i_network_id);
    return p_ldt;
}


/*****************************************************************************
 * dvbpsi_isdbt_ldt_empty
 *****************************************************************************
 * Clean a dvbpsi_isdbt_ldt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_ldt_empty(dvbpsi_isdbt_ldt_t* p_ldt)
{
    dvbpsi_isdbt_ldt_description_t* p_description = p_ldt->p_first_description;

    while (p_description != NULL)
    {
        dvbpsi_isdbt_ldt_description_t* p_tmp = p_description->p_next;
        dvbpsi_DeleteDescriptors(p_description->p_first_descriptor);
        free(p_description);
        p_description = p_tmp;
    }
    p_ldt->p_first_description = NULL;
}


/*****************************************************************************
 * dvbpsi_isdbt_ldt_delete
 *****************************************************************************
 * Clean and Delete dvbpsi_isdbt_ldt_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_ldt_delete(dvbpsi_isdbt_ldt_t *p_ldt)
{
    if (p_ldt)
        dvbpsi_isdbt_ldt_empty(p_ldt);
    free(p_ldt);
}


/*****************************************************************************
 * dvbpsi_isdbt_ldt_description_add
 *****************************************************************************
 * Add a description at the end of the LDT descriptions loop.
 *****************************************************************************/
dvbpsi_isdbt_ldt_description_t *dvbpsi_isdbt_ldt_description_add(dvbpsi_isdbt_ldt_t* p_ldt,
                                           uint16_t i_description_id,
                                           uint16_t i_reserved)
{
    dvbpsi_isdbt_ldt_description_t * p_description;
    p_description = (dvbpsi_isdbt_ldt_description_t*)calloc(1, sizeof(dvbpsi_isdbt_ldt_description_t));
    if (p_description == NULL)
        return NULL;

    p_description->i_description_id = i_description_id;
    p_description->i_reserved = i_reserved;
    p_description->p_next = NULL;
    p_description->p_first_descriptor = NULL;

    if (p_ldt->p_first_description == NULL)
    {
        p_ldt->p_first_description = p_description;
    }
    else
    {
        dvbpsi_isdbt_ldt_description_t * p_last_description = p_ldt->p_first_description;
        while(p_last_description->p_next != NULL)
            p_last_description = p_last_description->p_next;
        p_last_description->p_next = p_description;
    }

    return p_description;
}

/*****************************************************************************
 * dvbpsi_isdbt_ldt_description_descriptor_add
 *****************************************************************************
 * Add a descriptor in the LDT description loop.
 *****************************************************************************/
dvbpsi_descriptor_t *dvbpsi_isdbt_ldt_description_descriptor_add(
                                               dvbpsi_isdbt_ldt_description_t *p_description,
                                               uint8_t i_tag, uint8_t i_length,
                                               uint8_t *p_data)
{
    dvbpsi_descriptor_t * p_descriptor;
    p_descriptor = dvbpsi_NewDescriptor(i_tag, i_length, p_data);
    if (p_descriptor == NULL)
        return NULL;

    p_description->p_first_descriptor = dvbpsi_AddDescriptor(p_description->p_first_descriptor,
                                                         p_descriptor);
    assert(p_description->p_first_descriptor);
    if (p_description->p_first_descriptor == NULL)
        return NULL;

    return p_descriptor;
}

static void dvbpsi_ReInitLDT(dvbpsi_isdbt_ldt_decoder_t* p_decoder, const bool b_force)
{
    assert(p_decoder);

    dvbpsi_decoder_reset(DVBPSI_DECODER(p_decoder), b_force);

    /* Force redecoding */
    if (b_force)
    {
        /* Free structures */
        if (p_decoder->p_building_ldt)
            dvbpsi_isdbt_ldt_delete(p_decoder->p_building_ldt);
    }
    p_decoder->p_building_ldt = NULL;
}

void reset_isdbt_ldt(dvbpsi_decoder_t* p_decoder)
{
     assert(p_decoder);
     dvbpsi_isdbt_ldt_decoder_t* p_ldt_decoder = (dvbpsi_isdbt_ldt_decoder_t*)p_decoder;
     dvbpsi_decoder_reset(DVBPSI_DECODER(p_ldt_decoder), true);
}

static bool dvbpsi_CheckLDT(dvbpsi_t *p_dvbpsi, dvbpsi_isdbt_ldt_decoder_t *p_ldt_decoder,
                            dvbpsi_psi_section_t *p_section)
{
    bool b_reinit = false;
    assert(p_dvbpsi);
    assert(p_ldt_decoder);

    if (p_ldt_decoder->p_building_ldt->i_extension != p_section->i_extension)
    {
        /* transport_stream_id */
        dvbpsi_error(p_dvbpsi, "LDT decoder",
                "'transport_stream_id' differs"
                " whereas no TS discontinuity has occured");
        b_reinit = true;
    }
    else if (p_ldt_decoder->p_building_ldt->i_version != p_section->i_version)
    {
        /* version_number */
        dvbpsi_error(p_dvbpsi, "LDT decoder",
                "'version_number' differs"
                " whereas no discontinuity has occured");
        b_reinit = true;
    }
    else if (p_ldt_decoder->i_last_section_number != p_section->i_last_number)
    {
        /* last_section_number */
        dvbpsi_error(p_dvbpsi, "LDT decoder",
                "'last_section_number' differs"
                " whereas no discontinuity has occured");
        b_reinit = true;
    }

    return b_reinit;
}

static bool dvbpsi_AddSectionLDT(dvbpsi_t *p_dvbpsi, dvbpsi_isdbt_ldt_decoder_t *p_ldt_decoder,
                                 dvbpsi_psi_section_t* p_section)
{
    assert(p_dvbpsi);
    assert(p_ldt_decoder);
    assert(p_section);

    /* Initialize the structures if it's the first section received */
    if (!p_ldt_decoder->p_building_ldt)
    {
        uint16_t ts_id = ((uint16_t)(p_section->p_payload_start[0]) << 8)
                                                 | p_section->p_payload_start[1];
        uint16_t orig_net_id = ((uint16_t)(p_section->p_payload_start[2]) << 8)
                                                 | p_section->p_payload_start[3];
        p_ldt_decoder->p_building_ldt =
                dvbpsi_isdbt_ldt_new(p_section->i_table_id, p_section->i_extension,
                             p_section->i_version, p_section->b_current_next,
                             ts_id, orig_net_id);

        if (p_ldt_decoder->p_building_ldt == NULL)
            return false;
        p_ldt_decoder->i_last_section_number = p_section->i_last_number;
    }

    /* Add to linked list of sections */
    if (dvbpsi_decoder_psi_section_add(DVBPSI_DECODER(p_ldt_decoder), p_section))
        dvbpsi_debug(p_dvbpsi, "LDT decoder", "overwrite section number %d",
                     p_section->i_number);

    return true;
}



/*****************************************************************************
 * dvbpsi_isdbt_ldt_sections_gather
 *****************************************************************************
 * Callback for the subtable demultiplexor.
 *****************************************************************************/
void dvbpsi_isdbt_ldt_sections_gather(dvbpsi_t *p_dvbpsi,
                                dvbpsi_decoder_t *p_private_decoder,
                                dvbpsi_psi_section_t * p_section)
{
    assert(p_dvbpsi);
    assert(p_dvbpsi->p_decoder);

    if (!dvbpsi_CheckPSISection(p_dvbpsi, p_section, 0xC7, "LDT decoder"))
    {
        dvbpsi_DeletePSISections2(p_section);
        return;
    }

    /* We have a valid LDT section */
    dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *)p_dvbpsi->p_decoder;
    dvbpsi_isdbt_ldt_decoder_t *p_ldt_decoder = (dvbpsi_isdbt_ldt_decoder_t*)p_private_decoder;
    /* TS discontinuity check */
    if (p_demux->b_discontinuity)
    {
        dvbpsi_ReInitLDT(p_ldt_decoder, true);
        p_ldt_decoder->b_discontinuity = false;
        p_demux->b_discontinuity = false;
    }
    else
    {
        /* Perform a few sanity checks */
        if (p_ldt_decoder->p_building_ldt)
        {
            if (dvbpsi_CheckLDT(p_dvbpsi, p_ldt_decoder, p_section))
            {
                dvbpsi_ReInitLDT(p_ldt_decoder, true);
            }
        }
    }

    /* Add section to LDT */
    if (!dvbpsi_AddSectionLDT(p_dvbpsi, p_ldt_decoder, p_section))
    {
        dvbpsi_error(p_dvbpsi, "LDT decoder", "failed decoding section %d", p_section->i_number);
        dvbpsi_DeletePSISections2(p_section);
        return;
    }
    /* Check if we have all the sections */
    if (dvbpsi_decoder_psi_sections_completed(DVBPSI_DECODER(p_ldt_decoder)))
    {
        assert(p_ldt_decoder->pf_ldt_callback);
        p_ldt_decoder->b_current_valid = true;
        p_dvbpsi->p_decoder->b_current_valid = true;
        /* Decode the sections */
        dvbpsi_isdbt_ldt_sections_decode(p_ldt_decoder->p_building_ldt, p_ldt_decoder->p_sections);
        dvbpsi_psi_section_t* p_mysection =  p_ldt_decoder->p_sections;
        bool ldtEquals = true;
        for(int i=0; i<6; i++)
        {
            if(p_mysection)
            {
                p_ldt_decoder->p_building_ldt->i_crcs[i] = ((*(p_mysection->p_payload_end))<<24)+
                    ((*(p_mysection->p_payload_end+1))<<16)+
                    ((*(p_mysection->p_payload_end+2))<<8)+
                    ((*(p_mysection->p_payload_end+3)));
                p_mysection = p_mysection->p_next;
            }
            else
            {
                p_ldt_decoder->p_building_ldt->i_crcs[i] = 0;
            }
            if( p_ldt_decoder->current_ldt.i_crcs[i] != p_ldt_decoder->p_building_ldt->i_crcs[i])
            {
                ldtEquals = false;
            }
        }
        /* Save the current information */
        p_ldt_decoder->current_ldt = *p_ldt_decoder->p_building_ldt;
        /* Delete the sections */
        dvbpsi_DeletePSISections(p_ldt_decoder->p_sections);
        p_ldt_decoder->p_sections = NULL;

        /* signal the new LDT */
        if(!ldtEquals)
            p_ldt_decoder->pf_ldt_callback(p_ldt_decoder->p_cb_data, p_ldt_decoder->p_building_ldt);
        /* Reinitialize the structures */
        dvbpsi_ReInitLDT(p_ldt_decoder, true);
    }
}

/*****************************************************************************
 * dvbpsi_isdbt_ldt_sections_decode
 *****************************************************************************
 * LDT decoder.
 *****************************************************************************/
void dvbpsi_isdbt_ldt_sections_decode(dvbpsi_isdbt_ldt_t* p_ldt,
                                dvbpsi_psi_section_t* p_section)
{
    uint8_t *p_byte, *p_end;

    while (p_section)
    {
        for (p_byte = p_section->p_payload_start + 4; p_byte + 5 < p_section->p_payload_end;)
        {
            uint16_t i_description_id = ((uint16_t)(p_byte[0]) << 8) | p_byte[1];
            uint16_t i_reserved = ((uint16_t)(p_byte[2]) << 4) | ((p_byte[3] & 0xf0) >> 4);
            uint16_t i_descriptors_loop_length = ((uint16_t)(p_byte[3] & 0xf) <<8) | p_byte[4];
            dvbpsi_isdbt_ldt_description_t* p_description = dvbpsi_isdbt_ldt_description_add(p_ldt,
                    i_description_id, i_reserved);

            /* description descriptors */
            p_byte += 5;
            p_end = p_byte + i_descriptors_loop_length;
            if( p_end > p_section->p_payload_end ) break;

            while(p_byte + 2 <= p_end)
            {
                uint8_t i_tag = p_byte[0];
                uint8_t i_length = p_byte[1];
                if (i_length + 2 <= p_end - p_byte)
                    dvbpsi_isdbt_ldt_description_descriptor_add(p_description, i_tag, i_length, p_byte + 2);
                p_byte += 2 + i_length;
            }
        }
        p_section = p_section->p_next;
    }
}

/*****************************************************************************
 * dvbpsi_isdbt_ldt_sections_generate
 *****************************************************************************
 * Generate LDT sections based on the dvbpsi_isdbt_ldt_t structure.
 *****************************************************************************/
dvbpsi_psi_section_t *dvbpsi_isdbt_ldt_sections_generate(dvbpsi_t *p_dvbpsi, dvbpsi_isdbt_ldt_t* p_ldt)
{
    dvbpsi_psi_section_t *p_result = dvbpsi_NewPSISection(4094);
    dvbpsi_psi_section_t *p_current = p_result;
    dvbpsi_psi_section_t *p_prev;

    dvbpsi_isdbt_ldt_description_t *p_description = p_ldt->p_first_description;

    p_current->i_table_id = 0xC7;
    p_current->b_syntax_indicator = true;
    p_current->b_private_indicator = true;
    p_current->i_length = 13;                    /* header + CRC_32 */
    p_current->i_extension = p_ldt->i_extension; /* is transport_stream_id */
    p_current->i_version = p_ldt->i_version;
    p_current->b_current_next = p_ldt->b_current_next;
    p_current->i_number = 0;
    p_current->p_payload_end += 12;               /* just after the header */
    p_current->p_payload_start = p_current->p_data + 8; /* points to the byte after last_section_number */

    /* Transport Stream ID */
    p_current->p_data[8] = (p_ldt->i_transport_stream_id >> 8) ;
    p_current->p_data[9] = p_ldt->i_transport_stream_id;
    /* Original Network ID */
    p_current->p_data[10] = (p_ldt->i_network_id >> 8) ;
    p_current->p_data[11] = p_ldt->i_network_id;
    /* LDT description */
    while (p_description != NULL)
    {
        uint8_t * p_description_start = p_current->p_payload_end;
        uint16_t i_description_length = 5;

        dvbpsi_descriptor_t * p_descriptor = p_description->p_first_descriptor;

        while ((p_descriptor != NULL)&& ((p_description_start - p_current->p_data) + i_description_length <= 1020))
        {
            i_description_length += p_descriptor->i_length + 2;
            p_descriptor = p_descriptor->p_next;
        }

        if ((p_descriptor != NULL) && (p_description_start - p_current->p_data != 11) && (i_description_length <= 1009))
        {
            /* will put more descriptors in an empty section */
            dvbpsi_debug(p_dvbpsi, "LDT generator","create a new section to carry more Service descriptors");

            p_prev = p_current;
            p_current = dvbpsi_NewPSISection(4094);
            p_prev->p_next = p_current;

            p_current->i_table_id = 0xC7;
            p_current->b_syntax_indicator = true;
            p_current->b_private_indicator = true;
            p_current->i_length = 13;                 /* header + CRC_32 */
            p_current->i_extension = p_ldt->i_extension;;
            p_current->i_version = p_ldt->i_version;
            p_current->b_current_next = p_ldt->b_current_next;
            p_current->i_number = p_prev->i_number + 1;
            p_current->p_payload_end += 12;           /* just after the header */
            p_current->p_payload_start = p_current->p_data + 8;

            /* Original Network ID */
            p_current->p_data[8] = (p_ldt->i_transport_stream_id >> 8) ;
            p_current->p_data[9] = p_ldt->i_transport_stream_id;
            p_current->p_data[10] = (p_ldt->i_network_id >> 8) ;
            p_current->p_data[11] = p_ldt->i_network_id;

            p_description_start = p_current->p_payload_end;
        }

        p_description_start[0] = (p_description->i_description_id >>8);
        p_description_start[1] = (p_description->i_description_id );
        p_description_start[2] = (p_description->i_reserved >> 4);
        p_description_start[3] = ((p_description->i_reserved & 0xf) << 4) | ((p_description->i_descriptors_length & 0xf00)>>8);
        p_description_start[4] = (p_description->i_descriptors_length & 0xff);

        /* Increase the length by 5 */
        p_current->p_payload_end += 5;
        p_current->i_length += 5;

        /* ES descriptors */
        p_descriptor = p_description->p_first_descriptor;
        while ((p_descriptor != NULL) && ( (p_current->p_payload_end - p_current->p_data) + p_descriptor->i_length <= 1018))
        {
            /* p_payload_end is where the descriptor begins */
            p_current->p_payload_end[0] = p_descriptor->i_tag;
            p_current->p_payload_end[1] = p_descriptor->i_length;
            memcpy(p_current->p_payload_end + 2, p_descriptor->p_data, p_descriptor->i_length);
            /* Increase length by descriptor_length + 2 */
            p_current->p_payload_end += p_descriptor->i_length + 2;
            p_current->i_length += p_descriptor->i_length + 2;
            p_descriptor = p_descriptor->p_next;
        }

        if (p_descriptor != NULL)
            dvbpsi_error(p_dvbpsi, "LDT generator", "unable to carry all the descriptors");

//        i_description_length = p_current->p_payload_end - p_description_start - 5;
//        p_description_start[3] |= ((i_description_length  >> 8) & 0x0f);
//        p_description_start[4] = i_description_length;
        printf("********** p_description \n\n\n\n");
        p_description = p_description->p_next;
    }

    /* Finalization */
    p_prev = p_result;
    while (p_prev != NULL)
    {
        p_prev->i_last_number = p_current->i_number;
        dvbpsi_BuildPSISection(p_dvbpsi, p_prev);
        p_prev = p_prev->p_next;
    }
    return p_result;
}
