/****************************************************************************
Copyright Cambridge Silicon Radio Limited 2014 - 2015.

FILE NAME
    sink_ble_advertising.c

DESCRIPTION
    BLE Advertising functionality
*/

#include "sink_ble_advertising.h"

#include "sink_gatt_device.h"
#include "sink_private.h"
#include "sink_debug.h"
#include "sink_development.h"
#include "sink_utils.h"

#include <connection.h>
#include <gatt.h>

#include <csrtypes.h>
#include <stdlib.h>
#include <string.h>


#ifdef GATT_ENABLED


/* Macro for BLE AD Data Debug */
#ifdef DEBUG_BLE
#include <stdio.h>
#define BLE_AD_INFO(x) DEBUG(x)
#define BLE_AD_ERROR(x) DEBUG(x) TOLERATED_ERROR(x)
#else
#define BLE_AD_INFO(x)
#define BLE_AD_ERROR(x)
#endif


/******************************************************************************/
void bleStopAdvertising(void)
{
    BLE_AD_INFO(("BLE : Stop AD\n"));
    ConnectionDmBleSetAdvertiseEnable(FALSE);
}


/*******************************************************************************
NAME    
    setupFlagsAdData

DESCRIPTION
    Helper function to setup the advertising data, flags (MUST be advertised)

PARAMETERS
    ad_data_ptr  Pointer to the advertising data.
    
RETURNS
    void
*/
static void setupFlagsAdData(built_adv_data_t * ad_data_ptr, adv_discoverable_mode_t mode)
{
    #define FLAGS_AD_DATA_LENGTH 0x3
    
    uint16 flags = 0;
    
    if (mode == adv_discoverable_mode_general)
        flags |= BLE_FLAGS_GENERAL_DISCOVERABLE_MODE;
    else if (mode == adv_discoverable_mode_limited)
        flags |= BLE_FLAGS_LIMITED_DISCOVERABLE_MODE;
    
    /* Setup the advertising flags required to advertise data */
    ad_data_ptr->size = FLAGS_AD_DATA_LENGTH;
    
    /* Setup the flags ad data */
    ad_data_ptr->ptr = malloc(ad_data_ptr->size);
    ad_data_ptr->ptr[0] = 0x02;
    ad_data_ptr->ptr[1] = ble_ad_type_flags;
    ad_data_ptr->ptr[2] = flags;
    
#ifdef DEBUG_BLE
    {
        uint16 counter;
        BLE_AD_INFO(("AD Data: flags = ["));
        for (counter=0; counter < ad_data_ptr->size; counter++)
        {
            BLE_AD_INFO(("%02x,", ad_data_ptr->ptr[counter]));
        }
        BLE_AD_INFO(("]\n"));
    }
#endif
}


/*******************************************************************************
NAME    
    setupServicesAdData

DESCRIPTION
    Helper function to setup the services advertisement data
    
PARAMETERS
    ad_data_ptr      Pointer to the advertising data.
    num_free_octets  Number of free bytes in the advertising data.

RETURNS
    void
*/
static void setupServicesAdData(built_adv_data_t * ad_data_ptr, uint16 num_free_octets)
{
    /* How many services have been defined? */
    uint16 counter;
    uint16 num_services;
    uint8 ad_tag;
    bool complete_list;
    bool battery_service_required = TRUE; /* TODO : Remove this, it should be configurable (probably via a parameter to this function) but for now always require the battery service */
    
    /* TODO : Calculate the number of services currently configured (for now, hardcode to 1 as it's only supporting battery service) */
    num_services = 1;
    
    /* Is there enough room to store the complete list of services defined for the device? */
    if ( (AD_DATA_HEADER_SIZE + (num_services*OCTETS_PER_SERVICE)) <= num_free_octets )  
    {
        /* Advertise complete list */
        ad_tag = ble_ad_type_complete_uuid16;
        complete_list = TRUE;
        
        /* Allocate enough memory to store the complete list of services defined for the device */
        ad_data_ptr->size = (AD_DATA_HEADER_SIZE + (num_services*OCTETS_PER_SERVICE));    
        ad_data_ptr->ptr = malloc(ad_data_ptr->size);
    }
    else
    {
        /* Advertise incomplete list (only advertise the first service based on alpabetical priority) */
        ad_tag = ble_ad_type_more_uuid16;
        complete_list = FALSE;
        
        /* Allocate enough memory to store the services defined for the device */
        ad_data_ptr->size = (AD_DATA_HEADER_SIZE + OCTETS_PER_SERVICE);
        ad_data_ptr->ptr = malloc(ad_data_ptr->size);
    }
    
    /* Setup AD data for the services */
    ad_data_ptr->ptr[0] = ad_data_ptr->size - 1;   /* Do not count the 'length' value; length is AD_TAG + AD_DATA */
    ad_data_ptr->ptr[1] = ad_tag;                  /* AD_TAG (either complete or incomplete list of uint16 UUIDs */
    
    /* Start adding services from this position in the ad_data memory */
    counter = AD_DATA_HEADER_SIZE;
    
    /* Depending on which services have been defined, build the AD data */
    BLE_AD_INFO(("AD Data: services num=%d, included=", num_services));  
    
    /* TODO : This will need updating, for now, use this to find out which services have been configured */
    if (battery_service_required)
    {
        BLE_AD_INFO(("Battery Service, "));
        ad_data_ptr->ptr[counter] = (GATT_SERVICE_UUID_BATTERY_SERVICE & 0xFF);
        counter++;
        ad_data_ptr->ptr[counter] = (GATT_SERVICE_UUID_BATTERY_SERVICE >> 8);
        counter++;
        if (!complete_list) return;
    } 
    
    BLE_AD_INFO(("\n"));
    
#ifdef DEBUG_BLE
    BLE_AD_INFO(("AD Data: services = ["));
    for (counter=0; counter < ad_data_ptr->size; counter++)
    {
        BLE_AD_INFO(("%02x,", ad_data_ptr->ptr[counter]));
    }
    BLE_AD_INFO(("]\n"));
#endif
}


/*******************************************************************************
NAME    
    setupLocalNameAdvertisingData

DESCRIPTION
    Helper function to setup advertising data to advertise the devices local 
    name used by remote devices scanning for BLE services
*/      
static void setupLocalNameAdvertisingData(built_adv_data_t * ad_data_ptr, uint16 size_local_name, const uint8 * local_name, uint16 ad_data_free_space)
{
    uint8 ad_tag, ad_name_length;
    
    /* Is there a local name to be advertised? If so, is there enough free space in AD Data to advertise it? */
    if (size_local_name == 0)
    {
        /* No local name to advertise */
        ad_data_ptr->size = 0;
        return;
    }
    else if (ad_data_free_space < AD_DATA_HEADER_SIZE + 1)
    {
        /* Not enough space in AD Data to advertise the local name (nor a shortened local name of just 1 char) */
        ad_data_ptr->size = 0;
        return;
    }
    else if ((AD_DATA_HEADER_SIZE + size_local_name) < ad_data_free_space)
    {
        /* Can advertise the complete local name */
        ad_tag = ble_ad_type_complete_local_name;
        ad_name_length = size_local_name;
    }
    else
    {
        /* Can advertise a shortened local name */
        ad_tag = ble_ad_type_shortened_local_name;
        ad_name_length = ad_data_free_space - 2;
    }
    
    /* Setup the local name advertising data */
    ad_data_ptr->size = ad_name_length + AD_DATA_HEADER_SIZE;
    ad_data_ptr->ptr = malloc(ad_data_ptr->size);
    ad_data_ptr->ptr[0] = ad_name_length + 1;
    ad_data_ptr->ptr[1] = ad_tag;
    memmove(&ad_data_ptr->ptr[2], local_name, ad_name_length);
    
#ifdef DEBUG_BLE
    {
        uint16 i;
        BLE_AD_INFO(("AD Data: local name=[%02x,%02x,", ad_data_ptr->ptr[0], ad_data_ptr->ptr[1]));
        for (i=2; i<ad_name_length + AD_DATA_HEADER_SIZE; i++)
        {
            BLE_AD_INFO(("%c,", ad_data_ptr->ptr[i]));
        }
        BLE_AD_INFO(("]\n"));
    }
#endif
}


/******************************************************************************/
void bleSetupAdvertisingData(uint16 size_local_name, const uint8 *local_name, adv_discoverable_mode_t mode)
{
    uint16 ad_data_index = 0;
    uint16 ad_data_num_free_octets = MAX_AD_DATA_SIZE_IN_OCTECTS;
    built_adv_data_t temp;
    uint8 * ad_data;
    
    /* Setup the flags advertising data */
    setupFlagsAdData(&temp, mode);
    
    /* Add the flags to advertising data & update indexes/counters */
    ad_data = malloc(temp.size);
    memmove(&ad_data[ad_data_index], temp.ptr, temp.size);
    ad_data_num_free_octets -= temp.size;   /* Update number of free octets in AD data */
    ad_data_index += temp.size;             /* Update the ad_data index to point to the next free octet in AD data */
    free(temp.ptr);                         /* De-allocate memory ready for next advertising data chunk */
    
    BLE_AD_INFO(("AD Data: flags added, free=%d\n", ad_data_num_free_octets));
    
    /* Setup the services advertising data */
    setupServicesAdData(&temp, ad_data_num_free_octets);
    ad_data = PanicUnlessRealloc(ad_data, (temp.size) + ad_data_index);
    memmove(&ad_data[ad_data_index], temp.ptr, temp.size);
    ad_data_num_free_octets -= temp.size;   /* Update number of free octets in AD data */
    ad_data_index += temp.size;             /* Update the ad_data index to point to the next free octet in AD data */
    free(temp.ptr);                         /* De-allocate memory ready for next advertising data chunk */
    
    BLE_AD_INFO(("AD Data: services added, free%d\n", ad_data_num_free_octets));
    
    /* Setup the local name advertising data */
    setupLocalNameAdvertisingData(&temp, size_local_name, local_name, ad_data_num_free_octets);
    
    /* Was there enough free space to add the local name to AD data? */
    if (temp.size)
    {
        ad_data = PanicUnlessRealloc(ad_data, temp.size + ad_data_index);
        memmove(&ad_data[ad_data_index], temp.ptr, temp.size);
        ad_data_num_free_octets -= temp.size;   /* Update number of free octets in AD data */
        ad_data_index += temp.size;             /* Update the ad data index so it now counts the total number of octets in the AD data */
        free(temp.ptr);                         /* Tidy up to avoid a memory leak */
        
        BLE_AD_INFO(("AD Data: local name added, free=%d\n", ad_data_num_free_octets));
    }
    
    /* Register AD data with the Connection library & Tidy up allocated memory*/
    ConnectionDmBleSetAdvertisingDataReq(ad_data_index, ad_data);
    free (ad_data);
}


/******************************************************************************/
void bleHandleSetAdvertisingData(CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T * cfm)
{
    ble_gap_event_t event;
    
    BLE_AD_INFO(("CL_DM_BLE_SET_ADVERTISING_DATA_CFM [%x]\n", cfm->status));
    
    if (cfm->status != success)
    {
        BLE_AD_ERROR(("  Failed!\n"));
    }

    /* Send GAP event after set of advertising data */
    event.id = ble_gap_event_set_advertising_complete;
    event.args = NULL;
    sinkBleGapEvent(event);
}


#endif /* GATT_ENABLED */

