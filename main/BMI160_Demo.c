#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

/* ================== CẤU HÌNH CHÂN I2C ================== */
#define I2C_SDA_PIN     GPIO_NUM_21   // Chân P21 trên board
#define I2C_SCL_PIN     GPIO_NUM_22   // Chân P22 trên board
#define I2C_FREQ_HZ     400000        // Tốc độ I2C 400kHz (Fast mode)

/* ================== ĐỊA CHỈ I2C CỦA BMI160 ================== */
// Đã xác nhận bằng I2C Scanner: module này trả lời ở địa chỉ 0x69
// (vì chân SDO/SAO của module đang nối lên VCC, không phải GND)
#define BMI160_ADDR     0x69

/* ================== CÁC THANH GHI (REGISTER) CỦA BMI160 ================== */
#define REG_CHIP_ID     0x00   // Đọc ra phải là 0xD1 nếu kết nối đúng
#define REG_GYR_DATA    0x0C   // Bắt đầu dữ liệu gyro (12 byte: gyro XYZ + accel XYZ)
#define REG_ACC_CONF    0x40
#define REG_ACC_RANGE   0x41
#define REG_GYR_CONF    0x42
#define REG_GYR_RANGE   0x43
#define REG_CMD         0x7E   // Thanh ghi lệnh (bật nguồn accel/gyro, reset...)

static const char *TAG = "BMI160";
static i2c_master_dev_handle_t bmi160_handle;

/* ================== HÀM GHI 1 THANH GHI ================== */
static esp_err_t bmi160_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(bmi160_handle, buf, 2, 1000);
}

/* ================== HÀM ĐỌC NHIỀU BYTE TỪ 1 THANH GHI ================== */
static esp_err_t bmi160_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(bmi160_handle, &reg, 1, data, len, 1000);
}

/* ================== KHỞI TẠO I2C MASTER ================== */
static void i2c_master_setup(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_SCL_PIN,
        .sda_io_num = I2C_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // Bật điện trở kéo lên nội bộ cho SDA/SCL
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI160_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, &bmi160_handle));
}

/* ================== KHỞI TẠO CẢM BIẾN BMI160 ================== */
static bool bmi160_init(void)
{
    uint8_t chip_id = 0;

    // 1. Đọc CHIP_ID để kiểm tra đã kết nối đúng cảm biến chưa
    bmi160_read_regs(REG_CHIP_ID, &chip_id, 1);
    ESP_LOGI(TAG, "CHIP_ID doc duoc: 0x%02X (dung phai la 0xD1)", chip_id);
    if (chip_id != 0xD1) {
        ESP_LOGE(TAG, "Khong tim thay BMI160! Kiem tra lai day noi va dia chi I2C.");
        return false;
    }

    // 2. Bật nguồn (power mode) cho Accelerometer -> chế độ Normal
    bmi160_write_reg(REG_CMD, 0x11);
    vTaskDelay(pdMS_TO_TICKS(100)); // Accel cần ~3.8ms để khởi động, chờ dư cho chắc

    // 3. Bật nguồn cho Gyroscope -> chế độ Normal
    bmi160_write_reg(REG_CMD, 0x15);
    vTaskDelay(pdMS_TO_TICKS(100)); // Gyro cần ~55-80ms để khởi động

    // 4. Cấu hình dải đo (range) cho Accelerometer: ±2g
    bmi160_write_reg(REG_ACC_RANGE, 0x03);

    // 5. Cấu hình dải đo (range) cho Gyroscope: ±2000 độ/giây
    bmi160_write_reg(REG_GYR_RANGE, 0x00);

    ESP_LOGI(TAG, "Khoi tao BMI160 thanh cong!");
    return true;
}

/* ================== ĐỌC VÀ TÍNH TOÁN DỮ LIỆU CẢM BIẾN ================== */
static void bmi160_read_data(float *ax, float *ay, float *az,
                              float *gx, float *gy, float *gz)
{
    uint8_t raw[12];
    bmi160_read_regs(REG_GYR_DATA, raw, 12);

    // Ghép 2 byte (Low + High) thành số nguyên 16-bit có dấu
    int16_t gyro_x_raw  = (int16_t)((raw[1]  << 8) | raw[0]);
    int16_t gyro_y_raw  = (int16_t)((raw[3]  << 8) | raw[2]);
    int16_t gyro_z_raw  = (int16_t)((raw[5]  << 8) | raw[4]);
    int16_t accel_x_raw = (int16_t)((raw[7]  << 8) | raw[6]);
    int16_t accel_y_raw = (int16_t)((raw[9]  << 8) | raw[8]);
    int16_t accel_z_raw = (int16_t)((raw[11] << 8) | raw[10]);

    // Quy đổi ra đơn vị thực:
    // Accel: range ±2g, độ phân giải 16-bit -> 16384 LSB = 1g
    *ax = accel_x_raw / 16384.0f;
    *ay = accel_y_raw / 16384.0f;
    *az = accel_z_raw / 16384.0f;

    // Gyro: range ±2000 dps, độ phân giải 16-bit -> 16.4 LSB = 1 độ/giây
    *gx = gyro_x_raw / 16.4f;
    *gy = gyro_y_raw / 16.4f;
    *gz = gyro_z_raw / 16.4f;
}

/* ================== TÍNH GÓC NGHIÊNG (PITCH / ROLL) TỪ ACCEL ================== */
static void calc_angle(float ax, float ay, float az, float *pitch, float *roll)
{
    // Công thức lượng giác cơ bản dùng vector trọng lực đo được từ accel
    *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    *roll  = atan2f(ay, az) * 180.0f / M_PI;
}

/* ================== HÀM CHÍNH ================== */
void app_main(void)
{
    i2c_master_bus_handle_t bus_handle;

    ESP_LOGI(TAG, "Dang khoi tao I2C...");
    i2c_master_setup(&bus_handle);

    if (!bmi160_init()) {
        ESP_LOGE(TAG, "Dung chuong trinh do loi khoi tao cam bien.");
        return;
    }

    float ax, ay, az, gx, gy, gz, pitch, roll;

    while (1) {
        bmi160_read_data(&ax, &ay, &az, &gx, &gy, &gz);
        calc_angle(ax, ay, az, &pitch, &roll);

        printf("===== BMI160 SENSOR DATA =====\r\n");
        printf("Accel (g)   : X=%.3f  Y=%.3f  Z=%.3f\r\n", ax, ay, az);
        printf("Gyro (dps)  : X=%.3f  Y=%.3f  Z=%.3f\r\n", gx, gy, gz);
        printf("Goc nghieng : Pitch=%.2f  Roll=%.2f\r\n", pitch, roll);
        printf("===============================\r\n\r\n");

        vTaskDelay(pdMS_TO_TICKS(500)); // Đọc và in mỗi 0.5 giây
    }
}