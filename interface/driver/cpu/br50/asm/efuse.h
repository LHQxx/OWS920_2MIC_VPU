#ifndef  __EFUSE_H__
#define  __EFUSE_H__


u32 efuse_get_chip_id();
u16 get_chip_id();
u32 efuse_get_vbat_trim_4p20();

u32 efuse_get_gpadc_vbg_trim();



#define CHIP_VERSION_A      0x00
#define CHIP_VERSION_B      0x01
#define CHIP_VERSION_C      0x02

u32 get_chip_version();



u8 efuse_get_lvd_act();
u8 efuse_get_vddio_lvd_lev();
u8 efuse_get_lvd_bg_trim();
u8 efuse_get_vio_act();
u8 efuse_get_vddio_lev();
u8 efuse_get_flash_18v();
u8 efuse_get_sfc_fast_boot_dis();
u8 efuse_get_pin_reset_en();
u8 efuse_get_anc_enable();
u8 efuse_get_vbg_act();
u8 efuse_get_mvbg_lev();
u8 efuse_get_en_wvbg_lev();
u8 efuse_get_cp_pass();
u8 efuse_get_ft_pass();
u8 efuse_get_wvdd_level();
u16 efuse_get_vbat_trim();
u16 efuse_get_vbat_trim_4p35(void);
u16 efuse_get_charge_cur_trim(void);
u16 efuse_get_io_pu_100k(void);
u16 efuse_get_vtemp(void);
u16 efuse_get_pmu_act(void);

u8 efuse_get_btvbg_xosc_trim();
u8 efuse_get_btvbg_syspll_trim();
u8 efuse_get_bt_bg_20k();
u8 efuse_get_bttx_pwr_trim();
u8 efuse_get_dcvdd12();
u8 efuse_get_dcvdd18();
u8 efuse_get_bt_cp_act();
u8 efuse_get_bt_ft_act();
u8 efuse_get_audio_vbg_trim();
u8 efuse_get_btvbg_bg_20k();
u8 efuse_get_wvdd_level_trim();

void   efuse_init();

#endif  /*EFUSE_H*/
