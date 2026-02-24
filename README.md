# STM32 + Zephyr I2C 温度传感器示例说明

本项目演示基于 STM32 芯片、Zephyr OS，通过 I2C 读取 TMP102 温度传感器数据。

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
- `prj.conf`：开启 I2C、日志、串口等 Kconfig 功能
- `boards/nucleo_f103rb.overlay`：板级设备树覆盖，启用 `i2c1`
- `src/main.c`：业务代码，周期读取温度并打印

构建与烧录（示例）：

```bash
west build -b nucleo_f103rb .
west flash
```

## 2. I2C 控制温度传感器：理论与代码对应

I2C 基础：

- STM32 作为 I2C 主机（Master）
- TMP102 作为从机（Slave），默认 7-bit 地址 `0x48`
- 读寄存器标准时序：写寄存器地址 + 重启 + 读数据

代码对应关系：

- `DEVICE_DT_GET(DT_NODELABEL(i2c1))`：获取 I2C 控制器设备
- `device_is_ready()`：确认外设驱动已就绪
- `i2c_write_read()`：一次完成“写寄存器地址 + 读寄存器数据”
- `tmp102_read_temp_mC()`：把 2 字节原始值转换为摄氏度毫度（mC）

TMP102 数据处理要点：

- 温度寄存器是 12-bit 有符号值（MSB first）
- 需右移对齐并做符号扩展
- 分辨率 `0.0625C/LSB`，即 `62.5mC/LSB`

## 3. 代码设计思路与关键配置

初始化流程在 `main()`：

1. 获取 I2C 设备
2. 检查设备 ready 状态
3. 循环读取温度并打印日志

配置上需要特别注意：

- `prj.conf` 必须启用 `CONFIG_I2C=y`
- overlay 中目标 I2C 节点需 `status = "okay"`
- I2C 引脚复用必须与板卡硬件一致
- 传感器地址脚（A0/A1）可能改变实际地址，必须核对
- 总线需要上拉电阻（常见 4.7k）

## 4. 调试常见问题与解决方案

1. `I2C device not ready`

- 原因：设备树没启用、节点名不匹配、板子选择错误
- 处理：检查 `boards/*.overlay`、`west build -b <board>` 参数

2. 读写返回 `-EIO` / `-ENXIO`

- 原因：地址错误、线序错误、无上拉、传感器未上电
- 处理：先确认 `0x48` 是否正确，再检查 SCL/SDA 和供电

3. 温度值异常（固定值/明显错误）

- 原因：位宽解析错误、符号扩展错误
- 处理：核对 TMP102 手册与 `main.c` 的 12-bit 解析逻辑

4. 总线偶发卡死

- 原因：异常复位或时序边界问题
- 处理：先使用 `100kHz`（`I2C_BITRATE_STANDARD`），必要时做总线恢复

## 文件索引

- `CMakeLists.txt`
- `prj.conf`
- `boards/nucleo_f103rb.overlay`
- `src/main.c`

## 适配说明

当前默认目标板为 `nucleo_f103rb`，I2C 控制器节点为 `i2c1`。如果你使用其他 STM32 型号或温度传感器（如 LM75A、SHT3x），需要同步修改：

- `west build -b <你的板卡>`
- `boards/<你的板卡>.overlay`
- `src/main.c` 中 I2C 节点、传感器地址、寄存器解析逻辑
