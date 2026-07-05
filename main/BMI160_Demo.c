#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

/* ================== CẤU HÌNH CHÂN I2C ================== */
#define I2C_SDA_PIN     GPIO_NUM_21
#define I2C_SCL_PIN     GPIO_NUM_22
#define I2C_FREQ_HZ     400000

/* ================== ĐỊA CHỈ I2C CỦA BMI160 ================== */
#define BMI160_ADDR     0x69

/* ================== CÁC THANH GHI CỦA BMI160 ================== */
#define REG_CHIP_ID     0x00
#define REG_GYR_DATA    0x0C
#define REG_TEMP_DATA   0x20   // Thanh ghi nhiệt độ (2 byte)
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

/* ================== ĐỌC ACCEL + GYRO (12 BYTE) ================== */
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

    *ax = accel_x_raw / 16384.0f;  // ±2g  -> 16384 LSB/g
    *ay = accel_y_raw / 16384.0f;
    *az = accel_z_raw / 16384.0f;

    // Đổi Gyro sang rad/s luôn (đơn vị chuẩn cho Madgwick Filter)
    *gx = (gyro_x_raw / 16.4f) * (M_PI / 180.0f);
    *gy = (gyro_y_raw / 16.4f) * (M_PI / 180.0f);
    *gz = (gyro_z_raw / 16.4f) * (M_PI / 180.0f);
}

/* ================== ĐỌC NHIỆT ĐỘ (BÙ NHIỆT - BẬC 3) ================== */
// BMI160 tích hợp sẵn cảm biến nhiệt độ. Bias của Accel/Gyro trong thực tế
// thay đổi theo nhiệt độ môi trường (đặc tính vật lý của cảm biến MEMS).
// Đọc nhiệt độ giúp giám sát điều kiện đo, và có thể mở rộng thêm bảng bù
// trừ bias theo nhiệt độ nếu có dữ liệu hiệu chuẩn ở nhiều mức nhiệt khác nhau.
static float bmi160_read_temperature(void)
{
    uint8_t raw[2];
    bmi160_read_regs(REG_TEMP_DATA, raw, 2);
    int16_t temp_raw = (int16_t)((raw[1] << 8) | raw[0]);
    // Công thức theo datasheet BMI160: Nhiệt độ (°C) = 23 + raw/512
    return 23.0f + (temp_raw / 512.0f);
}

/* ============================================================================
   MADGWICK FILTER (BẬC 2) - Ước lượng hướng (orientation) bằng Quaternion
   ============================================================================
   Đây là thuật toán AHRS (Attitude and Heading Reference System) tiêu chuẩn
   trong công nghiệp (dùng trong drone, robot, tay cầm VR...), kết hợp Gyro
   (mượt, tức thời nhưng tự trôi theo t.gian) và Accel (chính xác dài hạn khi
   tĩnh nhưng nhiễu khi có chuyển động) thông qua 1 bộ lọc thích ứng duy nhất,
   biểu diễn hướng bằng Quaternion (q0,q1,q2,q3) thay vì góc Euler đơn giản
   -> xử lý đúng cho MỌI góc nghiêng, không bị "gimbal lock" như Pitch/Roll
   truyền thống khi góc tiến gần ±90°.
============================================================================ */
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f; // Quaternion trạng thái hiện tại
#define MADGWICK_BETA   0.1f  // Hệ số lọc: càng lớn càng tin Accel, càng nhỏ càng tin Gyro

static void madgwick_update(float gx, float gy, float gz,
                             float ax, float ay, float az, float dt)
{
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;
    float _2q0, _2q1, _2q2, _2q3, _4q0, _4q1, _4q2, _8q1, _8q2, q0q0, q1q1, q2q2, q3q3;

    // Tốc độ biến thiên quaternion theo gyro (tích phân thô ban đầu)
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

    // Chỉ hiệu chỉnh bằng Accel nếu dữ liệu accel hợp lệ (tránh chia cho 0)
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
        ax *= recipNorm; ay *= recipNorm; az *= recipNorm;

        _2q0 = 2.0f * q0; _2q1 = 2.0f * q1; _2q2 = 2.0f * q2; _2q3 = 2.0f * q3;
        _4q0 = 4.0f * q0; _4q1 = 4.0f * q1; _4q2 = 4.0f * q2;
        _8q1 = 8.0f * q1; _8q2 = 8.0f * q2;
        q0q0 = q0 * q0; q1q1 = q1 * q1; q2q2 = q2 * q2; q3q3 = q3 * q3;

        // Gradient descent - tính độ lệch giữa hướng Accel đo được và hướng
        // Quaternion hiện tại dự đoán, dùng để "kéo" Quaternion về đúng dần
        s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
        s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
        s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
        s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;

        recipNorm = 1.0f / sqrtf(s0*s0 + s1*s1 + s2*s2 + s3*s3);
        s0 *= recipNorm; s1 *= recipNorm; s2 *= recipNorm; s3 *= recipNorm;

        // Trừ phần hiệu chỉnh (có trọng số beta) vào tốc độ biến thiên từ Gyro
        qDot1 -= MADGWICK_BETA * s0;
        qDot2 -= MADGWICK_BETA * s1;
        qDot3 -= MADGWICK_BETA * s2;
        qDot4 -= MADGWICK_BETA * s3;
    }

    // Tích phân để cập nhật Quaternion theo thời gian
    q0 += qDot1 * dt;
    q1 += qDot2 * dt;
    q2 += qDot3 * dt;
    q3 += qDot4 * dt;

    // Chuẩn hóa lại Quaternion (bắt buộc, tránh sai số tích lũy làm mất chuẩn)
    recipNorm = 1.0f / sqrtf(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    q0 *= recipNorm; q1 *= recipNorm; q2 *= recipNorm; q3 *= recipNorm;
}

// Lấy vector trọng lực (đã chuẩn hóa, độ lớn 1g) trong hệ quy chiếu của board,
// tính từ Quaternion hiện tại - chính xác hơn nhiều so với công thức
// lượng giác Pitch/Roll thông thường, xử lý đúng ở MỌI góc nghiêng.
static void get_gravity_from_quaternion(float *gx, float *gy, float *gz)
{
    *gx = 2.0f * (q1 * q3 - q0 * q2);
    *gy = 2.0f * (q0 * q1 + q2 * q3);
    *gz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;
}

// Chuyển Quaternion sang góc Euler (Pitch/Roll/Yaw) quen thuộc, chỉ dùng để
// HIỂN THỊ cho dễ hiểu/so sánh - việc tính toán bên trong (bù trọng lực, ZUPT)
// vẫn dùng trực tiếp Quaternion để tránh lỗi "gimbal lock" ở góc gần ±90°.
static void get_euler_from_quaternion(float *pitch, float *roll, float *yaw)
{
    // Roll (xoay quanh trục X)
    float sinr_cosp = 2.0f * (q0 * q1 + q2 * q3);
    float cosr_cosp = 1.0f - 2.0f * (q1 * q1 + q2 * q2);
    *roll = atan2f(sinr_cosp, cosr_cosp) * 180.0f / M_PI;

    // Pitch (xoay quanh trục Y)
    float sinp = 2.0f * (q0 * q2 - q3 * q1);
    if (fabsf(sinp) >= 1.0f) {
        *pitch = copysignf(90.0f, sinp); // Trường hợp góc tiến sát ±90°
    } else {
        *pitch = asinf(sinp) * 180.0f / M_PI;
    }

    // Yaw (xoay quanh trục Z) - CHỈ Madgwick Filter mới tính được góc này,
    // vì Accelerometer đơn thuần "mù" hoàn toàn với hướng xoay quanh trục
    // thẳng đứng (trọng lực không đổi dù xoay Yaw bao nhiêu độ).
    float siny_cosp = 2.0f * (q0 * q3 + q1 * q2);
    float cosy_cosp = 1.0f - 2.0f * (q2 * q2 + q3 * q3);
    *yaw = atan2f(siny_cosp, cosy_cosp) * 180.0f / M_PI;
}

/* ============================================================================
   TÁI HIỆU CHUẨN THÍCH ỨNG (BẬC 4) - Adaptive Bias Re-calibration
   ============================================================================
   Thay vì chỉ hiệu chuẩn 1 lần lúc khởi động, liên tục theo dõi: nếu phát
   hiện board ĐANG ĐỨNG YÊN (dựa vào độ lớn Gyro gần 0 trong nhiều mẫu liên
   tiếp), tự động cập nhật lại bias ngay lúc đó bằng trung bình trượt
   (exponential moving average) - bù cho việc bias có thể trôi dần theo thời
   gian sử dụng thực tế (do nhiệt độ thay đổi, rung động cơ học tích lũy).
============================================================================ */
#define STATIONARY_GYRO_THRESHOLD  0.02f  // rad/s - dưới ngưỡng này coi là đứng yên
#define STATIONARY_CONFIRM_COUNT   10     // Số mẫu liên tiếp đứng yên mới tin
#define BIAS_UPDATE_RATE           0.01f  // Tốc độ cập nhật bias mới (càng nhỏ càng chậm nhưng ổn định)

static float bias_ax = 0, bias_ay = 0, bias_az = 0;
static int stationary_counter = 0;

static bool is_stationary(float gx, float gy, float gz)
{
    float gyro_mag = sqrtf(gx * gx + gy * gy + gz * gz);
    return gyro_mag < STATIONARY_GYRO_THRESHOLD;
}

static void adaptive_recalibrate(float ax, float ay, float az,
                                  float gravity_x, float gravity_y, float gravity_z)
{
    // Nếu board đang đứng yên, phần dư giữa Accel đo được và trọng lực dự
    // đoán (từ Quaternion) chính là bias thực - cập nhật dần bằng EMA
    float residual_x = ax - gravity_x;
    float residual_y = ay - gravity_y;
    float residual_z = az - gravity_z;

    bias_ax = (1.0f - BIAS_UPDATE_RATE) * bias_ax + BIAS_UPDATE_RATE * residual_x;
    bias_ay = (1.0f - BIAS_UPDATE_RATE) * bias_ay + BIAS_UPDATE_RATE * residual_y;
    bias_az = (1.0f - BIAS_UPDATE_RATE) * bias_az + BIAS_UPDATE_RATE * residual_z;
}

/* ================== TÍNH VẬN TỐC (KẾT HỢP TẤT CẢ CÁC BẬC TRÊN) ================== */
#define NOISE_THRESHOLD  0.015f
static float vx = 0, vy = 0, vz = 0;

static void calc_velocity(float ax, float ay, float az,
                           float gravity_x, float gravity_y, float gravity_z,
                           float dt, bool stationary,
                           float *out_vx, float *out_vy, float *out_vz)
{
    // Trừ bias thích ứng (đã được cập nhật liên tục) + trọng lực chính xác
    // từ Quaternion (không phải công thức Pitch/Roll gần đúng như bản cũ)
    float ax_clean = ax - bias_ax - gravity_x;
    float ay_clean = ay - bias_ay - gravity_y;
    float az_clean = az - bias_az - gravity_z;

    // Vùng chết - triệt nhiễu nhỏ còn sót
    if (fabsf(ax_clean) < NOISE_THRESHOLD) ax_clean = 0;
    if (fabsf(ay_clean) < NOISE_THRESHOLD) ay_clean = 0;
    if (fabsf(az_clean) < NOISE_THRESHOLD) az_clean = 0;

    vx += ax_clean * 9.81f * dt;
    vy += ay_clean * 9.81f * dt;
    vz += az_clean * 9.81f * dt;

    // ZUPT - nếu xác nhận đứng yên (từ Gyro) -> ép cứng vận tốc về 0
    if (stationary) {
        vx = 0; vy = 0; vz = 0;
    }

    *out_vx = vx; *out_vy = vy; *out_vz = vz;
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

    ESP_LOGI(TAG, "GIU YEN BOARD 2 GIAY DE ON DINH BO LOC BAN DAU...");
    vTaskDelay(pdMS_TO_TICKS(2000));

    float ax, ay, az, gx, gy, gz;
    float gravity_x, gravity_y, gravity_z;
    float pitch, roll, yaw;
    float v_x, v_y, v_z;
    float temperature;
    const float dt = 0.02f; // 20ms = 50Hz - tần số khuyến nghị cho Madgwick Filter

    while (1) {
        bmi160_read_data(&ax, &ay, &az, &gx, &gy, &gz);

        // BẬC 2: Cập nhật hướng bằng Madgwick Filter (Quaternion)
        madgwick_update(gx, gy, gz, ax, ay, az, dt);
        get_gravity_from_quaternion(&gravity_x, &gravity_y, &gravity_z);
        get_euler_from_quaternion(&pitch, &roll, &yaw);

        // BẬC 4: Kiểm tra đứng yên + tái hiệu chuẩn thích ứng
        bool stationary = is_stationary(gx, gy, gz);
        if (stationary) {
            stationary_counter++;
            if (stationary_counter >= STATIONARY_CONFIRM_COUNT) {
                adaptive_recalibrate(ax, ay, az, gravity_x, gravity_y, gravity_z);
            }
        } else {
            stationary_counter = 0;
        }

        // Tính vận tốc với đầy đủ các bậc cải tiến
        calc_velocity(ax, ay, az, gravity_x, gravity_y, gravity_z, dt,
                      (stationary_counter >= STATIONARY_CONFIRM_COUNT),
                      &v_x, &v_y, &v_z);

        // In log mỗi 5 lần lặp (giảm tần suất in, vì dt giờ chỉ 20ms - in mỗi lần sẽ quá dày)
        static int print_counter = 0;
        if (++print_counter >= 5) {
            print_counter = 0;
            temperature = bmi160_read_temperature();

            printf("===== BMI160 ADVANCED SENSOR DATA =====\r\n");
            printf("Accel (g)     : X=%.3f  Y=%.3f  Z=%.3f\r\n", ax, ay, az);
            printf("Gravity (Quat): X=%.3f  Y=%.3f  Z=%.3f\r\n", gravity_x, gravity_y, gravity_z);
            printf("Goc nghieng   : Pitch=%.2f  Roll=%.2f  Yaw=%.2f\r\n", pitch, roll, yaw);
            printf("Van toc (m/s) : X=%.3f  Y=%.3f  Z=%.3f\r\n", v_x, v_y, v_z);
            printf("Bias (adapt)  : X=%.4f  Y=%.4f  Z=%.4f\r\n", bias_ax, bias_ay, bias_az);
            printf("Nhiet do      : %.1f C   |   Dung yen: %s\r\n",
                   temperature, (stationary_counter >= STATIONARY_CONFIRM_COUNT) ? "CO" : "KHONG");
            printf("========================================\r\n\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS((int)(dt * 1000)));
    }
}