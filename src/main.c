#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define I2C_NODE DT_NODELABEL(i2c1)
#define TMP102_ADDR 0x48
#define TMP102_REG_TEMP 0x00

static int tmp102_read_temp_mC(const struct device *i2c_dev, int32_t *temp_mC)
{
    uint8_t reg = TMP102_REG_TEMP;
    uint8_t buf[2];
    int ret;

    ret = i2c_write_read(i2c_dev, TMP102_ADDR, &reg, 1, buf, 2);
    if (ret) {
        return ret;
    }

    /* TMP102: 12-bit signed, MSB first */
    int16_t raw = (int16_t)(((uint16_t)buf[0] << 8) | buf[1]);
    raw >>= 4;
    if (raw & 0x0800) {
        raw |= 0xF000;
    }

    /* 0.0625C per LSB => 62.5mC */
    *temp_mC = (raw * 625) / 10;
    return 0;
}

int main(void)
{
    const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return 0;
    }

    LOG_INF("TMP102 demo start");

    while (1) {
        int32_t t_mC;
        int ret = tmp102_read_temp_mC(i2c_dev, &t_mC);
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
