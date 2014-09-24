/*****************************************************************************
 * Bit.c: BIT decoder/generator
 *----------------------------------------------------------------------------
 * Copyright (C) 2001-2012 VideoLAN
 * $Id$
 *
 * Authors: Johann Hanne
 *          heavily based on pmt.c which was written by
 *          Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
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
#include "isdbt_bit.h"

/*****************************************************************************
 * dvbpsi_isdbt_bit_attach
 *****************************************************************************
 * Initialize a BIT subtable decoder.
 *****************************************************************************/
typedef struct dvbpsi_isdbt_bit_decoder_s {
	DVBPSI_DECODER_COMMON
	dvbpsi_isdbt_bit_callback pf_bit_callback;
	void * p_cb_data;
	dvbpsi_isdbt_bit_t current_bit;
	dvbpsi_isdbt_bit_t * p_building_bit;
} dvbpsi_isdbt_bit_decoder_t;

static void dvbpsi_isdbt_bit_sections_gather(dvbpsi_t *p_dvbpsi,
		dvbpsi_decoder_t *p_private_decoder, dvbpsi_psi_section_t *p_section);

static void dvbpsi_isdbt_bit_sections_decode(dvbpsi_isdbt_bit_t* p_bit,
		dvbpsi_psi_section_t* p_section);

bool dvbpsi_isdbt_attachBIT(dvbpsi_t* p_dvbpsi, uint8_t i_table_id,
		uint16_t i_extension, dvbpsi_isdbt_bit_callback pf_callback,
		void* p_cb_data) {
	assert(p_dvbpsi);
	assert(p_dvbpsi->p_decoder);

	dvbpsi_demux_t* p_demux = (dvbpsi_demux_t*) p_dvbpsi->p_decoder;

	if (dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension)) {
		dvbpsi_error(p_dvbpsi, "BIT decoder",
				"Already a decoder for (table_id == 0x%02x,"
						"extension == 0x%02x)", i_table_id, i_extension);
		return false;
	}

	dvbpsi_isdbt_bit_decoder_t* p_bit_decoder;
	p_bit_decoder = (dvbpsi_isdbt_bit_decoder_t*) dvbpsi_decoder_new(NULL, 0,
			true, sizeof(dvbpsi_isdbt_bit_decoder_t));
	if (p_bit_decoder == NULL)
		return false;

	/* subtable decoder configuration */
	dvbpsi_demux_subdec_t* p_subdec;
	p_subdec = dvbpsi_NewDemuxSubDecoder(i_table_id, i_extension,
			dvbpsi_isdbt_detachBIT, dvbpsi_isdbt_bit_sections_gather,
			DVBPSI_DECODER(p_bit_decoder));
	if (p_subdec == NULL) {
		dvbpsi_decoder_delete(DVBPSI_DECODER(p_bit_decoder));
		return false;
	}

	/* Attach the subtable decoder to the demux */
	dvbpsi_AttachDemuxSubDecoder(p_demux, p_subdec);

	/* BIT decoder information */
	p_bit_decoder->pf_bit_callback = pf_callback;
	p_bit_decoder->p_cb_data = p_cb_data;
	p_bit_decoder->p_building_bit = NULL;
	return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_bitdetach
 *****************************************************************************
 * Close a BIT decoder.
 *****************************************************************************/
void dvbpsi_isdbt_detachBIT(dvbpsi_t * p_dvbpsi, uint8_t i_table_id,
		uint16_t i_extension) {
	dvbpsi_demux_t *p_demux = (dvbpsi_demux_t *) p_dvbpsi->p_decoder;

	dvbpsi_demux_subdec_t* p_subdec;
	p_subdec = dvbpsi_demuxGetSubDec(p_demux, i_table_id, i_extension);
	if (p_subdec == NULL) {
		dvbpsi_error(p_dvbpsi, "BIT Decoder",
				"No such BIT decoder (table_id == 0x%02x,"
						"extension == 0x%02x)", i_table_id, i_extension);
		return;
	}

	dvbpsi_isdbt_bit_decoder_t* p_bit_decoder;
	p_bit_decoder = (dvbpsi_isdbt_bit_decoder_t*) p_subdec->p_decoder;
	if (p_bit_decoder->p_building_bit)
		dvbpsi_isdbt_bit_delete(p_bit_decoder->p_building_bit);
	p_bit_decoder->p_building_bit = NULL;

	/* Free demux sub table decoder */
	dvbpsi_DetachDemuxSubDecoder(p_demux, p_subdec);
	dvbpsi_DeleteDemuxSubDecoder(p_subdec);
}

/****************************************************************************
 * dvbpsi_isdbt_bit_init
 *****************************************************************************
 * Initialize a pre-allocated dvbpsi_isdbt_bit_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_bit_init(dvbpsi_isdbt_bit_t* p_bit, uint8_t i_table_id,
		uint16_t i_ori_network_id, uint8_t i_version,
		bool b_current_next, bool b_broadcast_view_propriety) {
	p_bit->i_table_id = i_table_id;
	p_bit->i_original_network_id = i_ori_network_id;
	p_bit->i_version = i_version;
	p_bit->b_current_next = b_current_next;
	p_bit->p_first_descriptor = NULL;
	p_bit->p_first_bi = NULL;
	p_bit->b_broadcast_view_propriety = b_broadcast_view_propriety;
}

/****************************************************************************
 * dvbpsi_isdbt_bit_new
 *****************************************************************************
 * Allocate and initialize a dvbpsi_isdbt_bit_t structure.
 *****************************************************************************/
dvbpsi_isdbt_bit_t *dvbpsi_isdbt_bit_new(uint8_t i_table_id,
		uint16_t i_original_network_id, uint8_t i_version,
		bool b_current_next, bool b_broadcast_view_propriety) {
	dvbpsi_isdbt_bit_t*p_bit = (dvbpsi_isdbt_bit_t*) malloc(
			sizeof(dvbpsi_isdbt_bit_t));
	if (p_bit != NULL)
		dvbpsi_isdbt_bit_init(p_bit, i_table_id, i_original_network_id,
				i_version, b_current_next, b_broadcast_view_propriety);
	return p_bit;
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_empty
 *****************************************************************************
 * Clean a dvbpsi_isdbt_bit_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_bit_empty(dvbpsi_isdbt_bit_t* p_bit) {
	dvbpsi_isdbt_bit_bi_t* p_bi = p_bit->p_first_bi;

	dvbpsi_DeleteDescriptors(p_bit->p_first_descriptor);

	while (p_bi != NULL) {
		dvbpsi_isdbt_bit_bi_t* p_tmp = p_bi->p_next;
		dvbpsi_DeleteDescriptors(p_bi->p_first_descriptor);
		free(p_bi);
		p_bi = p_tmp;
	}

	p_bit->p_first_descriptor = NULL;
	p_bit->p_first_bi = NULL;
}

/****************************************************************************
 * dvbpsi_isdbt_bit_delete
 *****************************************************************************
 * Clean and delete a dvbpsi_isdbt_bit_t structure.
 *****************************************************************************/
void dvbpsi_isdbt_bit_delete(dvbpsi_isdbt_bit_t *p_bit) {
	if (p_bit)
		dvbpsi_isdbt_bit_empty(p_bit);
	free(p_bit);
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_descriptor_add
 *****************************************************************************
 * Add a descriptor in the BIT.
 *****************************************************************************/
dvbpsi_descriptor_t* dvbpsi_isdbt_bit_descriptor_add(dvbpsi_isdbt_bit_t* p_nit,
		uint8_t i_tag, uint8_t i_length, uint8_t* p_data) {
	dvbpsi_descriptor_t* p_descriptor = dvbpsi_NewDescriptor(i_tag, i_length,
			p_data);
	if (p_descriptor == NULL)
		return NULL;

	if (p_nit->p_first_descriptor == NULL)
		p_nit->p_first_descriptor = p_descriptor;
	else {
		dvbpsi_descriptor_t* p_last_descriptor = p_nit->p_first_descriptor;
		while (p_last_descriptor->p_next != NULL)
			p_last_descriptor = p_last_descriptor->p_next;
		p_last_descriptor->p_next = p_descriptor;
	}
	return p_descriptor;
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_bi_add
 *****************************************************************************
 * Add an BI in the BIT.
 *****************************************************************************/
dvbpsi_isdbt_bit_bi_t* dvbpsi_isdbt_bit_bi_add(dvbpsi_isdbt_bit_t* p_bit,
		uint8_t i_broadcaster_id) {
	dvbpsi_isdbt_bit_bi_t* p_bi = (dvbpsi_isdbt_bit_bi_t*) malloc(
			sizeof(dvbpsi_isdbt_bit_bi_t));
	if (p_bi == NULL)
		return NULL;
	p_bi->i_broadcast_id = i_broadcaster_id;
	p_bi->p_first_descriptor = NULL;
	p_bi->p_next = NULL;

	if (p_bit->p_first_bi == NULL)
		p_bit->p_first_bi = p_bi;
	else {
		dvbpsi_isdbt_bit_bi_t* p_last_ts = p_bit->p_first_bi;
		while (p_last_ts->p_next != NULL)
			p_last_ts = p_last_ts->p_next;
		p_last_ts->p_next = p_bi;
	}
	return p_bi;
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_ts_descriptor_add
 *****************************************************************************
 * Add a descriptor in the BIT BI.
 *****************************************************************************/
dvbpsi_descriptor_t* dvbpsi_isdbt_bit_bi_descriptor_add(
		dvbpsi_isdbt_bit_bi_t* p_ts, uint8_t i_tag, uint8_t i_length,
		uint8_t* p_data) {
	dvbpsi_descriptor_t* p_descriptor = dvbpsi_NewDescriptor(i_tag, i_length,
			p_data);
	if (p_descriptor == NULL)
		return NULL;

	p_ts->p_first_descriptor = dvbpsi_AddDescriptor(p_ts->p_first_descriptor,
			p_descriptor);
	assert(p_ts->p_first_descriptor);
	if (p_ts->p_first_descriptor == NULL)
		return NULL;

	return p_descriptor;
}

void reset_bit(dvbpsi_decoder_t* p_decoder) {
	assert(p_decoder);
	dvbpsi_isdbt_bit_decoder_t* p_bit_decoder =
			(dvbpsi_isdbt_bit_decoder_t*) p_decoder;
	dvbpsi_decoder_reset(DVBPSI_DECODER(p_bit_decoder), true);
}

/* */
static void dvbpsi_ReInitBIT(dvbpsi_isdbt_bit_decoder_t* p_decoder,
		const bool b_force) {
	assert(p_decoder);

	dvbpsi_decoder_reset(DVBPSI_DECODER(p_decoder), b_force);

	/* Force redecoding */
	if (b_force) {
		/* Free structures */
		if (p_decoder->p_building_bit)
			dvbpsi_isdbt_bit_delete(p_decoder->p_building_bit);
	}
	p_decoder->p_building_bit = NULL;
}

static bool dvbpsi_CheckBIT(dvbpsi_t *p_dvbpsi,
		dvbpsi_isdbt_bit_decoder_t *p_bit_decoder,
		dvbpsi_psi_section_t *p_section) {
	assert(p_dvbpsi);
	assert(p_bit_decoder);
	bool b_reinit = false;
	if (p_bit_decoder->p_building_bit->i_version != p_section->i_version) {
		/* version_number */
		dvbpsi_error(p_dvbpsi, "BIT decoder", "'version_number' differs"
				" whereas no discontinuity has occured");
		b_reinit = true;
	} else if (p_bit_decoder->i_last_section_number
			!= p_section->i_last_number) {
		/* last_section_number */
		dvbpsi_error(p_dvbpsi, "BIT decoder", "'last_section_number' differs"
				" whereas no discontinuity has occured");
		b_reinit = true;
	}

	return b_reinit;
}

static bool dvbpsi_AddSectionBIT(dvbpsi_t *p_dvbpsi,
		dvbpsi_isdbt_bit_decoder_t *p_bit_decoder,
		dvbpsi_psi_section_t* p_section) {

	assert(p_dvbpsi);
	assert(p_bit_decoder);
	assert(p_section);

	/* Initialize the structures if it's the first section received */
	if (p_bit_decoder->p_building_bit == NULL) {
		p_bit_decoder->p_building_bit = dvbpsi_isdbt_bit_new(
				p_section->i_table_id, p_section->i_extension,
				p_section->i_version, p_section->b_current_next,
				(((p_section->p_payload_start[0]) & 0x10) == 0x10));
		if (p_bit_decoder->p_building_bit == NULL)
			return false;
		p_bit_decoder->i_last_section_number = p_section->i_last_number;
	}

	/* Add to linked list of sections */
	if (dvbpsi_decoder_psi_section_add(DVBPSI_DECODER(p_bit_decoder),
			p_section))
		dvbpsi_debug(p_dvbpsi, "BIT decoder", "overwrite section number %d",
				p_section->i_number);
	return true;
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_sections_gather
 *****************************************************************************
 * Callback for the PSI decoder.
 *****************************************************************************/
static void dvbpsi_isdbt_bit_sections_gather(dvbpsi_t *p_dvbpsi,
		dvbpsi_decoder_t *p_private_decoder, dvbpsi_psi_section_t *p_section) {

	assert(p_dvbpsi);
	assert(p_dvbpsi->p_decoder);
	const uint8_t i_table_id =
			(p_section->i_table_id == 0xC4) ? p_section->i_table_id : 0xC4;

	if (!dvbpsi_CheckPSISection(p_dvbpsi, p_section, i_table_id,
			"BIT decoder")) {
		dvbpsi_DeletePSISections(p_section);
		return;
	}

	/* */
	dvbpsi_isdbt_bit_decoder_t* p_bit_decoder =
			(dvbpsi_isdbt_bit_decoder_t*) p_private_decoder;

	/* We have a valid BIT section */
	/* TS discontinuity check */
	if (p_bit_decoder->b_discontinuity) {
		dvbpsi_ReInitBIT(p_bit_decoder, true);
		p_bit_decoder->b_discontinuity = false;
	} else {
		/* Perform some few sanity checks */
		if (p_bit_decoder->p_building_bit) {
			if (dvbpsi_CheckBIT(p_dvbpsi, p_bit_decoder, p_section))
				dvbpsi_ReInitBIT(p_bit_decoder, true);
		}
		//        else
		//        {
		//            if ((p_dvbpsi->p_decoder->b_current_valid) &&
		//                    (p_bit_decoder->b_current_valid)
		//                && (p_bit_decoder->current_bit.i_version == p_section->i_version)
		//                && (p_bit_decoder->current_bit.b_current_next == p_section->b_current_next))
		//            {
		//                /* Don't decode since this version is already decoded */
		//                dvbpsi_debug(p_dvbpsi, "BIT decoder",
		//                             "ignoring already decoded section %d",
		//                             p_section->i_number);
		//                dvbpsi_DeletePSISections(p_section);
		//                return;;
		//            }
		//        }
	}

	/* Add section to BIT */
	if (!dvbpsi_AddSectionBIT(p_dvbpsi, p_bit_decoder, p_section)) {
		dvbpsi_error(p_dvbpsi, "BIT decoder", "failed decoding section %d",
				p_section->i_number);
		dvbpsi_DeletePSISections(p_section);
		return;
	}

	/* Check if we have all the sections */
	if (dvbpsi_decoder_psi_sections_completed(DVBPSI_DECODER(p_bit_decoder))) {
		assert(p_bit_decoder->pf_bit_callback);
		p_bit_decoder->b_current_valid = true;
		p_dvbpsi->p_decoder->b_current_valid = true;
		/* Decode the sections */
		dvbpsi_isdbt_bit_sections_decode(p_bit_decoder->p_building_bit,
				p_bit_decoder->p_sections);
		dvbpsi_psi_section_t* p_mysection = p_bit_decoder->p_sections;
		bool tableDidntChange = true;
		for (int i = 0; i < 6; i++) {
			if (p_mysection) {
				p_bit_decoder->p_building_bit->i_crcs[i] =
						((*(p_mysection->p_payload_end)) << 24)
								+ ((*(p_mysection->p_payload_end + 1)) << 16)
								+ ((*(p_mysection->p_payload_end + 2)) << 8)
								+ ((*(p_mysection->p_payload_end + 3)));
				p_mysection = p_mysection->p_next;
			} else {
				p_bit_decoder->p_building_bit->i_crcs[i] = 0;
			}
			if (p_bit_decoder->current_bit.i_crcs[i]
					!= p_bit_decoder->p_building_bit->i_crcs[i]) {
				tableDidntChange = false;
			}
		}
		/* Save the current information */
		p_bit_decoder->current_bit = *p_bit_decoder->p_building_bit;
		/* Delete the sections */
		dvbpsi_DeletePSISections(p_bit_decoder->p_sections);
		p_bit_decoder->p_sections = NULL;

		/* signal the new BIT */
		if (!tableDidntChange)
			p_bit_decoder->pf_bit_callback(p_bit_decoder->p_cb_data,
					p_bit_decoder->p_building_bit);
		/* Reinitialize the structures */
		dvbpsi_ReInitBIT(p_bit_decoder, true);
	}
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_sections_decode
 *****************************************************************************
 * BIT decoder.
 *****************************************************************************/
static void dvbpsi_isdbt_bit_sections_decode(dvbpsi_isdbt_bit_t* p_bit,
		dvbpsi_psi_section_t* p_section) {
	uint8_t* p_byte, *p_end;

	while (p_section) {
		p_byte=p_section->p_payload_start+2;
		/* - BIT descriptors */
		p_end = p_byte
				+ (((uint16_t) (p_section->p_payload_start[0] & 0x0f) << 8)
						| p_section->p_payload_start[1]);

		while (p_byte + 2 <= p_end) {
			uint8_t i_tag = p_byte[1];
			uint8_t i_length = p_byte[1];
			if (i_length + 2 <= p_end - p_byte)
				dvbpsi_isdbt_bit_descriptor_add(p_bit, i_tag, i_length,
						p_byte + 2);
			p_byte += 2 + i_length;
		}
		for (p_byte = p_end;p_byte + 3 <= p_section->p_payload_end;) {
			/* - Broadcast id */

			uint8_t broadcast_id = (uint8_t) p_byte[0];
			uint16_t i_desc_broadcast = ((uint16_t) (p_byte[1] & 0xf) << 8)
					| p_byte[2];
			dvbpsi_isdbt_bit_bi_t*p_broacast = dvbpsi_isdbt_bit_bi_add(p_bit,broadcast_id);
			/* Broacast descriptors */
			p_byte += 3;
			p_end = p_byte + i_desc_broadcast;
			if (p_end > p_section->p_payload_end)
				break;

			while (p_byte + 2 <= p_end) {
				uint8_t i_tag = p_byte[0];
				uint8_t i_length = p_byte[1];
				if (i_length + 2 <= p_end - p_byte)
					dvbpsi_isdbt_bit_bi_descriptor_add(p_broacast,i_tag,
							i_length, p_byte + 2);
				p_byte += 2 + i_length;
			}

		}
		p_section = p_section->p_next;
	}
}

/*****************************************************************************
 * dvbpsi_isdbt_bit_sections_generate
 *****************************************************************************
 * Generate BIT sections based on the dvbpsi_isdbt_bit_t structure.
 *****************************************************************************/
dvbpsi_psi_section_t* dvbpsi_isdbt_bit_sections_generate(dvbpsi_t *p_dvbpsi,
		dvbpsi_isdbt_bit_t* p_bit, uint8_t i_table_id) {
	dvbpsi_psi_section_t* p_result = dvbpsi_NewPSISection(1024);
	dvbpsi_psi_section_t* p_current = p_result;
	dvbpsi_psi_section_t* p_prev;
	dvbpsi_descriptor_t* p_descriptor = p_bit->p_first_descriptor;
	dvbpsi_isdbt_bit_bi_t* p_bi = p_bit->p_first_bi;
	uint16_t i_first_descriptors_length;
	p_current->i_table_id = i_table_id;
	p_current->b_syntax_indicator = true;
	p_current->b_private_indicator = false;
	p_current->i_length = 11; /* including CRC_32 */
	p_current->i_extension = p_bit->i_original_network_id;
	p_current->i_version = p_bit->i_version;
	p_current->b_current_next = p_bit->b_current_next;
	p_current->i_number = 0;
	p_current->p_payload_end += 10;
	p_current->p_payload_start = p_current->p_data + 8;

	/* BIT descriptors */
	while (p_descriptor != NULL) {
		/* New section if needed */
		/* written_data_length + descriptor_length  > 1024 - CRC_32_length */
		if ((p_current->p_payload_end - p_current->p_data)
				+ p_descriptor->i_length > 1020) {
			/* first_descriptors_length */
			i_first_descriptors_length = (p_current->p_payload_end
					- p_current->p_payload_start);
			p_current->p_data[8] = (i_first_descriptors_length >> 8) | 0xe0
					| (p_bit->b_broadcast_view_propriety ? 0x10 : 0x00);
			p_current->p_data[9] = i_first_descriptors_length;

			p_prev = p_current;
			p_current = dvbpsi_NewPSISection(1024);
			p_prev->p_next = p_current;

			p_current->i_table_id = i_table_id;
			p_current->b_syntax_indicator = true;
			p_current->b_private_indicator = false;
			p_current->i_length = 11; /* including CRC_32 */
			p_current->i_extension = p_bit->i_original_network_id;
			p_current->i_version = p_bit->i_version;
			p_current->b_current_next = p_bit->b_current_next;
			p_current->i_number = p_prev->i_number + 1;
			p_current->p_payload_end += 10;
			p_current->p_payload_start = p_current->p_data + 8;
		}

		/* p_payload_end is where the descriptor begins */
		p_current->p_payload_end[0] = p_descriptor->i_tag;
		p_current->p_payload_end[1] = p_descriptor->i_length;
		memcpy(p_current->p_payload_end + 2, p_descriptor->p_data,
				p_descriptor->i_length);

		/* Increase length by descriptor_length + 2 */
		p_current->p_payload_end += p_descriptor->i_length + 2;
		p_current->i_length += p_descriptor->i_length + 2;

		p_descriptor = p_descriptor->p_next;
	}

	/* first_descriptors_length */
	i_first_descriptors_length = (p_current->p_payload_end
			- p_current->p_payload_start) - 2;
	p_current->p_data[8] = (i_first_descriptors_length >> 8) | 0xe0
			| (p_bit->b_broadcast_view_propriety ? 0x10 : 0x00);
	p_current->p_data[9] = i_first_descriptors_length;

	/* Store the position of the broadcast_id_loop_length field
	 and reserve two bytes for it */
	/* BIT BIs */
	while (p_bi != NULL) {
		uint8_t* p_bi_start = p_current->p_payload_end;
		uint16_t i_bi_length = 3;
		/* Can the current section carry all the descriptors ? */
		p_descriptor = p_bi->p_first_descriptor;
		while ((p_descriptor != NULL)
				&& ((p_bi_start - p_current->p_data) + i_bi_length <= 1020)) {
			i_bi_length += p_descriptor->i_length + 2;
			p_descriptor = p_descriptor->p_next;
		}

		/* If _no_ and the current section isn't empty and an empty section
		 may carry one more descriptor
		 then create a new section */
		if ((p_descriptor != NULL) && (p_bi_start - p_current->p_data != 12)
				&& (i_bi_length <= 1010)) {
			/* will put more descriptors in an empty section */
			dvbpsi_debug(p_dvbpsi, "BIT generator",
					"create a new section to carry more BI descriptors");

			p_prev = p_current;
			p_current = dvbpsi_NewPSISection(1024);
			p_prev->p_next = p_current;

			p_current->i_table_id = i_table_id;
			p_current->b_syntax_indicator = true;
			p_current->b_private_indicator = false;
			p_current->i_length = 11; /* including CRC_32 */
			p_current->i_extension = p_bit->i_original_network_id;
			p_current->i_version = p_bit->i_version;
			p_current->b_current_next = p_bit->b_current_next;
			p_current->i_number = p_prev->i_number + 1;
			p_current->p_payload_end += 8;
			p_current->p_payload_start = p_current->p_data + 8;

			/* first_descriptors_length = 0 */
			p_current->p_data[8] = 0xe0;
			p_current->p_data[9] = 0x00;

			/* Store the position of the transport_stream_loop_length field
			 and reserve two bytes for it */
			p_bi_start = p_current->p_payload_end;
		}

		/* p_bi_start is where the BI begins */

		p_bi_start[0] = p_bi->i_broadcast_id >> 8;
		p_bi_start[1] = p_bi->i_broadcast_id & 0xff;

		/* Increase the length by 3 */
		p_current->p_payload_end += 3;
		p_current->i_length += 3;

		/* BI descriptors */
		p_descriptor = p_bi->p_first_descriptor;
		while ((p_descriptor != NULL)
				&& ((p_current->p_payload_end - p_current->p_data)
						+ p_descriptor->i_length <= 1020)) {
			/* p_payload_end is where the descriptor begins */
			p_current->p_payload_end[0] = p_descriptor->i_tag;
			p_current->p_payload_end[1] = p_descriptor->i_length;
			memcpy(p_current->p_payload_end + 2, p_descriptor->p_data,
					p_descriptor->i_length);

			/* Increase length by descriptor_length + 2 */
			p_current->p_payload_end += p_descriptor->i_length + 2;
			p_current->i_length += p_descriptor->i_length + 2;

			p_descriptor = p_descriptor->p_next;
		}

		if (p_descriptor != NULL)
			dvbpsi_error(p_dvbpsi, "BIT generator",
					"unable to carry all the BI descriptors");

		/* BI_info_length */
		i_bi_length = p_current->p_payload_end - p_bi_start - 3;
		p_bi_start[4] = (i_bi_length >> 8) | 0xf0;
		p_bi_start[5] = i_bi_length;

		p_bi = p_bi->p_next;
	}

	/* Finalization */
	p_prev = p_result;
	while (p_prev != NULL) {
		p_prev->i_last_number = p_current->i_number;
		dvbpsi_BuildPSISection(p_dvbpsi, p_prev);
		p_prev = p_prev->p_next;
	}

	return p_result;
}
