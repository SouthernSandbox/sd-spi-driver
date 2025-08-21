/**
 * @file sd_spi_driver.h
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 可由用户引用的库头文件
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef SD_SPI_DRIVER_H
#define SD_SPI_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sd_config.h"
#include "sd_def.h"

#define SD_DRV_MAIN_VER     1
#define SD_DRV_SUB_VER      0
#define SD_DRV_UPDATE_STR   "2025.08.21"


enum sd_error   sd_spi_lib_init  (void);
struct sd_card* sd_card_find     (const char* name);

enum sd_error   sd_card_init    (struct sd_card* card);
enum sd_error   sd_card_deinit  (struct sd_card* card);
enum sd_error   sd_card_read    (struct sd_card* card, const uint64_t addr, uint8_t* buf, const uint32_t len);
enum sd_error   sd_card_write   (struct sd_card* card, const uint64_t addr, const uint8_t* buf, const uint32_t len);

enum sd_error   sd_card_erase_sector  (struct sd_card* card, const uint64_t addr, const uint32_t count);
enum sd_error   sd_card_erase_chip    (struct sd_card* card);

const char*     sd_card_get_name        (struct sd_card* card);
uint64_t        sd_card_get_capacity    (struct sd_card* card);
enum sd_type    sd_card_get_type        (struct sd_card* card);
uint32_t        sd_card_get_block_size  (struct sd_card* card);
uint64_t        sd_card_get_erase_size  (struct sd_card* card);
bool            sd_card_is_inserted     (struct sd_card* card);
void            sd_card_set_user_data   (struct sd_card* card, void* data);
void*           sd_card_get_user_data   (struct sd_card* card);

const char*     sd_get_capacity_class_name  (enum sd_type type);

#ifdef __cplusplus
}
#endif

#endif  // SD_SPI_DRIVER_H
