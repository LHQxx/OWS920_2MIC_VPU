#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".charge_config.data.bss")
#pragma data_seg(".charge_config.data")
#pragma const_seg(".charge_config.text.const")
#pragma code_seg(".charge_config.text")
#endif
#include "app_config.h"
#include "cpu/includes.h"
#include "asm/charge.h"
#include "system/init.h"
#include "system/timer.h"
#include "asm/power_interface.h"
#include "gpio.h"

static u16 recover_timer;
static u32 recover_in_count;
static u32 recover_out_count;
#define RECOVER_TIME_MAX    150//50ms
static void power_mode_recover_run(void *priv)
{
    //确认是上升沿 or 下降沿
    if (LVCMP_DET_GET()) {
        recover_in_count++;
        recover_out_count = 0;
        if (recover_in_count > RECOVER_TIME_MAX) {
            usr_timer_del(recover_timer);
            recover_timer = 0;
            p33_io_wakeup_filter(IO_VBTCH_DET, PORT_FLT_16ms);
        }
    } else {
        recover_out_count++;
        recover_in_count = 0;
        if (recover_out_count > RECOVER_TIME_MAX) {
            usr_timer_del(recover_timer);
            recover_timer = 0;

            p33_io_wakeup_filter(IO_VBTCH_DET, PORT_FLT_DISABLE);

            local_irq_disable();
            printf("reset power mode!\n");
            //通知P11可切换电源模式为DCDC
            M2P_DCDC_CFG = 0xff;
            msys_to_p11_sync_cmd(M2P_DCDC_SWITCH);
            local_irq_enable();
        }
    }
}

static void power_mode_detect(void)
{
    recover_in_count = 0;
    recover_out_count = 0;
    if (recover_timer == 0) {
        recover_timer = usr_timer_add(NULL, power_mode_recover_run, 2, 1);
    }
}

#if TCFG_CHARGE_ENABLE

static void chargefull_wakeup_callback(P33_IO_WKUP_EDGE edge)
{
    charge_wakeup_isr();
}

static void vpwr_indet_wakeup_callback(P33_IO_WKUP_EDGE edge)
{
    ldoin_wakeup_isr();
}

static void vpwr_chgdet_wakeup_callback(P33_IO_WKUP_EDGE edge)
{
    if (TCFG_LOWPOWER_POWER_SEL == PWR_DCDC15) {
        power_mode_detect();
    }
    ldoin_wakeup_isr();
}

static const struct _p33_io_wakeup_config charge_port = {
    .edge               = RISING_EDGE,      //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter             = PORT_FLT_16ms,
    .gpio               = IO_CHGFL_DET,     //唤醒口选择
    .callback			= chargefull_wakeup_callback,
};

static const struct _p33_io_wakeup_config vbat_port = {
    .edge               = BOTH_EDGE,        //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter      		= PORT_FLT_16ms,
    .gpio               = IO_VBTCH_DET,     //唤醒口选择
    .callback			= vpwr_chgdet_wakeup_callback,
};

static const struct _p33_io_wakeup_config ldoin_port = {
    .edge               = BOTH_EDGE,        //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter      		= PORT_FLT_16ms,
    .gpio               = IO_LDOIN_DET,     //唤醒口选择
    .callback			= vpwr_indet_wakeup_callback,
};

void charge_wakeup_init()
{
    p33_io_wakeup_port_init(&charge_port);
    p33_io_wakeup_enable(IO_CHGFL_DET, 1);

    p33_io_wakeup_port_init(&vbat_port);
    p33_io_wakeup_enable(IO_VBTCH_DET, 1);

    p33_io_wakeup_port_init(&ldoin_port);
    p33_io_wakeup_enable(IO_LDOIN_DET, 1);
}

static const struct charge_platform_data charge_data  = {
    .charge_en              = TCFG_CHARGE_ENABLE,                       //内置充电使能
    .charge_poweron_en      = TCFG_CHARGE_POWERON_ENABLE,               //是否支持充电开机
    .charge_full_V          = TCFG_CHARGE_FULL_V,                       //充电截止电压
    .charge_full_mA			= TCFG_CHARGE_FULL_MA,                      //充电截止电流
    .charge_mA				= TCFG_CHARGE_MA,                           //充电电流
    .charge_trickle_mA		= TCFG_CHARGE_TRICKLE_MA | TRICKLE_EN_FLAG, //涓流电流

    /* ldo5v拔出过滤值，过滤时间 = (filter*2 + 20)ms,
     * ldoin < 0.6V且时间大于过滤时间才认为拔出
     * 对于充满直接从5V掉到0V的充电仓，该值必须设置成0
     * 对于充满由5V先掉到0V之后再升压到xV的 充电仓，需要根据实际情况设置该值大小
     * */
    .ldo5v_off_filter		= TCFG_LDOIN_OFF_FILTER_TIME / 2,
    .ldo5v_on_filter        = TCFG_LDOIN_ON_FILTER_TIME / 2,
    .ldo5v_keep_filter      = TCFG_LDOIN_KEEP_FILTER_TIME / 2,
    .ldo5v_pulldown_lvl     = TCFG_LDOIN_PULLDOWN_LEV,
    .ldo5v_pulldown_keep    = TCFG_LDOIN_PULLDOWN_KEEP,

    /*
     * 1. 对于自动升压充电舱,若充电舱需要更大的负载才能检测到插入时，请将该变量置1
          并且根据需求配置下拉电阻档位
     * 2. 对于按键升压,并且是通过上拉电阻去提供维持电压的舱,请将该变量设置1,
          并且根据舱的上拉配置下拉需要的电阻挡位
     * 3. 对于常5V的舱,可将改变量设为0,省功耗
     */
    .ldo5v_pulldown_en		= TCFG_LDOIN_PULLDOWN_EN,
};

int board_charge_init()
{
    charge_wakeup_init();

    charge_init(&charge_data);

    if (get_charge_online_flag()) {
        power_set_mode(PWR_LDO15);
        p33_io_wakeup_filter(IO_VBTCH_DET, PORT_FLT_16ms);
    } else {
        power_set_mode(TCFG_LOWPOWER_POWER_SEL);
        if (TCFG_LOWPOWER_POWER_SEL == PWR_DCDC15) {
            p33_io_wakeup_filter(IO_VBTCH_DET, PORT_FLT_DISABLE);
        }
    }
    return 0;
}
static void board_charge_uninit()
{
    p33_io_wakeup_enable(IO_CHGFL_DET, 0);
}
platform_initcall(board_charge_init);

platform_uninitcall(board_charge_uninit);

#else

static void vpwr_chgdet_wakeup_callback(P33_IO_WKUP_EDGE edge)
{
    power_mode_detect();
}

static const struct _p33_io_wakeup_config vbat_port = {
    .edge               = BOTH_EDGE,        //唤醒方式选择,可选：上升沿\下降沿\双边沿
    .filter      		= PORT_FLT_16ms,
    .gpio               = IO_VBTCH_DET,     //唤醒口选择
    .callback			= vpwr_chgdet_wakeup_callback,
};

//没开启充电时,关闭漏电寄存器(约2uA)
#define VPWR_CHECK_COUNTER  10  //连续10ms
#define VPWR_CHECK_TIMEOUT  100 //最多检测100ms
int board_charge_init()
{
    u8 in_cnt = 0;
    u8 off_cnt = 0;
    u8 sum_in_cnt = 0;
    u8 sum_off_cnt = 0;
    u8 timeout_cnt = 0;
    L5V_IO_MODE(0);
    CHG_VILOOP_EN(0);
    CHG_VILOOP2_EN(0);
    if (TCFG_LOWPOWER_POWER_SEL == PWR_DCDC15) {
        L5V_RES_DET_S_SEL(CHARGE_PULLDOWN_200K);
        L5V_LOAD_EN(1);
        p33_io_wakeup_port_init(&vbat_port);
        p33_io_wakeup_enable(IO_VBTCH_DET, 1);
        while (1) {
            if (LVCMP_DET_GET()) {
                in_cnt++;
                off_cnt = 0;
                sum_in_cnt++;
                if (in_cnt > VPWR_CHECK_COUNTER) {
                    break;
                }
            } else {
                off_cnt++;
                in_cnt = 0;
                sum_off_cnt++;
                if (off_cnt > VPWR_CHECK_COUNTER) {
                    break;
                }
            }
            udelay(1000);
            timeout_cnt++;
            if (timeout_cnt > VPWR_CHECK_TIMEOUT) {
                in_cnt = sum_in_cnt;
                off_cnt = sum_off_cnt;
                break;
            }
        }
        L5V_LOAD_EN(0);
        if (in_cnt > off_cnt) {
            power_set_mode(PWR_LDO15);//vpwr > vbat: ldo mode
            p33_io_wakeup_filter(IO_VBTCH_DET, PORT_FLT_16ms);
        } else {
            power_set_mode(TCFG_LOWPOWER_POWER_SEL);
            p33_io_wakeup_filter(IO_VBTCH_DET, PORT_FLT_DISABLE);
        }
    } else {
        power_set_mode(TCFG_LOWPOWER_POWER_SEL);
    }
    return 0;
}
platform_initcall(board_charge_init);

static void board_charge_uninit()
{
    p33_io_wakeup_enable(IO_VBTCH_DET, 0);
}

platform_uninitcall(board_charge_uninit);

#endif

