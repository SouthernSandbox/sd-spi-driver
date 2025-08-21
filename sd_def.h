/**
 * @file sd_def.h
 * @author SouthernSandbox (https://github.com/SouthernSandbox)
 * @brief 类型定义
 * @version 0.1
 * @date 2025-08-14
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef SD_DEF_H
#define SD_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"


#ifndef COUNT_OF
    #define COUNT_OF(arr) (int)(sizeof(arr) / sizeof(arr[0]))
#endif


/**
 * @brief SD 卡类型
 * @note 可以通过 @see sd_get_capacity_class_name() 函数获取类型的字符串名称。
 */
enum sd_type
{
    Sd_Type_Not_SD,         // 非SD卡（MMC或其他）
    
    /*---- SD标准容量 (<=2GB) ----*/
    Sd_Type_SDSC_V1,        // SDSC v1.0 (协议版本1.0)
    Sd_Type_SDSC_V2,        // SDSC v2.0 (协议版本2.0)
    
    /*---- SD高容量 (2GB-32GB) ----*/
    Sd_Type_SDHC,           // SDHC v2.0 (协议版本2.0)
    
    /*---- SD扩展容量 (>32GB) ----*/
    Sd_Type_SDXC,           // SDXC v3.0+ (协议版本3.0及以上)
    
    Sd_Type_Unknown         // 未知类型（需错误处理）
};

/**
 * @brief SD 卡错误类型
 */
enum sd_error
{
    Sd_Err_OK,              // 无错误
    Sd_Err_IO,              // I/O 错误
    Sd_Err_Timeout,         // 超时
    Sd_Err_Unsupported,     // 不支持
    Sd_Err_Not_Inited,      // 未初始化
    Sd_Err_Failed,          // 执行失败
    Sd_Err_Param,           // 无效参数
    Sd_Err_No_Ready,        // 卡未就绪
    Sd_Err_Response,        // 不正常的响应
};

/**
 * @brief SD 命令响应类型
 */
enum sd_resp_type
{
    Sd_Resp_Type_R1,    // R1(1字节)
    Sd_Resp_Type_R2,    // R1(1字节) + OCR(16字节数据) + CRC(2字节)，struct sd_resp_res 中本库仅保留 16 位 OCR 数据。
    Sd_Resp_Type_R3,    // R1(1字节) + OCR寄存器(4字节)
    Sd_Resp_Type_R7,    // R1(1字节) + 数据(4字节)
    Sd_Resp_Type_R1b,   // R1(1字节) + 忙等待(1字节)
};

/**
 * @brief SD 命令索引
 */
enum sd_cmd_index
{
#define CMD_ADD_FLAG(_v)    (_v | 0x40)

    /** 基本命令 **/
    Sd_Cmd0_Idle                = CMD_ADD_FLAG(0),      // 复位卡，响应 R1

    // Sd_Cmd1_Op_Cond             = CMD_ADD_FLAG(1),      // 仅MMC使用
    Sd_Cmd8_If_Cond             = CMD_ADD_FLAG(8),      // 检查SD电压范围，响应 R7
    Sd_Cmd9_Csd                 = CMD_ADD_FLAG(9),      // 读取CSD寄存器，响应 R1
    Sd_Cmd10_Cid                = CMD_ADD_FLAG(10),     // 读取CID寄存器，响应 R1
    Sd_Cmd12_Stop_Xfer          = CMD_ADD_FLAG(12),     // 停止多块传输，响应 R1b
    Sd_Cmd13_Status             = CMD_ADD_FLAG(13),     // 读取状态寄存器，响应 R1

    /** 读取命令 **/
    Sd_Cmd16_Block_len          = CMD_ADD_FLAG(16),     // 设置块长度(仅SDSC)，响应 R1
    Sd_Cmd17_Rd_Single          = CMD_ADD_FLAG(17),     // 读取单块，响应 R1
    Sd_Cmd18_Rd_Multi           = CMD_ADD_FLAG(18),     // 读取多块，响应 R1

    /** 写入命令 **/
    Sd_Cmd24_Wr_Single_Blk      = CMD_ADD_FLAG(24),     // 写入单块，响应 R1
    Sd_Cmd25_Wr_Multi_Blk       = CMD_ADD_FLAG(25),     // 写入多块，响应 R1

    /** 擦除命令 **/
    Sd_Cmd32_Erase_Start        = CMD_ADD_FLAG(32),     // 设置擦除起始地址，响应 R1
    Sd_Cmd33_Erase_End          = CMD_ADD_FLAG(33),     // 设置擦除结束地址，响应 R1
    Sd_Cmd38_Erase              = CMD_ADD_FLAG(38),     // 执行擦除，响应 R1b
    
    /** 其他 **/
    Sd_Cmd55_App_Cmd            = CMD_ADD_FLAG(55),     // 应用命令前缀，响应 R1
    Sd_Cmd58_Rd_Ocr             = CMD_ADD_FLAG(58),     // 读取OCR寄存器，响应 R3

    /** 应用命令 (需要先发送CMD55) **/
    Sd_Acmd41_Op_Cond           = CMD_ADD_FLAG(41),     // 开始SD卡初始化和检查SD卡是否初始化完成，响应 R1

#undef CMD_ADD_FLAG
};

/**
 * @brief SD 命令响应标志位
 */
#define SD_FR_NONE                      (0 << 0)    // 无响应（0x00）
#define SD_FR_IN_IDLE_STATE             (1 << 0)    // 已空闲状态（0x01）
#define SD_FR_ERASE_RESET               (1 << 1)    // 擦除重置（0x02）
#define SD_FR_ILLEGAL_COMMAND           (1 << 2)    // 非法命令（0x04）
#define SD_FR_COM_CRC_ERROR             (1 << 3)    // 命令CRC错误（0x08）
#define SD_FR_ERASE_SEQUENCE_ERROR      (1 << 4)    // 擦除序列错误（0x10）
#define SD_FR_ADDRESS_ERROR             (1 << 5)    // 地址错误（0x20）
#define SD_FR_PARAMETER_ERROR           (1 << 6)    // 参数错误（0x40）
#define SD_FR_FAILED                    (0xff)      // 擦除失败

/**
 * @brief SD 命令请求结构体
 */
struct sd_cmd_req
{
    enum sd_cmd_index   cmd;            // 命令索引
    uint32_t            arg;            // 命令参数
    uint8_t             crc;            // CRC校验值
    enum sd_resp_type   resp_type;      // 期待的响应类型
    uint8_t             retry;          // 接收响应的重试次数
};

/**
 * @brief SD 命令响应结果结构体
 */
struct sd_resp_res
{
    uint8_t buf[16];    // 响应缓冲区
    uint8_t filled;     // 已填充数据长度
};

/**
 * @brief SD 逻辑块操作请求（用户输入）
 */
struct sd_lba_req
{
    uint64_t offset;    // 逻辑块偏移（字节地址）
    uint32_t len;       // 请求长度（字节）
};

/**
 * @brief SD 逻辑块操作参数（内部使用）
 */
struct sd_lba_oparg
{
    uint32_t lba_addr;      // 逻辑块地址（LBA）
    uint32_t lba_count;     // 连续块数量
};

/**
 * @brief SD 卡信息
 */
struct sd_info
{
    uint64_t      capacity;             // 容量（单位：字节）
    uint64_t      block_count;          // 总块数（单位：字节）
    uint32_t      erase_sector_size;    // 最小擦除扇区大小（单位：字节）
    uint16_t      block_size;           // 块大小（单位：字节）
    enum sd_type  type;                 // 类型
};

/**
 * @brief SD 卡用户控制操作
 * @note 这些枚举适用于 struct sd_spi_interface 中的 control() 函数，用于执行有关硬件控制操作。
 */
enum sd_user_ctrl
{
    /** 硬件初始化 **/
    Sd_User_Ctrl_Init_Hardware,      // 初始化硬件
    Sd_User_Ctrl_Deinit_Hardware,    // 去初始化硬件
    Sd_User_Ctrl_Is_Card_Detached,   // 检查卡是否已拔出

    /** 片选控制 **/
    Sd_User_Ctrl_Select_Card,        // 选择卡
    Sd_User_Ctrl_Deselect_Card,      // 取消选择卡

    /** 总线占用 **/
    Sd_User_Ctrl_Take_Bus,           // 获取总线
    Sd_User_Ctrl_Release_Bus,        // 释放总线

    /** 通信速率 **/
    Sd_User_Ctrl_Set_Low_Speed,      // 设置SPI为低速通信速率，用于初始化，一般建议在 kHz 级别（如 250~400kHz）
    Sd_User_Ctrl_Set_High_Speed,     // 设置SPI为高速通信速率，用于读写数据，可提高至 MHz 级别（如 4~50MHz）
};

/**
 * @brief 用于 struct sd_spi_interface 中的 transfer() 函数，用于  SPI 数据交互的结构体
 */
struct sd_spi_buf
{
    void* data;     // 指向实际的缓冲区
    size_t size;    // 缓冲区的大小
    size_t used;    // 缓冲区的数据填充量。发送时，表示发送成功的字节数；接收时，表示接收成功的字节数。
};


/**
 * @brief SD 卡用户 SPI 硬件交互接口
 */
struct sd_card;
struct sd_spi_interface
{
    int  (*control)         (struct sd_card* card, enum sd_user_ctrl ctrl);                          // 硬件控制，执行成功返回 0，失败返回 -1
    int  (*transfer)        (struct sd_card* card, struct sd_spi_buf* tx, struct sd_spi_buf* rx);    // 发送和接收数据
    void (*delay_us)        (struct sd_card* card, uint32_t us);                                     // 延时函数，单位为微秒
};

/**
 * @brief SD 卡用户调试接口
 */
struct sd_debug_interface
{
    void (*print) (struct sd_card* card, const char* format, ...);        // 打印调试信息
};

/**
 * @brief SD 卡对象
 */
struct sd_card
{
    const char*                 name;                 // 名称
    struct sd_spi_interface*    spi_if;               // SPI 接口
    struct sd_debug_interface*  debug_if;             // 调试接口
    void*                       user_data;            // 用户数据
    struct sd_info              info;                 // 卡信息
    bool                        is_inited     :1;     // 是否已初始化
    bool                        is_selected   :1;     // 是否已选中SD卡
    bool                        is_xfering    :1;     // 是否正处于数据收发状态
};
#define SD_CARD_OBJ_INIT(_name, _spi_if, _debug_if) \
   {                                                \
        .name           = _name,                    \
        .spi_if         = _spi_if,                  \
        .debug_if       = _debug_if,                \
        .user_data      = NULL,                     \
        .info           = (struct sd_info){0},      \
        .is_inited      = false,                    \
        .is_selected    = false,                    \
        .is_xfering     = false,                    \
    }



#ifdef __cplusplus
}
#endif

#endif  // SD_DEF_H
