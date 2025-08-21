/**
 * @file sd_config.h
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 库配置文件
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef SD_CONFIG_H
#define SD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 库调试追踪开关
 * @note 库内所有的打印都是基于用户的 print() 接口
 */
#define SD_SPI_TRACE_LEVEL_NONE     0       // 关闭任何打印内容
#define SD_SPI_TRACE_LEVEL_LIB      1       // 库信息，打印如卡识别结果、版本号等
#define SD_SPI_TRACE_LEVEL_ERROR    2       // 错误
#define SD_SPI_TRACE_LEVEL_WARN     3       // 警告
#define SD_SPI_TRACE_LEVEL_INFO     4       // 信息
#define SD_SPI_TRACE_LEVEL_DEBUG    5       // 调试
#define SD_SPI_TRACE_LEVEL          SD_SPI_TRACE_LEVEL_DEBUG
#define SD_SPI_TRACE_ENABLE         1       // 打印追踪开关


/**
 * @brief 声明 SD 卡对象
 * @note 用户需要将 port.c 文件中的 sd_card 结构体实例化，并定义为全局变量，然后在 sd_config.h 中引用
 *       之后用户可以通过 sd_card_find() 函数找到该卡，然后使用库函数进行操作。
 */
extern struct sd_card card0;

/**
 * @brief 定义 SD 卡数组
 */
#define SD_CARD_ARR_DEFINE      \
        {                       \
            &card0,             \
        }


#ifdef __cplusplus
}
#endif

#endif  // SD_CONFIG_H
