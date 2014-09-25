/*****************************************************************************
 * isdbt_sdtt.h
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
 * \file <isdbt_sdtt.h>
 * \brief Application interface for the sdtt decoder and the sdtt generator.
 *
 * Application interface for the sdtt decoder and the sdtt generator.
 * New decoded sdtt tables are sent by callback to the application.
 */

#ifndef ISDBT_SDTT_H_
#define ISDBT_SDTT_H_

#ifdef __cplusplus
extern "C"
{
#endif




/*****************************************************************************
 * dvbpsi_isdbt_sdtt_schedule_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_sdtt_schedule_s
 * \brief SDTT schedule structure.
 *
 * This structure is used to store a decoded SDTT schedule, referenced by sdtt content.
 * (ARIB STD-B21).
 */
/*!
 * \typedef struct dvbpsi_isdbt_sdtt_schedule_s dvbpsi_isdbt_sdtt_schedule_t
 * \brief dvbpsi_isdbt_sdtt_schedule_t type definition.
 */
typedef struct dvbpsi_isdbt_sdtt_schedule_s
{
    uint64_t  i_start_time; /*!< start time, 40 bit */
    uint32_t  i_duration; /*!< duration, 24 bit */

    struct dvbpsi_isdbt_sdtt_schedule_s * p_next; /*!< next element of the list */

} dvbpsi_isdbt_sdtt_schedule_t;



/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_sdtt_content_s
 * \brief SDTT content structure.
 *
 * This structure is used to store a decoded SDTT content.
 * (ARIB STD-B21).
 */
/*!
 * \typedef struct dvbpsi_isdbt_sdtt_content_s dvbpsi_isdbt_sdtt_content_t
 * \brief dvbpsi_isdbt_sdtt_content_t type definition.
 */
typedef struct dvbpsi_isdbt_sdtt_content_s
{
    uint8_t  i_group; /*!< group, 4 bit */
    uint16_t i_target_version; /*!< target_version, 12 bit */
    uint16_t i_new_version; /*!< new_version, 12 bit */
    uint8_t i_download_level; /*!< 2 bit, 01 indicates compulsory downloading, 00 indicates discretionary downloading */

    /* version_indicator, 2 bit
     * 00: All versions are targeted (Version specification is invalid).
     * 01: Version(s) specified or later are targeted.
     * 02: Version(s) specified or earlier are targeted.
     * 03: Only specified version is targeted. **/
    uint8_t i_version_indicator;

    /* 12 bit, This field indicates total byte length of a schedule
     * loop and a descriptor loop.**/
    uint16_t i_content_description_length;

    /* 12 bit, this field indicates byte length of the schedule loop. When this value is 0 in all receiver common data,
     * it indicates that the intended download content is being transmitted.*/
    uint16_t i_schedule_description_length;

    /* schedule_timeshift_information, 4 bit
     * 0: The same download content is transmitted by the same schedule with multiple service_id
     * 1 to 12: The same download content is transmitted by shifting the time of 1 to 12 hours for each service_id with multiple service_id
     * 13 to 14: Reserved
     *15: The download content is transmitted with a single service_id.
     * For detailed operation such as specification method of service_id when transmitting the
     *download content in multiple service_id, see “Download Function” in Appendix */
    uint8_t i_schedule_timeshift_information;

    dvbpsi_isdbt_sdtt_schedule_t * p_first_schedule; /*!< content description list */

    dvbpsi_descriptor_t * p_first_descriptor; /*!< First of the following ISDB-T descriptors */

    struct dvbpsi_isdbt_sdtt_content_s * p_next; /*!< next element of the list */

} dvbpsi_isdbt_sdtt_content_t;

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_t
 *****************************************************************************/
/*!
 * \struct dvbpsi_isdbt_sdtt_s
 * \brief sdtt structure.
 *
 * This structure is used to store a decoded sdtt.
 * (ABNT NBR 15603).
 */
/*!
 * \typedef struct dvbpsi_isdbt_sdtt_s dvbpsi_isdbt_sdtt_t
 * \brief dvbpsi_isdbt_sdtt_t type definition.
 */
typedef struct dvbpsi_isdbt_sdtt_s
{
    /* PSI table members */
    uint8_t i_table_id; /*!< table id, 8 bit */
    uint16_t i_extension; /*!< extension 16 bit total: maker_id(8 bit) + model id(8 bit)   */

    /* Table specific */
    uint8_t              i_version;                /*!< version_number, 5 bit*/
    bool                 b_current_next;           /*!< current_next_indicator */
    uint8_t              i_maker_id;               /*!< maker id, 8 bit , MSB of extension 16 bit field */
    uint8_t              i_model_id;               /*!< model id, 8 bit , LSB of extension 16 bit field */
    uint16_t             i_transport_stream_id;    /*!< transport stream id, 16 bit */
    uint16_t             i_original_network_id;    /*!< original network id, 16 bit */
    uint16_t             i_service_id;             /*!< service id, 16 bit */
    uint8_t              i_num_of_contents;        /*!< num of contents in the contents loop */
    dvbpsi_isdbt_sdtt_content_t * p_first_content; /*!< content description list */

    uint32_t             i_crcs[6];
} dvbpsi_isdbt_sdtt_t;

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_callback
 *****************************************************************************/
/*!
 * \typedef void (* dvbpsi_isdbt_sdtt_callback)(void* p_cb_data,
 dvbpsi_isdbt_sdtt_t* p_new_sdtt)
 * \brief Callback type definition.
 */
typedef void (*dvbpsi_isdbt_sdtt_callback)(void* p_cb_data,
        dvbpsi_isdbt_sdtt_t* p_new_sdtt);

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_attach
 *****************************************************************************/
/*!
 * \fn bool dvbpsi_isdbt_sdtt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
 uint16_t i_extension, dvbpsi_isdbt_sdtt_callback pf_callback,
 void* p_cb_data)
 * \brief Creation and initialization of a sdtt decoder. It is attached to p_dvbpsi.
 * \param p_dvbpsi pointer to dvbpsi to hold decoder/demuxer structure
 * \param i_table_id Table ID, 0xC8.
 * \param i_extension Table ID extension, here original service id.
 * \param pf_callback function to call back on new sdtt.
 * \param p_cb_data private data given in argument to the callback.
 * \return true on success, false on failure
 */
bool dvbpsi_isdbt_sdtt_attach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension, dvbpsi_isdbt_sdtt_callback pf_callback,
        void* p_cb_data);

/*****************************************************************************
 * reset_isdbt_sdtt
 *****************************************************************************/
/*!
 * \fn void reset_isdbt_sdtt(dvbpsi_decoder_t* p_decoder)
 * \brief Reset the decoder, force to re-decode table.
 * \param p_decoder pointer to decoder
 * \return nothing.
 */
void reset_isdbt_sdtt(dvbpsi_decoder_t* p_decoder);

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_detach
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_sdtt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id, uint16_t i_extension)
 * \brief Destroy a sdtt decoder.
 * \param p_dvbpsi pointer holding decoder/demuxer structure
 * \param i_table_id Table ID, 0xC8.
 * \param i_extension Table ID extension, here original service id.
 * \return nothing.
 */
void dvbpsi_isdbt_sdtt_detach(dvbpsi_t *p_dvbpsi, uint8_t i_table_id,
        uint16_t i_extension);

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_init/dvbpsi_Newsdtt
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_sdtt_init(dvbpsi_isdbt_sdtt_t* p_sdtt, uint8_t i_table_id, uint16_t i_extension,
 uint8_t i_version, bool b_current_next, uint16_t i_network_id, uint16_t i_transport_stream_id)
 * \brief Initialize a user-allocated dvbpsi_isdbt_sdtt_t structure.
 * \param p_sdtt pointer to the sdtt structure
 * \param i_table_id Table ID, 0xC8.
 * \param i_extension Table ID extension, here original service id.
 * \param i_version sdtt version
 * \param b_current_next current next indicator
 * \param i_transport_stream_id transport stream id
 * \param i_network_id original network id
 * \return nothing.
 */
void dvbpsi_isdbt_sdtt_init(dvbpsi_isdbt_sdtt_t *p_sdtt, uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_transport_stream_id, uint16_t i_original_network_id, uint16_t i_service_id);

/*!
 * \fn dvbpsi_isdbt_sdtt_t *dvbpsi_isdbt_sdtt_new(uint8_t i_table_id, uint16_t i_extension, uint8_t i_version,
 bool b_current_next, uint16_t i_network_id)
 * \brief Allocate and initialize a new dvbpsi_isdbt_sdtt_t structure.
 * \param i_table_id Table ID, 0x42 or 0x46.
 * \param i_extension Table ID extension, here TS ID.
 * \param i_version sdtt version
 * \param b_current_next current next indicator
 * \param i_transport_stream_id transport stream id
 * \param i_original_network_id original network id
 * \param i_service_id service id
 * \return p_sdtt pointer to the sdtt structure
 */
dvbpsi_isdbt_sdtt_t *dvbpsi_isdbt_sdtt_new(uint8_t i_table_id,
        uint16_t i_extension, uint8_t i_version, bool b_current_next,
        uint16_t i_transport_stream_id, uint16_t i_original_network_id, uint16_t i_service_id);

/*****************************************************************************
 * dvbpsi_isdbt_sdtt_empty/dvbpsi_isdbt_sdtt_delete
 *****************************************************************************/
/*!
 * \fn void dvbpsi_isdbt_sdtt_empty(dvbpsi_isdbt_sdtt_t* p_sdtt)
 * \brief Clean a dvbpsi_isdbt_sdtt_t structure.
 * \param p_sdtt pointer to the sdtt structure
 * \return nothing.
 */
void dvbpsi_isdbt_sdtt_empty(dvbpsi_isdbt_sdtt_t *p_sdtt);

/*!
 * \fn dvbpsi_isdbt_sdtt_delete(dvbpsi_isdbt_sdtt_t *p_sdtt)
 * \brief Clean and free a dvbpsi_isdbt_sdtt_t structure.
 * \param p_sdtt pointer to the sdtt structure
 * \return nothing.
 */
void dvbpsi_isdbt_sdtt_delete(dvbpsi_isdbt_sdtt_t *p_sdtt);


/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_isdbt_sdtt_content_t* dvbpsi_isdbt_sdtt_content_add(dvbpsi_sdt_t* p_sdt,
                                                  uint16_t i_service_id,
                                                  bool b_eit_schedule,
                                                  bool b_eit_present,
                                                  uint8_t i_running_status,
                                                  bool b_free_ca)
 * \brief Add a service at the end of the SDTT.
 * \param p_sdtt pointer to the SDTT structure
 * \param i_group
 * \param i_target_version
 * \param i_new_version
 * \param i_download_level
 * \param version_indicator
 * \param schedule_timeshift_information
 * \return a pointer to the added content.
 */
dvbpsi_isdbt_sdtt_content_t *dvbpsi_isdbt_sdtt_content_add(dvbpsi_isdbt_sdtt_t* p_sdtt,
        uint8_t  i_group,
        uint16_t i_target_version,
        uint16_t i_new_version,
        uint8_t i_download_level,
        uint8_t version_indicator,
        uint8_t schedule_timeshift_information);


/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_schedule_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_isdbt_sdtt_schedule_t* dvbpsi_isdbt_sdtt_content_schedule_add(dvbpsi_isdbt_sdtt_content_t* p_sdtt_content,
                                                  uint64_t i_start_time,
                                                  uint32_t i_duration)
 * \brief Add a service at the end of the SDTT.
 * \param p_sdtt_content pointer to the SDTT's content
 * \param i_start_time
 * \param i_duration
 * \return a pointer to the added content.
 */
dvbpsi_isdbt_sdtt_schedule_t *dvbpsi_isdbt_sdtt_content_schedule_add(
        dvbpsi_isdbt_sdtt_content_t* p_sdtt_content, uint64_t i_start_time,
        uint32_t i_duration);



/*****************************************************************************
 * dvbpsi_isdbt_sdtt_content_descriptor_add
 *****************************************************************************/
/*!
 * \fn dvbpsi_descriptor_t *dvbpsi_isdbt_sdtt_content_descriptor_add(
        dvbpsi_isdbt_sdtt_content_t *p_sdtt_content, uint8_t i_tag, uint8_t i_length,
        uint8_t *p_data)
 * \brief Add a descriptor to the sdtt content structure.
 * \param p_sdtt_content pointer to the content structure you want to add the descriptor to
 * \param i_tag descriptor's tag
 * \param i_length descriptor's length
 * \param p_data descriptor's data
 * \return a pointer to the added descriptor.
 */
dvbpsi_descriptor_t *dvbpsi_isdbt_sdtt_content_descriptor_add(dvbpsi_isdbt_sdtt_content_t* p_sdtt_content, uint8_t i_tag, uint8_t i_length,
        uint8_t *p_data);


#ifdef __cplusplus
};
#endif

#endif /* ISDBT_SDTT_H_ */
