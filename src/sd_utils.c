/**
 * @file sd_utils.c
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 工具/辅助类函数
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "sd_spi_driver.h"
#include "sd_private.h"

/**
 * @brief 令卡进入空闲状态
 * @param card              [in]  SD卡对象
 * @return enum sd_error    [out] 错误码
 */
enum sd_error sd_card_into_idle(struct sd_card* card)
{
    enum sd_error err = Sd_Err_OK;
    uint8_t timeout = 0xff;

    /** 发送 CMD0 尝试进入空闲状态 **/
    if((err = sd_spi_hw_select_card(card)) != Sd_Err_OK)
        return err;       // 选择卡

    while(timeout--)
    {
        /** 发送命令，等待响应 **/
        struct sd_resp_res resp = {0};
        struct sd_cmd_req req =
        {
            .cmd = Sd_Cmd0_Idle, .arg = 0, .crc = 0x95,
            .resp_type = Sd_Resp_Type_R1, .retry = 0xff,
        };
        if((err = sd_card_send_cmd_req(card, &req, &resp)) != Sd_Err_OK)
            goto _FINISH_;

        /** 检查响应 **/
        if(resp.buf[0] == SD_FR_IN_IDLE_STATE)
        {
            trace_i(card, "CMD0 idle success");
            break;
        }
    }

    if(timeout == 0)
    {
        trace_w(card, "CMD0 idle timeout");
        err = Sd_Err_Timeout;
    }

_FINISH_:;
    sd_spi_hw_deselect_card(card);      // 取消选择卡
    sd_spi_hw_send_dummy(card, 1);      // 发送时钟周期，让SD卡完成状态处理

    return err;
}

/**
 * @brief 发送命令请求，并等待响应
 * @param card              [in]  SD卡对象
 * @param req               [in]  命令请求
 * @param resp              [out] 响应结果
 * @return enum sd_error    [out] 错误码
 */
enum sd_error sd_card_send_cmd_req (struct sd_card* card, struct sd_cmd_req* req, struct sd_resp_res* resp)
{
    enum sd_error err = Sd_Err_OK;
    uint8_t cmd_buf[] = 
    {
        (uint8_t) (req->cmd),
        (uint8_t) (req->arg >> 24),
        (uint8_t) (req->arg >> 16),
        (uint8_t) (req->arg >> 8),
        (uint8_t) (req->arg),
        (uint8_t) (req->crc),
    };

    /** 发送命令 **/
    if((err = sd_spi_hw_write_bytes(card, cmd_buf, sizeof(cmd_buf))) != Sd_Err_OK)
        return err;

    /** 等待响应 **/
    {
        uint8_t byte = 0xff;
        do
        {
            if((err = sd_spi_hw_read_bytes(card, &byte, 1))!= Sd_Err_OK)
                return err;
        } while (byte == 0xff && --req->retry);

        /** 检查超时 **/
        if(req->retry == 0)
        {
            trace_w(card, "CMD%d response timeout", (req->cmd & ~0x40) & 0x3F);
            return Sd_Err_Timeout;
        }

        /** 存储刚获取的字节 **/
        resp->buf[resp->filled++] = byte;
    }


    /** 处理不同类型的响应 **/
    switch(req->resp_type)
    {
    case Sd_Resp_Type_R1:
        break;

    case Sd_Resp_Type_R2:
        {
            // 首先检查R1响应是否有错误
            if(resp->buf[0] != 0x00) 
            {
                trace_e(card, "CMD%d error before data phase: 0x%02X", (req->cmd & ~0x40) & 0x3F, resp->buf[0]);
                return Sd_Err_Response;
            }
            
            // 等待数据令牌0xFE
            uint8_t token;
            uint8_t token_retry = 0xff;
            do 
            {
                if((err = sd_spi_hw_read_bytes(card, &token, 1)) != Sd_Err_OK)
                    return err;
            } while(token != 0xFE && --token_retry);
            
            if(token_retry == 0) 
            {
                trace_w(card, "Data token timeout for CMD%d", (req->cmd & ~0x40) & 0x3F);
                return Sd_Err_Timeout;
            }
            
            // 读取数据块 (16字节)
            resp->filled = 0;
            if((err = sd_spi_hw_read_bytes(card, &resp->buf[resp->filled], 16)) != Sd_Err_OK)
                return err;
            else
                resp->filled += 16;
            
            // 丢弃2字节CRC
            uint8_t crc[2];
            if((err = sd_spi_hw_read_bytes(card, crc, sizeof(crc))) != Sd_Err_OK)
                return err;
        } break;

    case Sd_Resp_Type_R3:
    case Sd_Resp_Type_R7:
        // 读取额外4个字节
        if((err = sd_spi_hw_read_bytes(card, &resp->buf[resp->filled], 4)) != Sd_Err_OK)
            return err;
        else
            resp->filled += 4;
        break;

    case Sd_Resp_Type_R1b:
        {
            uint8_t busy = 0x00;
            do
            {
                if((err = sd_spi_hw_read_bytes(card, &busy, 1)) != Sd_Err_OK)
                    return err;
                else
                    sd_spi_hw_udelay(card, 5000);
            } while (busy != 0xff);     // 等待卡退出忙状态
        } break;
    }

    return Sd_Err_OK;
}

/**
 * @brief 获取SD卡状态
 * @param card              [in]  SD卡对象
 * @param status            [in]  状态值
 * @return enum sd_error    [out] 错误码
 */
enum sd_error sd_card_get_status(struct sd_card *card, uint8_t *status)
{
    struct sd_cmd_req req = 
    {
        .cmd = Sd_Cmd13_Status, .arg = 0, .crc = 1,
        .resp_type = Sd_Resp_Type_R1, .retry = 0xff
    };
    
    struct sd_resp_res resp;
    enum sd_error err = sd_card_send_cmd_req(card, &req, &resp);
    
    if (err == Sd_Err_OK)
        *status = resp.buf[0]; // R1响应

    return err;
}

/**
 * @brief 获取卡容量类型名称
 * @param type              [in]  卡类型
 * @return const char*      [out] 类型名称
 */
const char* sd_get_capacity_class_name(enum sd_type type)
{
    switch(type)
    {
    case Sd_Type_SDSC_V1:   return "SDSC v1.x";
    case Sd_Type_SDSC_V2:   return "SDSC v2.00";
    case Sd_Type_SDHC:      return "SDHC";
    case Sd_Type_SDXC:      return "SDXC";
    default:                break;
    }
    return "Unknown";
}

/**
 * @brief 打印SD卡信息
 * @param card  [in]  SD卡对象
 */
void sd_card_print_info (struct sd_card* card)
{
    trace_l(card, "This is a %s card",              sd_get_capacity_class_name(sd_card_get_type(card)));
    trace_l(card, "  > Name: \"%s\"",               sd_card_get_name(card));
    trace_l(card, "  > Capacity: %d MB",            sd_card_get_capacity(card) >> 20);
    trace_l(card, "  > Block size: %d B",           sd_card_get_block_size(card));
    trace_l(card, "  > Erase sector size: %d KB",   sd_card_get_erase_size(card) >> 10);
}