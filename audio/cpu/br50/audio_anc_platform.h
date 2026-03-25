
#ifndef _AUDIO_ANC_PLATFORM_H_
#define _AUDIO_ANC_PLATFORM_H_

#include "app_config.h"
#include "generic/typedef.h"
#include "anc.h"

/******************************************************************************/
/*                     平台相关宏定义 禁止修改                                */
/******************************************************************************/
#define AUDIO_ANC_COEFF_SMOOTH_ENABLE		   1	//支持无缝切换滤波器
#define TCFG_AUDIO_ANC_MAX_ORDER               10	//最大滤波器个数
#define ANC_MUSIC_DYNAMIC_GAIN_SINGLE_FB       0	//动态增益仅修改FB


void audio_anc_platform_param_init(audio_anc_t *param);
void audio_anc_platform_gains_printf(u8 cmd);
void audio_anc_platform_task_msg(int *msg);
void audio_anc_platform_clk_update(void);

#endif
