/**
 * @file sd_core.c
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 核心文件，库初始化、实现逻辑等
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "sd_spi_driver.h"
#include "sd_private.h"
#include "string.h"

/**
 * @brief 管理器
 */
struct manager
{
    uint8_t card_count;         // 卡数量
};

static struct sd_card* arr_cards[] = SD_CARD_ARR_DEFINE;    // 定义数组并初始化
static struct manager mgr =
{
    .card_count = (uint16_t) COUNT_OF(arr_cards),
};











/**
 * @brief 卡上电操作
 * @param card              [in]  SD卡对象
 * @return enum sd_error    [out] 错误码
 */
static enum sd_error _card_power_on (struct sd_card* card)
{
    enum sd_error err = Sd_Err_OK;

    /** 等待芯片上电完毕 **/
    sd_spi_hw_deselect_card(card);      // 取消选择卡
    sd_spi_hw_send_dummy(card, 10);     // 发送80个时钟周期

    return err;
}

/**
 * @brief 卡下电操作
 * @param card 
 * @return enum sd_error 
 */
static enum sd_error _card_power_off (struct sd_card* card)
{
    return sd_card_into_idle(card);
}

/**
 * @brief 将用户请求转换为内部块操作参数
 * @param card              [in]  SD卡对象
 * @param request           [in]  用户请求（字节地址和长度）
 * @param operation         [out] 内部操作参数（块地址和块数）
 * @return enum sd_error    [out] 错误码
 */
static void _conv_req_to_lba(struct sd_card *card, struct sd_lba_req* req, struct sd_lba_oparg* oparg)
{
    /** 计算块数 **/
    oparg->lba_count = req->len / card->info.block_size;

    /** 根据卡类型计算块地址 **/
    if (card->info.type == Sd_Type_SDHC || card->info.type == Sd_Type_SDXC)
        oparg->lba_addr = (uint32_t) (req->offset / card->info.block_size);       // SDHC/SDXC使用块地址
    else
        oparg->lba_addr = (uint32_t) req->offset;                                 // SDSC使用字节地址

    trace_d(card, "req: offset=0x%lx, len=%d, blk_size=%d", req->offset, req->len, card->info.block_size); 
    trace_d(card, "oparg: lba_addr=0x%x, lba_count=%d", oparg->lba_addr, oparg->lba_count);
}

/**
 * @brief 读取单个数据块
 * @param card              [in]  SD卡对象
 * @param lba               [in]  逻辑块地址
 * @param buf               [out] 数据缓冲区（至少512字节）
 * @return enum sd_error    [out] 错误码
 */
static enum sd_error _read_single_block(struct sd_card *card, uint32_t lba, uint8_t *buf)
{
    enum sd_error err = Sd_Err_OK;

    /** 1. 发送CMD17读取单个块，并检查响应 **/
    {
        struct sd_cmd_req req = 
        {
            .cmd = Sd_Cmd17_Rd_Single, .arg = lba, .crc = 1,
            .resp_type = Sd_Resp_Type_R1, .retry = 5
        };

        struct sd_resp_res resp = {0};
        if ((err = sd_card_send_cmd_req(card, &req, &resp)) != Sd_Err_OK)
            return err;

        if (resp.buf[0] != SD_FR_NONE)
        {
            trace_e(card, "CMD17 error: 0x%02X", resp.buf[0]);
            return Sd_Err_Response;
        }
    }

    /** 2. 等待数据令牌 (0xFE) **/
    {
        uint8_t token;
        uint8_t timeout = 100;
        do
        {
            if ((err = sd_spi_hw_read_byte(card, &token)) != Sd_Err_OK)
                return err;
        } while (token != 0xFE && --timeout);

        if (timeout == 0)
        {
            trace_w(card, "Data token timeout for CMD17");
            return Sd_Err_Timeout;
        }
    }

    /** 3. 读取数据 **/
    {
        /** 读取数据块 **/
        if ((err = sd_spi_hw_read_bytes(card, buf, card->info.block_size)) != Sd_Err_OK)
            return err;

        /** 读取并丢弃CRC **/
        uint8_t crc[2];
        if ((err = sd_spi_hw_read_bytes(card, crc, sizeof(crc))) != Sd_Err_OK)
            return err;
    }

    return Sd_Err_OK;
}

/**
 * @brief 写入单个数据块
 * @param card              [in]  SD卡对象
 * @param lba               [in]  块地址
 * @param buf               [out] 数据缓冲区（至少512字节）
 * @return enum sd_error    [out] 错误码
 */
static enum sd_error _write_single_block(struct sd_card *card, uint32_t lba, const uint8_t *buf)
{
    enum sd_error err = Sd_Err_OK;

    /** 1. 发送CMD24写入单个块命令并等待响应 **/
    {
        struct sd_cmd_req req = 
        {
            .cmd = Sd_Cmd24_Wr_Single_Blk, .arg = lba, .crc = 1,
            .resp_type = Sd_Resp_Type_R1, .retry = 5
        };

        struct sd_resp_res resp = {0};
        if ((err = sd_card_send_cmd_req(card, &req, &resp)) != Sd_Err_OK)
        {
            trace_e(card, "CMD24 send cmd error: 0x%02X", resp.buf[0]);
            return err;
        }

        /** 检查响应 **/
        if (resp.buf[0] != 0x00)
        {
            trace_e(card, "CMD24 resp error: 0x%02X", resp.buf[0]);
            return Sd_Err_Response;
        }
    }

    /** 2. 发送数据令牌 (0xFE)，写入数据块和虚拟CRC（通常被忽略） **/
    {
        uint8_t token = 0xFE;
        if ((err = sd_spi_hw_write_byte(card, token)) != Sd_Err_OK)
        {
            trace_e(card, "Write token(0x%02X) failed", token);
            return err;
        }  
        if ((err = sd_spi_hw_write_bytes(card, (void *)buf, card->info.block_size)) != Sd_Err_OK)
        {
            trace_e(card, "Write data error");
            return err;
        }
        uint8_t crc[2] = {0xFF, 0xFF};
        if ((err = sd_spi_hw_write_bytes(card, crc, sizeof(crc))) != Sd_Err_OK)
        {
            trace_e(card, "Write crc error");
            return err;
        }
    }

    /** 3. 检查数据响应令牌 **/
    {
        /** 检查数据响应令牌 **/
        uint8_t data_resp;
        if ((err = sd_spi_hw_read_byte(card, &data_resp)) != Sd_Err_OK)
        {
            trace_e(card, "Read data resp error");
            return err;
        }

        /** 检查响应令牌是否有效 **/
        if ((data_resp & 0x1F) != 0x05)
        {
            trace_e(card, "Data response error: 0x%02X", data_resp);
            return Sd_Err_Response;
        }
    }

    /** 4. 等待卡完成编程（读取忙状态） **/
    {
        uint8_t busy;
        uint32_t timeout = 500; // 500ms超时
        do
        {
            if ((err = sd_spi_hw_read_byte(card, &busy)) != Sd_Err_OK)
            {
                trace_e(card, "Read busy error");
                return err;
            }

            if (--timeout == 0)
            {
                trace_w(card, "Write busy timeout");
                return Sd_Err_Timeout;
            }
            sd_spi_hw_udelay(card, 1000); // 每 1ms 检查一次
        } while (busy == 0x00);
    }

    return Sd_Err_OK;
}




















/**
 * @brief 库初始化
 * @return  [out] int 
 */
enum sd_error sd_spi_lib_init (void)
{
    return Sd_Err_OK;
}

/**
 * @brief 查找卡对象
 * @param name              [in]  卡名
 * @return struct sd_card*  [out] SD卡对象指针
 */
struct sd_card* sd_card_find (const char* name)
{
    if(name == NULL)
        return NULL;
    for (int i = 0; i < mgr.card_count; i++)
        if (strcmp(arr_cards[i]->name, name) == 0)
            return arr_cards[i];
    return NULL;
}

/**
 * @brief 初始化SD卡
 * @param card           [in]  SD卡对象
 * @return enum sd_error [out] 错误码
 */
enum sd_error sd_card_init (struct sd_card* card)
{   
    if(card == NULL)
        return Sd_Err_Param;

    enum sd_error err = Sd_Err_OK;

    /** 卡初始化完成 **/
    card->is_inited = false;
    card->is_selected = false;
    card->is_xfering = false;

    /** 硬件接口初始化 **/
    if((err = sd_spi_hw_io_init(card)) != Sd_Err_OK)
        return err;

    /** 调整通信速率 **/
    sd_spi_hw_set_speed(card, Sd_User_Ctrl_Set_Low_Speed);

    /** 卡上电检查，等待卡就绪 **/
    if((err = _card_power_on(card)) != Sd_Err_OK)
        return err;

    /** 获取卡信息：识别卡类型、容量等 **/
    if((err = sd_card_identify(card)) != Sd_Err_OK)
        return err;

    /** 调整通信速率 **/
    sd_spi_hw_set_speed(card, Sd_User_Ctrl_Set_High_Speed);

    /** 卡初始化完成 **/
    card->is_inited = true;

    /** 打印卡识别信息 **/
    sd_card_print_info(card);

    return Sd_Err_OK;
}

/**
 * @brief 去初始化SD卡
 * @param card           [in]  SD卡对象
 * @return enum sd_error [out] 错误码
 */
enum sd_error sd_card_deinit (struct sd_card* card)
{
    if(card == NULL)
        return Sd_Err_Param;

    enum sd_error err = Sd_Err_OK;

    /** 卡去初始化 **/
    if((err = _card_power_off(card)) != Sd_Err_OK)
        return err;

    /** SPI 去初始化 **/
    if((err = sd_spi_hw_io_deinit(card)) != Sd_Err_OK)
        return err;

    /** 卡去初始化完成 **/
    card->is_inited = false;

    return Sd_Err_OK;
}

/**
 * @brief 读取SD指定地址的数据
 * @param card              [in]  SD卡对象
 * @param addr              [in]  字节地址（必须是块大小的倍数）
 * @param buf               [in]  数据缓冲区
 * @param len               [in]  读取长度（必须是块大小的倍数）
 * @return enum sd_error    [out] 错误码
 */
enum sd_error sd_card_read(struct sd_card *card, const uint64_t addr, uint8_t *buf, const uint32_t len)
{
    if(card == NULL || buf == NULL || len == 0)
        return Sd_Err_Param;
    if (!card->is_inited)
        return Sd_Err_Not_Inited;
    if (len % card->info.block_size != 0)
        return Sd_Err_Param;

    enum sd_error err = Sd_Err_OK;

    /** 计算要操作的块地址和块数 **/
    struct sd_lba_req user_req = {.offset = addr, .len = len};
    struct sd_lba_oparg oparg = {0};
    _conv_req_to_lba(card, &user_req, &oparg);                      // SDSC使用字节地址

    /** 执行读块操作 **/
    if((err = sd_spi_hw_select_card(card)) != Sd_Err_OK)
        return err;

    for (uint32_t i = 0; i < oparg.lba_count; i++)
        if ((err = _read_single_block( card, 
                                       oparg.lba_addr + i, 
                                       buf + (i * card->info.block_size))) != Sd_Err_OK)
        {
            trace_e(card, "Read lba[%d] failed, code: 0x%02x", oparg.lba_addr + i, err);
            break;
        }
            
    sd_spi_hw_deselect_card(card);

    return err;
}

/**
 * @brief 写入SD指定地址的数据
 * @note 一般来说，SD卡写入数据时不需要用户显式擦除，擦除过程通常由卡内的控制器自动处理。
 * @param card              [in]  SD卡对象
 * @param addr              [in]  字节地址（必须是块大小的倍数）
 * @param buf               [in]  数据缓冲区
 * @param len               [in]  写入长度（必须是块大小的倍数）
 * @return enum sd_error    [out] 错误码
 */
enum sd_error sd_card_write(struct sd_card *card, const uint64_t addr, const uint8_t *buf, const uint32_t len)
{
    if(card == NULL || buf == NULL || len == 0)
        return Sd_Err_Param;
    if (!card->is_inited)
        return Sd_Err_Not_Inited;
    if (len % card->info.block_size != 0)
        return Sd_Err_Param;

    enum sd_error err = Sd_Err_OK;

    /** 计算要操作的块地址和块数 **/
    struct sd_lba_req user_req = {.offset = addr, .len = len};
    struct sd_lba_oparg oparg = {0};
    _conv_req_to_lba(card, &user_req, &oparg);

    /** 执行写块操作 **/
    if((err = sd_spi_hw_select_card(card)) != Sd_Err_OK)
        return err;

    for (uint32_t i = 0; i < oparg.lba_count; i++)
        if ((err = _write_single_block( card, 
                                        oparg.lba_addr + i, 
                                        buf + (i * card->info.block_size))) != Sd_Err_OK)
        {
            trace_e(card, "Write lba[%d] failed, code: 0x%02x", oparg.lba_addr + i, err);
            break;
        }
    sd_spi_hw_deselect_card(card);

    return err;
}

/**
 * @brief 擦除SD指定数量扇区
 * @warning 该函数建议在擦除大面积数据时使用。
 * @param card            [in]  SD卡对象
 * @param addr            [in]  字节地址（必须是块大小的倍数）
 * @param len             [in]  擦除长度（必须是擦除扇区大小的倍数）
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_card_erase_sector(struct sd_card *card, const uint64_t addr, const uint32_t count)
{
    if(card == NULL || count == 0)
        return Sd_Err_Param;
    if (!card->is_inited)
        return Sd_Err_Not_Inited;

    enum sd_error err = Sd_Err_OK;
    
    /** 1. 获取擦除扇区大小和单块擦除支持 **/
    uint32_t start_block, end_block;
    {
        uint64_t total_erase_size = (uint64_t) card->info.erase_sector_size * count;

        /** 计算块地址（考虑不同卡类型） **/
        switch(card->info.type)
        {
            case Sd_Type_SDHC:
            case Sd_Type_SDXC:
                start_block = addr / card->info.block_size;
                end_block = (addr + total_erase_size - 1) / card->info.block_size;
                break;

            case Sd_Type_SDSC_V1:
            case Sd_Type_SDSC_V2:
                start_block = addr;
                end_block = addr + total_erase_size - 1;
                break;

            default:
                trace_e(card, "Unsupported card type: %d", card->info.type);
                return Sd_Err_Unsupported;
        }
    }

    /** 选择卡 **/
    if((err = sd_spi_hw_select_card(card)) != Sd_Err_OK)
        return err;

    /** 2. 设置擦除起始地址 (CMD32) **/
    {
        struct sd_cmd_req req_start =
        {
            .cmd = Sd_Cmd32_Erase_Start, .arg = start_block, .crc = 1,
            .resp_type = Sd_Resp_Type_R1, .retry = 5
        };
        struct sd_resp_res resp = {0};
        if ((err = sd_card_send_cmd_req(card, &req_start, &resp)) != Sd_Err_OK)
            goto _END_;
        if (resp.buf[0] != SD_FR_NONE)
        {
            trace_e(card, "CMD32 error: 0x%02X", resp.buf[0]);
            err = Sd_Err_Failed;
            goto _END_;
        }
    }

    /** 3. 设置擦除结束地址 (CMD33) **/
    {
        struct sd_cmd_req req_end =
        {
            .cmd = Sd_Cmd33_Erase_End, .arg = end_block, .crc = 1,
            .resp_type = Sd_Resp_Type_R1, .retry = 5
        };
        struct sd_resp_res resp = {0};
        if ((err = sd_card_send_cmd_req(card, &req_end, &resp)) != Sd_Err_OK)
            goto _END_;
        if (resp.buf[0] != SD_FR_NONE)
        {
            trace_e(card, "CMD33 error: 0x%02X", resp.buf[0]);
            err = Sd_Err_Failed;
            goto _END_;
        }
    }

    /** 4. 执行擦除操作 (CMD38) **/
    {
        struct sd_cmd_req req_erase =
        {
            .cmd = Sd_Cmd38_Erase, .arg = 0, .crc = 1,
            .resp_type = Sd_Resp_Type_R1b, .retry = 5
        };
        struct sd_resp_res resp = {0};
        if ((err = sd_card_send_cmd_req(card, &req_erase, &resp)) != Sd_Err_OK)
            goto _END_;
        if (resp.buf[0] != SD_FR_NONE)
        {
            trace_e(card, "CMD38 error: 0x%02X", resp.buf[0]);
            err = Sd_Err_Failed;
            goto _END_;
        }
    }

_END_:;
    sd_spi_hw_deselect_card(card);

    return err;
}

/**
 * @brief 擦除整个SD卡
 * @param card            [in]  SD卡对象
 * @return enum sd_error  [out] 错误码
 */
enum sd_error sd_card_erase_chip (struct sd_card* card)
{
    if(card == NULL)
        return Sd_Err_Param;
    uint32_t count = card->info.capacity / card->info.erase_sector_size;
    trace_l(card, "Erase chip, capacity: %d MB, sector-size: %d KB, total sector-count: %d",
            card->info.capacity >> 20, card->info.erase_sector_size >> 10, count);
    return sd_card_erase_sector(card, 0, count);
}

/**
 * @brief 获取SD卡名称
 * @param card            [in]  SD卡对象
 * @return const char*    [out] 名称字符串指针
 */
const char* sd_card_get_name (struct sd_card* card)
{
    if(card == NULL)
        return NULL;
    return card->name;
}

/**
 * @brief 获取SD卡容量
 * @warning 只有在卡完成初始化后才能获取有效的返回结果
 * @param card      [in]  SD卡对象
 * @return uint64_t [out] 容量（字节）
 */
uint64_t sd_card_get_capacity (struct sd_card* card)
{
    if(card == NULL || !card->is_inited)
        return 0;
    return card->info.capacity;
}

/**
 * @brief 获取SD卡类型
 * @warning 只有在卡完成初始化后才能获取有效的返回结果
 * @param card           [in]  SD卡对象
 * @return enum sd_type  [out] 类型
 */
enum sd_type sd_card_get_type (struct sd_card* card)
{
    if(card == NULL)
        return Sd_Type_Not_SD;
    return card->info.type;
}

/**
 * @brief 获取SD卡块大小
 * @warning 只有在卡完成初始化后才能获取有效的返回结果
 * @param card       [in]  SD卡对象
 * @return uint32_t  [out] 块大小（字节）
 */
uint32_t sd_card_get_block_size (struct sd_card* card)
{
    if(card == NULL || !card->is_inited)
        return 0;
    return card->info.block_size;
}

/**
 * @brief 获取SD卡擦除的扇区的大小
 * @warning 只有在卡完成初始化后才能获取有效的返回结果
 * @param card       [in]  SD卡对象
 * @return uint64_t  [out] 擦除扇区大小（字节）
 */
uint64_t sd_card_get_erase_size (struct sd_card* card)
{
    if(card == NULL || !card->is_inited)
        return 0;
    return card->info.erase_sector_size;
}

/**
 * @brief 判断SD卡是否插入
 * @warning 该函数优先通过硬件 CD 引脚检查卡是否插入，若硬件未检测到卡，则通过 CMD0 命令复查（必须保证卡已初始化）。
 * @param card       [in]  SD卡对象
 * @return true      [out] 卡已插入
 * @return false     [out] 卡未插入
 */
bool sd_card_is_inserted (struct sd_card* card)
{
    if(card == NULL)
        return false;

    /** 硬件检查 **/
    if(sd_spi_hw_is_card_detached(card) == false)
        return true;

    /** 软件检查：在已初始化的情况下，发送CMD0，若返回 SD_FR_IN_IDLE_STATE，则代表卡已插入 **/
    if(card->is_inited == true)
        if(sd_card_into_idle(card) == Sd_Err_OK)
            return true;

    return false;
}

/**
 * @brief 设置用户数据
 * @param card  [in]  SD卡对象
 * @param data  [in]  用户数据指针
 */
void sd_card_set_user_data (struct sd_card* card, void* data)
{
    if(card == NULL)
        return;
    card->user_data = data;
}

/**
 * @brief 获取用户数据
 * @param card    [in]  SD卡对象
 * @return void*  [out] 用户数据指针
 */
void* sd_card_get_user_data (struct sd_card* card)
{
    if(card == NULL)
        return NULL;
    return card->user_data;
}