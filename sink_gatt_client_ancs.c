/****************************************************************************
Copyright (C) Cambridge Silicon Radio Ltd. 2015

FILE NAME
    sink_gatt_client_ancs.c

DESCRIPTION
    Routines to handle the GATT ANCS Client.
*/

#include <stdlib.h>
#include <connection.h>

#include "sink_gatt_client_ancs.h"

#include "sink_debug.h"
#include "sink_gatt_client.h"
#include "sink_private.h"
#include "sink_gap_ad_types.h"



#ifdef GATT_ANCS_CLIENT

static const uint8 ancs_ble_advertising_filter[] = {0x05, 0x79, 0x31, 0xf4, 0xce, 0xb5, 0x99, 0x4e, 0x0f, 0xa4, 0x1e, 0x4b, 0x2d, 0x12, 0xd0, 0x00};

#ifdef DEBUG_GATT_ANCS_CLIENT
#define GATT_ANCS_CLIENT_DEBUG(x) DEBUG(x)
#else
#define GATT_ANCS_CLIENT_DEBUG(x) 
#endif


/*******************************************************************************
NAME
    gattAncsFindConnection
    
DESCRIPTION
    Finds a client connection associated with an Ancs instance.
    
PARAMETERS
    gancs    The Ancs client instance pointer
    
RETURNS    
    The client connection pointer associated with the Ancs instance. NULL if not found.   
    
*/
static gatt_client_connection_t *gattAncsFindConnection(const GANCS *gancs)
{
    uint16 index = 0;
    gatt_client_services_t *data = NULL;
    
    if (gancs == NULL)
    {
        return NULL;
    }
    
    for (index = 0; index < GATT_CLIENT.number_connections; index++)
    {
        data = gattClientGetServiceData(&GATT_CLIENT.connection[index]);
        if (data && (data->ancs == gancs))
        {
            return &GATT_CLIENT.connection[index];
        }
    }
    
    return NULL;
}


/*******************************************************************************
NAME
    gattAncsServiceInitialised
    
DESCRIPTION
    Called when the Ancs service is initialised.
    
PARAMETERS
    gancs    The Ancs client instance pointer
    
*/
static void gattAncsServiceInitialised(const GANCS *gancs, gatt_ancs_status_t status)
{
    gatt_client_connection_t *conn = gattAncsFindConnection(gancs);
 
    if (conn != NULL)
    {
        /* ANCS library was not able to successfully initialize 
         * Remove the ANCS client from client_data structure */
        if(status == gatt_ancs_status_failed)
        {
            gattClientRemoveDiscoveredService(conn, gatt_client_ancs);
        }
        /* Done with this ANCS client */
        gattClientDiscoveredServiceInitialised(conn);
    }
}

/*******************************************************************************
NAME
    gattAncsSetNSNotificationCfm
    
DESCRIPTION
    Handle the GATT_ANCS_SET_NS_NOTIFICATION_CFM message.
    
PARAMETERS
    cfm    The GATT_ANCS_SET_NS_NOTIFICATION_CFM message
    
*/
static void gattAncsSetNSNotificationCfm(const GATT_ANCS_SET_NS_NOTIFICATION_CFM_T *cfm)
{
    GATT_ANCS_CLIENT_DEBUG(("Ancs Set NS Notification Cfm status[%u]\n", cfm->status));
}

/*******************************************************************************
NAME
    gattAncsInitCfm
    
DESCRIPTION
    Handle the GATT_ANCS_INIT_CFM message.
    
PARAMETERS
    cfm    The GATT_ANCS_INIT_CFM message
    
*/
static void gattAncsInitCfm(const GATT_ANCS_INIT_CFM_T *cfm)
{
    GATT_ANCS_CLIENT_DEBUG(("Ancs Init Cfm status[%u]\n", cfm->status));

    /* Update the service as initialized */
    gattAncsServiceInitialised(cfm->ancs, cfm->status);

    if((cfm->status == gatt_ancs_status_success) && (gattAncsFindConnection(cfm->ancs) != NULL))
    {
        /*GattAncsSetNSNotificationEnableRequest(cfm->ancs, TRUE, ANCS_EMAIL_CATEGORY);*/
		GattAncsSetNSNotificationEnableRequest(cfm->ancs, TRUE, ANCS_INCOMING_CALL_CATEGORY|ANCS_SOCIAL_CATEGORY|ANCS_MISSED_CALL_CATEGORY);

		GattAncsSetDSNotificationEnableRequest(cfm->ancs, TRUE ); /*20160202*/
		
    }
}

	
static void gattAncsDSAttrNotificationInd( const GATT_ANCS_DS_ATTR_IND_T *ind )
{
     /*  Notification Attribute ID format
     * --------------------------------
     * |  Attr   |  Attr Value |  Attr Value   |
     * |  ID     |    length    |   and so on   |  
     * |------------------------------|
     * |  Byte1 |   Byte 2-3 |  Byte 4 -n    |   
     * --------------------------------
	*/

	 ANCS_DATA *pancs_data = (ANCS_DATA*)theSink.rundata->ancs_data;

	uint8 i;
	/*MYDEBUG(("----- value=\r\n"));*/



	pancs_data->notification_uid = ind->notification_uid;
	pancs_data->pktLen = ind->size_value; /*first packet len*/
	memcpy( &theSink.rundata->ancs_data[3], ind->value, ind->size_value );
	
	 
	switch( ind->value[0] )
	{
    case gatt_ancs_attr_app_id:       /* Attribute Id for getting Application Identifier */
	{
		MYDEBUG(("Attribute AppID: "));
		
		/*uint8 attribute_list[] = {gatt_ancs_app_attr_name};
		GattAncsGetAppAttributes(ind->ancs,  ind->value*/

		/*theSink.rundata->ancs_data[0]= ind->size_value;*/
		

    }
		break;
		
    case gatt_ancs_attr_title:            /* Attribute Id for getting Title */
	{
		MYDEBUG(("Attribute Title: "));
		if( ind->size_value+5 < 20 )
		{
             uint8 attribute_list[] = {gatt_ancs_attr_message_size};
	  		 GattAncsGetNotificationAttributes( ind->ancs, pancs_data->notification_uid, sizeof(attribute_list), attribute_list );			
		}
    }
		break;
			
    case gatt_ancs_attr_subtitle:         /* Attribute Id for getting SubTitle */
		MYDEBUG(("Attribute SubTitle: "));		
		break;
		
    case gatt_ancs_attr_message:          /* Attribute Id for getting Messages */
		MYDEBUG(("Attribute Message: "));
		break;
		
    case gatt_ancs_attr_message_size:     /* Attribute Id for getting Message Size */
	{
		uint8 attribute_list[3] = { gatt_ancs_attr_message };
		uint8 strSize[8] = {0};
		uint16 len = 0;

		MYDEBUG(("Attribute Message Size: "));
		
		memcpy( strSize, &ind->value[3], MAKEWORD(ind->value[1],ind->value[2]) );
		len = atoi((char*)strSize);
		attribute_list[1] = LOBYTE(len);
		attribute_list[2] = HIBYTE(len);

		GattAncsGetNotificationAttributes( ind->ancs, ind->notification_uid, sizeof(attribute_list), attribute_list );
    }
		break;
		
    case gatt_ancs_attr_date:             /* Attribute Id for getting Date */
		MYDEBUG(("Attribute Date: "));
		break;
		
    case gatt_ancs_attr_pos_action:       /* Attribute Id for getting postive action lable */
		break;
		
    case gatt_ancs_attr_neg_action:       /* Attribute Id for getting negative action lable */
    	break;
	}

	for( i=0; i<ind->size_value; i++ )
	{
		MYDEBUG(("%02X ", ind->value[i]));	
	}
	MYDEBUG(("\r\n"));
	
}

/*******************************************************************************
NAME
    gattAncsNSNotificationInd
    
DESCRIPTION
    Handle the GATT_ANCS_NS_IND message.
    
PARAMETERS
    ind    The GATT_ANCS_NS_IND message
    
*/
static void gattAncsNSNotificationInd(const GATT_ANCS_NS_IND_T *ind)
{
	/*uint8 attribute_list[] = {gatt_ancs_attr_title,gatt_ancs_attr_subtitle,gatt_ancs_attr_message,gatt_ancs_attr_message_size};*/

/******gatt_ancs_attr_date -> gatt_ancs_attr_title -> gatt_ancs_attr_message_size -> gatt_ancs_attr_message ******/
	
	uint8 attribute_list[] = {/*gatt_ancs_attr_app_id , gatt_ancs_attr_title, 0xFF,0x00,  gatt_ancs_attr_message_size,*/ gatt_ancs_attr_date};
	

	/*  Notification Source Data format
     * -------------------------------------------------------
     * |  Event  |  Event  |  Cat  |  Cat   |  Notification  |
     * |  ID     |  Flag   |  ID   |  Count |  UUID          |
     * |---------------------------------------------------- |
     * |   1B    |   1B    |   1B  |   1B   |   4B           |
     * -------------------------------------------------------
     */
    GATT_ANCS_CLIENT_DEBUG(("Recieved ANCS NS Notification for category[%d]\n", ind->category_id));
    
    /* Since the app has request notification for only email, no need to verify the category id */
    MessageCancelAll(&theSink.task, EventSysAncsEmailAlert);
    MessageSend(&theSink.task, EventSysAncsEmailAlert , 0);



	MYDEBUG(("event_id=%02X flag=%02X cat_id=%02X, cat_count=%02X, uid=%04lX\r\n", ind->event_id, ind->event_flag, ind->category_id, ind->category_count, ind->notification_uid));
	/*switch( ind->category_id )*/
	{
		GattAncsGetNotificationAttributes( ind->ancs, ind->notification_uid, sizeof(attribute_list), attribute_list );
	}
}


/****************************************************************************/
void sinkGattAncsClientSetupAdvertisingFilter(void)
{
    GATT_ANCS_CLIENT_DEBUG(("GattAncs: Add ANCS scan filter\n"));
    ConnectionBleAddAdvertisingReportFilter(AD_TYPE_SERVICE_UUID_128BIT_LIST, sizeof(ancs_ble_advertising_filter), sizeof(ancs_ble_advertising_filter), ancs_ble_advertising_filter);
}
    
/****************************************************************************/
bool sinkGattAncsClientAddService(uint16 cid, uint16 start, uint16 end)
{
    GANCS *gancs = NULL;
    gatt_client_services_t *client_services = NULL;
    gatt_client_connection_t *connection = gattClientFindByCid(cid);
    uint16 *service = gattClientAddService(connection, sizeof(GANCS));
    
    if (service)
    {
        client_services = gattClientGetServiceData(connection);
        client_services->ancs = (GANCS *)service;
        gancs = client_services->ancs;
        if (GattAncsInit(gancs, sinkGetBleTask(), cid, start, end) == gatt_ancs_status_initiating)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/****************************************************************************/
void sinkGattAncsClientRemoveService(GANCS *gancs)
{
    GattAncsDestroy(gancs);
}


/******************************************************************************/
void sinkGattAncsClientMsgHandler (Task task, MessageId id, Message message)
{
    GATT_ANCS_CLIENT_DEBUG(("sinkGattAncsClientMsgHandler[%x]\n", id));

	/*MYDEBUG((".............[%x]\r\n", id));*/
	
    switch(id)
    {
		case GATT_ANCS_DS_MORE_IND:
		{
			/*uint8 len = theSink.rundata->ancs_data[0];*/
			GATT_ANCS_DS_MORE_IND_T *ind = (GATT_ANCS_DS_MORE_IND_T*)message;
			ANCS_DATA *pancs_data = (ANCS_DATA*)theSink.rundata->ancs_data;

			uint8 i;
			uint8 value_size;
								
						
			/*memcpy( &theSink.rundata->ancs_data[3+pancs_data->pktLen], ind->value, ind->size_value );*/
			pancs_data->pktLen += ind->size_value;
			

 			value_size = MAKEWORD(pancs_data->value_size[0], pancs_data->value_size[1]);
 
			/*MYDEBUG((".......pktlen=%d value_size=%d\r\n", pancs_data->pktLen, value_size));*/

                
				
                for(i=0; i<ind->size_value; i++ )
                {
                    MYDEBUG(("%02X ", ind->value[i] ));
                }
				MYDEBUG(("\r\n"));
				
			if( pancs_data->pktLen == value_size+3 ) /*3=value_size(2) + */
			{
                /*uint8 i;
				
                for(i=0; i<value_size; i++ )
                {
                    MYDEBUG(("%02X ", pancs_data->value[i] ));
                }*/

				switch( pancs_data->attrID )
				{
                  case gatt_ancs_attr_app_id:       /* Attribute Id for getting Application Identifier */
                    {
#if 1
                     uint8 app_attr_list[] = {gatt_ancs_app_attr_name};

										 
                     pancs_data->value[value_size++] = 0x00; /*end of NULL */

                     GattAncsGetAppAttributes( ind->ancs, value_size, pancs_data->value, sizeof(app_attr_list),app_attr_list );					

					 
#else
                        

					i=0;
					pancs_data->value[i++] = 0x01;
					pancs_data->value[i++] = 0x63;
					pancs_data->value[i++] = 0x6F;
					pancs_data->value[i++] = 0x6D;
					pancs_data->value[i++] = 0x2E;
					pancs_data->value[i++] = 0x61;
					pancs_data->value[i++] = 0x70;
					pancs_data->value[i++] = 0x70;
					pancs_data->value[i++] = 0x6C;
					pancs_data->value[i++] = 0x65;
					pancs_data->value[i++] = 0x2E;
					pancs_data->value[i++] = 0x4D;
					pancs_data->value[i++] = 0x6F;
					pancs_data->value[i++] = 0x62;
					pancs_data->value[i++] = 0x69;
					pancs_data->value[i++] = 0x6C;
					pancs_data->value[i++] = 0x65;
					pancs_data->value[i++] = 0x53;
					pancs_data->value[i++] = 0x4D;
					pancs_data->value[i++] = 0x53;
					pancs_data->value[i++] = 0x00;
					pancs_data->value[i++] = 0x00;

					 GattManagerWriteCharacteristicValue((Task)&ind->ancs->lib_task, 
					 										ind->ancs->control_point, 
					 										22, pancs_data->value);                    

#endif
                    }
                    break;
                 case gatt_ancs_attr_title:
                    {
                     uint8 attribute_list[] = {gatt_ancs_attr_message_size};

			  		 GattAncsGetNotificationAttributes( ind->ancs, pancs_data->notification_uid, sizeof(attribute_list), attribute_list );
                    }
                    break;
				 case gatt_ancs_attr_date:
				 	{
                     uint8 attribute_list[] = {gatt_ancs_attr_title,0x14,0x00};
			  		 GattAncsGetNotificationAttributes( ind->ancs, pancs_data->notification_uid, sizeof(attribute_list), attribute_list );
				 	}
				 	break;
				 default:
                    break;
			    }
            }
		}
		break;

		case GATT_ANCS_DS_APP_ATTR_IND:
		{
			GATT_ANCS_DS_APP_ATTR_IND_T *ind = (GATT_ANCS_DS_APP_ATTR_IND_T*)message;
			uint8 i=0;
			MYDEBUG(("app name: "));
			for( i=0; i<ind->size_value; i++ )
			{
				MYDEBUG(("%02X ", ind->value[i] ));
			}
			MYDEBUG(("\r\n"));
		}
		break;
		
        case GATT_ANCS_DS_ATTR_IND:
        {
			gattAncsDSAttrNotificationInd( (GATT_ANCS_DS_ATTR_IND_T*)message );
		
        }
        break;
        case GATT_ANCS_SET_NS_NOTIFICATION_CFM:
        {
            gattAncsSetNSNotificationCfm((GATT_ANCS_SET_NS_NOTIFICATION_CFM_T*)message);
        }
        break;
        case GATT_ANCS_INIT_CFM:
        {
            gattAncsInitCfm((GATT_ANCS_INIT_CFM_T*)message);
        }
        break;
		case GATT_ANCS_WRITE_CP_CFM:
		{/*
			GATT_ANCS_WRITE_CP_CFM_T *cfm = (GATT_ANCS_WRITE_CP_CFM_T*)message;
			MYDEBUG(("cmd_id=%x, status=%x\r\n", cfm->command_id, cfm->status));*/
			
			/*GattAncsGetAppAttributes(*/
		}
		break;
        case GATT_ANCS_NS_IND:
        {
            gattAncsNSNotificationInd((GATT_ANCS_NS_IND_T*)message);
        }
        break;
        default:
        {
            GATT_ANCS_CLIENT_DEBUG(("Unhandled ANCS msg[%x]\n", id));
        }
        break;
    }
}


#endif /* GATT_ANCS_CLIENT */

