#ifndef __PMU_FLAG_H__
#define __PMU_FLAG_H__

enum soft_flag_io_stage {
    SOFTFLAG_HIGH_RESISTANCE,
    SOFTFLAG_PU,
    SOFTFLAG_PD,

    SOFTFLAG_OUT0,
    SOFTFLAG_OUT0_HD0,
    SOFTFLAG_OUT0_HD,
    SOFTFLAG_OUT0_HD0_HD,

    SOFTFLAG_OUT1,
    SOFTFLAG_OUT1_HD0,
    SOFTFLAG_OUT1_HD,
    SOFTFLAG_OUT1_HD0_HD,
};

struct soft_flag0_t {
    u8 wdt_dis: 1;
    u8 poweroff: 1;
    u8 lvd_en: 1;
    u8 flash_power_keep: 1;
    u8 skip_flash_reset: 1;
    u8 sfc_fast_boot: 1;
    u8 flash_stable_delay_sel: 2;
};

struct soft_flag1_t {
    u8 usbdp: 4;
    u8 usbdm: 4;
};

struct soft_flag2_t {
    u8 res: 8;
};

struct soft_flag3_t {
    u8 pp0: 4;
    u8 disable_uart_upgrade: 1;
    u8 res: 3;
};

struct soft_flag4_t {
    u8 uart_key_port_pull_down : 1;
    u8 flash_spi_baud : 2;
    u8 res: 4;
};

struct boot_soft_flag_t {
    u8 soff_wkup;
    union {
        struct soft_flag0_t boot_ctrl;
        u8 value;
    } flag0;
    union {
        struct soft_flag1_t boot_ctrl;
        u8 value;
    } flag1;
    union {
        struct soft_flag2_t boot_ctrl;
        u8 value;
    } flag2;
    union {
        struct soft_flag3_t boot_ctrl;
        u8 value;
    } flag3;
    union {
        struct soft_flag4_t boot_ctrl;
        u8 value;
    } flag4;
};

#endif
