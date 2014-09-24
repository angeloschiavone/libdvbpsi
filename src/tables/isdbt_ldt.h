/*****************************************************************************
 * isdbt_ldt.h
 *
 * Author: Angelo Schiavone <angelo.schiavone@gmail.com>
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
 * \file <isdbt_ldt.h>
 * \brief Application interface for the LDT decoder and the LDT generator.
 *
 * Application interface for the LDT decoder and the LDT generator.
 * New decoded LDT tables are sent by callback to the application.
 */

#ifndef ISDBT_LDT_H_
#define ISDBT_LDT_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * dvbpsi_isdbt_ldt_description_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_ldt_description_s
 * \brief LDT description structure.
 *
 * This structure is used to store a decoded LDT description in the LDT descriptions loop.
 * (ABNT NBR 15603).
 */
/*!
 * \typedef struct dvbpsi_isdbt_ldt_description_s dvbpsi_isdbt_ldt_description_t
 * \brief dvbpsi_isdbt_ldt_description_t type definition.
 */
typedef struct dvbpsi_isdbt_ldt_description_s
{
    uint16_t i_description_id; /*!< description_id 16 bit */
    uint16_t i_reserved; /*!< reserved_for_future_use 12 bit */
    uint16_t i_descriptors_length; /*!< Descriptors loop length 12 bit */
    dvbpsi_descriptor_t * p_first_descriptor; /*!< First of the following DVB descriptors */
    struct dvbpsi_isdbt_ldt_description_s * p_next; /*!< next element of the list */

} dvbpsi_isdbt_ldt_description_t;

/*****************************************************************************
 * dvbpsi_isdbt_ldt_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_ldt_s
 * \brief LDT structure.
 *
 * This structure is used to store a decoded LDT.
 * (ABNT NBR 15603).
 */
/*!
 * \typedef struct dvbpsi_isdbt_ldt_s dvbpsi_isdbt_ldt_t
 * \brief dvbpsi_isdbt_ldt_t type definition.
 */
typedef struct dvbpsi_isdbt_ldt_s
{
    /* PSI table members */
    uint8_t i_table_id; /*!< table id, 8 bit */
    uint16_t i_extension; /*!< original service id, 16 bit */

    /* Table specific */
    uint8_t i_version; /*!< version_number, 5 bit*/
    bool b_current_next; /*!< current_next_indicator */
    uint16_t i_transport_stream_id; /*!< transport stream id, 16 bit */
    uint16_t i_network_id; /*!< original network id, 16 bit */

    dvbpsi_isdbt_ldt_description_t * p_first_description; /*!< description list */
    uint32_t i_crcs[6];
} dvbpsi_isdbt_ldt_t;

/*****************************************************************************
 * dvbpsi_isdbt_ldt_callback
 *****************************************************************************/
/*!
 * \typedef void (* dvbpsi_isdbt_ldt_callback)(void* p_cb_data,
 dvbpsi_isdbt_ldt_t* p_new_ldt)
 * \brief Callback type definition.
 */
typedef void (*dvbpsi_isdbt_ldt_callback)(void* p_cb_data,
        dvbpsi_isdbt_ldt_t* p_new_ldt);

/*****************************************************************************
 * dvbpsi_isdbt_ldt_attach
 *****************************************************************************/
/*!
 * \fn bool dvbpsi_isdbt_ldt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
 uint16_t i_extension, dvbpsi_isdbt_ldt_callback pf_callback,
 void* p_cb_data)
 * \brief Creation and initialization of a LDT decoder. It is attached to p_dvbpsi.
 * \param p_dvbpsi pointer to dvbpsi to hold decoder/demuxer structure
 * \param i_table_id Table ID, 0xC7.
 * \param i_extension Table ID extension, here original service id.
 * \param pf_callback function to call back on new LDT.
 * \param p_cb_data private data given in argument to the callback.
 * \return true on success, false on failure
 */
bool dvbpsi_isdbt_ldt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension, dvbpsi_isdbt_ldt_callback pf_callback,
        void* p_cb_data);

/*****************************************************************************
 * reset_isdbt_ldt
 *****************************************************************************/
/*!
 * \fn void reset_isdbt_ldt(dvbpsi_decoder_t* p_decoder)
 * \brief Reset the decoder, force to re-decode table.
 * \param p_decoder pointer to decoder
 * \return nothing.
 */
void reset_isdbt_ldt(dvbpsi_decoder_t* p_decoder);

/*****************************************************************************
 * dvbpsi_isdbt_ldt_detach
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_ldt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension)
 * \brief Destroy a LDT decoder.
 * \param p_dvbpsi pointer holding decoder/demuxer structure
 * \param i_table_id Table ID, 0xC7.
 * \param i_extension Table ID extension, here original service id.
 * \return nothing.
 */
void dvbpsi_isdbt_ldt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension);

/*****************************************************************************
 * dvbpsi_isdbt_ldt_init/dvbpsi_NewLDT
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_ldt_init(dvbpsi_isdbt_ldt_t* p_ldt, uint8_t i_table_id, uint16_t i_extension,
 uint8_t i_version, bool b_current_next, uint16_t i_network_id, uint16_t i_transport_stream_id)
 * \brief Initialize a user-allocated dvbpsi_isdbt_ldt_t structure.
 * \param p_ldt pointer to the LDT structure
 * \param i_table_id Table ID, 0xC7.
 * \param i_extension Table ID extension, here original service id.
 * \param i_version LDT version
 * \param b_current_next current next indicator
 * \param i_transport_stream_id transport stream id
 * \param i_network_id original network id
 * \return nothing.
 */
void dvbpsi_isdbt_ldt_init(dvbpsi_isdbt_ldt_t *p_ldt, uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_transport_stream_id, uint16_t i_network_id);

/*!
 * \fn dvbpsi_isdbt_ldt_t *dvbpsi_isdbt_ldt_new(uint8_t i_table_id, uint16_t i_extension, uint8_t i_version,
 bool b_current_next, uint16_t i_network_id)
 * \brief Allocate and initialize a new dvbpsi_isdbt_ldt_t structure.
 * \param i_table_id Table ID, 0x42 or 0x46.
 * \param i_extension Table ID extension, here TS ID.
 * \param i_version LDT version
 * \param b_current_next current next indicator
 * \param i_transport_stream_id transport stream id
 * \param i_network_id original network id
 * \return p_ldt pointer to the LDT structure
 */
dvbpsi_isdbt_ldt_t *dvbpsi_isdbt_ldt_new(uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_transport_stream_id, uint16_t i_network_id);

/*****************************************************************************
 * dvbpsi_isdbt_ldt_empty/dvbpsi_isdbt_ldt_delete
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_ldt_empty(dvbpsi_isdbt_ldt_t* p_ldt)
 * \brief Clean a dvbpsi_isdbt_ldt_t structure.
 * \param p_ldt pointer to the LDT structure
 * \return nothing.
 */
void dvbpsi_isdbt_ldt_empty(dvbpsi_isdbt_ldt_t *p_ldt);

/*!
 * \fn dvbpsi_isdbt_ldt_delete(dvbpsi_isdbt_ldt_t *p_ldt)
 * \brief Clean and free a dvbpsi_isdbt_ldt_t structure.
 * \param p_ldt pointer to the LDT structure
 * \return nothing.
 */
void dvbpsi_isdbt_ldt_delete(dvbpsi_isdbt_ldt_t *p_ldt);

/*****************************************************************************
 * dvbpsi_isdbt_ldt_description_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_isdbt_ldt_description_t* dvbpsi_isdbt_ldt_description_add(dvbpsi_isdbt_ldt_t* p_ldt,
 uint16_t i_description_id, uint16_t i_reserved )
 * \brief Add LDT description to the end of LDT descriptions loop
 * \param p_ldt pointer to the LDT structure
 * \param i_description_id Description ID
 * \param i_reserved reserved_for_future_use 12 bit
 * \return a pointer to the added description.
 */
dvbpsi_isdbt_ldt_description_t* dvbpsi_isdbt_ldt_description_add(
        dvbpsi_isdbt_ldt_t* p_ldt, uint16_t i_description_id,
        uint16_t i_reserved);


/*****************************************************************************
 * dvbpsi_isdbt_ldt_description_descriptor_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_descriptor_t *dvbpsi_isdbt_ldt_description_descriptor_add(
        dvbpsi_isdbt_ldt_description_t *p_description, uint8_t i_tag, uint8_t i_length,
        uint8_t *p_data)
 * \brief Add a descriptor to the LDT description.
 * \param p_description pointer to the description structure
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t *dvbpsi_isdbt_ldt_description_descriptor_add(
        dvbpsi_isdbt_ldt_description_t *p_description, uint8_t i_tag, uint8_t i_length,
        uint8_t *p_data);


/*****************************************************************************
 * dvbpsi_isdbt_ldt_sections_generate
 *****************************************************************************
 * Generate LDT sections based on the dvbpsi_isdbt_ldt_t structure.
 *****************************************************************************/
/*!
 * \fn dvbpsi_psi_section_t *dvbpsi_isdbt_ldt_sections_generate(dvbpsi_t *p_dvbpsi,
        dvbpsi_isdbt_ldt_t * p_ldt)
 * \brief LDT generator
 * \param p_dvbpsi handle to dvbpsi with attached decoder
 * \param p_ldt LDT structure
 * \return a pointer to the list of generated PSI sections.
 *
 * Generate LDT sections based on the dvbpsi_isdbt_ldt_t structure.
 */
dvbpsi_psi_section_t *dvbpsi_isdbt_ldt_sections_generate(dvbpsi_t *p_dvbpsi,
        dvbpsi_isdbt_ldt_t * p_ldt);


#ifdef __cplusplus
};
#endif

#endif /* ISDBT_LDT_H_ */
