/*****************************************************************************
 * bit.h
 * Copyright (C) 2001-2011 VideoLAN
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
 *****************************************************************************/

/*!
 * \file <bit.h>
 * \author Vincenzo MArulli
 * \brief Application interface for the BIT decoder and the BIT generator.
 *
 * Application interface for the BIT decoder and the BIT generator.
 * New decoded BIT tables are sent by callback to the application.
 */

#ifndef _dvbpsi_isdbt_bit_H_
#define _dvbpsi_isdbt_bit_H_

#ifdef __cplusplus
extern "C" {
#endif


/*****************************************************************************
 * isdbtpsi__ts_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_bit_ts_s
 * \brief BIT TS structure.
 *
 * This structure is used to store a decoded Broadcast description.
 * (ABNT NBR 15603-2).
 */
/*!
 * \typedef struct dvbpsi_isdbt_bit_bi_s dvbpsi_isdbt_bit_bi_t
 * \brief dvbpsi_isdbt_bit_bi_t type definition.
 */

typedef struct dvbpsi_isdbt_bit_bi_s {
	uint8_t i_broadcast_id; /*broadcast id*/
	dvbpsi_descriptor_t * p_first_descriptor; /*!< descriptor list */
	struct dvbpsi_isdbt_bit_bi_s * p_next; /*!< next element of */
} dvbpsi_isdbt_bit_bi_t;

/*****************************************************************************
 * dvbpsi_isdbt_bit_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_bit_s
 * \brief bIT structure.
 *
 * This structure is used to store a decoded BIT.
 * (ABNT NBR 15603-2).
 */
/*!
 * \typedef struct dvbpsi_isdbt_bit_s dvbpsi_isdbt_bit_t
 * \brief dvbpsi_isdbt_bit_t type definition.
 */
typedef struct dvbpsi_isdbt_bit_s {
	uint8_t i_table_id; /*!< table id */
	uint16_t i_original_network_id; /*!< original_network_id */
	uint8_t i_version; /*!< version_number */
	bool b_current_next; /*!< current_next_indicator */
	bool b_broadcast_view_propriety; /*broadcast_view_propriety*/
	dvbpsi_descriptor_t *p_first_descriptor; /*!< descriptor list */
	dvbpsi_isdbt_bit_bi_t* p_first_bi; /*!< BI list */
	uint32_t i_crcs[6];
} dvbpsi_isdbt_bit_t;

/*****************************************************************************
 * dvbpsi_isdbt_bit_callback
 *****************************************************************************/
/*!
 * \typedef void (* dvbpsi_isdbt_bit_callback)(void* p_cb_data,
 dvbpsi_isdbt_bit_t* p_new_bit)
 * \brief Callback type definition.
 */
typedef void (*dvbpsi_isdbt_bit_callback)(void* p_cb_data,
		dvbpsi_isdbt_bit_t* p_new_bit);

/*****************************************************************************
 * dvbpsi_isdbt_bit_attach
 *****************************************************************************/
/*!
 * \fn bool dvbpsi_isdbt_bit_attach(dvbpsi_t* p_dvbpsi, uint8_t i_table_id, uint16_t i_extension,
 dvbpsi_isdbt_bit_callback pf_callback, void* p_cb_data)
 * \brief Creation and initialization of a BIT decoder. It is attached to p_dvbpsi.
 * \param p_dvbpsi dvbpsi handle to Subtable demultiplexor to which the decoder is attached.
 * \param i_table_id Table ID, 0xc4
 * \param i_extension Table ID extension, here service ID.
 * \param pf_callback function to call back on new BIT.
 * \param p_cb_data private data given in argument to the callback.
 * \return true on success, false on failure
 */
bool dvbpsi_isdbt_attachBIT(dvbpsi_t* p_dvbpsi, uint8_t i_table_id,
		uint16_t i_extension, dvbpsi_isdbt_bit_callback pf_callback,
		void* p_cb_data);

/*****************************************************************************
 * dvbpsi_isdbt_bit_detach
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_bit_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
 uint16_t i_extension)
 * \brief Destroy a BIT decoder.
 * \param p_dvbpsi dvbpsi handle to Subtable demultiplexor to which the decoder is attached.
 * \param i_table_id Table ID, 0xC4
 * \param i_extension Table ID extension, here service ID.
 * \return nothing.
 */
void dvbpsi_isdbt_detachBIT(dvbpsi_t * p_dvbpsi, uint8_t i_table_id,
		uint16_t i_extension);

/*****************************************************************************
 * dvbpsi_isdbt_bit_init/dvbpsi_isdbt_bit_new
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_bit_init(dvbpsi_isdbt_bit_t* p_bit, uint8_t i_table_id, uint16_t i_extension,
  uint8_t i_version, bool b_current_next)
 * \brief Initialize a user-allocated dvbpsi_isdbt_bit_t structure.
 * \param i_table_id Table ID, 0xc4
 * \param i_extension Table ID extension, here service ID.
 * \param p_bit pointer to the BIT structure
 * \param i_version BIT version
 * \param b_current_next current next indicator
 * \param b_broadcast_view_propriety
 * \return nothing.
 */
void dvbpsi_isdbt_bit_init(dvbpsi_isdbt_bit_t* p_bit, uint8_t i_table_id,
		uint16_t i_original_network_id, uint8_t i_version,
		bool b_current_next,bool b_broadcast_view_propriety);

/*!
 * \fn dvbpsi_isdbt_bit_t *dvbpsi_isdbt_bit_new(uint8_t i_table_id,
 *                                  uint16_t i_original_network_id, uint8_t i_version,
 *                                  bool b_current_next,bool b_broadcast_view_propriety);
 * \brief Allocate and initialize a new isdbpsi_bit_t structure.
 * \param i_table_id Table ID, 0xC4
 * \param i_extension Table ID extension, here service ID.
 * \param i_version BIT version
 * \param b_current_next current next indicator
 * \param bool b_broadcast_view_propriety
 * \return p_bit pointer to the BIT structure
 */
dvbpsi_isdbt_bit_t *dvbpsi_isdbt_bit_new(uint8_t i_table_id,
		uint16_t i_original_network_id, uint8_t i_version, bool b_current_next,bool b_broadcast_view_propriety);

/*****************************************************************************
 * dvbpsi_isdbt_bit_empty/dvbpsi_isdbt_bit_delete
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_bit_empty(dvbpsi_isdbt_bit_t* p_bit)
 * \brief Clean a dvbpsi_isdbt_bit_t structure.
 * \param p_bit pointer to the BIT structure
 * \return nothing.
 */
void dvbpsi_isdbt_bit_empty(dvbpsi_isdbt_bit_t* p_bit);

/*!
 * \fn dvbpsi_isdbt_bit_delete(dvbpsi_isdbt_bit_t *p_bit)
 * \brief Clean and free a dvbpsi_isdbt_bit_t structure.
 * \param p_bit pointer to the BIT structure
 * \return nothing.
 */
void dvbpsi_isdbt_bit_delete(dvbpsi_isdbt_bit_t *p_bit);

/*****************************************************************************
 * dvbpsi_isdbt_bit_descriptor_add
 *****************************************************************************/
/*!
 * \fn isdbtpsi_descriptor_t* dvbpsi_isdbt_bit_descriptor_add(dvbpsi_isdbt_bit_t* p_bit,
 uint8_t i_tag,
 uint8_t i_length,
 uint8_t* p_data)
 * \brief Add a descriptor in the BIT.
 * \param p_bit pointer to the BIT structure
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t* dvbpsi_isdbt_bit_descriptor_add(dvbpsi_isdbt_bit_t *p_bit,
		uint8_t i_tag, uint8_t i_length, uint8_t *p_data);

/*****************************************************************************
 * dvbpsi_isdbt_bit_bi_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_isdbt_bit_bi_t* dvbpsi_isdbt_bit_ts_add(dvbpsi_isdbt_bit_t* p_bit,
 uint8_t i_broad_id)
 * \brief Add an BI in the BIT.
 * \param p_bit pointer to the BIT structure
 * \param i_broadcaster_id of the BI
 * \return a pointer to the added BI.
 */
dvbpsi_isdbt_bit_bi_t* dvbpsi_isdbt_bit_bi_add(dvbpsi_isdbt_bit_t* p_bit,uint8_t i_broadcaster_id);

/*****************************************************************************
 * dvbpsi_isdbt_bit_bi_descriptor_add
 *****************************************************************************/
/*!
 * \fn isdbtpsi_descriptor_t* dvbpsi_isdbt_bit_bi_descriptor_add(dvbpsi_isdbt_bit_bi_t* p_bi,
 uint8_t i_tag,
 uint8_t i_length,
 uint8_t* p_data)
 * \brief Add a descriptor in the BIT BI.
 * \param p_bi pointer to the BI structure
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t* dvbpsi_isdbt_bit_bi_descriptor_add(dvbpsi_isdbt_bit_bi_t* p_bi,
		uint8_t i_tag, uint8_t i_length, uint8_t* p_data);

void reset_bit(dvbpsi_decoder_t* p_decoder);

/*****************************************************************************
 * dvbpsi_isdbt_bit_sections_generate
 *****************************************************************************/
/*!
 * \fn isdbtpsi_psi_section_t* dvbpsi_isdbt_bit_sections_generate(isdbtpsi_t *p_isdbtpsi, dvbpsi_isdbt_bit_t* p_bit,
 uint8_t i_table_id)
 * \brief BIT generator
 * \param p_isdbtpsi handle to isdbtpsi with attached decoder
 * \param p_bit BIT structure
 * \param i_table_id table id, 0xC4
 * \return a pointer to the list of generated PSI sections.
 *
 * Generate BIT sections based on the psi_bit_t structure.
 */
dvbpsi_psi_section_t* dvbpsi_isdbt_bit_sections_generate(dvbpsi_t* p_dvbpsi,
		dvbpsi_isdbt_bit_t* p_bit, uint8_t i_table_id);

#ifdef __cplusplus
}
;
#endif

#else
#error "Multiple inclusions of bit.h"
#endif

