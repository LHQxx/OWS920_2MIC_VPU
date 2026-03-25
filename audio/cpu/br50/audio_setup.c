#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_setup.data.bss")
#pragma data_seg(".audio_setup.data")
#pragma const_seg(".audio_setup.text.const")
#pragma code_seg(".audio_setup.text")
#endif
/*
 ******************************************************************
 *					      Audio Setup
 *
 * Discription: 音频模块初始化，配置，调试等
 *
 * Notes:
 ******************************************************************
 */
#include "cpu/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "audio_config.h"
#include "sdk_config.h"
#include "audio_adc.h"
#include "media/audio_energy_detect.h"
#include "adc_file.h"
#include "linein_file.h"
#include "asm/audio_common.h"
#include "gpio_config.h"
#include "audio_demo/audio_demo.h"
#include "gpadc.h"
#include "update.h"
#include "media/audio_general.h"
#include "asm/lpower_mem.h"
#include "effects/audio_eq.h"

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
#include "audio_dvol.h"
#endif

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif

#if TCFG_AUDIO_DUT_ENABLE
#include "audio_dut_control.h"
#endif/*TCFG_AUDIO_DUT_ENABLE*/

#if TCFG_SMART_VOICE_ENABLE
#include "smart_voice.h"
#endif /*TCFG_SMART_VOICE_ENABLE*/

struct audio_dac_hdl dac_hdl;
struct audio_adc_hdl adc_hdl;

typedef struct {
    u8 audio_inited;
    atomic_t ref;
} audio_setup_t;
audio_setup_t audio_setup = {0};
#define __this      (&audio_setup)

#if TCFG_MC_BIAS_AUTO_ADJUST
u8 mic_bias_rsel_use_save[AUDIO_ADC_MIC_MAX_NUM] = {0};
u8 save_mic_bias_rsel[AUDIO_ADC_MIC_MAX_NUM]     = {0};
u8 mic_ldo_vsel_use_save = 0;
u8 save_mic_ldo_vsel     = 0;
#endif // #if TCFG_MC_BIAS_AUTO_ADJUST

struct dac_platform_data dac_data;


void dac_power_on_hook()
{
    dev_mem_exit_low_power(DEV_DAC, DEV_MODE_PSD);
}

void dac_power_off_hook()
{
    dev_mem_enter_low_power(DEV_DAC, DEV_MODE_PSD);
}

int get_dac_channel_num(void)
{
    return dac_hdl.channel;
}

static u8 audio_dac_hpvdd_check()
{
    u8 res;
    u16 hpvdd = adc_get_voltage_blocking(AD_CH_AUDIO_HPVDD);
    /* printf("HPVDD: %d\n", hpvdd); */
    if (hpvdd > 1500) {
        res = DAC_HPVDD_18V;
        puts("DAC_HPVDD: 1.8V");
    } else {
        res = DAC_HPVDD_12V;
        puts("DAC_HPVDD: 1.2V");
    }
    SFR(JL_ADDA->ADDA_CON0,  0,  1,  0);					// AUDIO到SARADC的总测试通道使能
    SFR(JL_ADDA->ADDA_CON0,  3,  1,  0);					// DAC测试通道总使能
    SFR(JL_ADDA->ADDA_CON0,  4,  3,  0);					// DAC待测试信号选择位
    return res;
}

/*
 * DAC MUTE/UNMUTE 回调
 */
#if 0
void audio_dac_ch_mute_notify(u8 mute_state, u8 step)
{
}
#endif

static void audio_common_initcall()
{
    audio_common_param_t common_param = {0};
    common_param.cic.en = TCFG_AUDIO_ANC_ENABLE;
    common_param.cic.scale = 0;
    common_param.cic.shift = 15;
    common_param.drc.bypass = 1;
    common_param.drc.threshold = 1023;
    common_param.drc.ratio = 64;
    common_param.drc.kneewidth = 16;
    common_param.drc.makeup_gain = 0;
    common_param.drc.attack_time = 3;
    common_param.drc.release_time = 10;

    //audio vbg trim配置
    common_param.vbg_trim_value = efuse_get_audio_vbg_trim();
    if (common_param.vbg_trim_value == 0x1F) {
        common_param.vbg_trim_value = 20;
        printf("[Warning]audio vbg trim value invalid,default=%d\n", common_param.vbg_trim_value);
    }
    common_param.vbg_i_trim_value = ((JL_ADDA->ADDA_CON0 >> 25) & 0xF);
    if (common_param.vbg_i_trim_value == 0) {
        common_param.vbg_i_trim_value = 8;
        printf("[Warning]audio vbg i trim value invalid,default=%d\n", common_param.vbg_i_trim_value);
    }

    /* common_param.clock_mode = AUDIO_COMMON_CLK_DIG_SINGLE; */
    common_param.clock_mode = AUDIO_COMMON_CLK_DIF_XOSC;
    /* common_param.clock_mode = AUDIO_COMMON_CLK_DIG_XOSC; //低功耗配置时钟 */
    audio_common_init(&common_param);
    common_param.aud_en = 1;
    audio_common_clock_open();
}

void audio_dac_initcall(void)
{
    printf("audio_dac_initcall\n");

    dac_data.max_sample_rate    = AUDIO_DAC_MAX_SAMPLE_RATE;
    dac_data.hpvdd_sel = audio_dac_hpvdd_check();
    dac_data.bit_width = audio_general_out_dev_bit_width();
    dac_data.mute_delay_isel = 1;
    dac_data.mute_delay_time = 20;
    dac_data.fast_close = 0;
    audio_dac_init(&dac_hdl, &dac_data);
    //dac_hdl.ng.threshold = 4;			//DAC底噪优化阈值
    //dac_hdl.ng.detect_interval = 200;	//DAC底噪优化检测间隔ms

    //ANC & DAC_CIC时钟分配参数设置在audio_common_init & audio_dac_init之后
#if TCFG_AUDIO_ANC_ENABLE
    /*
       1、根据ANC参数生成CLOCK DIV以及ANC & DAC CIC配置
       2、读取ANC DAC DRC配置
    */
    audio_anc_common_param_init();
#endif/*TCFG_AUDIO_ANC_ENABLE*/

    audio_dac_set_analog_vol(&dac_hdl, 0);

#if AUD_DAC_TRIM_ENABLE
    struct audio_dac_trim dac_trim = {0};
    int len = syscfg_read(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(dac_trim));
    if (len != sizeof(dac_trim)) {
        struct trim_init_param_t trim_init = {0};
        trim_init.precision = 1; //DAC trim的收敛精度(-precision, +precision)
        int trim_offset;
        int trim_limit;
        if ((JL_SYSTEM->CHIP_VER >= 0xA3) && (JL_SYSTEM->CHIP_VER < 0xAC)) { //D版以后DAC TRIM参数需要调整
            printf("DAC trim:chip version >= Version-D");
            trim_offset = -850;
            trim_limit = 350;
        } else {
            printf("DAC trim:chip version <= Version-C");
            trim_offset = 0;
            trim_limit = 50;
        }
        int ret = audio_dac_do_trim(&dac_hdl, &dac_trim, &trim_init);
        if ((ret == 0) && (__builtin_abs(dac_trim.left - trim_offset) < trim_limit) && (__builtin_abs(dac_trim.right - trim_offset) < trim_limit)) {
            AUD_STDLOG_DAC_TRIM_SUCC();
            syscfg_write(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(struct audio_dac_trim));
        } else {
            AUD_STDLOG_DAC_TRIM_ERROR();
            dac_trim.left = trim_offset;
            dac_trim.right = trim_offset;
        }
        audio_dac_close(&dac_hdl);
        syscfg_write(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(dac_trim));
    }
    audio_dac_set_trim_value(&dac_hdl, &dac_trim);
#endif

    audio_dac_set_fade_handler(&dac_hdl, NULL, audio_fade_in_fade_out);

    /*硬件SRC模块滤波器buffer设置，可根据最大使用数量设置整体buffer*/
    /* audio_src_base_filt_init(audio_src_hw_filt, sizeof(audio_src_hw_filt)); */

#if AUDIO_OUTPUT_AUTOMUTE
    mix_out_automute_open();
#endif  //#if AUDIO_OUTPUT_AUTOMUTE
}

static u8 audio_init_complete()
{
    if (!__this->audio_inited) {
        return 0;
    }
    return 1;
}

REGISTER_LP_TARGET(audio_init_lp_target) = {
    .name = "audio_init",
    .is_idle = audio_init_complete,
};

struct audio_adc_private_param adc_private_param = {
    .performance_mode = TCFG_ADC_PERFORMANCE_MODE,
    .mic_ldo_vsel   = TCFG_AUDIO_MIC_LDO_VSEL,
    /* .mic_ldo_isel   = TCFG_AUDIO_MIC_LDO_ISEL, */
    .adca_reserved0 = 0,
    .adcb_reserved0 = 0,
    .lowpower_lvl = 0,
};


#if TCFG_AUDIO_ADC_ENABLE
const struct adc_platform_cfg adc_platform_cfg_table[AUDIO_ADC_MAX_NUM] = {
#if TCFG_ADC0_ENABLE
    [0] = {
        .mic_mode           = TCFG_ADC0_MODE,
        .mic_ain_sel        = TCFG_ADC0_AIN_SEL,
        .mic_bias_sel       = TCFG_ADC0_BIAS_SEL,
        .mic_bias_rsel      = TCFG_ADC0_BIAS_RSEL,
        .power_io           = TCFG_ADC0_POWER_IO,
        .mic_dcc_en         = TCFG_ADC0_DCC_EN,
        .mic_dcc            = TCFG_ADC0_DCC_LEVEL,
    },
#endif
#if TCFG_ADC1_ENABLE
    [1] = {
        .mic_mode           = TCFG_ADC1_MODE,
        .mic_ain_sel        = TCFG_ADC1_AIN_SEL,
        .mic_bias_sel       = TCFG_ADC1_BIAS_SEL,
        .mic_bias_rsel      = TCFG_ADC1_BIAS_RSEL,
        .power_io           = TCFG_ADC1_POWER_IO,
        .mic_dcc_en         = TCFG_ADC1_DCC_EN,
        .mic_dcc            = TCFG_ADC1_DCC_LEVEL,
    },
#endif
#if TCFG_ADC2_ENABLE
    [2] = {
        .mic_mode           = TCFG_ADC2_MODE,
        .mic_ain_sel        = TCFG_ADC2_AIN_SEL,
        .mic_bias_sel       = TCFG_ADC2_BIAS_SEL,
        .mic_bias_rsel      = TCFG_ADC2_BIAS_RSEL,
        .power_io           = TCFG_ADC2_POWER_IO,
        .mic_dcc_en         = TCFG_ADC2_DCC_EN,
        .mic_dcc            = TCFG_ADC2_DCC_LEVEL,
    },
#endif
#if TCFG_ADC3_ENABLE
    [3] = {
        .mic_mode           = TCFG_ADC3_MODE,
        .mic_ain_sel        = TCFG_ADC3_AIN_SEL,
        .mic_bias_sel       = TCFG_ADC3_BIAS_SEL,
        .mic_bias_rsel      = TCFG_ADC3_BIAS_RSEL,
        .power_io           = TCFG_ADC3_POWER_IO,
        .mic_dcc_en         = TCFG_ADC3_DCC_EN,
        .mic_dcc            = TCFG_ADC3_DCC_LEVEL,
    },
#endif
#if TCFG_ADC4_ENABLE
    [4] = {
        .mic_mode           = TCFG_ADC4_MODE,
        .mic_ain_sel        = TCFG_ADC4_AIN_SEL,
        .mic_bias_sel       = TCFG_ADC4_BIAS_SEL,
        .mic_bias_rsel      = TCFG_ADC4_BIAS_RSEL,
        .power_io           = TCFG_ADC4_POWER_IO,
        .mic_dcc_en         = TCFG_ADC4_DCC_EN,
        .mic_dcc            = TCFG_ADC4_DCC_LEVEL,
    },
#endif
#if TCFG_LPADC_ENABLE
    [5] = {
        .mic_mode           = TCFG_LPADC_MODE,
        .mic_ain_sel        = TCFG_LPADC_AIN_SEL,
        .mic_bias_sel       = TCFG_LPADC_BIAS_SEL,
        .mic_bias_rsel      = TCFG_LPADC_BIAS_RSEL,
        .power_io           = TCFG_LPADC_POWER_IO,
        .mic_dcc_en         = TCFG_LPADC_DCC_EN,
        .mic_dcc            = TCFG_LPADC_DCC_LEVEL,
    },
#endif
};
#endif

void audio_input_initcall(void)
{
    printf("audio_input_initcall\n");
#if TCFG_MC_BIAS_AUTO_ADJUST
    if (mic_ldo_vsel_use_save) {
        adc_private_param.mic_ldo_vsel = save_mic_ldo_vsel;
    }
#endif

    u16 dvol_441k = (u16)(50 * eq_db2mag(TCFG_ADC_DIGITAL_GAIN));
    u16 dvol_48k  = (u16)(35 * eq_db2mag(TCFG_ADC_DIGITAL_GAIN));
    adc_private_param.dvol_441k = (dvol_441k >= AUDIO_ADC_DVOL_LIMIT) ? AUDIO_ADC_DVOL_LIMIT : dvol_441k;
    adc_private_param.dvol_48k = (dvol_48k >= AUDIO_ADC_DVOL_LIMIT) ? AUDIO_ADC_DVOL_LIMIT : dvol_48k;
    audio_adc_init(&adc_hdl, &adc_private_param);
    /* adc_hdl.bit_width = audio_general_in_dev_bit_width(); */
    audio_adc_file_init();

#if TCFG_AUDIO_DUT_ENABLE
    audio_dut_init();
#endif /*TCFG_AUDIO_DUT_ENABLE*/

#if TCFG_AUDIO_LINEIN_ENABLE
    audio_linein_file_init();
#endif/*TCFG_AUDIO_LINEIN_ENABLE*/
}

struct dac_platform_data dac_data = {//临时处理
    .power_on_mode  = TCFG_AUDIO_DAC_POWER_ON_MODE,
    .dma_buf_time_ms = TCFG_AUDIO_DAC_BUFFER_TIME_MS,
    .performance_mode = TCFG_DAC_PERFORMANCE_MODE,
    .l_ana_gain     = TCFG_AUDIO_L_CHANNEL_GAIN,
    .r_ana_gain     = TCFG_AUDIO_R_CHANNEL_GAIN,
    .dcc_level      = 15,
    .bit_width      = DAC_BIT_WIDTH_16,
    .fade_en        = 1,
    .fade_points    = 4,
    .fade_volume    = 1,
#if (TCFG_DAC_PERFORMANCE_MODE == DAC_MODE_HIGH_PERFORMANCE)
    .pa_isel0       = TCFG_AUDIO_DAC_HP_PA_ISEL0,
    .pa_isel1       = TCFG_AUDIO_DAC_HP_PA_ISEL1,
#else
    .pa_isel0       = TCFG_AUDIO_DAC_LP_PA_ISEL0,
    .pa_isel1       = TCFG_AUDIO_DAC_LP_PA_ISEL1,
#endif
};

static void wl_audio_clk_on(void)
{
    JL_WL_AUD->CON0 = 1;
    /*JL_ASS->CLK_CON |= BIT(0);//audio时钟*/
}

static int audio_init()
{
    wl_audio_clk_on();

    audio_general_init();

    audio_input_initcall();

#if TCFG_AUDIO_ANC_ENABLE
    anc_init();
#endif

#if (SYS_VOL_TYPE == VOL_TYPE_DIGITAL)
    audio_digital_vol_init(NULL, 0);
#endif

#if (TCFG_DAC_NODE_ENABLE || TCFG_AUDIO_ADC_ENABLE)
    audio_common_initcall();
#endif
    //AC700N DAC初始化在ANC之后，因为需要读取ANC 采样率以及DAC DRC配置
#if TCFG_DAC_NODE_ENABLE
    audio_dac_initcall();
#endif

#if TCFG_EQ_ENABLE
    audio_eq_lib_init();
#endif

#if TCFG_SMART_VOICE_ENABLE
    audio_smart_voice_detect_init(NULL);
#endif /* #if TCFG_SMART_VOICE_ENABLE */
    __this->audio_inited = 1;
    return 0;
}
platform_initcall(audio_init);

static void audio_uninit()
{
#if TCFG_DAC_NODE_ENABLE
    audio_dac_close(&dac_hdl);
#endif
}
platform_uninitcall(audio_uninit);

/*关闭audio相关模块使能*/
static void audio_disable_all(void)
{
    printf("audio_disable_all\n");
#if TCFG_DAC_NODE_ENABLE
    audio_dac_close(&dac_hdl);
#endif
    //DAC:DACEN
    JL_AUD->AUD_CON0 &= ~BIT(2);
    //ADC:ADCEN
    JL_AUD->AUD_CON0 &= ~(BIT(1) | BIT(4) | BIT(9));
    //EQ:
    JL_EQ->CON0 &= ~BIT(1);
    //FFT:
    //JL_FFT->CON = BIT(1);//置1强制关闭模块，不管是否已经运算完成
    //SRC:
    JL_SRC0->CON1 |= BIT(22);
    JL_SRC1->CON0 |= BIT(10);

    //ANC:anc_en anc_start
#if 0//build
    JL_ANC->CON0 &= ~(BIT(1) | BIT(29));
#endif

}

REGISTER_UPDATE_TARGET(audio_update_target) = {
    .name = "audio",
    .driver_close = audio_disable_all,
};

