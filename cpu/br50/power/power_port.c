#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".power_port.data.bss")
#pragma data_seg(".power_port.data")
#pragma const_seg(".power_port.text.const")
#pragma code_seg(".power_port.text")
#endif
#include "asm/power_interface.h"
#include "iokey.h"
#include "irkey.h"
#include "adkey.h"
#include "app_config.h"

void gpio_config_soft_poweroff(void)
{
    PORT_TABLE(g);

#if TCFG_IOKEY_ENABLE
    PORT_PROTECT(get_iokey_power_io());
#endif

#if TCFG_ADKEY_ENABLE
    PORT_PROTECT(get_adkey_io());
#endif

    __port_init((u32)gpio_config);
}
