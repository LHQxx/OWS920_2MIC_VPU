#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".audio_config.data.bss")
#pragma data_seg(".audio_config.data")
#pragma const_seg(".audio_config.text.const")
#pragma code_seg(".audio_config.text")
#endif
/*
 ******************************************************************************************
 *							Audio Config
 *
 * Discription: 音频模块与芯片系列相关配置
 *
 * Notes:
 ******************************************************************************************
 */
#include "cpu/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "audio_config.h"
#include "clock_manager/clock_manager.h"

/*
 *******************************************************************
 *						Audio Codec Config
 *******************************************************************
 */
const int config_audio_dac_mix_enable = 1;

//***********************
//*		AAC Codec       *
//***********************
const int AAC_DEC_MP4A_LATM_ANALYSIS = 1;

#ifdef AAC_DEC_IN_MASK
const int AAC_DEC_LIB_SUPPORT_24BIT_OUTPUT = 0;
#else
const int AAC_DEC_LIB_SUPPORT_24BIT_OUTPUT = 1;
#endif

#ifdef WTS_DEC_IN_MASK
const int WTS_DEC_LIB_SUPPORT_24BIT_OUTPUT = 0;
#else
const int WTS_DEC_LIB_SUPPORT_24BIT_OUTPUT = 1;
#endif
//***********************
//*		MP3 Codec       *
//***********************
#ifdef MP3_DEC_IN_MASK
const int MP3_DEC_LIB_SUPPORT_24BIT_OUTPUT = 0;
#else
const int MP3_DEC_LIB_SUPPORT_24BIT_OUTPUT = 1;
#endif

//***********************
//*		Audio Params    *
//***********************
void audio_adc_param_fill(struct mic_open_param *mic_param, struct adc_platform_cfg *platform_cfg)
{
    mic_param->mic_mode      = platform_cfg->mic_mode;
    mic_param->mic_ain_sel   = platform_cfg->mic_ain_sel;
    mic_param->mic_bias_sel  = platform_cfg->mic_bias_sel;
    mic_param->mic_bias_rsel = platform_cfg->mic_bias_rsel;
    mic_param->mic_dcc       = platform_cfg->mic_dcc;
    mic_param->mic_dcc_en    = platform_cfg->mic_dcc_en;
}
void audio_linein_param_fill(struct linein_open_param *linein_param, const struct adc_platform_cfg *platform_cfg)
{
    linein_param->linein_mode    = platform_cfg->mic_mode;
    linein_param->linein_ain_sel = platform_cfg->mic_ain_sel;
    linein_param->linein_dcc     = platform_cfg->mic_dcc;
}

bool contains_str_SRC(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++) {
        //识别是否为SRC
        if (str[i] == 'S' && str[i + 1] == 'R' && str[i + 2] == 'C') {
            return true;
        }
    }
    return false;
}

//***********************
//*		Audio Clock     *
//***********************
int aud_clock_alloc(const char *name, u32 clk)
{
    if (contains_str_SRC(name)) {
        y_printf("aud_clock_alloc:%s(%d), abandon!\n", name, clk);
        return 0;
    }
    y_printf("aud_clock_alloc:%s(%d)\n", name, clk);
    return clock_alloc(name, clk);
}

int aud_clock_free(char *name)
{
    y_printf("aud_clock_free:%s\n", name);
    return clock_free(name);

}
