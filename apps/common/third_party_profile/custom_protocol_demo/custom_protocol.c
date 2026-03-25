#include "sdk_config.h"
#include "app_msg.h"
// #include "earphone.h"
#include "bt_tws.h"
#include "app_main.h"
#include "btstack/avctp_user.h"
#include "multi_protocol_main.h"
#include "custom_protocol.h"

//add by joe
#if ((defined TCFG_AUDIO_SPATIAL_EFFECT_ENABLE) && TCFG_AUDIO_SPATIAL_EFFECT_ENABLE)
#include "spatial_effects_process.h"
#include "spatial_effect.h"
#endif
#include "low_latency.h"

#if (THIRD_PARTY_PROTOCOLS_SEL & CUSTOM_DEMO_EN)

void *custom_demo_ble_hdl = NULL;
void *custom_demo_spp_hdl = NULL;

#if ATT_OVER_EDR_DEMO_EN
void *att_over_edr_hdl = NULL;
#define EDR_ATT_HDL_UUID \
	(((u8)('E' + 'D' + 'R') << (1 * 8)) | \
	 ((u8)('A' + 'T' + 'T') << (0 * 8)))
#endif

/*************************************************
                  BLE 相关内容
*************************************************/

const uint8_t custom_demo_profile_data[] = {
    //////////////////////////////////////////////////////
    //
    // 0x0001 PRIMARY_SERVICE  1800
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x28, 0x00, 0x18,

    /* CHARACTERISTIC,  2a00, READ | WRITE | DYNAMIC, */
    // 0x0002 CHARACTERISTIC 2a00 READ | WRITE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x02, 0x00, 0x03, 0x28, 0x0a, 0x03, 0x00, 0x00, 0x2a,
    // 0x0003 VALUE 2a00 READ | WRITE | DYNAMIC
    0x08, 0x00, 0x0a, 0x01, 0x03, 0x00, 0x00, 0x2a,

    //////////////////////////////////////////////////////
    //
    // 0x0004 PRIMARY_SERVICE  ae00
    //
    //////////////////////////////////////////////////////
    0x0a, 0x00, 0x02, 0x00, 0x04, 0x00, 0x00, 0x28, 0x00, 0xae,

    /* CHARACTERISTIC,  ae01, WRITE_WITHOUT_RESPONSE | DYNAMIC, */
    // 0x0005 CHARACTERISTIC ae01 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x0d, 0x00, 0x02, 0x00, 0x05, 0x00, 0x03, 0x28, 0x04, 0x06, 0x00, 0x01, 0xae,
    // 0x0006 VALUE ae01 WRITE_WITHOUT_RESPONSE | DYNAMIC
    0x08, 0x00, 0x04, 0x01, 0x06, 0x00, 0x01, 0xae,

    /* CHARACTERISTIC,  ae02, NOTIFY, */
    // 0x0007 CHARACTERISTIC ae02 NOTIFY
    0x0d, 0x00, 0x02, 0x00, 0x07, 0x00, 0x03, 0x28, 0x10, 0x08, 0x00, 0x02, 0xae,
    // 0x0008 VALUE ae02 NOTIFY
    0x08, 0x00, 0x10, 0x00, 0x08, 0x00, 0x02, 0xae,
    // 0x0009 CLIENT_CHARACTERISTIC_CONFIGURATION
    0x0a, 0x00, 0x0a, 0x01, 0x09, 0x00, 0x02, 0x29, 0x00, 0x00,

    // END
    0x00, 0x00,
};

//
// characteristics <--> handles
//
#define ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE 0x0003
#define ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE 0x0006
#define ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE 0x0008
#define ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE 0x0009

static u16 custom_adv_interval_min = 150;

static void custom_cbk_packet_handler(void *hdl, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    u16 con_handle;
    // printf("cbk packet_type:0x%x, packet[0]:0x%x, packet[2]:0x%x", packet_type, packet[0], packet[2]);
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            printf("ATT_EVENT_CAN_SEND_NOW");
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                con_handle = little_endian_read_16(packet, 4);
                printf("HCI_SUBEVENT_LE_CONNECTION_COMPLETE: %0x", con_handle);
                // reverse_bd_addr(&packet[8], addr);
                put_buf(&packet[8], 6);
                break;
            default:
                break;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("HCI_EVENT_DISCONNECTION_COMPLETE: %0x", packet[5]);
            custom_demo_adv_enable(1);
            break;
        default:
            break;
        }
        break;
    }
    return;
}

static uint16_t custom_att_read_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    uint16_t  att_value_len = 0;
    uint16_t handle = att_handle;
    printf("<-------------read_callback, handle= 0x%04x,buffer= %08x", handle, (u32)buffer);
    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        const char *gap_name = bt_get_local_name();
        att_value_len = strlen(gap_name);
        if ((offset >= att_value_len) || (offset + buffer_size) > att_value_len) {
            break;
        }
        if (buffer) {
            memcpy(buffer, &gap_name[offset], buffer_size);
            att_value_len = buffer_size;
            printf("\n------read gap_name: %s", gap_name);
        }
        break;
    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
        if (buffer) {
            buffer[0] = multi_att_get_ccc_config(connection_handle, handle);
            buffer[1] = 0;
        }
        att_value_len = 2;
        break;
    default:
        break;
    }
    printf("att_value_len= %d", att_value_len);
    return att_value_len;
    return 0;
}

static int custom_att_write_callback(void *hdl, hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{
    int result = 0;
    u16 tmp16;
    u16 handle = att_handle;
    printf("<-------------write_callback, handle= 0x%04x,size = %d", handle, buffer_size);
    switch (handle) {
    case ATT_CHARACTERISTIC_2a00_01_VALUE_HANDLE:
        break;
    case ATT_CHARACTERISTIC_ae01_01_VALUE_HANDLE:
        printf("rx(%d):\n", buffer_size);
        put_buf(buffer, buffer_size);
        // test
        custom_demo_ble_send(buffer, buffer_size);
        break;
    case ATT_CHARACTERISTIC_ae02_01_CLIENT_CONFIGURATION_HANDLE:
        printf("\nwrite ccc:%04x, %02x\n", handle, buffer[0]);
        multi_att_set_ccc_config(connection_handle, handle, buffer[0]);
        break;
    default:
        break;
    }
    return 0;
}

static u8 custom_fill_adv_data(u8 *adv_data)
{
    u8 offset = 0;
    const char *name_p = bt_get_local_name();
    int name_len = strlen(name_p);
    offset += make_eir_packet_data(&adv_data[offset], offset, HCI_EIR_DATATYPE_COMPLETE_LOCAL_NAME, (void *)name_p, name_len);
    if (offset > ADV_RSP_PACKET_MAX) {
        puts("***rsp_data overflow!!!!!!\n");
        return 0;
    }
    return offset;
}

static u8 custom_fill_rsp_data(u8 *rsp_data)
{
    return 0;
}


int custom_demo_adv_enable(u8 enable)
{
    uint8_t adv_type = ADV_IND;
    uint8_t adv_channel = ADV_CHANNEL_ALL;
    uint8_t advData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t rspData[ADV_RSP_PACKET_MAX] = {0};
    uint8_t len = 0;

    if (enable == app_ble_adv_state_get(custom_demo_ble_hdl)) {
        return 0;
    }
    if (enable) {
        app_ble_set_adv_param(custom_demo_ble_hdl, custom_adv_interval_min, adv_type, adv_channel);
        len = custom_fill_adv_data(advData);
        if (len) {
            app_ble_adv_data_set(custom_demo_ble_hdl, advData, len);
        }
        len = custom_fill_rsp_data(rspData);
        if (len) {
            app_ble_rsp_data_set(custom_demo_ble_hdl, rspData, len);
        }
    }
    app_ble_adv_enable(custom_demo_ble_hdl, enable);
    return 0;
}

int custom_demo_ble_send(u8 *data, u32 len)
{
    int ret = 0;
    int i;
    printf("custom_demo_ble_send len = %d", len);
    put_buf(data, len);
    ret = app_ble_att_send_data(custom_demo_ble_hdl, ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    if (ret) {
        printf("send fail\n");
    }
    return ret;
}

#if ATT_OVER_EDR_DEMO_EN
int custom_demo_gatt_over_edr_send(u8 *data, u32 len)
{
    int ret = 0;
    int i;
    printf("custom_demo_ble_send len = %d", len);
    put_buf(data, len);
    ret = app_ble_att_send_data(att_over_edr_hdl, ATT_CHARACTERISTIC_ae02_01_VALUE_HANDLE, data, len, ATT_OP_AUTO_READ_CCC);
    if (ret) {
        printf("send fail\n");
    }
    return ret;
}
#endif
/*************************************************
                  BLE 相关内容 end
*************************************************/

/*************************************************
                  SPP 相关内容
*************************************************/
#if TCFG_USER_TWS_ENABLE
////////////////////////////////////////////////////////////////
#define TWS_SYNC_WTONE_VOL_UUID         0x49687F11
static void set_sys_wtone_volume(void)
{
    printf("Joe:set_sys_wtone_volume\n");

    app_var.music_volume =double_cfg.toneVolumeLevel;
	extern void app_audio_set_volume(u8 state, s16 volume, u8 fade);
	app_audio_set_volume(APP_AUDIO_STATE_WTONE, app_var.music_volume, 1);
	
	syscfg_write(CFG_KEY_DOUBLE_CFG_ID, &double_cfg, sizeof(double_cfg_t));
}
static void tws_sync_call_wtone_vol_set(int priv, int err)
{
    set_sys_wtone_volume();
}
TWS_SYNC_CALL_REGISTER(tws_wtone_vol_entry) = {
    .uuid = TWS_SYNC_WTONE_VOL_UUID,
    .task_name = "app_core",
    .func = tws_sync_call_wtone_vol_set,
};
void tws_sync_wtone_vol(void)
{
    int err = tws_api_sync_call_by_uuid(TWS_SYNC_WTONE_VOL_UUID, 0, 400);
    if (err < 0) {
        set_sys_wtone_volume();
    }
}
////////////////////////////////////////////////////////////////
static void set_sys_poweroff_reset(void)
{
    printf("Joe:set_sys_poweroff_reset\n");
    
	int msg[3];
	msg[0] = (int)sys_enter_soft_poweroff;
	msg[1] = 1;
	msg[2] = POWEROFF_RESET;
	os_taskq_post_type("app_core", Q_CALLBACK, 3, msg);
}
#define TWS_SYNC_RESET_UUID         0x49687FF0
static void tws_sync_call_poweroff_reset(int priv, int err)
{
    set_sys_poweroff_reset();
}
TWS_SYNC_CALL_REGISTER(tws_poweroff_reset_entry) = {
    .uuid = TWS_SYNC_RESET_UUID,
    .task_name = "app_core",
    .func = tws_sync_call_poweroff_reset,
};
static void sys_poweroff_reset_deal(void *priv)
{
    int err = tws_api_sync_call_by_uuid(TWS_SYNC_RESET_UUID, 0, 400);
    if (err < 0) {
        set_sys_poweroff_reset();
    }
}
void tws_sync_poweroff_reset(void)
{
    sys_poweroff_reset_deal(NULL);
}
////////////////////////////////////////////////////////////////
#define TWS_FUNC_ID_DOUBLE_CFG_SYNC    TWS_FUNC_ID('D', 'C', 'F', 'G')
#if 0
static void rx_double_cfg_info(u8 *data, int len)
{
	printf("Joe:tws_sync_double_cfg_func %d\n", data[1]);

	double_cfg.spatialMode =data[0];
	double_cfg.codecState =data[1];
	
	syscfg_write(CFG_KEY_DOUBLE_CFG_ID, &double_cfg, sizeof(double_cfg_t));
	free(data);
}

static void tws_sync_double_cfg_func(void *_data, u16 len, bool rx)
{

	if (rx) {
		u8 *data = malloc(len);
		memcpy(data, _data, len);
		int msg[4] = { (int)rx_double_cfg_info, 2, (int)data, len};
		int err = os_taskq_post_type("app_core", Q_CALLBACK, 4, msg);
		if (err != OS_NO_ERR) {
			free(data);
		}
	}
}
#endif


static void tws_sync_double_cfg_func(void *_data, u16 len, bool rx)
{
	if (rx) {
		memcpy(&double_cfg, _data, sizeof(double_cfg_t));
		
		printf("Joe:Check_spatialMode  %d\n", double_cfg.spatialMode);
		printf("Joe:Check_codecState %d\n", double_cfg.codecState);
		
		syscfg_write(CFG_KEY_DOUBLE_CFG_ID, &double_cfg, sizeof(double_cfg_t));
	}

}
REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
	.func_id = TWS_FUNC_ID_DOUBLE_CFG_SYNC,
	.func	 = tws_sync_double_cfg_func,
};
void tws_sync_double_cfg(void)
{
    if(tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }

	tws_api_send_data_to_slave(&double_cfg, sizeof(double_cfg_t), TWS_FUNC_ID_DOUBLE_CFG_SYNC);
}
#endif
bool app_cmd(u8 *data, u32 len)
{
	if((data[0]== 'a')&&(data[1]== 't')&&(data[2]== '+'))
	{
		 return 0;
	}
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
	if(len<2)
	return 0;

	return 1;
}
void handle_at_cmd(u8 *data, u32 len)
{ 
    if((data[3]== 'g')&&(data[4]== 'e')&&(data[5]== 't')&&(data[6]== 'v')&&(data[7]== 'i'))
    {
	   if(bt_tws_get_local_channel()=='L')
	       custom_demo_spp_send((u8 *)"L:\r\n",4);   //0x52
	   else
	       custom_demo_spp_send((u8 *)"R:\r\n",4);   //0x4C

		char Joe_VEI[20];
		sprintf(Joe_VEI,"version:%04x\r\n",MMI_CC_FIRMWARE_VERSION_BUILD);
		custom_demo_spp_send((u8 *)Joe_VEI,strlen(Joe_VEI));
		
		if(tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED)
		{
	      custom_demo_spp_send((u8 *)"TWS CONNECTED\r\n",15);
		}
		else
		{
	      custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18);
		}
    }
    else if((data[3]== 's')&&(data[4]== 'h')&&(data[5]== 'i')&&(data[6]== 'p')&&(data[7]== 'p'))
    {
#if MMI_CC_SHIPPING_MODE
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);

		gpio_set_mode(PORTB, PORT_PIN_0, PORT_OUTPUT_LOW);
		BOAT_PIN_OUT_LEVEL(TRUE);

		custom_demo_spp_send((u8 *)"Enter shipping OK\r\n",19);	
#endif
    }
    else if((data[3]== 'p')&&(data[4]== 'e')&&(data[5]== 'e')&&(data[6]== 'r')&&(data[7]== 'A'))
    {
		 u8 j = 0;
         u8 peer_addr[6]={0x12,0x34,0x56,0x78,0x90,0xAB};
		 u8 addr_p[6]={0x00,0x00,0x00,0x00,0x00,0x00};

         syscfg_read(CFG_TWS_REMOTE_ADDR, peer_addr, sizeof(peer_addr));
         memcpy(&addr_p[0], &peer_addr, 6);
		 for (j = 0; j < 6; j++) {
               peer_addr[j] =addr_p[5-j];  
	     }

		 //u8 k;
		 //char s[]=0;
		 //for(k=11; k>=0; k--; peer_addr[0] >>=4)
		 //{
			//if((peer_addr[0] &0x0F) <=9)
			//{
			  //s[k] =(peer_addr[0] &0x0F)+'0';
			//}
			//else
			//{
			  //s[k] =(peer_addr[0] &0x0F)+'A'-0x0A;
			//}
		 //}

	     custom_demo_spp_send(peer_addr,6);
         custom_demo_spp_send((u8 *)"+peerAddr\r\n",14);
    }
    else if((data[3]== 'g')&&(data[4]== 'a')&&(data[5]== 'm')&&(data[6]== 'o')&&(data[7]== 'n'))
    {
         if(bt_tws_get_local_channel()=='L')
             custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
         else
             custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C
  
         if(bt_get_low_latency_mode() == 0)
         {
            bt_enter_low_latency_mode();
            custom_demo_spp_send((u8 *)"GameModeOn OK\r\n",15);
         }
    }
    else if((data[3]== 'g')&&(data[4]== 'a')&&(data[5]== 'm')&&(data[6]== 'o')&&(data[7]== 'f')&&(data[8]== 'f'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

        if(bt_get_low_latency_mode() != 0)
        {
            bt_exit_low_latency_mode();
            custom_demo_spp_send((u8 *)"GameModeOff OK\r\n",15);
        }
    }
    else if((data[3]== 's')&&(data[4]== 'p')&&(data[5]== 'a')&&(data[6]== 't')&&(data[7]== 'i')&&(data[8]== 'a')&&(data[9]== 'l'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18);
           //return;
		}

         if(get_a2dp_spatial_audio_mode()==0)
         {
             //audio_spatial_effects_mode_switch(1);
             custom_demo_spp_send((u8 *)"Spatial On\r\n",11);
         }
         else
         {
         	 //audio_spatial_effects_mode_switch(0);
             custom_demo_spp_send((u8 *)"Spatial Off\r\n",12);
         }

         extern void bt_tws_sync_spatial_effect_switch(void);
         bt_tws_sync_spatial_effect_switch(); 
    }
    else if((data[3]== 'g')&&(data[4]== 'e')&&(data[5]== 't')&&(data[6]== 's')&&(data[7]== 'p')&&(data[8]== 'a')&&(data[9]== 't')
             &&(data[10]== 'i')&&(data[11]== 'a')&&(data[12]== 'l'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);

         //没有A2DP的播放，此状态不会被更新
         if(get_a2dp_spatial_audio_mode() !=0)
         {
             custom_demo_spp_send((u8 *)"SpatialIsOn\r\n",13);
         }
         else
         {
             custom_demo_spp_send((u8 *)"SpatialIsOff\r\n",14);
         }  
    }
    else if((data[3]== 'o')&&(data[4]== 'p')&&(data[5]== 'e')&&(data[6]== 'n')&&(data[7]== 'L')&&(data[8]== 'D')&&(data[9]== 'A')&&(data[10]== 'C'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18);
           //return;
		}
		if(double_cfg.codecState ==0x00)
		{
           double_cfg.codecState =0x01;
           custom_demo_spp_send((u8 *)"Is LDAC close sysReboot!!\r\n",29);
		}
		else
		{
           double_cfg.codecState =0x00;
           custom_demo_spp_send((u8 *)"Is LDAC open sysReboot!!\r\n",29);
		}

        int ret =0x00;
        ret =syscfg_write(CFG_KEY_DOUBLE_CFG_ID, &double_cfg, sizeof(double_cfg_t));
        if(ret ==1)
        {
          printf("Joe:Write_config_success \n");
        }
        else
        {
          printf("Joe:Write_config_fail \n");
        }

        tws_sync_double_cfg();
        tws_sync_poweroff_reset();
    }
    else if((data[3]== 'v')&&(data[4]== 'p')&&(data[5]== 'v')&&(data[6]== 'o')&&(data[7]== 'l')&&(data[8]== '+'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18);
           //return;
		}

        extern s16 app_audio_get_volume(u8 state);
        double_cfg.toneVolumeLevel =app_audio_get_volume(APP_AUDIO_STATE_WTONE);
		printf("Joe:get_current_wtone_volume %d\n", double_cfg.toneVolumeLevel);

        double_cfg.toneVolumeLevel +=1;
        if(double_cfg.toneVolumeLevel >=16)
           double_cfg.toneVolumeLevel =16;
           
        tws_sync_double_cfg();
        tws_sync_wtone_vol();

		custom_demo_spp_send((u8 *)"wtone_volume_++\r\n",16);
    }
    else if((data[3]== 'v')&&(data[4]== 'p')&&(data[5]== 'v')&&(data[6]== 'o')&&(data[7]== 'l')&&(data[8]== '-'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18);
           //return;
		}

        extern s16 app_audio_get_volume(u8 state);
        double_cfg.toneVolumeLevel =app_audio_get_volume(APP_AUDIO_STATE_WTONE);
		printf("Joe:get_current_wtone_volume %d\n", double_cfg.toneVolumeLevel);

        double_cfg.toneVolumeLevel -=1;
        if(double_cfg.toneVolumeLevel <=0)
           double_cfg.toneVolumeLevel =1;
           
        tws_sync_double_cfg();
        tws_sync_wtone_vol();

		custom_demo_spp_send((u8 *)"wtone_volume_--\r\n",16);
    }
    else if((data[3]== 'v')&&(data[4]== 'p')&&(data[5]== 'o')&&(data[6]== 'n'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18);
           //return;
		}

        double_cfg.toneVolumeLevel =7;
           
        tws_sync_double_cfg();
        tws_sync_wtone_vol();

		custom_demo_spp_send((u8 *)"tone_is_open\r\n",12);
    }
    else if((data[3]== 'v')&&(data[4]== 'p')&&(data[5]== 'o')&&(data[6]== 'f')&&(data[7]== 'f'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18); 
           //return;
		}

        double_cfg.toneVolumeLevel =0;
           
        tws_sync_double_cfg();
        tws_sync_wtone_vol();

		custom_demo_spp_send((u8 *)"tone_is_close\r\n",13);
    }
    else if((data[3]== 'g')&&(data[4]== 'e')&&(data[5]== 't')&&(data[6]== 'l')&&(data[7]== 'i')&&(data[8]== 'n')&&(data[9]== 'k')
             &&(data[10]== 'k')&&(data[11]== 'e')&&(data[12]== 'y'))
    {
		if(bt_tws_get_local_channel()=='L')
			custom_demo_spp_send((u8 *)"L:\r\n",4);    // 0x52
		else
			custom_demo_spp_send((u8 *)"R:\r\n",4);    // 0x4C

		if(tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED)
		{
           custom_demo_spp_send((u8 *)"TWS DISCONNECTED\r\n",18); 
           //return;
		}
#if MMI_CC_GET_DEVICE_LINKKEY
		extern u8 *get_last_device_connect_linkkey(u16 *len);
		u8 *get_linkkey;
		u16 get_linkkey_len =16;

		get_linkkey =get_last_device_connect_linkkey(&get_linkkey_len);
		if(get_linkkey)
		{
           printf("Joe:get_linkkey======= \n");
           put_buf(get_linkkey, get_linkkey_len);

           custom_demo_spp_send((u8 *)"get_linkkey_ok\r\n",16);
		}
#endif
    }



}


static void custom_spp_state_callback(void *hdl, void *remote_addr, u8 state)
{
    int i;
    int bond_flag = 0;
    switch (state) {
    case SPP_USER_ST_CONNECT:
        printf("custom spp connect#########\n");
        // 将 custom_demo_spp_hdl 绑定到连接上的设备地址，否则后续会收到所有已连接设备地址的事件和数据
        app_spp_set_filter_remote_addr(custom_demo_spp_hdl, remote_addr);
        break;
    case SPP_USER_ST_DISCONN:
        printf("custom spp disconnect#########\n");
        app_spp_clean_filter_remote_addr(custom_demo_spp_hdl);
        break;
    };
}

static void custom_spp_recieve_callback(void *hdl, void *remote_addr, u8 *buf, u16 len)
{
    printf("custom_spp_recieve_callback len=%d\n", len);
    put_buf(buf, len);

#if MMI_CC_SPP_BLE_ENABLE
#if TCFG_USER_TWS_ENABLE
	// TWS 从机不需要回复
	printf("Joe: custom_spp_send_data role:%d\n", tws_api_get_role());
	if (tws_api_get_role() == TWS_ROLE_SLAVE) {
		printf("Joe: custom_spp slave don't send\n");
		return ;
	}
#endif
	printf("Joe: spp_data_len:%d\r\n",len);
	if(!app_cmd(buf,len))
	{
	   handle_at_cmd(buf, len);//Joe_add

	   return ;
	}

#endif


    // test send
    custom_demo_spp_send(buf, len);
}

int custom_demo_spp_send(u8 *data, u32 len)
{
    return app_spp_data_send(custom_demo_spp_hdl, data, len);
}

/*************************************************
                  SPP 相关内容 end
*************************************************/

#define CUSTOM_BLE_HDL_UUID \
    (((u8)('C' + 'U' + 'S') << (3 * 8)) | \
     ((u8)('T' + 'O' + 'M') << (2 * 8)) | \
     ((u8)('B' + 'L' + 'E') << (1 * 8)) | \
     ((u8)('H' + 'D' + 'L') << (0 * 8)))

#define CUSTOM_SPP_HDL_UUID \
    (((u8)('C' + 'U' + 'S') << (3 * 8)) | \
     ((u8)('T' + 'O' + 'M') << (2 * 8)) | \
     ((u8)('S' + 'P' + 'P') << (1 * 8)) | \
     ((u8)('H' + 'D' + 'L') << (0 * 8)))

void custom_demo_all_init(void)
{
    printf("custom_demo_all_init\n");
    const uint8_t *edr_addr = bt_get_mac_addr();
    printf("edr addr:");
    put_buf((uint8_t *)edr_addr, 6);

    // BLE init
    if (custom_demo_ble_hdl == NULL) {
        custom_demo_ble_hdl = app_ble_hdl_alloc();
        if (custom_demo_ble_hdl == NULL) {
            printf("custom_demo_ble_hdl alloc err !\n");
            return;
        }
        app_ble_hdl_uuid_set(custom_demo_ble_hdl, CUSTOM_BLE_HDL_UUID);
        app_ble_set_mac_addr(custom_demo_ble_hdl, (void *)edr_addr);
        app_ble_profile_set(custom_demo_ble_hdl, custom_demo_profile_data);
        app_ble_att_read_callback_register(custom_demo_ble_hdl, custom_att_read_callback);
        app_ble_att_write_callback_register(custom_demo_ble_hdl, custom_att_write_callback);
        app_ble_att_server_packet_handler_register(custom_demo_ble_hdl, custom_cbk_packet_handler);
        app_ble_hci_event_callback_register(custom_demo_ble_hdl, custom_cbk_packet_handler);
        app_ble_l2cap_packet_handler_register(custom_demo_ble_hdl, custom_cbk_packet_handler);

        custom_demo_adv_enable(1);
    }
    // BLE init end

    // SPP init
    if (custom_demo_spp_hdl == NULL) {
        custom_demo_spp_hdl = app_spp_hdl_alloc(0x0);
        if (custom_demo_spp_hdl == NULL) {
            printf("custom_demo_spp_hdl alloc err !\n");
            return;
        }
        app_spp_hdl_uuid_set(custom_demo_spp_hdl, CUSTOM_SPP_HDL_UUID);
        app_spp_recieve_callback_register(custom_demo_spp_hdl, custom_spp_recieve_callback);
        app_spp_state_callback_register(custom_demo_spp_hdl, custom_spp_state_callback);
        app_spp_wakeup_callback_register(custom_demo_spp_hdl, NULL);
    }
    // SPP init end

#if ATT_OVER_EDR_DEMO_EN
    // att_over_edr init
    if (att_over_edr_hdl == NULL) {
        att_over_edr_hdl = app_ble_hdl_alloc();
        if (att_over_edr_hdl == NULL) {
            printf("att_over_edr_hdl alloc err !\n");
            return;
        }
        app_ble_profile_set(att_over_edr_hdl, custom_demo_profile_data);
        app_ble_adv_address_type_set(att_over_edr_hdl, 0);
        app_ble_gatt_over_edr_connect_type_set(att_over_edr_hdl, 1);
        app_ble_hdl_uuid_set(att_over_edr_hdl, EDR_ATT_HDL_UUID);
        app_ble_att_read_callback_register(att_over_edr_hdl, custom_att_read_callback);
        app_ble_att_write_callback_register(att_over_edr_hdl, custom_att_write_callback);
        app_ble_att_server_packet_handler_register(att_over_edr_hdl, custom_cbk_packet_handler);
        app_ble_hci_event_callback_register(att_over_edr_hdl, custom_cbk_packet_handler);
        app_ble_l2cap_packet_handler_register(att_over_edr_hdl, custom_cbk_packet_handler);
    }
#endif
}

void custom_demo_ble_disconnect(void)
{
    if (app_ble_get_hdl_con_handle(custom_demo_ble_hdl)) {
        app_ble_disconnect(custom_demo_ble_hdl);
    }
}

void custom_demo_all_exit(void)
{
    printf("custom_demo_all_exit\n");

    // BLE exit
    if (app_ble_get_hdl_con_handle(custom_demo_ble_hdl)) {
        app_ble_disconnect(custom_demo_ble_hdl);
    }
    app_ble_hdl_free(custom_demo_ble_hdl);
    custom_demo_ble_hdl = NULL;

    // SPP init
    if (NULL != app_spp_get_hdl_remote_addr(custom_demo_spp_hdl)) {
        app_spp_disconnect(custom_demo_spp_hdl);
    }
    app_spp_hdl_free(custom_demo_spp_hdl);
    custom_demo_spp_hdl = NULL;

#if ATT_OVER_EDR_DEMO_EN
    // gatt over edr exit
    if (app_ble_get_hdl_con_handle(att_over_edr_hdl)) {
        app_ble_disconnect(att_over_edr_hdl);
    }
    app_ble_hdl_free(att_over_edr_hdl);
    att_over_edr_hdl = NULL;
#endif
}

#endif
