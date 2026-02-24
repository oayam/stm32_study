# STM32 + Zephyr I2C 温度传感器示例说明

本项目演示基于 STM32 芯片、Zephyr OS，通过 I2C 读取 TMP102 温度传感器数据。  
重点补充了你关心的两件事：

1. 驱动中 I2C 的具体初始化动作  
2. I2C 控制温度传感器时的步骤拆解

## 1. 如何将 Zephyr OS 代码加入工程

当前目录已采用标准 Zephyr App 结构：

```text
.
├─ CMakeLists.txt
├─ prj.conf
├─ boards/
│  └─ nucleo_f103rb.overlay
└─ src/
   └─ main.c
```

关键文件作用：

- `CMakeLists.txt`：声明 Zephyr 工程并编译 `src/main.c`
- `prj.conf`：打开 I2C、串口、日志等内核配置
- `boards/nucleo_f103rb.overlay`：启用板级 `i2c1` 节点
- `src/main.c`：初始化 I2C、读 TMP102、打印温度

构建与烧录（示例）：

```bash
west build -b nucleo_f103rb .
west flash
```

## 2. I2C 总线基础（入门必读）

I2C 是两线串行总线：

- `SCL`：时钟线
- `SDA`：数据线

关键知识点：

- 总线是开漏/开集电极结构，`SCL/SDA` 需要上拉电阻（常见 4.7k）
- 总线空闲时两根线都为高电平
- 主机发起通信，从机按地址应答
- 每发送 8 bit 后，会有 1 bit ACK/NACK 应答位
- 常见信号：
  - `START`：通信开始
  - `RESTART`：不释放总线、直接切换读写方向
  - `STOP`：通信结束

在本项目中：

- STM32 是主机（controller/master）
- TMP102 是从机（slave）
- 默认 7-bit 地址为 `0x48`

## 3. 驱动中 I2C 初始化动作（对应 `src/main.c`）

代码里新增了 `app_i2c_init()`，把底层初始化显式拆出来。执行顺序如下：

1. 获取设备对象  
`DEVICE_DT_GET(DT_NODELABEL(i2c1))`

- 从设备树取 `i2c1` 这个控制器实例
- 得到 Zephyr 的 `struct device *`

2. 检查驱动是否就绪  
`device_is_ready(i2c_dev)`

- 若返回 false，说明驱动未成功初始化或节点未启用
- 直接返回错误，避免后续 I2C 访问失败

3. 配置 I2C 控制器参数  
`i2c_configure(i2c_dev, I2C_MODE_CONTROLLER | I2C_SPEED_SET(I2C_SPEED_STANDARD))`

- `I2C_MODE_CONTROLLER`：主机模式
- `I2C_SPEED_STANDARD`：100kHz
- 调试阶段先用 100kHz，通常更稳定

4. 输出句柄给业务层  
`*i2c_dev_out = i2c_dev`

- 后续读写函数统一使用这个已初始化设备

说明：

- Zephyr 底层驱动会在系统启动阶段完成硬件寄存器初始化
- 应用层 `app_i2c_init()` 属于“运行时确认 + 参数配置”，便于排错和维护

## 4. I2C 控制温度传感器的步骤拆解

### 4.1 业务目标

读取 TMP102 温度寄存器 `0x00` 的 2 字节数据，并换算成摄氏度。

### 4.2 总线事务时序（代码在 `tmp102_read_temp_mC()`）

`i2c_write_read()` 做的是一次组合事务：

1. `START`
2. 发送 `ADDR(0x48) + W`
3. 从机 ACK
4. 发送寄存器地址 `0x00`
5. 从机 ACK
6. `RESTART`
7. 发送 `ADDR(0x48) + R`
8. 从机 ACK
9. 读取字节 1（MSB），主机 ACK
10. 读取字节 2（LSB），主机 NACK
11. `STOP`

这正是“先写寄存器地址，再读寄存器数据”的标准做法。

### 4.3 数据解析步骤（TMP102）

读到 `buf[0], buf[1]` 后：

1. 拼成 16-bit：`raw = (buf[0] << 8) | buf[1]`
2. 右移 4 位，得到 12-bit 有效温度值
3. 若 12-bit 符号位为 1，做符号扩展
4. 按分辨率换算：
   - `1 LSB = 0.0625C = 62.5mC`
   - 代码里用 `*temp_mC = (raw * 625) / 10`

## 5. 配置与初始化的易错点

- `prj.conf` 必须有 `CONFIG_I2C=y`
- `boards/<board>.overlay` 中对应 I2C 节点必须 `status = "okay"`
- `DT_NODELABEL(i2c1)` 要与你板子实际 I2C 控制器一致
- 引脚复用必须对应你的硬件连线
- 地址脚变化会导致从机地址变化，不一定总是 `0x48`

## 6. 调试常见问题与解决方案

1. `I2C init failed: -19`（`-ENODEV`）

- 原因：设备树节点没启用、板子名不匹配
- 处理：检查 overlay、`west build -b <board>`

2. `Read failed: -5/-6`（常见 `-EIO/-ENXIO`）

- 原因：地址错误、没有 ACK、硬件连线或上拉问题
- 处理：核对地址、SCL/SDA、传感器供电

3. 温度数值明显错误

- 原因：位对齐/符号扩展逻辑错
- 处理：对照 TMP102 手册确认 12-bit 有符号格式

4. 总线偶发卡死

- 原因：时序边界或异常复位后从机状态异常
- 处理：先固定 100kHz，再考虑总线恢复或硬复位从机

## 7. 文件索引

- `CMakeLists.txt`
- `prj.conf`
- `boards/nucleo_f103rb.overlay`
- `src/main.c`

## 8. 适配说明

当前默认目标板为 `nucleo_f103rb`，I2C 控制器节点为 `i2c1`。如果你换成其他 STM32 或其他温度传感器（如 LM75A）：

- 修改构建板卡：`west build -b <你的板卡>`
- 修改 overlay：`boards/<你的板卡>.overlay`
- 修改业务参数：设备树节点、I2C 地址、寄存器与换算逻辑
