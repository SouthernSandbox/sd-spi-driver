/**
 * @file sd_hwio.c
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 用于实现与SD卡进行硬件交互的操作
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "sd_spi_driver.h"
#include "sd_private.h"


/**
 * @brief 硬件 SPI IO 初始化
 * @param card           [in]  SD卡对象
 * @return enum sd_error [out] 错误码
 */
enum sd_error sd_spi_hw_io_init(struct sd_card* card)
{
    if(card->spi_if == NULL || card->spi_if->control  == NULL)
        return Sd_Err_IO;

    card->spi_if->control(card, Sd_User_Ctrl_Init_Hardware);

    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI IO 去初始化
 * @param card           [in]  SD卡对象
 * @return enum sd_error [out] 错误码
 */
enum sd_error sd_spi_hw_io_deinit(struct sd_card* card)
{
    if(card->spi_if== NULL || card->spi_if->control == NULL)
        return Sd_Err_IO;

    card->spi_if->control(card, Sd_User_Ctrl_Deinit_Hardware);

    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI 选择卡
 * @param card           [in]  SD卡对象
 * @return enum sd_error [out] 错误码
 */
enum sd_error sd_spi_hw_select_card (struct sd_card* card)
{
    if(card->spi_if== NULL || card->spi_if->control == NULL)
        return Sd_Err_IO;
    if(card->spi_if->control(card, Sd_User_Ctrl_Take_Bus) != 0)
        return Sd_Err_Timeout;

    card->spi_if->control(card, Sd_User_Ctrl_Select_Card);
    card->is_selected = true;

    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI 取消选择卡
 * @param card           [in]  SD卡对象
 * @return enum sd_error [out] 错误码
 */
enum sd_error sd_spi_hw_deselect_card (struct sd_card* card)
{
    if(card->spi_if== NULL || card->spi_if->control == NULL)
        return Sd_Err_IO;

    card->spi_if->control(card, Sd_User_Ctrl_Deselect_Card);
    card->spi_if->control(card, Sd_User_Ctrl_Release_Bus);
    card->is_selected = false;

    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI 读取单字节数据
 * @param card            [in]  SD卡对象
 * @param buf             [in]  数据缓冲区
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_spi_hw_read_byte(struct sd_card* card, void* buf)
{
    return sd_spi_hw_read_bytes(card, buf, 1);
}

/**
 * @brief 硬件 SPI 读取多字节数据
 * @param card            [in]  SD卡对象
 * @param buf             [in]  数据缓冲区
 * @param len             [in]  期望读取的字节数
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_spi_hw_read_bytes(struct sd_card* card, void* buf, uint32_t len)
{
    if(card->spi_if== NULL || card->spi_if->transfer == NULL)
        return Sd_Err_IO;

    struct sd_spi_buf rx = 
    {
        .data = buf,
        .size = len,
        .used = 0,
    };

    card->is_xfering = true;
    card->spi_if->transfer(card, NULL, &rx);
    card->is_xfering = false;

    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI 写入单字节数据
 * @param card            [in]  SD卡对象
 * @param buf             [in]  数据缓冲区
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_spi_hw_write_byte(struct sd_card* card, uint8_t buf)
{
    return sd_spi_hw_write_bytes(card, &buf, 1);
}

/**
 * @brief 硬件 SPI 写入多字节数据
 * @param card            [in]  SD卡对象
 * @param buf             [in]  数据缓冲区
 * @param len             [in]  期望写出的字节数
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_spi_hw_write_bytes(struct sd_card* card, void* buf, uint32_t len)
{
    if(card->spi_if== NULL || card->spi_if->transfer == NULL)
        return Sd_Err_IO;

    struct sd_spi_buf tx =
    {
       .data = buf,
       .size = len,
       .used = 0,
    };

    card->is_xfering = true;
    card->spi_if->transfer(card, &tx, NULL);
    card->is_xfering = false;

    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI 延时
 * @param card [in]  SD卡对象
 * @param us   [in]  延时时间，单位：微秒
 */
void sd_spi_hw_udelay (struct sd_card* card, uint32_t us)
{
    if(card->spi_if== NULL || card->spi_if->delay_us == NULL)
        return;
    card->spi_if->delay_us(card, us);
}

/**
 * @brief 硬件 SPI 发送多次 dummy 数据
 * @param card              [in]  SD卡对象
 * @param count             [in]  发送次数
 * @return enum sd_error    [out] 错误码
 */
enum sd_error sd_spi_hw_send_dummy (struct sd_card* card, uint8_t count)
{
    enum sd_error err = Sd_Err_OK;
    while(count--)
        if((err = sd_spi_hw_write_byte(card, 0xFF)) != Sd_Err_OK)
            return err;
    return Sd_Err_OK;
}

/**
 * @brief 硬件 SPI 设置速度
 * @param card  [in]  SD卡对象
 * @param speed [in]  速度
 */
void sd_spi_hw_set_speed (struct sd_card* card, enum sd_user_ctrl speed)
{
    if(card->spi_if== NULL || card->spi_if->control == NULL)
        return;
    card->spi_if->control(card, speed);
}

/**
 * @brief 硬件检查卡是否已拔出
 * @param card   [in]  SD卡对象
 * @return true  [out] 卡已拔出
 * @return false [out] 卡未拔出
 */
bool sd_spi_hw_is_card_detached (struct sd_card* card)
{
    if(card->spi_if== NULL || card->spi_if->control == NULL)
        return true;
    return (card->spi_if->control(card, Sd_User_Ctrl_Is_Card_Detached) == 0);
}