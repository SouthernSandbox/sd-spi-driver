/**
 * @file f103ze_spi2_port.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "sd_spi_driver.h"
#include "CH58x_common.h"
#include "stdarg.h"
#include "stdio.h"
#include "stdbool.h"

/**
 * @brief 硬件初始化
 * @param card [in] SD卡对象
 */
static void _init(struct sd_card* card)
{
    /* SPI 0 */
    // 引脚初始化
    GPIOA_SetBits(GPIO_Pin_12);
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14, GPIO_ModeOut_PP_5mA);

    // SPI 默认初始化：8MHz
    SPI0_MasterDefInit();
    SPI0_CLKCfg(150);

    printf("spi0 init ok\r\n");
}

/**
 * @brief 硬件去初始化
 * @param card [in] SD卡对象
 */
static void _deinit(struct sd_card* card)
{

}

/**
 * @brief SPI 读写操作
 * @param card  [in]  SD卡对象
 * @param tx    [in]  发送数据
 * @param rx    [in]  接收数据
 * @return int  [out] 成功返回0，失败返回-1
 */
static int _transfer(struct sd_card* card, struct sd_spi_buf* tx, struct sd_spi_buf* rx)
{
    if(tx)
    {
        SPI0_MasterTrans(tx->data, tx->size);
        tx->used = tx->size;

    #if 0
        printf("write len: %d, content: ", tx->used);
        for(int i = 0; i < tx->used; i++)
            printf("%02x ", ((uint8_t*)tx->data)[i]);
        printf("\r\n");
    #endif
    }

    if(rx)
    {
        for(rx->used = 0; rx->used != rx->size; rx->used++)
            ((uint8_t* )rx->data)[rx->used] = SPI0_MasterRecvByte();

    #if 0
        printf("read len: %d, content: ", rx->used);
        for(int i = 0; i < rx->used; i++)
            printf("%02x ", ((uint8_t*)rx->data)[i]);
        printf("\r\n");
    #endif
    }

    return 0;
}

/**
 * @brief 延时函数
 * @param card  [in]  SD卡对象
 * @param us    [in]  延时时间，单位：us
 */
static void _delay_us(struct sd_card* card, uint32_t us)
{
    DelayUs(us);
}

/**
 * @brief SPI通信速率设置
 * @param card  [in] D卡对象
 * @param speed [in] 通信速率枚举
 */
static void _set_speed(struct sd_card* card, enum sd_user_ctrl speed)
{

}

/**
 * @brief 硬件控制函数
 * @param card  [in]  SD卡对象
 * @param ctrl  [in]  控制命令
 * @return int  [out] 成功返回0，失败返回-1
 */
static int _control(struct sd_card* card, enum sd_user_ctrl ctrl)
{
    switch(ctrl)
    {
    case Sd_User_Ctrl_Init_Hardware:     _init(card); break;
    case Sd_User_Ctrl_Deinit_Hardware:   _deinit(card); break;
    case Sd_User_Ctrl_Is_Card_Detached:  return -1;

    case Sd_User_Ctrl_Select_Card:       GPIOA_ResetBits(GPIO_Pin_12); break;
    case Sd_User_Ctrl_Deselect_Card:     GPIOA_SetBits(GPIO_Pin_12); break;

    case Sd_User_Ctrl_Take_Bus:          break;
    case Sd_User_Ctrl_Release_Bus:       break;

    case Sd_User_Ctrl_Set_Low_Speed:    
    case Sd_User_Ctrl_Set_High_Speed:    _set_speed(card, ctrl); break;
    }
    return 0;
}

/**
 * @brief 打印函数
 * @param card      [in]  SD卡对象
 * @param format    [in]  格式化字符串
 * @param ...       [in]  可变参数
 */
static void _print(struct sd_card* card, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    static char buf[256];
    vsnprintf(buf, sizeof(buf), format, args);
    printf("%s", buf);
    va_end(args);
}

/**
 * @brief 用户SPI通信接口
 */
static struct sd_spi_interface _spi2_intf =
{
    .control  = _control,
    .transfer = _transfer,
    .delay_us = _delay_us,
};

/**
 * @brief 用户调试接口
 */
static struct sd_debug_interface _debug_intf =
{
    .print = _print,
};

/**
 * @brief sd_card 对象
 */
struct sd_card card0 = SD_CARD_OBJ_INIT("card0", &_spi2_intf, &_debug_intf);
