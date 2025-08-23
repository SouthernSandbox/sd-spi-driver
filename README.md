# 一、sd-spi-driver 简介
sd-spi-driver 是一个简易的SD卡SPI驱动库，其内部实现与实际硬件分离，适用于单片机平台，提供最基本的卡识别、读、写、擦除和卡信息获取等功能，并具备可移植性和扩展性。
目录：
- 一、sd-spi-driver 简介
- 二、注意事项
- 三、文件结构
- 四、移植过程
- 五、库的使用
- 六、卡信息的打印
- 七、用户如何实现自定义的卡控制?
- 八、与文件系统的对接
- 九、未来

# 二、注意事项
1. sd-spi-driver 并没有实现很多SD卡的复杂操作，因为它的初衷是 “实现轻量级的跨平台SD卡SPI驱动，同时为移植文件系统（如FATFS）提供最基础的功能”；
2. 本库内部实现与具体硬件无关，通过抽象的用户硬件通信接口间接与硬件交互；
3. 本库采用C99标准。
4. 本库默认仅开放 sd_spi_driver.h 下提供的函数所支持的功能，若用户想处理其他尚未支持的SD命令的话，可参考本文 `七、如何实现自定义的卡控制`。
5. 现阶段已完成 4GB-SDHC卡 和 128GB-SDXC卡 的测试，SDSC 卡尚未纳入实际测试，若后续完成则将补充文字说明。

# 三、文件结构
- `./inc/sd_config.h` 库配置文件
- `./inc/sd_def.h` 类型定义
- `./inc/sd_private.h` 适用于内部使用的私有头文件，不应该被外部使用
- `./inc/sd_spi_driver.h` 可由用户引用的库头文件
- `./port/port.c` 与平台有关移植接口文件
- `./src/sd_core.c` 核心文件，库初始化、实现逻辑等
- `./src/sd_hwio.c` 用于实现与SD卡进行硬件交互的操作
- `./src/sd_info.c` 解析SD卡身份与配置信息
- `./src/sd_utils.c` 工具/辅助类函数

# 四、移植过程
## 4.1 添加库文件
将 ./inc/、./port/、./src/ 的文件移植到工程中，引用头文件时，可包含 `sd_spi_driver.h` 头文件。

## 4.2 硬件平台适配
用户可参考 ./port/ 中的示例文件针对自己运行的平台进行修改，此处进行简要说明。平台适配最主要的是实现 `struct sd_spi_interface` 结构体中定义的函数（最重要的硬件交互就在这里实现），其次是 `struct sd_debug_interface` 实现打印输出，然后在 `sd_config.h` 引用在 port.c 中定义的 `struct sd_card*` 对象。 

### 4.2.1 实现 control()
control() 函数涉及片选、总线获取、硬件初始化、延时等多个重要操作，用户需要实现其中必要的操作。执行成功时函数返回 0，反之返回-1。
```c
// 例子
static int _control(struct sd_card* card, enum sd_user_ctrl ctrl)
{
    switch(ctrl)
    {
    case Sd_User_Ctrl_Init_Hardware:     _init(card); break;                    // 初始化硬件
    case Sd_User_Ctrl_Deinit_Hardware:   _deinit(card); break;                  // 硬件去初始化
    case Sd_User_Ctrl_Is_Card_Detached:  return -1;                             // 卡是否已拔出，return -1; 代表软件不做检查，始终认为硬件上卡没有被拔出
    case Sd_User_Ctrl_Select_Card:       GPIOA_ResetBits(GPIO_Pin_12); break;   // 选中卡
    case Sd_User_Ctrl_Deselect_Card:     GPIOA_SetBits(GPIO_Pin_12); break;     // 取消选中卡
    case Sd_User_Ctrl_Take_Bus:          break;                                 // 获取总线资源，适用于操作系统环境或可能存在资源竞争的情况
    case Sd_User_Ctrl_Release_Bus:       break;                                 // 释放总线资源，适用于操作系统环境或可能存在资源竞争的情况
    case Sd_User_Ctrl_Set_Low_Speed:                                            // 设置SPI通信速率为低速，用于卡上电初始化阶段
    case Sd_User_Ctrl_Set_High_Speed:    _set_speed(card, ctrl); break;         // 设置SPI通信速率为高速，用于卡初始化完成后的高速数据交互
    }
    return 0;
}
```

### 4.2.2 实现 transfer()
transfer() 函数涉及SPI通信，是最核心的数据通信函数。**需要注意的是，在接收SD卡数据时，需要确保MOSI总线始终能发送 0xff，这里不同平台的实现有所差别，使用时需要仔细检查**。例如，STM32 接收数据时，推荐使用 HAL_SPI_TransmitReceive()，接收时手动发送 0xff；而 CH583M 可直接调用 SPI0_MasterRecvByte()，因为库函数内部已实现向 MOSI 发送 0xff。
```c
static int _transfer(struct sd_card* card, struct sd_spi_buf* tx, struct sd_spi_buf* rx)
{
    if(tx)
    {
        SPI0_MasterTrans(tx->data, tx->size);
        tx->used = tx->size;
    }

    if(rx)
    {
        for(rx->used = 0; rx->used != rx->size; rx->used++)
            ((uint8_t* )rx->data)[rx->used] = SPI0_MasterRecvByte();
    }
    return 0;
}
```
### 4.2.3 实现 delay_us()
部分情况下，我们需要等待SD卡一定时间才能得到想要的结果，因此这里实现 delay_us()，避免过于频繁的操作请求。
```c
// 例子
static void _delay_us(struct sd_card* card, uint32_t us)
{
    DelayUs(us);
}
```

### 4.2.4 实现 print()
sd-spi-driver 的打印输出通过 `struct sd_debug_interface` 的 `print()` 字段实现，用于库内部的日志打印。
```c
// 例子
static void _print(struct sd_card* card, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    static char buf[256];
    vsnprintf(buf, sizeof(buf), format, args);
    printf("%s", buf);
    va_end(args);
}
```

### 4.2.5 封装接口函数
完成上面所有函数的实现后，用户需要定义 `struct sd_spi_interface` 和 `struct sd_debug_interface` 两个结构体变量，并将函数赋值给结构体内部的函数指针字段。
```c
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
```

### 4.2.6 定义 struct sd_card 变量
在 port.c 的最后，用户需要定义 `struct sd_card` 结构体变量，然后通过 `SD_CARD_OBJ_INIT()` 宏函数对变量进行初始化，用户需要为这个结构体对象命名，并提供前面编写的封装了函数接口的结构体。
```c
struct sd_card card0 = SD_CARD_OBJ_INIT("card0", &_spi2_intf, &_debug_intf);
```

## 4.3 将新建的 struct sd_card 结构体变量交由库进行管理
完成以上操作后，用户需要到 `sd_config.h` 文件中，使用 extern 关键字声明 struct sd_card 结构体变量，并将变量地址填入到 SD_CARD_ARR_DEFINE 中（即“注册”到库的数组中）。
```c
extern struct sd_card card0;

/**
 * @brief 定义 SD 卡数组
 */
#define SD_CARD_ARR_DEFINE      \
        {                       \
            &card0,             \
        }
```

# 五、库的使用
完成移植后，用户可以调用 `sd_spi_lib_init()` 对库进行初始化，然后通过 `sd_card_find()` 查找符合名字的 `struct sd_card*` 变量指针。如果获取成功，则通过 `sd_card_init()` 对卡进行初始化，若返回 `Sd_Err_OK` 则代表初始化成功，用户就可以使用 `sd-spi-driver.h` 下的其他库函数对SD卡进行读写擦或者信息读取操作。
```c
#include "sd_spi_driver.h"

int main(void)
{
    // ...

    /** 初始化库 **/
    sd_spi_lib_init();

    /** 查询目标卡对象 **/
    struct sd_card* card = sd_card_find("card0");
    if(card == NULL)
    {
        printf("card not found\r\n");
        while(1);
    }
    else
        printf("card(\"%s\") found\r\n", card->name);

    /** SD卡初始化 **/
    if(sd_card_init(card) != Sd_Err_OK)
    {
        printf("card init failed\r\n");
        while(1);
    }
    else
        printf("card init success, type: %s\r\n", sd_get_capacity_class_name(sd_card_get_type(card)));

    // ...
};

```

# 六、卡信息的打印
若用户的调试追踪等级为 `SD_SPI_TRACE_LEVEL_LIB ` 及以下，则库在初始化成功后会打印以下调试信息以表示卡的识别情况。
```shell
This is a SDHC card
  > Name: "card0"
  > Capacity: 3724 MB
  > Block size: 512 B
  > Erase sector size: 65536 KB
```

# 七、用户如何实现自定义的卡控制?
本库的在设计之初并没有考虑支持复杂的SD卡功能，如果用户确实需要对卡执行其他本库尚未支持的命令控制或自行封装对卡的操作的话，可以手动包含 `sd_private.h`，其下声明的部分函数可能会满足你的需求。

例如，若用户向SD卡发送命令并获取响应，可使用 `sd_card_send_cmd_req()`，但是 `sd_card_send_cmd_req()` 并未支持所有响应处理，因此用户可能需要结合 `sd_spi_hw_write_byte()`、`sd_spi_hw_read_byte()` 等基础函数自行实现功能和封装等。使用这些函数时需要注意，用户需要手动选中卡和取消选中卡（仅 `sd_spi_driver.h` 下的函数会自动处理卡的选中），否则可能导致读写失败，必要时请参考所使用函数的具体实现。

```c
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
enum sd_error sd_card_identify      (struct sd_card* card);
enum sd_error sd_card_send_cmd_req  (struct sd_card* card, struct sd_cmd_req* req, struct sd_resp_res* resp);
enum sd_error sd_card_get_status    (struct sd_card *card, uint8_t *status);

void          sd_spi_hw_set_speed           (struct sd_card* card, enum sd_user_ctrl speed);
bool          sd_spi_hw_is_card_detached    (struct sd_card* card);

void          sd_card_print_info    (struct sd_card* card);
```

# 八、与文件系统的对接
本库已为移植文件系统提供了最基础的功能，下面以 FATFS 为例进行简单介绍。在 FATFS 中用户需要实现以下接口函数的实现：
- disk_status()
- disk_initialize()
- disk_read()
- disk_write()
- disk_ioctl()
其中，后三个是与SD卡交互的核心。

## 8.1 disk_read() 的对接
FATFS的 sector 相当于块号，而 sd-spi-driver 的 sd_card_read() 则要求的是字节地址（需满足块地址的对齐），同时 count 意为块数，而 sd_card_read() 则要求读取长度是字节数（同样需满足读取长度是块长度的倍数）。
```c
DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	uint32_t block_size = sd_card_get_block_size(card);

	switch (pdrv) 
	{
	case DEV_SDCARD :
		if(sd_card_read(card, sector * block_size, buff, count * block_size) == Sd_Err_OK)
			return RES_OK;
		break;
	}

	return RES_ERROR;
}
```

## 8.2 disk_write() 的对接
FATFS的 sector 相当于块号，而 sd-spi-driver 的 sd_card_write() 则要求的是字节地址（需满足块地址的对齐），同时 count 意为块数，而 sd_card_write() 则要求写入长度是字节数（同样需满足写入长度是块长度的倍数）。
```c
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	uint32_t block_size = sd_card_get_block_size(card);

	switch (pdrv) 
	{
	case DEV_SDCARD :
		if(sd_card_write(card, sector * block_size, buff, count * block_size) == Sd_Err_OK)
			return RES_OK;
		break;
	}

	return RES_ERROR;
}
```

## 8.3 disk_ioctl() 的对接
此处的 disk_ioctl() 仅实现对卡部分信息的获取操作，其他指令请参考 FATFS 官网。
- `GET_SECTOR_COUNT` 获取SD卡块的个数
- `GET_SECTOR_SIZE` 获取SD卡单块的大小
- `GET_BLOCK_SIZE` 获取SD卡块擦除的大小，尽管本库仅提供了擦除扇区的函数，但是SD卡在写入单块时会自行处理单块擦除操作而无需用户控制，此处直接填1即可

```c
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	switch (pdrv) 
	{
	case DEV_SDCARD :
		switch(cmd)
		{
		case GET_SECTOR_COUNT:
			*(DWORD*)buff = sd_card_get_capacity(card) / sd_card_get_block_size(card);
			return RES_OK;

		case GET_SECTOR_SIZE:
			*(WORD*)buff = sd_card_get_block_size(card);
			return RES_OK;

		case GET_BLOCK_SIZE:
			*(DWORD*)buff = 1;
			return RES_OK;

		case CTRL_SYNC:
			return RES_OK;
		}
		break;
	}

	return RES_PARERR;
}
```
完成 FATFS 所有移植函数的要求后即可正常使用文件系统。

# 九、未来
当前 sd-spi-driver 已经完成了大部分既定的功能，未来可能会不定期的修复一些可能的BUG，或优化内部实现结构，同时补充 SDSC 卡的测试。

















