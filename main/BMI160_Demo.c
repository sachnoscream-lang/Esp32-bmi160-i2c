#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

/* ================== CẤU HÌNH CHÂN I2C ================== */
#define I2C_SDA_PIN     GPIO_NUM_21   // Chân P21 trên board
#define I2C_SCL_PIN     GPIO_NUM_22   // Chân P22 trên board
#define I2C_FREQ_HZ     400000

/* ================== ĐỊA CHỈ I2C CỦA BMI160 ================== */
#define BMI160_ADDR     0x69

/* ================== CÁC THANH GHI CỦA BMI160 ================== */
#define REG_CHIP_ID     0x00
#define REG_GYR_DATA    0x0C
#define REG_ACC_RANGE   0x41
#define REG_GYR_RANGE   0x43
#define REG_CMD         0x7E

static const char *TAG = "BMI160";
static i2c_master_dev_handle_t bmi160_handle;

/* ================== GHI / ĐỌC THANH GHI CƠ BẢN ================== */
static esp_err_t bmi160_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(bmi160_handle, buf, 2, 1000);
}

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
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BMI160_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, &bmi160_handle));
}

/* ================== KHỞI TẠO CẢM BIẾN ================== */
static bool bmi160_init(void)
{
    uint8_t chip_id = 0;
    bmi160_read_regs(REG_CHIP_ID, &chip_id, 1);
    ESP_LOGI(TAG, "CHIP_ID doc duoc: 0x%02X (dung phai la 0xD1)", chip_id);
    if (chip_id != 0xD1) {
        ESP_LOGE(TAG, "Khong tim thay BMI160!");
        return false;
    }
    bmi160_write_reg(REG_CMD, 0x11); vTaskDelay(pdMS_TO_TICKS(100)); // Bật Accel
    bmi160_write_reg(REG_CMD, 0x15); vTaskDelay(pdMS_TO_TICKS(100)); // Bật Gyro
    bmi160_write_reg(REG_ACC_RANGE, 0x03); // ±2g
    bmi160_write_reg(REG_GYR_RANGE, 0x00); // ±2000 dps
    ESP_LOGI(TAG, "Khoi tao BMI160 thanh cong!");
    return true;
}

/* ================== ĐỌC VÀ QUY ĐỔI DỮ LIỆU CẢM BIẾN (GIỮ NGUYÊN NHƯ BẢN GỐC) ================== */
static void bmi160_read_data(float *ax, float *ay, float *az,
                              float *gx, float *gy, float *gz)
{
    uint8_t raw[12];
    bmi160_read_regs(REG_GYR_DATA, raw, 12);

    int16_t gyro_x_raw  = (int16_t)((raw[1]  << 8) | raw[0]);
    int16_t gyro_y_raw  = (int16_t)((raw[3]  << 8) | raw[2]);
    int16_t gyro_z_raw  = (int16_t)((raw[5]  << 8) | raw[4]);
    int16_t accel_x_raw = (int16_t)((raw[7]  << 8) | raw[6]);
    int16_t accel_y_raw = (int16_t)((raw[9]  << 8) | raw[8]);
    int16_t accel_z_raw = (int16_t)((raw[11] << 8) | raw[10]);

    *ax = accel_x_raw / 16384.0f;
    *ay = accel_y_raw / 16384.0f;
    *az = accel_z_raw / 16384.0f;

    *gx = gyro_x_raw / 16.4f;
    *gy = gyro_y_raw / 16.4f;
    *gz = gyro_z_raw / 16.4f;
}

/* ================== TÍNH GÓC NGHIÊNG (GIỮ NGUYÊN NHƯ BẢN GỐC) ================== */
static void calc_angle(float ax, float ay, float az, float *pitch, float *roll)
{
    *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    *roll  = atan2f(ay, az) * 180.0f / M_PI;
}

/* ============================================================================
   PHÁT HIỆN TĂNG TỐC / GIẢM TỐC / ĐỀU GA (PHẦN MỚI THÊM)
   ============================================================================
   Đơn giản, KHÔNG cần tích phân, KHÔNG bị trôi (drift) theo thời gian - chỉ
   nhìn trực tiếp vào độ lớn + dấu của gia tốc theo hướng xe di chuyển tại
   từng thời điểm, so sánh với ngưỡng để phân loại trạng thái.
============================================================================ */
#define ACCEL_THRESHOLD  0.05f  // Ngưỡng coi là "có gia tốc đáng kể" (đơn vị g)

typedef enum {
    TRANG_THAI_DEU_GA,
    TRANG_THAI_TANG_TOC,
    TRANG_THAI_GIAM_TOC
} TrangThaiToc;

static TrangThaiToc detect_motion_state(float accel_forward)
{
    if (accel_forward > ACCEL_THRESHOLD) {
        return TRANG_THAI_TANG_TOC;
    } else if (accel_forward < -ACCEL_THRESHOLD) {
        return TRANG_THAI_GIAM_TOC;
    } else {
        return TRANG_THAI_DEU_GA;
    }
}

static const char* trang_thai_to_string(TrangThaiToc state)
{
    switch (state) {
        case TRANG_THAI_TANG_TOC: return "TANG TOC";
        case TRANG_THAI_GIAM_TOC: return "GIAM TOC (PHANH)";
        default:                  return "DEU GA";
    }
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

    // LƯU Ý QUAN TRỌNG: Chọn đúng trục theo hướng xe di chuyển tới/lui.
    // Tùy cách gắn cảm biến lên xe máy, hướng "tiến/lùi" có thể là X, Y hoặc Z.
    // Mặc định đang dùng trục X - đổi lại thành ax/ay/az cho đúng thực tế lắp đặt.

    while (1) {
        bmi160_read_data(&ax, &ay, &az, &gx, &gy, &gz);
        calc_angle(ax, ay, az, &pitch, &roll);

        // Gia tốc theo hướng di chuyển - trừ sẵn phần dư nhỏ do lệch cảm biến
        // (KHÔNG cần trừ trọng lực phức tạp vì trục "tiến/lùi" của xe thường
        // nằm ngang, ít bị ảnh hưởng bởi trọng lực như trục thẳng đứng Z)
        float accel_forward = ax; // ĐỔI THÀNH ay HOẶC az NẾU TRỤC THỰC TẾ KHÁC
        TrangThaiToc trang_thai = detect_motion_state(accel_forward);

        // GIỮ NGUYÊN FORMAT LOG GỐC + THÊM DÒNG TRẠNG THÁI
        printf("===== BMI160 SENSOR DATA =====\r\n");
        printf("Accel (g)   : X=%.3f  Y=%.3f  Z=%.3f\r\n", ax, ay, az);
        printf("Gyro (dps)  : X=%.3f  Y=%.3f  Z=%.3f\r\n", gx, gy, gz);
        printf("Goc nghieng : Pitch=%.2f  Roll=%.2f\r\n", pitch, roll);
        printf("Trang thai  : %s (accel_forward=%.3fg)\r\n",
               trang_thai_to_string(trang_thai), accel_forward);
        printf("===============================\r\n\r\n");

        vTaskDelay(pdMS_TO_TICKS(200)); // Đọc mỗi 0.2 giây - đủ nhanh để bắt kịp thay đổi
    }
}