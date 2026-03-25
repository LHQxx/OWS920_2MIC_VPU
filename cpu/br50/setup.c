#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".setup.data.bss")
#pragma data_seg(".setup.data")
#pragma const_seg(".setup.text.const")
#pragma code_seg(".setup.text")
#endif
#include "cpu/includes.h"
#include "system/includes.h"
#include "app_config.h"
#include "asm/lpower_mem.h"
#include "sdk_config.h"

#define LOG_TAG             "[SETUP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

extern void sys_timer_init(void);
extern void tick_timer_init(void);
extern void exception_irq_handler(void);
extern int __crc16_mutex_init();
extern void debug_uart_init();
extern void boot_power_init();

#if (TCFG_POWER_OFF_VOLTAGE < 3300)
#error "JL708 cant support poweroff voltage under 3300!"
#endif

/* --------------------------------------------------------------------------*/
/**
 * @brief 裸机程序入口
 */
/* ----------------------------------------------------------------------------*/
void system_main(void)
{
    //分支预测
    q32DSP(core_num())->PMU_CON1 &= ~BIT(8);

    wdt_close();

#if (TCFG_MAX_LIMIT_SYS_CLOCK==MAX_LIMIT_SYS_CLOCK_160M)
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 240 * MHz);//  240:max clock 160
#else
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 192 * MHz);
#endif

    //heap内存管理，STDIO需要heap
    memory_init();

    //uart init
    debug_uart_init();

    //STDIO init
    log_early_init(1024 * 2);

    //debug info

    while (1) {
        asm("idle");
    }
}

/* --------------------------------------------------------------------------*/
/**
 * @brief 裸机环境启动，进入无操作系统环境
 */
/* ----------------------------------------------------------------------------*/
#define SYSTEM_SSP_SIZE 512
u32 system_ssp_stack[SYSTEM_SSP_SIZE];//2Kbyte
void system_start(void)
{
    asm("ssp = %0"::"i"(system_ssp_stack + SYSTEM_SSP_SIZE));
    asm("r0 = %0"::"i"(system_main));
    asm("reti = r0");
    asm("rti");
}


#if CONFIG_DEBUG_ENABLE || CONFIG_DEBUG_LITE_ENABLE
#endif

#if 0
___interrupt
void exception_irq_handler(void)
{
    ___trig;

    exception_analyze();

    log_flush();
    while (1);
}
#endif



/*
 * 此函数在cpu0上电后首先被调用,负责初始化cpu内部模块
 *
 * 此函数返回后，操作系统才开始初始化并运行
 *
 */

#if 0
static void early_putchar(char a)
{
    if (a == '\n') {
        UT2_BUF = '\r';
        __asm_csync();
        while ((UT2_CON & BIT(15)) == 0);
    }
    UT2_BUF = a;
    __asm_csync();
    while ((UT2_CON & BIT(15)) == 0);
}

void early_puts(char *s)
{
    do {
        early_putchar(*s);
    } while (*(++s));
}
#endif

void cpu_assert_debug()
{
#if CONFIG_DEBUG_ENABLE
    log_flush();
    local_irq_disable();
    while (1);
#else
    P3_PCNT_SET0 = 0xac;
    cpu_reset();
#endif
}

_NOINLINE_
void cpu_assert(char *file, int line, bool condition, char *cond_str)
{
    if (config_asser) {
        if (!(condition)) {
            printf("cpu %d file:%s, line:%d\n", current_cpu_id(), file, line);
            printf("ASSERT-FAILD: %s\n", cond_str);
            cpu_assert_debug();
        }
    } else {
        if (!(condition)) {
            assert_reset();
        }
    }
}

extern void sputchar(char c);
extern void sput_buf(const u8 *buf, int len);
void sput_u32hex(u32 dat);



__attribute__((weak))
void maskrom_init(void)
{
    return;
}


#if (CPU_CORE_NUM > 1)
void cpu1_setup_arch()
{
    q32DSP(core_num())->PMU_CON1 &= ~BIT(8); //open bpu

    request_irq(IRQ_EXCEPTION_IDX, 7, exception_irq_handler, 1);

    //用于控制其他核进入停止状态。
    extern void cpu_suspend_handle(void);
    request_irq(IRQ_SOFT0_IDX + OS_SYNC_SOFT_IRQ_ID, 7, cpu_suspend_handle, 0);
    request_irq(IRQ_SOFT0_IDX + OS_SYNC_SOFT_IRQ_ID, 7, cpu_suspend_handle, 1);
    irq_unmask_set(IRQ_SOFT0_IDX + OS_SYNC_SOFT_IRQ_ID, 0, 0); //设置CPU0软中断0为不可屏蔽中断
    irq_unmask_set(IRQ_SOFT0_IDX + OS_SYNC_SOFT_IRQ_ID, 0, 1); //设置CPU1软中断1为不可屏蔽中断

    debug_init();
}

void cpu1_main()
{
    extern void cpu1_run_notify(void);
    cpu1_run_notify();

    interrupt_init();

    q32DSP(core_num())->PMU_CON1 &= ~BIT(8); //分支预测
    cpu1_setup_arch();

    os_start();

    log_e("os err \r\n") ;
    while (1) {
        __asm__ volatile("idle");
    }
}

#else

void cpu1_main()
{

}
#endif /* #if (CPU_CORE_NUM > 1) */

//==================================================//

/* extern void gpio_longpress_pin0_reset_config(u32 pin, u32 level, u32 time); */
void memory_init(void);

__attribute__((weak))
void app_main()
{
    while (1) {
        asm("idle");
    }
}

void port_hd_init(u32 hd_lev)
{
    u16 port_hd_mask[3];

    port_hd_mask[0] = -1;
    port_hd_mask[1] = -1;
    port_hd_mask[2] = -1;

    switch (hd_lev) {
    case 0:
        break;
    case 1:
        JL_PORTA->HD0 |= port_hd_mask[0];
        JL_PORTB->HD0 |= port_hd_mask[1];
        JL_PORTC->HD0 |= port_hd_mask[2];
        break;
    }
}
void setup_arch()
{
    //IO开1档强驱，避免IO短路的时候烧毁IO
    //开启后需要确认是否对蓝牙，audio，EMI指标造成影响
    port_hd_init(1);

    boot_power_init();

    //system_start();

    //关闭所有timer的ie使能
    bit_clr_ie(IRQ_TIME0_IDX);
    bit_clr_ie(IRQ_TIME1_IDX);
    bit_clr_ie(IRQ_TIME2_IDX);
    bit_clr_ie(IRQ_TIME3_IDX);
    bit_clr_ie(IRQ_TIME4_IDX);
    bit_clr_ie(IRQ_TIME5_IDX);

    /* gpio_longpress_pin0_reset_config(IO_PORTB_01, 0, 0); */

    memory_init();

    wdt_init(WDT_16S);
    /* wdt_close(); */
    q32DSP(core_num())->PMU_CON1 &= ~BIT(8); //分支预测

    gpadc_mem_init(8);
    efuse_init();

    sdfile_init();
    syscfg_tools_init();

    /* clk_voltage_init(TCFG_CLOCK_MODE, SYSVDD_VOL_SEL_126V); */
    /* xosc_hcs_trim(); */

#if (TCFG_MAX_LIMIT_SYS_CLOCK==MAX_LIMIT_SYS_CLOCK_160M)
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 240 * MHz);//  240:max clock 160
#else
    clk_early_init(PLL_REF_XOSC_DIFF, TCFG_CLOCK_OSC_HZ, 192 * MHz);
#endif
    /* clk_set_osc_cap(0,0); */

    //临时添加：by IC wangjie
    SFR(JL_LSBCLK->PRP_CON1,  0, 3, 1);
    SFR(JL_LSBCLK->PRP_CON1, 26, 1, 1);
    // bt clk config by IC luokaijie
    SFR(JL_LSBCLK->PRP_CON2, 0, 2, 1);
    SFR(JL_LSBCLK->PRP_CON2, 2, 2, 1);
    SFR(JL_LSBCLK->PRP_CON2, 4, 2, 1);
    SFR(JL_LSBCLK->PRP_CON2, 8, 1, 1);
    asm("csync");

    /* dev_mem_low_power_init(); // 需要依赖时钟的初始化 */
    os_init();
    tick_timer_init();
    sys_timer_init();

#if CONFIG_DEBUG_ENABLE || CONFIG_DEBUG_LITE_ENABLE
    debug_uart_init();
#if CONFIG_DEBUG_ENABLE
    log_early_init(1024 * 2);
#endif

#endif
    log_i("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    log_i("         setup_arch");
    log_i("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

    power_early_flowing();

    clock_dump();

    void mvbg_current_trim();        //pmu used
    mvbg_current_trim();
    void audio_vbg_current_trim();        //audio used
    audio_vbg_current_trim();

    //Register debugger interrupt
    request_irq(0, 2, exception_irq_handler, 0);
    request_irq(1, 2, exception_irq_handler, 0);
    code_movable_init();
    debug_init();

    __crc16_mutex_init();

    ASSERT((u32)local_irq_disable < 0x120000);
    app_main();
}


