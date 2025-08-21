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
#include "stm32f1xx_hal.h"
#include "stdarg.h"
#include "stdio.h"
#include "rtthread.h"
#include "rthw.h"

static SPI_HandleTypeDef hspi2;
static struct rt_mutex mutex_spisd;

static void _init(struct sd_card* card)
{
    /** 初始化互斥锁 **/
    rt_mutex_init(&mutex_spisd, "spisd", RT_IPC_FLAG_FIFO);

    /** SPI 初始化 **/
    __HAL_RCC_SPI2_CLK_ENABLE();
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi2);

    /** GPIO 初始化 **/
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void _deinit(struct sd_card* card)
{
    __HAL_RCC_SPI2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_14);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15);

    /** 卸载互斥锁 **/
    rt_mutex_detach(&mutex_spisd);
}

static int _transfer(struct sd_card* card, struct sd_spi_buf* tx, struct sd_spi_buf* rx)
{
    if(tx)
    {
        tx->used = 0;
        if(HAL_SPI_Transmit(&hspi2, tx->data, tx->size, 0xffff) != HAL_OK)
            return -1;
        else
            tx->used = tx->size;

    #if 1
        rt_kprintf("write len: %d, content: ", tx->used);
        for(int i = 0; i < tx->used; i++)
            rt_kprintf("%02x ", ((uint8_t*)tx->data)[i]);
        rt_kprintf("\r\n");
    #endif
    }

    if(rx)
    {
        rx->used = 0;
        while(rx->used < rx->size)
        {
            uint8_t dummy = 0xff;
            if(HAL_SPI_TransmitReceive(&hspi2, &dummy, rx->data + rx->used, 1, 0xffff)!= HAL_OK)
                return -1;
            else
                rx->used++;
        }

    #if 1
        rt_kprintf("read len: %d, content: ", rx->used);
        for(int i = 0; i < rx->used; i++)
            rt_kprintf("%02x ", ((uint8_t*)rx->data)[i]);
        rt_kprintf("\r\n");
    #endif
    }

    return 0;
}

static void _cs_control(struct sd_card* card, bool is_sel)
{
    switch((int) is_sel)
    {
    case true:  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_14, GPIO_PIN_RESET); break;
    case false: HAL_GPIO_WritePin(GPIOG, GPIO_PIN_14, GPIO_PIN_SET); break;
    }
}

static void _delay_us(struct sd_card* card, uint32_t us)
{
    if(us >= 1000)
        rt_thread_mdelay(us / 1000);
    else
        rt_hw_us_delay(us);
}

static void _set_speed(struct sd_card* card, enum sd_user_ctrl speed)
{
    /** 停止 SPI 外设 **/
    __HAL_RCC_SPI2_CLK_DISABLE();
    while (__HAL_RCC_SPI2_IS_CLK_ENABLED());

    /** 重置时钟分频 **/
    switch(speed)
    {
    case Sd_User_Ctrl_Set_Low_Speed:  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; break;
    case Sd_User_Ctrl_Set_High_Speed: hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2; break;
    default: break;
    }

    /** 重新初始化 SPI 外设 **/
    HAL_SPI_Init(&hspi2);

    /** 重新启动 SPI 外设 **/
    __HAL_RCC_SPI2_CLK_ENABLE();
    while (__HAL_RCC_SPI2_IS_CLK_DISABLED());
}

static int _control(struct sd_card* card, enum sd_user_ctrl ctrl)
{
    switch(ctrl)
    {
    case Sd_User_Ctrl_Init_Hardware:     _init(card); break;
    case Sd_User_Ctrl_Deinit_Hardware:   _deinit(card); break;
    case Sd_User_Ctrl_Is_Card_Detached:  return -1;

    case Sd_User_Ctrl_Select_Card:       _cs_control(card, true); break;
    case Sd_User_Ctrl_Deselect_Card:     _cs_control(card, false); break;

    case Sd_User_Ctrl_Take_Bus:          rt_mutex_take(&mutex_spisd, RT_WAITING_FOREVER); break;
    case Sd_User_Ctrl_Release_Bus:       rt_mutex_release(&mutex_spisd); break;

    case Sd_User_Ctrl_Set_Low_Speed:    
    case Sd_User_Ctrl_Set_High_Speed:    _set_speed(card, ctrl); break;
    }
    return 0;
}

static void _print(struct sd_card* card, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    static char buf[256];
    vsnprintf(buf, sizeof(buf), format, args);
    rt_kputs(buf);
    va_end(args);
}

static struct sd_spi_interface _spi2_intf =
{
    .control  = _control,
    .transfer = _transfer,
    .delay_us = _delay_us,
};

static struct sd_debug_interface _debug_intf =
{
    .print = _print,
};

/**
 * @brief sd_card 对象
 */
struct sd_card card0 = SD_CARD_OBJ_INIT("card0", &_spi2_intf, &_debug_intf);

















