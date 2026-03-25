#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".charge.data.bss")
#pragma data_seg(".charge.data")
#pragma const_seg(".charge.text.const")
#pragma code_seg(".charge.text")
#endif
#include "timer.h"
#include "asm/charge.h"
#include "gpadc.h"
#include "uart.h"
#include "device/device.h"
#include "asm/power_interface.h"
#include "asm/efuse.h"
#include "gpio.h"
#include "clock.h"
#include "app_config.h"
#include "spinlock.h"
#if TCFG_CHARGE_CALIBRATION_ENABLE
#include "asm/charge_calibration.h"
#endif

#define LOG_TAG_CONST   CHARGE
#define LOG_TAG         "[CHARGE]"
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#include "debug.h"

typedef struct _CHARGE_VAR {
    const struct charge_platform_data *data;
    u8 charge_flag;
    u8 charge_poweron_en;
    u8 cc_flag;
    u8 cc_counter;
    u8 charge_online_flag;
    u8 charge_event_flag;
    u8 init_ok;
    u8 detect_stop;
    u16 full_voltage;
    u16 ldo5v_timer;   //检测LDOIN状态变化的usr timer
    u16 charge_timer;  //检测充电是否充满的usr timer
    u16 cc_timer;      //涓流切恒流的usr timer
    u16 status_timer;  //用于防止边沿检测pending不响应的情况
} CHARGE_VAR;

#define __this 	(&charge_var)
static CHARGE_VAR charge_var;
static spinlock_t charge_lock;
static spinlock_t ldo5v_lock;

// PMU结构简述:(NVDC架构)
// VPWR为充电输入引脚,VBAT为电池接口
// 充电模块的输入为VIN
// 正常情况下,(VPWR>VBAT时,VIN=VPWR) (VPWR<VBAT时,VIN=VBAT)
// >>/<<为电流可能走向
// VPWR----->>-----VIN--->><<-----CHARGE-->><<----VBAT
//                  |
//                  |
//                IOVDD

/*
 * 充电线性环路1说明,开启后对跟随充更友好,将会限制 VIN>VBAT+100mV(@SLT=0)
 * 例如:
 * VPWR外部供电5V限流50mA,充电模块设置100mA充电,充电开启后VIN电压将被拉低
 * 但是由于线性环路开启,VIN恒定处于VBAT+100mV的电压,此时充电电流为50mA
 */

/*
 * 充电线性环路2说明,开启后将不支持跟随充功能,充电模块输入端VIN电压将限制在不低于4V.
 * 例如:
 * 1、VPWR外部供电5V限流50mA,充电模块设置100mA充电,充电开启后,VIN将被拉低,
 * 当VIN被拉低至4V时,线性环路2将起作用,主动限制充电电流,VIN电压会恒定处于4V;
 *
 * 2、VPWR外部供电5V限流50mA,充电模块设置20mA充电,系统推屏需要从IOVDD耗电40mA,充电推屏开启后,VIN将被拉低
 * 当VIN被拉低至4V时,线性环路2将起作用,将优先满足系统供电需求,主动限制充电电流,VIN电压会恒定处于4V;
 *
 * 3、VPWR外部供电5V限流50mA,电池为3.7V,充电模块设置20mA充电,系统推屏需要从IOVDD耗电80mA;
 * 充电推屏开启后,VIN将被拉低,系统会把VIN的电压扯到低于4V,充电电流会降低至0mA,VIN会继续下降,
 * 一旦下降到低于VBAT电压,电池就会给系统供电,这种模式称为补电模式,在改模式下,VPWR和VBAT同时为系统供电
 */

#ifndef TCFG_CHARGE_NVDC_EN
#define TCFG_CHARGE_NVDC_EN 0 // NVDC架构使能
#endif

#define CHARGE_VILOOP1_ENABLE   1//默认开启
#define CHARGE_VILOOP2_ENABLE   TCFG_CHARGE_NVDC_EN//默认关闭

//判满时,VBAT的最小电压值
#define CHARGE_FULL_VBAT_MIN_VOLTAGE            (__this->full_voltage - 100)
//判满时,VPWR和VBAT的最小压差值
#define CHARGE_FULL_VPWR_VBAT_MIN_DIFF_VOLTAGE  (200)
//涓流切恒流滤波/s
#define CHARGE_TC2CC_FILTER_TIMES               4

#define BIT_LDO5V_IN		BIT(0)
#define BIT_LDO5V_OFF		BIT(1)
#define BIT_LDO5V_KEEP		BIT(2)

#define GET_PINR_EN()		((P33_CON_GET(P3_PINR_CON1) & BIT(0)) ? 1: 0)
#define GET_PINR_LEVEL()	((P33_CON_GET(P3_PINR_CON1) & BIT(2)) ? 1: 0)
#define GET_PINR_PIN()		((P33_CON_GET(P3_PINR_CON1) & BIT(3)) ? 1: 0)

extern void charge_event_to_user(u8 event);

u8 check_pinr_shutdown_enable(void)
{
    log_info("pinr lvl %d, ldo5v det %d", GET_PINR_LEVEL(), LDO5V_DET_GET());
    if (GET_PINR_EN() && (GET_PINR_LEVEL() == LDO5V_DET_GET()) && !GET_PINR_PIN()) {
        return 0;
    }
    return 1;
}

u8 get_charge_poweron_en(void)
{
    return __this->charge_poweron_en;
}

void set_charge_poweron_en(u32 onOff)
{
    __this->charge_poweron_en = onOff;
}

void charge_check_and_set_pinr(u8 level)
{
    u8 reg;
    reg = P33_CON_GET(P3_PINR_CON1);
    //开启LDO5V_DET长按复位
    if ((reg & BIT(0)) && ((reg & BIT(3)) == 0)) {
        if (level == 0) {
            P33_CON_SET(P3_PINR_CON1, 2, 1, 0);
        } else {
            P33_CON_SET(P3_PINR_CON1, 2, 1, 1);
        }
    }
}

static u8 check_charge_state(void)
{
    u16 i;
    __this->charge_online_flag = 0;

    for (i = 0; i < 20; i++) {
        if (LVCMP_DET_GET() || LDO5V_DET_GET()) {
            __this->charge_online_flag = 1;
            break;
        }
        udelay(1000);
    }
    return __this->charge_online_flag;
}

void set_charge_online_flag(u8 flag)
{
    __this->charge_online_flag = flag;
}

u8 get_charge_online_flag(void)
{
    return __this->charge_online_flag;
}

void set_charge_event_flag(u8 flag)
{
    __this->charge_event_flag = flag;
}

u8 get_ldo5v_online_hw(void)
{
    return LDO5V_DET_GET();
}

u8 get_lvcmp_det(void)
{
    return LVCMP_DET_GET();
}

u8 get_ldo5v_pulldown_en(void)
{
    if (!__this->data) {
        return 0;
    }
    if (get_ldo5v_online_hw()) {
        if (__this->data->ldo5v_pulldown_keep == 0) {
            return 0;
        }
    }
    return __this->data->ldo5v_pulldown_en;
}

u8 get_ldo5v_pulldown_res(void)
{
    if (__this->data) {
        return __this->data->ldo5v_pulldown_lvl;
    }
    return CHARGE_PULLDOWN_200K;
}

static void charge_cc_check(void *priv)
{
    u8 first_entry = (u8)priv;
    if (((adc_get_voltage_blocking(AD_CH_PMU_VBAT) * AD_CH_PMU_VBAT_DIV) / 10) > CHARGE_CCVOL_V) {
        if (__this->cc_flag == 0) {
            __this->cc_counter++;
        }
        if (first_entry || ((__this->cc_flag == 0) && (__this->cc_counter > CHARGE_TC2CC_FILTER_TIMES))) {
            __this->cc_flag = 1;
            set_charge_mA(__this->data->charge_mA);
#if TCFG_CHARGE_CALIBRATION_ENABLE
            charge_enter_calibration_mode();
#endif
            p33_io_wakeup_enable(IO_CHGFL_DET, 1);
            if (first_entry == 0) {
                charge_wakeup_isr();
            }
        }
    } else {
        __this->cc_counter = 0;
        if (first_entry || (__this->cc_flag == 1)) {
            __this->cc_flag = 0;
            set_charge_mA(__this->data->charge_trickle_mA);
            p33_io_wakeup_enable(IO_CHGFL_DET, 0);
            if (__this->charge_timer) {
                usr_timer_del(__this->charge_timer);
                __this->charge_timer = 0;
            }
        }
    }

    //TC2CC or CC2TC check
    if (first_entry && (__this->cc_timer == 0)) {
        __this->cc_counter = 0;
        __this->cc_timer = usr_timer_add(0, charge_cc_check, 1000, 1);
    }
}

void charge_start(void)
{
    log_info("%s\n", __func__);

    if (__this->charge_timer) {
        usr_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
    }

    charge_cc_check((void *)1);

    CHG_VILOOP_EN(CHARGE_VILOOP1_ENABLE);
    CHG_VILOOP2_EN(CHARGE_VILOOP2_ENABLE);
    CHARGE_EN(1);
    CHGGO_EN(1);

    charge_event_to_user(CHARGE_EVENT_CHARGE_START);

    //开启充电时,充满标记为1时,主动检测一次是否充满
    if (__this->cc_flag && CHARGE_FULL_FLAG_GET()) {
        charge_wakeup_isr();
    }
}

void charge_close(void)
{
    log_info("%s\n", __func__);

    CHARGE_EN(0);
    CHGGO_EN(0);
    CHG_VILOOP_EN(0);
    CHG_VILOOP2_EN(0);

    p33_io_wakeup_enable(IO_CHGFL_DET, 0);

    charge_event_to_user(CHARGE_EVENT_CHARGE_CLOSE);

    if (__this->charge_timer) {
        usr_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
    }
    if (__this->cc_timer) {
        usr_timer_del(__this->cc_timer);
        __this->cc_timer = 0;
    }
}

static void charge_full_detect(void *priv)
{
    static u16 charge_full_cnt = 0;
    u16 vbat_vol, vpwr_vol;

    if (CHARGE_FULL_FILTER_GET()) {
        log_char('F');
        if (CHARGE_FULL_FLAG_GET() && LVCMP_DET_GET()) {
            log_char('1');

            vbat_vol = gpadc_battery_get_voltage();
            //判断电池电压不小于满电电压-100mV
            if (vbat_vol < CHARGE_FULL_VBAT_MIN_VOLTAGE) {
                charge_full_cnt = 0;
                log_debug("vbat voltage not enough: %d < %d\n", vbat_vol, CHARGE_FULL_VBAT_MIN_VOLTAGE);
                return;
            }

            vpwr_vol = adc_get_voltage(AD_CH_PMU_VPWR) * 4;
            //VPWR和VBAT压差大于配置值时才允许判满
            if (vpwr_vol < (vbat_vol + CHARGE_FULL_VPWR_VBAT_MIN_DIFF_VOLTAGE)) {
                charge_full_cnt = 0;
                log_debug("vpwr-vbat not enough: %d(vpwr) - %d(vbat) < %d mV\n", vpwr_vol, vbat_vol, CHARGE_FULL_VPWR_VBAT_MIN_DIFF_VOLTAGE);
                return;
            }

            if (charge_full_cnt < 10) {
                charge_full_cnt++;
            } else {
                charge_full_cnt = 0;
                p33_io_wakeup_enable(IO_CHGFL_DET, 0);
                usr_timer_del(__this->charge_timer);
                __this->charge_timer = 0;
                charge_event_to_user(CHARGE_EVENT_CHARGE_FULL);
            }
        } else {
            log_char('0');
            charge_full_cnt = 0;
        }
    } else {
        log_char('K');
        spin_lock(&charge_lock);
        charge_full_cnt = 0;
        usr_timer_del(__this->charge_timer);
        __this->charge_timer = 0;
        spin_unlock(&charge_lock);
    }
}

static void ldo5v_detect(void *priv)
{
    static u16 ldo5v_on_cnt = 0;
    static u16 ldo5v_keep_cnt = 0;
    static u16 ldo5v_off_cnt = 0;

    if (__this->detect_stop) {
        return;
    }

    if (LVCMP_DET_GET()) {	//ldoin > vbat
        log_char('X');
        if (ldo5v_on_cnt < __this->data->ldo5v_on_filter) {
            ldo5v_on_cnt++;
            if (ldo5v_off_cnt >= (__this->data->ldo5v_off_filter + 4)) {
                ldo5v_off_cnt = __this->data->ldo5v_off_filter + 4;
            }
            if (__this->data->ldo5v_keep_filter <= 16) {
                ldo5v_keep_cnt = 0;
            } else if (ldo5v_keep_cnt >= (__this->data->ldo5v_keep_filter - 16)) {
                ldo5v_keep_cnt = __this->data->ldo5v_keep_filter - 16;
            }
        } else {
            log_debug("ldo5V_IN\n");
            set_charge_online_flag(1);
            ldo5v_off_cnt = 0;
            ldo5v_keep_cnt = 0;
            //消息线程未准备好接收消息,继续扫描
            if (__this->charge_event_flag == 0) {
                return;
            }
            ldo5v_on_cnt = 0;
            spin_lock(&ldo5v_lock);
            usr_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            spin_unlock(&ldo5v_lock);
            if ((__this->charge_flag & BIT_LDO5V_IN) == 0) {
                __this->charge_flag = BIT_LDO5V_IN;
                charge_event_to_user(CHARGE_EVENT_LDO5V_IN);
            }
        }
    } else if (LDO5V_DET_GET() == 0) {	//ldoin<拔出电压（0.6）
        log_char('Q');
        if (ldo5v_off_cnt < (__this->data->ldo5v_off_filter + 20)) {
            ldo5v_off_cnt++;
            if (__this->data->ldo5v_on_filter <= 16) {
                ldo5v_on_cnt = 0;
            } else if (ldo5v_on_cnt >= (__this->data->ldo5v_on_filter - 16)) {
                ldo5v_on_cnt = __this->data->ldo5v_on_filter - 16;
            }
            if (__this->data->ldo5v_keep_filter <= 16) {
                ldo5v_keep_cnt = 0;
            } else if (ldo5v_keep_cnt >= (__this->data->ldo5v_keep_filter - 16)) {
                ldo5v_keep_cnt = __this->data->ldo5v_keep_filter - 16;
            }
        } else {
            log_debug("ldo5V_OFF\n");
            set_charge_online_flag(0);
            ldo5v_on_cnt = 0;
            ldo5v_keep_cnt = 0;
            //消息线程未准备好接收消息,继续扫描
            if (__this->charge_event_flag == 0) {
                return;
            }
            ldo5v_off_cnt = 0;
            spin_lock(&ldo5v_lock);
            usr_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            spin_unlock(&ldo5v_lock);
            if ((__this->charge_flag & BIT_LDO5V_OFF) == 0) {
                __this->charge_flag = BIT_LDO5V_OFF;
                charge_event_to_user(CHARGE_EVENT_LDO5V_OFF);
            }
        }
    } else {	//拔出电压（0.6左右）< ldoin < vbat
        log_char('E');
        if (ldo5v_keep_cnt < __this->data->ldo5v_keep_filter) {
            ldo5v_keep_cnt++;
            if (ldo5v_off_cnt >= (__this->data->ldo5v_off_filter + 4)) {
                ldo5v_off_cnt = __this->data->ldo5v_off_filter + 4;
            }
            if (__this->data->ldo5v_on_filter <= 16) {
                ldo5v_on_cnt = 0;
            } else if (ldo5v_on_cnt >= (__this->data->ldo5v_on_filter - 16)) {
                ldo5v_on_cnt = __this->data->ldo5v_on_filter - 16;
            }
        } else {
            log_debug("ldo5V_ERR\n");
            set_charge_online_flag(1);
            ldo5v_off_cnt = 0;
            ldo5v_on_cnt = 0;
            //消息线程未准备好接收消息,继续扫描
            if (__this->charge_event_flag == 0) {
                return;
            }
            ldo5v_keep_cnt = 0;
            spin_lock(&ldo5v_lock);
            usr_timer_del(__this->ldo5v_timer);
            __this->ldo5v_timer = 0;
            spin_unlock(&ldo5v_lock);
            if ((__this->charge_flag & BIT_LDO5V_KEEP) == 0) {
                __this->charge_flag = BIT_LDO5V_KEEP;
                if (__this->data->ldo5v_off_filter) {
                    charge_event_to_user(CHARGE_EVENT_LDO5V_KEEP);
                }
            }
        }
    }
}

void ldoin_wakeup_isr(void)
{
    if (!__this->init_ok) {
        return;
    }
    /* printf(" %s \n", __func__); */
    spin_lock(&ldo5v_lock);
    if (__this->ldo5v_timer == 0) {
        __this->ldo5v_timer = usr_timer_add(0, ldo5v_detect, 2, 1);
    }
    spin_unlock(&ldo5v_lock);
}

void charge_wakeup_isr(void)
{
    if (!__this->init_ok) {
        return;
    }
    /* printf(" %s \n", __func__); */
    spin_lock(&charge_lock);
    if (__this->charge_timer == 0) {
        __this->charge_timer = usr_timer_add(0, charge_full_detect, 2, 1);
    }
    spin_unlock(&charge_lock);
}

static void ldo5v_status_detect(void *priv)
{
    u8 tmp_flag;
    if (LVCMP_DET_GET()) {
        tmp_flag = BIT_LDO5V_IN;
    } else if (LDO5V_DET_GET() == 0) {
        tmp_flag = BIT_LDO5V_OFF;
    } else {
        tmp_flag = BIT_LDO5V_KEEP;
    }
    //检测到状态不一致时,主动触发进入检测中断
    if (tmp_flag != __this->charge_flag) {
        ldoin_wakeup_isr();
    }
}

void charge_set_ldo5v_detect_stop(u8 stop)
{
    __this->detect_stop = stop;
}

u16 get_charge_mA_config(void)
{
    return __this->data->charge_mA;
}

u16 get_charge_current_value(void)
{
    u16 charge_curr;
    if (IS_TRICKLE_EN()) {
        charge_curr = ((float)(1.16f + 0.01f * CHGV_VREF_GET()) * 0.0179f * (CHARGE_mA_GET() + 1));
    } else {
        charge_curr = ((float)(1.16f + 0.01f * CHGV_VREF_GET()) * 0.179f * (CHARGE_mA_GET() + 1));
    }
    return charge_curr;
}

void set_charge_mA(u16 charge_mA)
{
    u8 vref, type;
    u16 chgi, temp;

    type = (charge_mA & TRICKLE_EN_FLAG) ? 1 : 0;
    charge_mA &= ~TRICKLE_EN_FLAG;
    log_info("charge_mA: %d, type: %d", charge_mA, type);

    for (vref = 0; vref < 8; vref++) {
        for (chgi = 0; chgi < 1024; chgi++) {
            if (type) {
                temp = (float)(1.16 + 0.01 * vref) * 0.0179 * (chgi + 1);
            } else {
                temp = (float)(1.16 + 0.01 * vref) * 0.179 * (chgi + 1);
            }
            if (!((temp > charge_mA) ? (temp - charge_mA) : (charge_mA - temp))) {
                log_info("chgi check success!");
                goto _check_end;
            }
        }
    }

    //没有找到对应的电流则设置为最大档
    vref = 7;
    chgi = 1023;
    log_info("chgi check fail! set max level!");

_check_end:
    log_info("vref: %d, chgi: %d", vref, chgi);
    CHGV_VREF_SEL(vref);
    CHARGE_mA_SEL(chgi);
    CHG_TRICKLE_EN(type);
}

u16 get_charge_full_value(void)
{
    ASSERT(__this->init_ok, "charge not init ok!\n");
    return __this->full_voltage;
}

static void charge_config(void)
{
    u8 charge_trim_val;
    u8 offset = 0;
    u8 charge_full_v_val = 0;
    u8 charge_curr_trim;
    //判断是用高压档还是低压档
    if (__this->data->charge_full_V < CHARGE_FULL_V_4237) {
        CHG_HV_MODE(0);
        charge_trim_val = efuse_get_vbat_trim();//4.20V对应的trim出来的实际档位
        log_info("low charge_trim_val = %d\n", charge_trim_val);
        if (__this->data->charge_full_V >= CHARGE_FULL_V_4199) {
            offset = __this->data->charge_full_V - CHARGE_FULL_V_4199;
            charge_full_v_val = charge_trim_val + offset;
            if (charge_full_v_val > 0xf) {
                charge_full_v_val = 0xf;
            }
            __this->full_voltage = 4200 + (charge_full_v_val - charge_trim_val) * 20;
        } else {
            offset = CHARGE_FULL_V_4199 - __this->data->charge_full_V;
            if (charge_trim_val >= offset) {
                charge_full_v_val = charge_trim_val - offset;
            } else {
                charge_full_v_val = 0;
            }
            __this->full_voltage = 4200 - (charge_trim_val - charge_full_v_val) * 20;
        }
    } else {
        CHG_HV_MODE(1);
        charge_trim_val = efuse_get_vbat_trim_4p35();//4.35V对应的trim出来的实际档位
        log_info("high charge_trim_val = %d\n", charge_trim_val);
        if (__this->data->charge_full_V >= CHARGE_FULL_V_4354) {
            offset = __this->data->charge_full_V - CHARGE_FULL_V_4354;
            charge_full_v_val = charge_trim_val + offset;
            if (charge_full_v_val > 0xf) {
                charge_full_v_val = 0xf;
            }
            __this->full_voltage = 4350 + (charge_full_v_val - charge_trim_val) * 20;
        } else {
            offset = CHARGE_FULL_V_4354 - __this->data->charge_full_V;
            if (charge_trim_val >= offset) {
                charge_full_v_val = charge_trim_val - offset;
            } else {
                charge_full_v_val = 0;
            }
            __this->full_voltage = 4350 - (charge_trim_val - charge_full_v_val) * 20;
        }
    }
    log_info("charge_full_v_val = %d\n", charge_full_v_val);
    log_info("charge_full_voltage = %d mV\n", __this->full_voltage);

    //电流校准
    charge_curr_trim = efuse_get_charge_cur_trim();
    log_info("charge curr set value = %d\n", charge_curr_trim);

    CHGI_TRIM_SEL(charge_curr_trim);
    CHARGE_FULL_V_SEL(charge_full_v_val);
    CHARGE_FULL_mA_SEL(__this->data->charge_full_mA);
    set_charge_mA(__this->data->charge_trickle_mA);
}

int charge_init(const struct charge_platform_data *data)
{
    log_info("%s\n", __func__);

    __this->data = data;

    ASSERT(__this->data);

    __this->init_ok = 0;
    __this->charge_online_flag = 0;
    __this->charge_poweron_en = data->charge_poweron_en;

    spin_lock_init(&charge_lock);
    spin_lock_init(&ldo5v_lock);

    /*先关闭充电使能，后面检测到充电插入再开启*/
    p33_io_wakeup_enable(IO_CHGFL_DET, 0);
    L5V_IO_MODE(0);
    CHARGE_EN(0);
    CHGGO_EN(0);
    CHG_CCLOOP_EN(1);
    CHG_VILOOP_EN(0);
    CHG_VILOOP2_EN(0);
    L5V_IO_MODE(0);
    //消除vbat到vpwr的漏电再判断ldo5v状态
    u8 temp = 10;
    if (is_reset_source(P33_VDDIO_POR_RST)) {
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_00), PORT_INPUT_PULLDOWN_10K);
        while (temp) {
            udelay(1000);
            if (LDO5V_DET_GET() == 0) {
                temp = 0;
            } else {
                temp--;
            }
        }
        gpio_set_mode(IO_PORT_SPILT(IO_PORTP_00), PORT_INPUT_FLOATING);
    }

    /*LDO5V的100K下拉电阻使能*/
    L5V_RES_DET_S_SEL(__this->data->ldo5v_pulldown_lvl);
    L5V_LOAD_EN(__this->data->ldo5v_pulldown_en);

    charge_config();

    adc_add_sample_ch(AD_CH_PMU_VPWR);

    if (check_charge_state()) {
        if (__this->ldo5v_timer == 0) {
            __this->ldo5v_timer = usr_timer_add(0, ldo5v_detect, 2, 1);
        }
    } else {
        __this->charge_flag = BIT_LDO5V_OFF;
    }

    __this->status_timer = usr_timer_add(0, ldo5v_status_detect, 1000, 0);
    __this->init_ok = 1;
    return 0;
}

void charge_module_stop(void)
{
    if (!__this->init_ok) {
        return;
    }
    charge_close();
    p33_io_wakeup_enable(IO_LDOIN_DET, 0);
    p33_io_wakeup_enable(IO_VBTCH_DET, 0);
    if (__this->ldo5v_timer) {
        usr_timer_del(__this->ldo5v_timer);
        __this->ldo5v_timer = 0;
    }
    if (__this->status_timer) {
        usr_timer_del(__this->status_timer);
        __this->status_timer = 0;
    }
}

void charge_system_reset(void)
{
    //充电采用LV0的复位
    system_reset(CHARGE_FLAG);
}

void charge_module_restart(void)
{
    if (!__this->init_ok) {
        return;
    }
    if (!__this->ldo5v_timer) {
        __this->ldo5v_timer = usr_timer_add(NULL, ldo5v_detect, 2, 1);
    }
    if (!__this->status_timer) {
        __this->status_timer = usr_timer_add(NULL, ldo5v_status_detect, 1000, 0);
    }
    p33_io_wakeup_enable(IO_LDOIN_DET, 1);
    p33_io_wakeup_enable(IO_VBTCH_DET, 1);
}

// 开机激活锂保退船运
void charge_exit_shipping(void)
{
    if (LVCMP_DET_GET()) {
        CHARGE_FULL_V_SEL(CHARGE_FULL_V_4041);
        set_charge_mA(3);
        L5V_IO_MODE(0);
        CHG_CCLOOP_EN(1);
        CHG_VILOOP_EN(CHARGE_VILOOP1_ENABLE);
        CHG_VILOOP2_EN(CHARGE_VILOOP2_ENABLE);
        CHARGE_EN(1);
        CHGGO_EN(1);
    }
}

//系统进入低功耗时会调用
void charge_enter_lowpower(enum LOW_POWER_LEVEL lp_mode)
{
    if (lp_mode == LOW_POWER_MODE_SOFF) {
        CHARGE_EN(0);
        CHGGO_EN(0);
        CHG_CCLOOP_EN(0);
        CHG_VILOOP_EN(0);
        CHG_VILOOP2_EN(0);
        L5V_LOAD_EN(get_ldo5v_pulldown_en());
        L5V_RES_DET_S_SEL(get_ldo5v_pulldown_res());
    }
}


