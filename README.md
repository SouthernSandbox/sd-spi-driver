# 一、sd-spi-driver 简介
sd-spi-driver 是一个简易的SD卡SPI驱动库，其内部实现与实际硬件分离，适用于单片机平台，支持最基本的卡识别、读、写、擦除和卡信息获取等功能。

# 二、注意事项
1. sd-spi-driver 并没有实现很多SD卡的复杂操作，因为它的初衷是 “实现轻量级的跨平台SD卡SPI驱动，同时为移植文件系统（如FATFS）提供最基础的功能”；
2. 本库内部实现与具体硬件无关，通过抽象的用户硬件通信接口间接与硬件交互；
3. 本库采用C99标准。

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





























