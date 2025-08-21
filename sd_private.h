/**
 * @file sd_private.h
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 适用于内部使用的私有头文件，不应该被外部使用
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef SD_PRIVATE_H
#define SD_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sd_config.h"

#if (SD_SPI_TRACE_ENABLE == 1)
    #define trace(_card, _color, _fmt, ...)       \
        do \
        { \
            if (_card->debug_if != NULL && _card->debug_if->print != NULL) \
                _card->debug_if->print(_card, _color "[%s:%d] %s: " _fmt "\033[0m\r\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        } while (0)

    #if (SD_SPI_TRACE_LEVEL >= SD_SPI_TRACE_LEVEL_DEBUG)
        #define trace_d(_card, _fmt,...)    trace(_card, "\033[37m", _fmt, ##__VA_ARGS__)
    #else
        #define trace_d(_card, _fmt,...)
    #endif

    #if (SD_SPI_TRACE_LEVEL >= SD_SPI_TRACE_LEVEL_INFO)
        #define trace_i(_card, _fmt,...)    trace(_card, "\033[32m", _fmt, ##__VA_ARGS__)
    #else
        #define trace_i(_card, _fmt,...)
    #endif

    #if (SD_SPI_TRACE_LEVEL >= SD_SPI_TRACE_LEVEL_WARN)
        #define trace_w(_card, _fmt,...)    trace(_card, "\033[33m", _fmt, ##__VA_ARGS__)
    #else
        #define trace_w(_card, _fmt,...)
    #endif

    #if (SD_SPI_TRACE_LEVEL >= SD_SPI_TRACE_LEVEL_ERROR)
        #define trace_e(_card, _fmt,...)    trace(_card, "\033[31m", _fmt, ##__VA_ARGS__)
    #else
        #define trace_e(_card, _fmt,...)
    #endif

    #if (SD_SPI_TRACE_LEVEL >= SD_SPI_TRACE_LEVEL_LIB)
        #define trace_l(_card, _fmt,...)       \
            do \
            { \
                if (_card->debug_if != NULL && _card->debug_if->print != NULL) \
                    _card->debug_if->print(_card, "\033[34;1m" _fmt "\033[0m\r\n", ##__VA_ARGS__); \
            } while (0)
    #else
        #define trace_l(card, fmt,...)
    #endif

#else
    #define trace_l(card, fmt,...)
    #define trace_e(card, fmt,...)
    #define trace_w(card, fmt,...)
    #define trace_i(card, fmt,...)
    #define trace_d(card, fmt,...)
    #define trace(_card, _level, _fmt,...)
#endif




enum sd_error sd_spi_hw_io_init     (struct sd_card* card);
enum sd_error sd_spi_hw_io_deinit   (struct sd_card* card);

enum sd_error sd_spi_hw_select_card    (struct sd_card* card);
enum sd_error sd_spi_hw_deselect_card  (struct sd_card* card);

enum sd_error sd_spi_hw_read_byte   (struct sd_card* card, void* buf);
enum sd_error sd_spi_hw_read_bytes  (struct sd_card* card, void* buf, uint32_t len);
enum sd_error sd_spi_hw_write_byte  (struct sd_card* card, uint8_t buf);
enum sd_error sd_spi_hw_write_bytes (struct sd_card* card, void* buf, uint32_t len);

void          sd_spi_hw_udelay      (struct sd_card* card, uint32_t us);
enum sd_error sd_spi_hw_send_dummy  (struct sd_card* card, uint8_t count);

enum sd_error sd_card_into_idle     (struct sd_card* card);
enum sd_error sd_card_identify     (struct sd_card* card);
enum sd_error sd_card_send_cmd_req  (struct sd_card* card, struct sd_cmd_req* req, struct sd_resp_res* resp);
enum sd_error sd_card_get_status    (struct sd_card *card, uint8_t *status);

void          sd_spi_hw_set_speed           (struct sd_card* card, enum sd_user_ctrl speed);
bool          sd_spi_hw_is_card_detached    (struct sd_card* card);

void          sd_card_print_info    (struct sd_card* card);

#ifdef __cplusplus
}
#endif

#endif  // SD_PRIVATE_H
