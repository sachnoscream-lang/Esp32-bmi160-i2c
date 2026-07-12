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
    bmi160_write_reg(REG_CMD, 0x11); vTaskDelay(pdMS_TO_TICKS(100));
    bmi160_write_reg(REG_CMD, 0x15); vTaskDelay(pdMS_TO_TICKS(100));
    bmi160_write_reg(REG_ACC_RANGE, 0x03);
    bmi160_write_reg(REG_GYR_RANGE, 0x00);
    ESP_LOGI(TAG, "Khoi tao BMI160 thanh cong!");
    return true;
}

/* ================== ĐỌC VÀ QUY ĐỔI DỮ LIỆU (GIỮ NGUYÊN NHƯ BẢN GỐC) ================== */
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
   BỘ LỌC 1: LOW-PASS FILTER (LPF) - Tách phần tín hiệu "MƯỢT, TẦN SỐ THẤP"
   ============================================================================
   Dùng để lấy ra xu hướng gia tốc thay đổi TỪ TỪ (đặc trưng của tăng/giảm
   ga) - loại bỏ các đỉnh nhọn tức thời (sốc, va chạm) ra khỏi tín hiệu này.
============================================================================ */
#define LPF_ALPHA  0.15f  // Càng nhỏ càng mượt (chỉ giữ thay đổi chậm), càng lớn càng nhạy

static float lpf_forward_accel = 0;

static float apply_lpf(float raw_value)
{
    lpf_forward_accel = LPF_ALPHA * raw_value + (1.0f - LPF_ALPHA) * lpf_forward_accel;
    return lpf_forward_accel;
}

/* ============================================================================
   BỘ LỌC 2: PHÁT HIỆN CÚ SỐC (SHOCK DETECTION) - Tín hiệu "NHỌN, TẦN SỐ CAO"
   ============================================================================
   Độ lớn gia tốc tổng hợp (magnitude) khi ở trạng thái bình thường luôn xấp
   xỉ 1g (do trọng lực). Khi có va chạm/sốc đột ngột, magnitude tăng vọt bất
   thường trong tích tắc - đây là tín hiệu chung ban đầu của CẢ té ngã LẪN
   sóc ổ gà, cần phân biệt tiếp ở bước sau dựa vào TIME (thời gian kéo dài).
============================================================================ */
#define SHOCK_MAGNITUDE_THRESHOLD  1.8f   // Vượt ngưỡng này (g) coi là có cú sốc
#define FALL_ANGLE_THRESHOLD       60.0f  // Góc nghiêng vượt mức này nghi ngờ té ngã
#define FALL_CONFIRM_TIME_MS       1500   // Giữ góc lớn > 1.5s mới CHẮC CHẮN là té ngã
#define BUMP_RECOVER_TIME_MS       500    // Góc trở lại bình thường trong 0.5s -> chỉ là sóc

typedef enum {
    SU_KIEN_BINH_THUONG,
    SU_KIEN_SOC_O_GA,
    SU_KIEN_TE_NGA
} SuKienVaCham;

typedef enum {
    TOC_DO_DEU_GA,
    TOC_DO_TANG_TOC,
    TOC_DO_GIAM_TOC
} TrangThaiTocDo;

static bool dang_theo_doi_soc = false;
static int64_t thoi_diem_bat_dau_soc = 0;

// Phân loại sự kiện va chạm: BINH_THUONG / SOC_O_GA / TE_NGA
static SuKienVaCham detect_impact_event(float ax, float ay, float az, float roll, float pitch)
{
    float magnitude = sqrtf(ax * ax + ay * ay + az * az);
    int64_t now_ms = esp_timer_get_time() / 1000;
    bool goc_nghieng_lon = (fabsf(roll) > FALL_ANGLE_THRESHOLD) || (fabsf(pitch) > FALL_ANGLE_THRESHOLD);

    if (!dang_theo_doi_soc) {
        // Chưa có sốc nào đang theo dõi - kiểm tra xem có cú sốc mới xảy ra không
        if (magnitude > SHOCK_MAGNITUDE_THRESHOLD) {
            dang_theo_doi_soc = true;
            thoi_diem_bat_dau_soc = now_ms;
            ESP_LOGW(TAG, ">>> Phat hien cu soc (magnitude=%.2fg) - dang theo doi...", magnitude);
        }
        return SU_KIEN_BINH_THUONG;
    } else {
        // Đang theo dõi 1 cú sốc - xem diễn biến TIẾP THEO để phân loại
        int64_t elapsed = now_ms - thoi_diem_bat_dau_soc;

        if (goc_nghieng_lon && elapsed > FALL_CONFIRM_TIME_MS) {
            // Góc nghiêng lớn VÀ giữ nguyên đủ lâu -> CHẮC CHẮN té ngã
            ESP_LOGE(TAG, "!!! XAC NHAN TE NGA !!! Roll=%.1f Pitch=%.1f", roll, pitch);
            dang_theo_doi_soc = false; // Reset để theo dõi sự kiện tiếp theo
            return SU_KIEN_TE_NGA;
        }

        if (!goc_nghieng_lon && elapsed > BUMP_RECOVER_TIME_MS) {
            // Góc đã trở lại bình thường trong thời gian ngắn -> chỉ là sóc ổ gà
            ESP_LOGI(TAG, ">>> Xac nhan CHI LA SOC O GA (da tro lai binh thuong)");
            dang_theo_doi_soc = false;
            return SU_KIEN_SOC_O_GA;
        }

        // Vẫn đang trong giai đoạn theo dõi, chưa đủ dữ kiện kết luận
        return SU_KIEN_BINH_THUONG;
    }
}

// Phân loại trạng thái tốc độ: DEU_GA / TANG_TOC / GIAM_TOC
// Dùng tín hiệu ĐÃ QUA LPF (mượt) để loại bỏ ảnh hưởng của sốc/rung xóc ngắn
#define ACCEL_THRESHOLD  0.05f

static TrangThaiTocDo detect_motion_state(float accel_forward_filtered)
{
    if (accel_forward_filtered > ACCEL_THRESHOLD) {
        return TOC_DO_TANG_TOC;
    } else if (accel_forward_filtered < -ACCEL_THRESHOLD) {
        return TOC_DO_GIAM_TOC;
    } else {
        return TOC_DO_DEU_GA;
    }
}

static const char* impact_to_string(SuKienVaCham e)
{
    switch (e) {
        case SU_KIEN_TE_NGA:   return "TE NGA";
        case SU_KIEN_SOC_O_GA: return "SOC O GA";
        default:                return "Binh thuong";
    }
}

static const char* speed_to_string(TrangThaiTocDo s)
{
    switch (s) {
        case TOC_DO_TANG_TOC: return "TANG TOC";
        case TOC_DO_GIAM_TOC: return "GIAM TOC";
        default:               return "DEU GA";
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

    // LƯU Ý: Chọn đúng trục theo hướng xe di chuyển tới/lui (tùy cách gắn cảm biến)
    // Mặc định dùng trục X - đổi thành ay/az nếu thực tế lắp đặt khác.

    while (1) {
        bmi160_read_data(&ax, &ay, &az, &gx, &gy, &gz);
        calc_angle(ax, ay, az, &pitch, &roll);

        float accel_forward = ax; // ĐỔI TRỤC NẾU CẦN
        float accel_forward_filtered = apply_lpf(accel_forward); // Qua LPF - mượt hóa

        SuKienVaCham su_kien = detect_impact_event(ax, ay, az, roll, pitch);
        TrangThaiTocDo trang_thai_toc = detect_motion_state(accel_forward_filtered);

        // GIỮ NGUYÊN FORMAT LOG GỐC + THÊM 2 DÒNG PHÂN LOẠI MỚI
        printf("===== BMI160 SENSOR DATA =====\r\n");
        printf("Accel (g)     : X=%.3f  Y=%.3f  Z=%.3f\r\n", ax, ay, az);
        printf("Gyro (dps)    : X=%.3f  Y=%.3f  Z=%.3f\r\n", gx, gy, gz);
        printf("Goc nghieng   : Pitch=%.2f  Roll=%.2f\r\n", pitch, roll);
        printf("Trang thai toc: %s (loc LPF=%.3fg)\r\n", speed_to_string(trang_thai_toc), accel_forward_filtered);
        printf("Su kien va cham: %s\r\n", impact_to_string(su_kien));
        printf("===============================\r\n\r\n");

        vTaskDelay(pdMS_TO_TICKS(50)); // Đọc nhanh (50ms=20Hz) để bắt kịp cú sốc tức thời
    }
}