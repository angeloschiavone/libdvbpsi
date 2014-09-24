/*****************************************************************************
 * isdbt_cdt.h
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
 * \file <isdbt_cdt.h>
 * \brief Application interface for the cdt decoder and the cdt generator.
 *
 * Application interface for the cdt decoder and the cdt generator.
 * New decoded cdt tables are sent by callback to the application.
 */

#ifndef ISDBT_cdt_H_
#define ISDBT_cdt_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * dvbpsi_isdbt_cdt_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_cdt_s
 * \brief cdt structure.
 *
 * This structure is used to store a decoded cdt.
 * (ABNT NBR 15603).
 */
/*!
 * \typedef struct dvbpsi_isdbt_cdt_s dvbpsi_isdbt_cdt_t
 * \brief dvbpsi_isdbt_cdt_t type definition.
 */
typedef struct dvbpsi_isdbt_cdt_s
{
    /* PSI table members */
    uint8_t i_table_id; /*!< table id, 8 bit */
    uint16_t i_extension; /*!< download data id, 16 bit */

    /* Table specific */
    uint8_t              i_version;                /*!< version_number, 5 bit*/
    bool                 b_current_next;           /*!< current_next_indicator */
    uint16_t             i_original_network_id;    /*!< original network id, 16 bit */
    uint8_t              i_data_type;              /*!< original network id, 16 bit */
    dvbpsi_descriptor_t *p_first_descriptor;       /*!< descriptor list */
    uint8_t              i_data_module_byte[4093]; /*!< data module byte */
    uint16_t             i_data_module_byte_length;
    uint32_t             i_crcs[6];
} dvbpsi_isdbt_cdt_t;

/*****************************************************************************
 * dvbpsi_isdbt_cdt_callback
 *****************************************************************************/
/*!
 * \typedef void (* dvbpsi_isdbt_cdt_callback)(void* p_cb_data,
 dvbpsi_isdbt_cdt_t* p_new_cdt)
 * \brief Callback type definition.
 */
typedef void (*dvbpsi_isdbt_cdt_callback)(void* p_cb_data,
        dvbpsi_isdbt_cdt_t* p_new_cdt);

/*****************************************************************************
 * dvbpsi_isdbt_cdt_attach
 *****************************************************************************/
/*!
 * \fn bool dvbpsi_isdbt_cdt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
 uint16_t i_extension, dvbpsi_isdbt_cdt_callback pf_callback,
 void* p_cb_data)
 * \brief Creation and initialization of a cdt decoder. It is attached to p_dvbpsi.
 * \param p_dvbpsi pointer to dvbpsi to hold decoder/demuxer structure
 * \param i_table_id Table ID, 0xC8.
 * \param i_extension Table ID extension, here original service id.
 * \param pf_callback function to call back on new cdt.
 * \param p_cb_data private data given in argument to the callback.
 * \return true on success, false on failure
 */
bool dvbpsi_isdbt_cdt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension, dvbpsi_isdbt_cdt_callback pf_callback,
        void* p_cb_data);

/*****************************************************************************
 * reset_isdbt_cdt
 *****************************************************************************/
/*!
 * \fn void reset_isdbt_cdt(dvbpsi_decoder_t* p_decoder)
 * \brief Reset the decoder, force to re-decode table.
 * \param p_decoder pointer to decoder
 * \return nothing.
 */
void reset_isdbt_cdt(dvbpsi_decoder_t* p_decoder);

/*****************************************************************************
 * dvbpsi_isdbt_cdt_detach
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_cdt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension)
 * \brief Destroy a cdt decoder.
 * \param p_dvbpsi pointer holding decoder/demuxer structure
 * \param i_table_id Table ID, 0xC8.
 * \param i_extension Table ID extension, here original service id.
 * \return nothing.
 */
void dvbpsi_isdbt_cdt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension);

/*****************************************************************************
 * dvbpsi_isdbt_cdt_init/dvbpsi_Newcdt
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_cdt_init(dvbpsi_isdbt_cdt_t* p_cdt, uint8_t i_table_id, uint16_t i_extension,
 uint8_t i_version, bool b_current_next, uint16_t i_network_id, uint16_t i_transport_stream_id)
 * \brief Initialize a user-allocated dvbpsi_isdbt_cdt_t structure.
 * \param p_cdt pointer to the cdt structure
 * \param i_table_id Table ID, 0xC8.
 * \param i_extension Table ID extension, here original service id.
 * \param i_version cdt version
 * \param b_current_next current next indicator
 * \param i_transport_stream_id transport stream id
 * \param i_network_id original network id
 * \return nothing.
 */
void dvbpsi_isdbt_cdt_init(dvbpsi_isdbt_cdt_t *p_cdt, uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_original_network_id, uint8_t i_data_type);

/*!
 * \fn dvbpsi_isdbt_cdt_t *dvbpsi_isdbt_cdt_new(uint8_t i_table_id, uint16_t i_extension, uint8_t i_version,
 bool b_current_next, uint16_t i_network_id)
 * \brief Allocate and initialize a new dvbpsi_isdbt_cdt_t structure.
 * \param i_table_id Table ID, 0x42 or 0x46.
 * \param i_extension Table ID extension, here TS ID.
 * \param i_version cdt version
 * \param b_current_next current next indicator
 * \param i_transport_stream_id transport stream id
 * \param i_network_id original network id
 * \return p_cdt pointer to the cdt structure
 */
dvbpsi_isdbt_cdt_t *dvbpsi_isdbt_cdt_new(uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_original_network_id, uint8_t i_data_type);

/*****************************************************************************
 * dvbpsi_isdbt_cdt_empty/dvbpsi_isdbt_cdt_delete
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_cdt_empty(dvbpsi_isdbt_cdt_t* p_cdt)
 * \brief Clean a dvbpsi_isdbt_cdt_t structure.
 * \param p_cdt pointer to the cdt structure
 * \return nothing.
 */
void dvbpsi_isdbt_cdt_empty(dvbpsi_isdbt_cdt_t *p_cdt);

/*!
 * \fn dvbpsi_isdbt_cdt_delete(dvbpsi_isdbt_cdt_t *p_cdt)
 * \brief Clean and free a dvbpsi_isdbt_cdt_t structure.
 * \param p_cdt pointer to the cdt structure
 * \return nothing.
 */
void dvbpsi_isdbt_cdt_delete(dvbpsi_isdbt_cdt_t *p_cdt);


/*****************************************************************************
 * dvbpsi_isdbt_cdt_descriptor_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_descriptor_t *dvbpsi_isdbt_cdt_description_descriptor_add(
        dvbpsi_isdbt_cdt_description_t *p_description, uint8_t i_tag, uint8_t i_length,
        uint8_t *p_data)
 * \brief Add a descriptor to the cdt description.
 * \param p_description pointer to the description structure
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t *dvbpsi_isdbt_cdt_descriptor_add(dvbpsi_isdbt_cdt_t* p_cdt, uint8_t i_tag, uint8_t i_length,
        uint8_t *p_data);


#ifdef __cplusplus
};
#endif

#endif /* ISDBT_cdt_H_ */
