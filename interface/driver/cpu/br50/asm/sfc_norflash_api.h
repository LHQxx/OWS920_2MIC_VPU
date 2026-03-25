#ifndef __SFC_NORFLASH_API_H__
#define __SFC_NORFLASH_API_H__

#include "typedef.h"
#include "device.h"



/**
 * @brief 设置内置flash的数据线宽, 通过打印 get_sfc_read_mode() 返回值判断:1线，2线，4线
 *
 * @param data_width: 支持切换2线，4线
 * @return
 */
void  inside_flash_switch_data_width(u32 data_width);

int norflash_init(const struct dev_node *node, void *arg);
int norflash_open(const char *name, struct device **device, void *arg);
int norflash_read(struct device *device, void *buf, u32 len, u32 offset);
int norflash_write(struct device *device, void *buf, u32 len, u32 offset);
int norflash_ioctl(struct device *device, u32 cmd, u32 arg);


#endif
