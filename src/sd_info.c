/**
 * @file sd_info.c
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 解析 SD 卡身份与配置信息
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "sd_spi_driver.h"
#include "sd_private.h"





/**
 * @brief 解析V2卡的CSD寄存器（用于SDHC/SDXC卡）
 * @param card            [in]  SD卡对象
 * @param csd             [in]  CSD寄存器数据
 * @param info            [in]  待被填充的卡信息结构体
 * @return enum sd_error  [out] 错误码
 */
static enum sd_error _parse_csd_v2(struct sd_card* card, uint8_t csd[16], struct sd_info* info)
{
    /** 检查CSD结构版本 (V2卡应为1) **/
    if((csd[0] >> 6) != 1)
        return Sd_Err_Failed;
        
    /** 对于V2卡，C_SIZE是一个22位的值 **/
    uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | 
                      ((uint32_t)csd[8] << 8) | 
                      csd[9];

    /** 计算擦除块的大小 **/
    uint32_t erase_sector_size = (((csd[4] >> 7) & 0x01) << 6) | (csd[5] & 0x3F);
    
    /** 计算块数量、块大小和总容量 **/
    info->block_count           = (c_size + 1) * 1024;      // 每个C_SIZE单位代表 1024 个块，这是规范定义，固定1024
    info->block_size            = 512;
    info->capacity              = (uint64_t)info->block_count * info->block_size;
    info->erase_sector_size     = 1 << (erase_sector_size + 1);
    
    return Sd_Err_OK;
}

/**
 * @brief 解析V1卡的CSD寄存器
 * @param card            [in]  SD卡对象
 * @param csd             [in]  CSD寄存器数据
 * @param info            [in]  待被填充的卡信息结构体
 * @return enum sd_error  [out] 错误码
 */
static enum sd_error _parse_csd_v1(struct sd_card* card, uint8_t csd[16], struct sd_info* info)
{
    /** 检查CSD结构版本 (V1卡应为0) **/
    if((csd[0] >> 6) != 0)
        return Sd_Err_Failed;
    
    /** 计算块大小 **/
    uint8_t read_bl_len = csd[5] & 0x0F;
    uint16_t block_size = 1 << read_bl_len;
    
    /** 计算块数量 **/
    uint16_t c_size     = ((csd[6] & 0x03) << 10) | (csd[7] << 2) | (csd[8] >> 6);
    uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);

    /** 计算擦除块的大小 **/
    uint32_t erase_sector_size = (((csd[4] >> 7) & 0x01) << 6) | (csd[5] & 0x3F);
    
    /** 计算总块数 **/
    uint32_t block_count = (c_size + 1) * (1 << (c_size_mult + 2));
    
    /** 计算总容量、块大小和总容量 **/
    info->block_size            = block_size;
    info->block_count           = block_count;
    info->capacity              = (uint64_t)block_size * block_count;
    info->erase_sector_size     = 1 << (erase_sector_size + 1);      
    
    return Sd_Err_OK;
}

/**
 * @brief 检查卡是否可能是 v2.00 版本
 * @param card            [in]  SD卡对象
 * @return enum sd_error  [out] 错误码
 */
static enum sd_error _check_card_maybe_v2(struct sd_card* card)
{
    enum sd_error err = Sd_Err_OK;
    uint8_t timeout = 0xff; 

    /** 1. 发送 CMD55+ACMD41，开始SD卡初始化并检查SD卡是否初始化完成 **/
    {
        do
        {
            // 发送CMD55 (应用命令前缀)
            struct sd_resp_res resp_cmd55 = {0};
            struct sd_cmd_req req_cmd55 = 
            {
                .cmd = Sd_Cmd55_App_Cmd, .arg = 0, .crc = 1,
                .resp_type = Sd_Resp_Type_R1, .retry = 5
            };
            if ((err = sd_card_send_cmd_req(card, &req_cmd55, &resp_cmd55)) != Sd_Err_OK)
                return err;

            // 检查 CMD55 响应是否有效
            if (resp_cmd55.buf[0] != SD_FR_IN_IDLE_STATE)
            { 
                trace_i(card, "CMD55 failed, resp is 0x%02X", resp_cmd55.buf[0]);
                return Sd_Err_Unsupported;
            }

            // 发送 ACMD41 (初始化卡)
            struct sd_resp_res resp_acmd41 = {0};
            struct sd_cmd_req req_acmd41 = 
            {
                .cmd = Sd_Acmd41_Op_Cond,
                .arg = 0x40000000, // 设置HCS位(bit30)表示支持高容量卡
                .crc = 1,
                .resp_type = Sd_Resp_Type_R1,
                .retry = 5
            };
            if ((err = sd_card_send_cmd_req(card, &req_acmd41, &resp_acmd41)) != Sd_Err_OK)
                return err;

            // 检查 ACMD41 响应：当bit0为0时表示初始化完成
            if ((resp_acmd41.buf[0] & SD_FR_IN_IDLE_STATE) == 0)
                break; // 初始化完成，退出循环

            // 延时等待
            sd_spi_hw_udelay(card, 1000);

        } while (--timeout);

        if (timeout == 0)
        {
            trace_w(card, "ACMD41 init timeout");
            return Sd_Err_Timeout;
        }
    }

    /** 2. 发送CMD58读取OCR寄存器，初步判断卡类型  **/
    {
        struct sd_resp_res resp_cmd58 = {0};
        struct sd_cmd_req req_cmd58 = 
        {
            .cmd = Sd_Cmd58_Rd_Ocr, .arg = 0, .crc = 1,
            .resp_type = Sd_Resp_Type_R3, .retry = 5
        };
        if ((err = sd_card_send_cmd_req(card, &req_cmd58, &resp_cmd58)) != Sd_Err_OK)
            return err;

        // 检查 CMD58 响应错误标志（最高位表示错误）
        if (resp_cmd58.buf[0] & 0x80)
        { 
            trace_e(card, "CMD58 error: 0x%02X", resp_cmd58.buf[0]);
            return Sd_Err_Unsupported;
        }

        // 解析OCR寄存器 (响应字节1-4)
        uint32_t ocr = ((uint32_t)resp_cmd58.buf[1] << 24) |
                       ((uint32_t)resp_cmd58.buf[2] << 16) |
                       ((uint32_t)resp_cmd58.buf[3] << 8) |
                       (uint32_t)resp_cmd58.buf[4];

        // 根据CCS位判断卡类型
        if (ocr & (1 << 30))
        {                                       // CCS位(bit30)
            card->info.type = Sd_Type_SDHC;     // 或SDXC
            trace_d(card, "CCS set, maybe a SDHC/SDXC...");
        }
        else
        {
            card->info.type = Sd_Type_SDSC_V2;
            trace_d(card, "Card identified as SDSC v2!");
        }
    }

    /** 3. 发送CMD9读取CSD寄存器获取卡容量信息 **/
    {
        struct sd_resp_res resp_cmd9 = {0};
        struct sd_cmd_req req_cmd9 = 
        {
            .cmd = Sd_Cmd9_Csd, .arg = 0, .crc = 1,
            .resp_type = Sd_Resp_Type_R2, .retry = 5
        };
        sd_spi_hw_send_dummy(card, 2);      // 发送 CMD9 前必须发送 2 个虚拟字节
        if ((err = sd_card_send_cmd_req(card, &req_cmd9, &resp_cmd9)) != Sd_Err_OK)
            return err;

        // 解析CSD寄存器获取卡容量
        if (card->info.type == Sd_Type_SDHC)
        {
            // SDHC/SDXC卡使用CSD v2格式
            if (_parse_csd_v2(card, resp_cmd9.buf, &card->info) != Sd_Err_OK)
            {
                trace_e(card, "Failed to parse CSD for SDHC/SDXC card");
                return Sd_Err_Failed;
            }

            // 依据容量区分 SDHC/SDXC
            if (card->info.capacity > 32000000000ULL) 
            {
                card->info.type = Sd_Type_SDXC;
                trace_d(card, "Card identified as SDXC!");
            } 
            else
                trace_d(card, "Card identified as SDHC!");
        }
        else
        {
            // SDSC v2卡使用CSD v1格式
            if (_parse_csd_v1(card, resp_cmd9.buf, &card->info) != Sd_Err_OK)
            {
                trace_e(card, "Failed to parse CSD for SDSC v2 card");
                return Sd_Err_Failed;
            }
        }
    }

    return Sd_Err_OK;
}

/**
 * @brief 检查卡是否可能是 v1.00 版本或者 MMC 卡
 * @param card            [in]  SD卡对象
 * @return enum sd_error  [out] 错误码
 */
static enum sd_error _check_card_maybe_v1(struct sd_card *card)
{
    enum sd_error err = Sd_Err_OK;
    uint8_t timeout = 0xff; // 合理的超时次数

    /** 1. 发送 CMD55+ACMD41，开始SD卡初始化并检查SD卡是否初始化完成  **/
    {
        do
        {
            // 发送CMD55 (应用命令前缀)
            struct sd_resp_res resp_cmd55 = {0};
            struct sd_cmd_req req_cmd55 = 
            {
                .cmd = Sd_Cmd55_App_Cmd, .arg = 0, .crc = 1,
                .resp_type = Sd_Resp_Type_R1, .retry = 5
            };
            if ((err = sd_card_send_cmd_req(card, &req_cmd55, &resp_cmd55)) != Sd_Err_OK)
                return err;

            // 检查CMD55响应是否有效
            if (resp_cmd55.buf[0] != 0x01)
            { // 期望响应：空闲状态
                trace_e(card, "CMD55 failed: 0x%02X", resp_cmd55.buf[0]);
                return Sd_Err_Failed;
            }

            // 发送ACMD41 (初始化卡) - V1卡不需要HCS位
            struct sd_resp_res resp_acmd41 = {0};
            struct sd_cmd_req req_acmd41 = 
            {
                .cmd = Sd_Acmd41_Op_Cond, .arg = 0, .crc = 1, // V1卡忽略 arg 参数中的 HCS 位
                .resp_type = Sd_Resp_Type_R1, .retry = 5
            };
            if ((err = sd_card_send_cmd_req(card, &req_acmd41, &resp_acmd41)) != Sd_Err_OK)
                return err;

            // 检查ACMD41响应：当bit0为0时表示初始化完成
            if ((resp_acmd41.buf[0] & 0x01) == 0)
                break; // 初始化完成，退出循环

        } while (--timeout);

        if (timeout == 0)
        {
            trace_w(card, "ACMD41 initialization timeout for V1 card");
            return Sd_Err_Timeout;
        }
    }

    /** 2. 设置卡类型，并发送 CMD16 读取块长度 **/
    {
        // 设置卡类型
        card->info.type = Sd_Type_SDSC_V1;
        trace_d(card, "Card identified as SDSC v1.x!");

        // 发送CMD16设置块长度（V1卡需要显式设置块大小）
        struct sd_resp_res resp_cmd16 = {0};
        struct sd_cmd_req req_cmd16 = 
        {
            .cmd = Sd_Cmd16_Block_len, .arg = 512, .crc = 1,
            .resp_type = Sd_Resp_Type_R1, .retry = 5
        };
        if ((err = sd_card_send_cmd_req(card, &req_cmd16, &resp_cmd16)) != Sd_Err_OK)
            return err;

        // 检查CMD16响应
        if (resp_cmd16.buf[0] != SD_FR_NONE)
        {
            trace_e(card, "CMD16 failed: 0x%02X", resp_cmd16.buf[0]);
            return Sd_Err_Response;
        }
    }

    /** 3. 发送CMD9读取CSD寄存器获取卡容量信息 **/
    {
        struct sd_resp_res resp_cmd9 = {0};
        struct sd_cmd_req req_cmd9 = 
        {
            .cmd = Sd_Cmd9_Csd, .arg = 0, .crc = 1,
            .resp_type = Sd_Resp_Type_R2, .retry = 5    // CSD是16字节响应
        };  
        if ((err = sd_card_send_cmd_req(card, &req_cmd9, &resp_cmd9)) != Sd_Err_OK)
            return err;

        // 解析CSD寄存器获取卡容量
        if (_parse_csd_v1(card, resp_cmd9.buf, &card->info) != Sd_Err_OK)
        {
            trace_e(card, "Failed to parse CSD for V1 card");
            return Sd_Err_Failed;
        }
    }

    return Sd_Err_OK;
}

/**
 * @brief 识别卡类型
 * @param card            [in]  SD卡对象
 * @return enum sd_error  [out] 错误码
 */
static enum sd_error _card_identify(struct sd_card *card)
{
    enum sd_error err = Sd_Err_OK;
    struct sd_resp_res resp = {0};

    trace_d(card, "Start to identify card type...");

    /** 重置卡信息 **/
    card->info = (struct sd_info)
    {
        .type = Sd_Type_Unknown,
        .block_count = 0,
        .block_size = 0,
        .capacity = 0,
    };

    /** 发送CMD8之前确保卡被选中 **/
    if((err = sd_spi_hw_select_card(card)) != Sd_Err_OK)
        return err;

    /** 发送CMD8，检查卡电压范围 **/
    struct sd_cmd_req req = 
    {
        .cmd = Sd_Cmd8_If_Cond, .arg = 0x1AA, .crc = 0x87,
        .resp_type = Sd_Resp_Type_R7, .retry = 5,
    };

    if ((err = sd_card_send_cmd_req(card, &req, &resp)) != Sd_Err_OK)
    {
        trace_e(card, "CMD8 failed, maybe a SDSC v1.x or MMC...");
        err = _check_card_maybe_v1(card);       // 如果CMD8失败，可能是V1卡或MMC
        goto _FINISH_;
    }

    /** 检查响应状态 **/
    if (resp.buf[0] == SD_FR_IN_IDLE_STATE)
    {
        //** 检查电压范围和模式匹配 **/
        if ((resp.buf[3] & 0x0F) == 0x01 && resp.buf[4] == 0xAA)
        {
            trace_d(card, "CMD8 allowed, maybe a SD v2.00+...");
            err = _check_card_maybe_v2(card);
        }
        else
        {
            trace_e(card, "CMD8 voltage or pattern mismatch");
            err = Sd_Err_Unsupported;
        }
    }
    else
    {
        trace_e(card, "CMD8 response error: 0x%02X", resp.buf[0]);
        err = Sd_Err_Response;
    }

_FINISH_:;
    sd_spi_hw_deselect_card(card); // 取消选择卡
    sd_spi_hw_send_dummy(card, 1); // 发送时钟周期，让SD卡完成状态处理

    return err;
}








/**
 * @brief 卡识别与信息加载
 * @warning 此函数默认卡处于空闲状态
 * @param card            [in]  SD卡对象
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_card_identify(struct sd_card* card)
{
    enum sd_error err = Sd_Err_OK;

    /** 令卡进入空闲状态 **/
    if((err = sd_card_into_idle(card))!= Sd_Err_OK)
        return err;

    /** 识别卡类型 **/
    if((err = _card_identify(card))!= Sd_Err_OK)
        return err;

    return err;
}

























