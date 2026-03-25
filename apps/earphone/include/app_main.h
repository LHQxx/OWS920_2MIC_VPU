#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "app_msg.h"
#include "app_mode_manager/app_mode_manager.h"
#include "poweroff.h"
#include "app_config.h"
#include "bt_background.h"
#include "earphone.h"

/**************************************************************************************************
  User Define start
**************************************************************************************************/
#define MMI_CC_INIT_VARIABLE                         1
#define MMI_CC_BT_NAME_ENCRYPTION                    1
#define MMI_CC_USER_LED_CTRL                         1
#define MMI_CC_ADD_TWS_CONNECTED_EVENT_LED           1
#define MMI_CC_SIRI_OPENCLOSE_SAME_KEYACTION         1

#define MMI_CC_PRINT_ROLE_xTIMER                     1
#define MMI_CC_ADD_PRINTF_STATE_TIMER                1
 #define STATE_CHECK_TIME   5L 

#define MMI_CC_LONGPRESS_KEY_xS_POWER_ON             1
  #define D_TIMER_CNT                   5  //500*10MS

#define MMI_CC_PLAY_PAIR_VP_TONE                     1
#define MMI_CC_ENTER_PAIR_BEFORE_ADD_DET             1
#define MMI_CC_PAIR_TIMEOUT_INTO_CONECTABLE          1
#define MMI_CC_DISABLE_PAIR_LINKLOSS_AFTER           1
//在LINKLOSS回连超时未连接设备，也需要自动关机
#define MMI_CC_ENABLE_SHUT_DOWN_IN_CONNECTABLE       1

#define MMI_CC_CLEAR_ALL_DEVICE_RECORD               1
 #define MMI_CC_CPU_RESET_REBOOT_CLEAR_RECORD        1
#define MMI_CC_DISCONNECTION_HCI_ENABLE              1

#define MMI_CC_GET_REMOTE_NAME_AFTER_CONNECTED       1
//#define ANCTOOL_PAIR_KEY  		"666666" /*ANC工具默认配对码*/

//解决设备端删除了LINKKEY后，BT还在回连状态，应该直接进入PAIR MODE
#define MMI_CC_LINKKEY_MISSING_CANCEL_RECON          1

#define MMI_CC_SPP_BLE_ENABLE                        1
#define MMI_CC_FIRMWARE_VERSION_BUILD	  0x2003
#define MMI_VEI_X_APP 03

#define MMI_CC_SHIPPING_MODE                         1
//int io config
#define BOAT_CTL_PIN               IO_PORTB_00
#define BOAT_PIN_OUT_LEVEL(hl)     gpio_write(BOAT_CTL_PIN, hl)
#define BOAT_PIN_SET_MODE          gpio_set_mode(PORTB, PORT_PIN_0, PORT_OUTPUT_LOW) //dp

#define MMI_CC_SPATIAL_MODLE                          1
#define MMI_CC_BT_SUPPORT_LDAC                        1
#define MMI_CC_BT_SUPPORT_LHDC                        0

#define MMI_CC_TONE_VOL_ADJUST                         1
 #define D_DEFULAT_TONE_VOL_LEVEL      7
 
#define MMI_CC_POWEROFF_BEFORE_WRITE_VM               1
#define MMI_CC_TWS_SYNC_USER_DATA                     1

#define MMI_CC_GET_DEVICE_LINKKEY                     1

//bt_discover_connectable
//BT STA
enum{
	 BT_STA_OFF =0x00,								 //蓝牙不可发现,不可连接
	 BT_STA_DISCOVER_NONCONECTABLE,					 //蓝牙可发现,不可连接
	 BT_STA_NONDISCOVER_CONECTABLE,					 //蓝牙不可发现,可连接
	 BT_STA_DISCOVER_CONECTABLE,					 //蓝牙可发现,可连接

	 BT_STA_CONNECTED,                               //蓝牙已连接
 };
 enum{
	 PAGE_TYPE_NULL =0x00, 							 
	 PAGE_TYPE_POWERON,				      //回连类型是开机回连
	 PAGE_TYPE_LINKLOSS,				  //回连类型是超距回连
};


/**************************************************************************************************
  User Define end
**************************************************************************************************/

enum {
    SYS_POWERON_BY_KEY = 1,
    SYS_POWERON_BY_OUT_BOX,
};

enum {
    SYS_POWEROFF_BY_KEY = 1,
    SYS_POWEROFF_BY_IN_BOX,
    SYS_POWEROFF_BY_TIMEOUT,
};

typedef struct {
    float talk;
    float ff;
    float fb;
} audio_mic_cmp_t;

typedef struct _APP_VAR {
    u8 volume_def_state;
    s16 bt_volume;
    s16 dev_volume;
    s16 music_volume;
    s16 call_volume;
    s16 wtone_volume;
    s16 ktone_volume;
    s16 ring_volume;
#if JL_UNICAST_DUAL_UAC_ENABLE
    u8 uac0_vol;   //双声卡模式下UAC0音量
    u8 uac1_vol;   //双声卡模式下UAC1音量
#endif
    u8 opid_play_vol_sync;
    u8 aec_dac_gain;
    u8 aec_mic_gain;
    u8 aec_mic1_gain;
    u8 aec_mic2_gain;
    u8 aec_mic3_gain;
    u8 rf_power;
    u8 goto_poweroff_flag;
    u16 goto_poweroff_cnt;
    u8 poweroff_sametime_flag;
    u8 play_poweron_tone;
    u8 remote_dev_company;
    u8 siri_stu;
    u8 cycle_mode;
    u8 poweron_reason;
    u8 poweroff_reason;
    u8 update_tone_end_flag;//升级完成后提示音播放结束标志位
    int auto_stop_page_scan_timer;     //用于1拖2时，有一台连接上后，超过三分钟自动关闭Page Scan
    u16 auto_off_time;
    u16 warning_tone_v;
    u16 poweroff_tone_v;
    u16 phone_dly_discon_time;
    u8 usb_mic_gain;
    int wait_timer_do;
    float audio_mic_array_diff_cmp;//麦克风阵列校准补偿值
    u8 audio_mic_array_trim_en; //麦克风阵列校准
    audio_mic_cmp_t audio_mic_cmp;
    float enc_degradation;//default:1,range[0:1]
    u32 start_time;
    s16 mic_eff_volume;
} APP_VAR;

struct bt_mode_var {
    //phone
    u8 phone_ring_flag;
    u8 phone_num_flag;
    u8 phone_income_flag;
    u8 phone_call_dec_begin;
    u8 phone_ring_sync_tws;
    u8 phone_ring_addr[6];

    u8 inband_ringtone;
    u8 phone_vol;
    u16 phone_timer_id;
    u16 dongle_check_a2dp_timer;
    u8 dongle_addr[6];
    u8 last_call_type;
    u8 income_phone_num[30];
    u8 income_phone_len;
    s32 auto_connection_counter;
    int auto_connection_timer;
    u8 auto_connection_addr[6];
    int tws_con_timer;
    u8 tws_start_con_cnt;
    u8 tws_conn_state;
    bool search_tws_ing;
    int sniff_timer;
    bool fast_test_mode;
    u16 exit_check_timer;
    u8 init_start; //蓝牙协议栈已经开始初始化标志位
    u8 init_ok; //蓝牙初始化完成标志
    u8 exiting; //蓝牙正在退出
    u8 wait_exit; //蓝牙等待退出
    u8 ignore_discon_tone;  // 1-退出蓝牙模式， 不响应discon提示音
    u8 bt_dual_conn_config;
    u8 control_device_type;  //用于区分1T2场景想要控制哪个连接类型的设备
    background_var background;  //蓝牙后台相关变量
    u16 get_music_player_timer;
};

//add by joe
typedef struct _BT_USER_COMM_VAR {
    bool a2dp_stream_ing;
    bool poweron_recon_timeout;
    bool linkloss_recon_timeout;
    bool first_paired;
    u16 printf_check_timer;
    u16 tmr5ms_cnt;
    u8 bt_discover_connectable;
    u8 page_type;
    u8 remote_addr[2][6];   //almost 2devices
} BT_USER_COMM_VAR;

//(virtual memory)VM
typedef struct
{
	//u8 box_battery;
    u8 spatialMode;
    u8 codecState;
    //u8 multipointState;
    //u8 languageMode;
    s16 toneVolumeLevel;
    
    //u8 findmeL;
    //u8 findmeR;
    //u8 touchL_en;
    //u8 touchR_en;
    //u8 ancMode;
    //u8 autoPowerOffLevel;

    //u8 left_single_func;
    //u8 left_double_func;
    //u8 left_triple_func;
    //u8 left_long_func;
    //u8 right_single_func;
    //u8 right_double_func;
    //u8 right_triple_func;
    //u8 right_long_func;

    //u8 userAncCfg_L;
    //u8 userAncCfg_R;
    //u8 userAncMode;

    //bool custNameSetFlag;
    //bool autoPowerOffEnabled;
    
    //bool appUpdataEqFlag;
}double_cfg_t;


extern APP_VAR app_var;
extern struct bt_mode_var g_bt_hdl;
//add by joe
extern BT_USER_COMM_VAR mmi_var;
extern double_cfg_t   double_cfg;

enum app_mode_t {
    APP_MODE_POWERON,
    APP_MODE_IDLE,
    APP_MODE_BT,
    APP_MODE_MUSIC,
    APP_MODE_LINEIN,
    APP_MODE_PC,
    APP_MODE_SINK,
};

enum app_mode_index {
    APP_MODE_BT_INDEX,
    APP_MODE_MUSIC_INDEX,
    APP_MODE_LINEIN_INDEX,
    APP_MODE_PC_INDEX,
};

enum {
    CONTROL_ALL,  //不限制连接类型进行控制
    CONTROL_EDR,  //控制edr设备
    CONTROL_CIS,
    CONTROL_BIS,
};

#define earphone (&bt_user_priv_var)

void app_power_off(void *priv);
void bt_bredr_enter_dut_mode(u8 mode, u8 inquiry_scan_en);
void bt_bredr_exit_dut_mode();
u8 get_charge_online_flag(void);
u8 check_local_not_accept_sniff_by_remote();

struct app_mode *app_mode_switch_handler(int *msg);

#endif
