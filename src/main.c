#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define I2C_NODE DT_NODELABEL(i2c1)
#define TMP102_ADDR 0x48
#define TMP102_REG_TEMP 0x00

static int app_i2c_init(const struct device **i2c_dev_out)
{
    /* 1) 从设备树拿到 i2c1 对应的 Zephyr device 对象 */
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    /* 2) 驱动必须处于 ready 状态，否则后续访问一定失败 */
    if (!device_is_ready(i2c_dev)) {
        return -ENODEV;
    }

    /*
     * 3) 配置 I2C 控制器工作参数：
     *    - I2C_MODE_CONTROLLER: 主机模式（旧版宏叫 MASTER）
     *    - I2C_SPEED_STANDARD: 100kHz，调试阶段更稳
     */
    int ret = i2c_configure(i2c_dev, I2C_MODE_CONTROLLER | I2C_SPEED_SET(I2C_SPEED_STANDARD));
    if (ret) {
        return ret;
    }

    *i2c_dev_out = i2c_dev;
    return 0;
}

static int tmp102_read_temp_mC(const struct device *i2c_dev, int32_t *temp_mC)
{
    /* 温度寄存器地址 0x00 */
    uint8_t reg = TMP102_REG_TEMP;
    /* TMP102 温度寄存器长度 2 字节 */
    uint8_t buf[2];
    int ret;

    /*
     * I2C 复合事务（最常用）：
     *   START
     *   [ADDR + W] [ACK]
     *   [REG=0x00] [ACK]
     *   RESTART
     *   [ADDR + R] [ACK]
     *   [DATA_MSB] [ACK]
     *   [DATA_LSB] [NACK]
     *   STOP
     */
    ret = i2c_write_read(i2c_dev, TMP102_ADDR, &reg, 1, buf, 2);
    if (ret) {
        return ret;
    }

    /* TMP102 温度数据格式：12-bit 有符号，MSB first */
    int16_t raw = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    /* 低 4 位不是温度有效位，右移对齐 */
    raw >>= 4;
    /* 12-bit 符号位扩展到 int16_t */
    if (raw & 0x0800) {
        raw |= 0xF000;
    }

    /* TMP102 分辨率 0.0625C/LSB，即 62.5mC/LSB */
    *temp_mC = (raw * 625) / 10;
    return 0;
}

int main(void)
{
    const struct device *i2c_dev = NULL;
    int ret = app_i2c_init(&i2c_dev);
    if (ret) {
        LOG_ERR("I2C init failed: %d", ret);
        return 0;
    }

    LOG_INF("TMP102 demo start");

    while (1) {
        int32_t t_mC;
        ret = tmp102_read_temp_mC(i2c_dev, &t_mC);
        if (ret == 0) {
            int32_t abs_mC = (t_mC < 0) ? -t_mC : t_mC;
            LOG_INF("Temperature: %d.%03d C", t_mC / 1000, abs_mC % 1000);
        } else {
            LOG_ERR("Read failed: %d", ret);
        }
        k_sleep(K_SECONDS(1));
    }

    return 0;
}
