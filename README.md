# Esp32-bmi160-i2c

5/7/2026
------------------------------------------------------------------------------
Chân trên BMI160    ||    Nối vào chân trên board 
VCC                 ||    3V3 (chân số 38, cột bên phải)
GND                 ||    GND (chân số 1, cột bên trái)
SCL                 ||    P22 (GPIO22, chân số 3, cột bên trái)
SDA                 ||    P21 (GPIO21, chân số 6, cột bên trái)
-----------------------------------------------------------------------------



-Phần 1: Include thư viện
+Thư viện chuẩn C             
+Thư viện toán học             
+Thư viện lõi FreeRTOS và task      
+Thư viện driver I2C mới của ESP-IDF
+Thư viện log chuẩn ESP-IDF

-Phần 2: Định nghĩa hằng số
+Đặt tên cho 2 chân GPIO dùng làm đường truyền dữ liệu I2C — SDA và SCL 
+Tốc độ truyền I2C 400.000 Hz
+Chân SDO nối lên VCC    → Địa chỉ I2C = 0x69  [x]
+Địa chỉ các thanh ghi bên trong chip BMI160 — mỗi thanh ghi giữ 1 chức năng riêng (nhận diện chip, cấu hình dải đo, lệnh bật/tắt nguồn...). Các con số này lấy từ datasheet của BMI160 

-Phần 3: Biến toàn cục
+Gắn "nhãn" (TAG) cho BMI160 để xem log dễ hơn (phòng trường hợp nhiều thiết bị)
+Biến handle — đại diện cho kết nối tới thiết bị BMI160 trên bus I2C và khởi tạo xong, mọi lệnh đọc/ghi đều thông qua biến này.

-Phần 4: Hàm ghi 1 thanh ghi

static esp_err_t bmi160_write_reg(uint8_t reg, uint8_t data)
** esp_err_t là kiểu dữ liệu riêng do ESP-IDF định nghĩa, bản chất giống int 
** bmi160_write_reg là tên hàm tự đặt để write giá trị vô thanh ghi 
** uint8_t reg là không dấu và 8-bit, giá trị có thể lưu trong biến kiểu này nằm trong khoảng từ 0 đến 255,    reg là tên biến tham số, do mình tự đặt. Biến này sẽ lưu địa chỉ của thanh ghi mà hàm cần ghi dữ liệu vào (vd 0x7E).
** uint8_t data là uint8_t — cùng kiểu dữ liệu số nguyên không dấu 8-bit, giá trị từ 0-255.
data — tên biến, lưu giá trị thực sự muốn ghi vào thanh ghi (ví dụ 0x11).
Lưu ý: Mỗi lần gọi, reg và data sẽ thay đổi — nhận giá trị khác nhau tùy theo bạn muốn ghi vào thanh ghi nào, giá trị gì. Nếu không có biến (reg, data) mà viết cứng số vào bên trong hàm, bạn sẽ phải viết 4 hàm riêng biệt cho 4 trường hợp (bật accel gyro và chọn dải accel gyro) — code sẽ dài dòng và khó bảo trì hơn rất nhiều.

uint8_t buf[2] = {reg, data};
** uint8_t để khai báo 1 mảng 
** buf[2] viết tắt buffer (tự đặt) và kích thước mảng 2 phần tử mỗi phần tử chứa 1 giá trị kiểu uint8_t
** {reg, data} gán cho phần tử đầu tiên buf[0] và phần tử thứ hai buf[1]

 return i2c_master_transmit(bmi160_handle, buf, 2, 1000);
** hàm bmi160_write_reg được khai báo có kiểu trả về là esp_err_t. Vậy nên sau từ return, bắt buộc phải có 1 giá trị kiểu esp_err_t đi kèm — đó chính là kết quả của hàm i2c_master_transmit(...) ngay phía sau.
** i2c_master_transmit(...) là hàm có sẵn trong thư viện i2c master, hàm này gồm 4 tham số :
bmi160_handle       //  là 1 biến đại diện cho xác định đang giao tiếp với cảm biến nào
buf                 // mảng 2 byte mới tạo phía trên
2                   // độ dài mảng buf
1000                // chờ 1s

==> Trả kết quả của việc gọi hàm có sẵn i2c_master_transmit, hàm này sẽ gửi dữ liệu qua I2C tới đúng thiết bị BMI160 (xác định qua bmi160_handle), nội dung gửi đi là mảng buf (chứa 2 byte: địa chỉ thanh ghi + giá trị ghi), độ dài dữ liệu gửi là 2 byte, và chờ tối đa 1000 mili-giây trước khi báo lỗi nếu không có phản hồi.

- Phần 5 hàm đọc dữ liệu từ thanh ghi
static esp_err_t bmi160_read_regs(uint8_t reg, uint8_t *data, size_t len)
**  hàm này dùng để đọc giá trị từ cảm biến ra — ví dụ đọc CHIP_ID, đọc dữ liệu accel/gyro.
  uint8_t *data là nơi dùng để lưu kết quả đọc đc từ cảm biến vì hàm cần đọc ra NHIỀU byte cùng lúc (ví dụ 12 byte khi đọc accel+gyro), mà 1 hàm trong C chỉ có thể "trả về" 1 giá trị duy nhất qua return. Nên thay vào đó, người ta đưa sẵn địa chỉ của data vào, hàm sẽ tự động đổ dữ liệu vào khay đó, thay vì trả về qua return.

**size_t len là 1 kiểu số nguyên khác, dùng phổ biến khi biểu diễn "kích thước" hoặc "số lượng" 
len — tên biến, viết tắt "length" (độ dài) — cho biết muốn đọc bao nhiêu byte dữ liệu.

return i2c_master_transmit_receive(bmi160_handle, &reg, 1, data, len, 1000);
**Hàm i2c_master_transmit_receive — nghĩa là vừa gửi (transmit) vừa nhận (receive) — vì cơ chế đọc dữ liệu qua I2C luôn cần 2 bước:

Bước gửi (transmit): ESP32 phải gửi trước muốn đọc thanh ghi số mấy — đây là lý do có tham số &reg(Địa chỉ của biến reg — gửi đi để báo đọc thanh ghi ) và 1 (gửi 1 byte, , vì địa chỉ thanh ghi chỉ có 1 byte).
Bước nhận (receive): Ngay sau đó, BMI160 sẽ gửi ngược lại dữ liệu — ESP32 nhận về, đổ vào địa chỉ data, với số lượng byte là len.

Vì sao tham số reg lại có dấu & ở đây, còn data thì không?

reg khi khai báo trong hàm là kiểu số bình thường (uint8_t reg) — không phải "địa chỉ" sẵn có, nên khi cần đưa nó cho hàm khác dùng theo kiểu con trỏ, phải thêm & phía trước (nghĩa nôm na: "lấy vị trí của biến reg này ra dùng tạm").
data thì ngay từ đầu đã được khai báo là con trỏ rồi (uint8_t *data) — nó vốn dĩ đã là "địa chỉ", nên đưa thẳng vào, không cần thêm & nữa.
Vậy tại sao BÊN TRONG hàm lại cần &reg?
Vì hàm i2c_master_transmit_receive() (hàm có sẵn của thư viện) yêu cầu bắt buộc tham số thứ 2 phải là con trỏ (địa chỉ của 1 vùng nhớ chứa dữ liệu cần gửi) — đây là do người viết thư viện ESP-IDF quy định sẵn, không thể đổi được.
Nhưng trong hàm bmi160_read_regs, biến reg (tham số truyền vào) lại là giá trị thường, không phải con trỏ. Vậy để "biến giá trị thường thành con trỏ tạm thời" ngay tại chỗ, chỉ cần thêm & trước tên biến đó — đây gọi là "lấy địa chỉ của biến", hoàn toàn hợp lệ dù reg chỉ là tham số hàm bình thường (biến reg vẫn tồn tại thật trong bộ nhớ, dù chỉ tồn tại tạm thời trong lúc hàm đang chạy, nên lấy địa chỉ của nó là hoàn toàn hợp lệ).

-Phần 6 Khởi tạo I2C

Hàm này làm 2 việc lớn, theo đúng 2 bước bắt buộc của driver I2C mới trong ESP-IDF:
+Tạo ra "con đường chung" (bus) — cấu hình 2 chân GPIO nào sẽ dùng làm SDA/SCL, tốc độ ra sao.
+Đăng ký 1 thiết bị cụ thể (BMI160) lên con đường đó — báo địa chỉ I2C của BMI160 là gì.

static void i2c_master_setup(i2c_master_bus_handle_t *bus_handle)
** i2c_master_bus_handle_t *bus_handle là tham số đầu vào, cũng là 1 loại "handle" giống bmi160_handle, nhưng đây là handle đại diện cho cả bus I2C (không phải riêng 1 thiết bị) — có dấu * phía trước vì hàm này cần ghi giá trị mới vào biến bus_handle ở bên ngoài (giống cách giải thích dấu &/* ở phần trước — cần "địa chỉ" để ghi ngược kết quả ra ngoài).

i2c_master_bus_config_t bus_config = {
** tạo cấu hình cho bus 
tạo 1 struct bus config bao gồm các thông số :
 .clk_source = I2C_CLK_SRC_DEFAULT,     //Chọn nguồn tạo xung nhịp (clock) — dùng nguồn mặc định ESP32
 .i2c_port = I2C_NUM_0,  //ESP32 có 2 bộ điều khiển I2C (đánh số 0 và 1). Dòng này chọn dùng bộ điều khiển số 0.
 .scl_io_num = I2C_SCL_PIN,   //set chân SCL (đã chọn chân 22)
 .sda_io_num = I2C_SDA_PIN,    // set chấn SDA ( đã chọn chân 21)
 .glitch_ignore_cnt = 7,   // Bộ lọc chống nhiễu — bỏ qua các tín hiệu nhiễu cực ngắn trên đường dây (do nhiễu điện, dây dài...). Giá trị 7 là con số khuyến nghị phổ biến trong tài liệu ESP-IDf
 .flags.enable_internal_pullup = true, //Bật điện trở kéo lên có sẵn bên trong chip ESP32, gắn cho 2 chân SDA/SCL

ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
**ESP_ERROR_CHECK(...) — đây là 1 hàm có sẵn của ESP-IDF: nếu hàm bên trong chạy lỗi, nó sẽ tự động in ra thông báo lỗi và dừng cả chương trình ngay lập tức — không cần tự viết code kiểm tra if (lỗi) {...} thủ công ở mọi chỗ, đỡ code dài dòng.
**i2c_new_master_bus(...) — hàm có sẵn của thư viện, nhận vào 6 thông số (&bus_config) vừa điền, tạo ra 1 bus I2C thật sự hoạt động trên phần cứng, rồi ghi kết quả (handle của bus) vào biến bus_handle.

i2c_device_config_t dev_config = {   // Tạo cấu hình cho Thiết bị (BMI160)
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,   //Khai báo địa chỉ I2C dùng chuẩn 7-bit 
        .device_address = BMI160_ADDR,  //Gán địa chỉ thiết bị — chính là 0x69 đã xác định được qua bước quét (scanner) trước đó.
        .scl_speed_hz = I2C_FREQ_HZ,  //Tốc độ truyền dữ liệu cho thiết bị BMI160 này (400kHz), lấy từ hằng số đã định nghĩa đầu file.
    };

ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, &bmi160_handle));
** i2c_master_bus_add_device(...) — hàm có sẵn, nhận vào: bus đã tạo (*bus_handle), cấu hình thiết bị (&dev_config), rồi tạo ra 1 handle đại diện cho riêng thiết bị BMI160, ghi kết quả vào biến toàn cục bmi160_handle (đã khai báo ở Phần 3).
**Sau dòng này, biến bmi160_handle chính thức có giá trị sử dụng được — mọi hàm đọc/ghi sau này (bmi160_write_reg, bmi160_read_regs) mới hoạt động đúng.
Lưu ý về *bus_handle: Vì tham số đầu vào của hàm i2c_master_setup là i2c_master_bus_handle_t *bus_handle (1 con trỏ trỏ tới handle), nên khi cần lấy giá trị thật của bus (không phải địa chỉ của nó) để đưa vào hàm i2c_master_bus_add_device, cần thêm dấu * phía trước để "mở ra lấy giá trị bên trong" — ngược lại hoàn toàn với việc thêm & (lấy địa chỉ).
=-=> Hàm này làm 2 bước tuần tự: (1) Dựng lên con đường I2C chung (chọn chân SDA/SCL, tốc độ, bộ điều khiển số 0), (2) Đăng ký cảm biến BMI160 vào con đường đó (khai báo địa chỉ 0x69, tốc độ riêng), kết quả cuối cùng là biến bmi160_handle sẵn sàng để dùng cho việc đọc/ghi dữ liệu ở các hàm khác.

Phần 7 — hàm khởi tạo cảm biến BMI160:
static bool bmi160_init(void)
** bmi160_init — tên hàm, nghĩa là "khởi tạo BMI160".

uint8_t chip_id = 0;
**Tạo 1 biến số nguyên 8-bit tên chip_id, gán giá trị ban đầu là 0 (giá trị tạm, sẽ bị ghi đè ngay sau đó).

bmi160_read_regs(REG_CHIP_ID, &chip_id, 1);
**Gọi hàm đã viết ở Phần 5 — đọc 1 byte từ thanh ghi REG_CHIP_ID (địa chỉ 0x00), kết quả đọc được sẽ tự động ghi vào biến chip_id (nhờ dùng &chip_id cho hàm đổ kết quả vào).

ESP_LOGI(TAG, "CHIP_ID doc duoc: 0x%02X (dung phai la 0xD1)", chip_id);
** ESP_LOGI — in log cấp độ "Info" (thông tin thường)
"CHIP_ID doc duoc: 0x%02X..." — chuỗi ký tự cần in, trong đó có 1 "chỗ trống" đặc biệt là %02X.
%02X — đây là định dạng in số (format specifier):

X — in số theo hệ thập lục phân (hex), chữ in HOA (ví dụ D1 thay vì d1).
02 — luôn in đủ 2 chữ số, nếu số đó chỉ có 1 chữ số thì tự thêm số 0 phía trước (ví dụ số 5 sẽ in ra 05).
chip_id — giá trị thực sự sẽ được "lắp vào" chỗ trống %02X đó khi in ra.
Kết quả in ra ví dụ: CHIP_ID doc duoc: 0xD1 (dung phai la 0xD1).

if (chip_id != 0xD1) {
        ESP_LOGE(TAG, "Khong tim thay BMI160! Kiem tra lai day noi va dia chi I2C.");
        return false;
    }
** Kiểm tra điều kiện — có đúng là BMI160 không?

bmi160_write_reg(REG_CMD, 0x11);
vTaskDelay(pdMS_TO_TICKS(100));
**Bật nguồn cho Accelerometer
Gọi hàm ghi (Phần 4) — ghi giá trị 0x11 vào thanh ghi REG_CMD (0x7E) — theo datasheet của BMI160, đây là mã lệnh để chuyển Accelerometer từ chế độ sleep sang chế độ "Normal" (hoạt động bình thường, sẵn sàng đo).

bmi160_write_reg(REG_CMD, 0x15);
vTaskDelay(pdMS_TO_TICKS(100));
** Bật nguồn cho Gyroscope tương tự như Accel 

bmi160_write_reg(REG_ACC_RANGE, 0x03);
bmi160_write_reg(REG_GYR_RANGE, 0x00);
**Chọn dải đo (range)
Ghi 0x03 vào thanh ghi REG_ACC_RANGE (0x41) → theo datasheet, mã này chọn dải đo gia tốc là ±2g (nghĩa là cảm biến chỉ đo chính xác trong khoảng từ -2g đến +2g).
Ghi 0x00 vào REG_GYR_RANGE (0x43) → chọn dải đo tốc độ góc là ±2000 độ/giây.

ESP_LOGI(TAG, "Khoi tao BMI160 thanh cong!");
    return true;
}
** Báo thành công

Đọc CHIP_ID
     │
     ▼
CHIP_ID có đúng 0xD1 không?
     │
   ┌─┴─┐
  Sai   Đúng
   │     │
In lỗi   Bật Accel (0x11) → chờ 100ms
return   Bật Gyro (0x15)  → chờ 100ms
false    Chọn range Accel (±2g)
         Chọn range Gyro (±2000dps)
         In "thành công"
         return true

Phần 8 ĐỌC VÀ TÍNH TOÁN DỮ LIỆU CẢM BIẾN
static void bmi160_read_data(float *ax, float *ay, float *az,
                              float *gx, float *gy, float *gz)
** khai báo 6 con trỏ 3 trục accel 3 trục gyro ( hàm sẽ trả 6 kết quả)

uint8_t raw[12];
**mảng 12 ô, chứa dữ liệu thô đọc thẳng từ cảm biến (chưa qua xử lý).

bmi160_read_regs(REG_GYR_DATA, raw, 12);
**đọc liền 1 lúc 12 byte, bắt đầu từ thanh ghi 0x0C (theo datasheet, 12 thanh ghi liền kề chứa đủ gyro XYZ + accel XYZ, mỗi trục 2 byte).

int16_t gyro_x_raw  = (int16_t)((raw[1]  << 8) | raw[0]);
int16_t gyro_y_raw  = (int16_t)((raw[3]  << 8) | raw[2]);
int16_t gyro_z_raw  = (int16_t)((raw[5]  << 8) | raw[4]);
int16_t accel_x_raw = (int16_t)((raw[7]  << 8) | raw[6]);
int16_t accel_y_raw = (int16_t)((raw[9]  << 8) | raw[8]);
int16_t accel_z_raw = (int16_t)((raw[11] << 8) | raw[10]);
**mỗi trục có 2 byte (Low + High), phải ghép lại thành 1 số 16-bit: dịch byte cao lên 8 bit, rồi cộng OR với byte thấp. Ép kiểu (int16_t) vì giá trị có thể âm.

*ax = accel_x_raw / 16384.0f;
*ay = accel_y_raw / 16384.0f;
*az = accel_z_raw / 16384.0f;
**quy đổi từ số thô sang đơn vị g (gia tốc trọng trường), vì đã chọn dải đo ±2g và cảm biến 16-bit → hệ số 16384.

*gx = gyro_x_raw / 16.4f;
*gy = gyro_y_raw / 16.4f;
*gz = gyro_z_raw / 16.4f;
** quy đổi sang đơn vị độ/giây, vì dải đo ±2000 dps → hệ số 16.4.

Phần 9 Tính góc nghiêng

static void calc_angle(float ax, float ay, float az, float *pitch, float *roll)
{
    *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
    *roll  = atan2f(ay, az) * 180.0f / M_PI;
}
**Nhận vào 3 giá trị accel (ax, ay, az — giá trị thường, chỉ đọc dùng, không cần sửa ngược ra ngoài) và 2  pitch, roll (con trỏ — sẽ ghi kết quả vào).
atan2f(...) — hàm lượng giác có sẵn trong math.h, tính góc dựa trên tỷ lệ giữa các trục accel (khi đứng yên, trọng lực chiếu lên 3 trục X/Y/Z cho biết cảm biến đang nghiêng bao nhiêu).
sqrtf(...) — hàm tính căn bậc 2, cũng có sẵn trong math.h.
Kết quả atan2f trả về đơn vị radian, nhân với 180.0f / M_PI để đổi sang đơn vị độ .
Pitch = góc ngẩng/chúc, Roll = góc lắc trái/phải — 2 góc mô tả độ nghiêng vật lý của cảm biến.

PHẦN 10: Hàm chính

i2c_master_bus_handle_t bus_handle;
** gọi hàm , Dùng &bus_handle vì hàm cần ghi kết quả bus mới tạo vào biến này.

ESP_LOGI(TAG, "Dang khoi tao I2C...");
**In ra 1 dòng log cấp độ "Info" (thông tin thường), nội dung là "Dang khoi tao I2C..." (đang khởi tạo I2C).
TAG — chính là biến "BMI160" đã khai báo ở đầu file (Phần 3), giúp log này hiện ra kèm nhãn BMI160: phía trước.
Mục đích: chỉ để thông báo cho người xem log biết chương trình đang chạy tới bước nào 

i2c_master_setup(&bus_handle);
**Gọi hàm i2c_master_setup đã viết ở Phần 6 — hàm này thực hiện việc dựng bus I2C (chọn chân SDA/SCL, tốc độ...) và đăng ký thiết bị BMI160 vào bus đó.
 nhớ lại hàm được khai báo là:i2c_master_setup(i2c_master_bus_handle_t *bus_handle)
Tham số đầu vào của hàm này là con trỏ (*bus_handle), nghĩa là hàm cần nhận địa chỉ của 1 biến để nó ghi kết quả (bus vừa tạo) ngược vào biến đó.

 if (!bmi160_init()) {
        ESP_LOGE(TAG, "Dung chuong trinh do loi khoi tao cam bien.");
        return;
    }
** gọi khởi tạo cảm biến (Phần 7). Dấu ! nghĩa là đảo ngược giá trị bool — bmi160_init() trả false thì !false = true → vào nhánh lỗi, in log rồi return thoát hẳn khỏi app_main.

float ax, ay, az, gx, gy, gz, pitch, roll;
**Khai báo 8 biến float để chứa kết quả accel/gyro/góc.

 while (1) {
        bmi160_read_data(&ax, &ay, &az, &gx, &gy, &gz);    //Đọc dữ liệu thô, quy đổi 
        calc_angle(ax, ay, az, &pitch, &roll);         //Tính góc nghiêng

printf("===== BMI160 SENSOR DATA =====\r\n");
printf("Accel (g)   : X=%.3f  Y=%.3f  Z=%.3f\r\n", ax, ay, az);
printf("Gyro (dps)  : X=%.3f  Y=%.3f  Z=%.3f\r\n", gx, gy, gz);
printf("Goc nghieng : Pitch=%.2f  Roll=%.2f\r\n", pitch, roll);
printf("===============================\r\n\r\n");

 vTaskDelay(pdMS_TO_TICKS(500)); // Đọc và in mỗi 0.5 giây
    }
}

==>Khởi tạo kết nối I2C → kiểm tra đúng cảm biến → bật nguồn accel/gyro → lặp vô hạn: đọc 12 byte thô → quy đổi ra đơn vị thực (g, độ/giây) → tính góc nghiêng → in ra Serial mỗi 0.5 giây.


=====================================================================================================
6/7/2026
## Cập nhật mới nhất
- Tích hợp Madgwick Filter (Quaternion-based AHRS) để ước lượng hướng chính xác hơn
- Thêm tính năng đo góc Yaw (trước đây chỉ có Pitch/Roll)
- Bù nhiệt độ và tái hiệu chuẩn bias thích ứng (Adaptive Recalibration)
- Cải thiện đáng kể độ ổn định khi tính vận tốc từ tích phân gia tốc

### Các hàm/tính năng mới thêm:
1. bmi160_read_temperature() - đọc nhiệt độ tích hợp sẵn trong BMI160
2. madgwick_update() - thuật toán AHRS kết hợp Gyro+Accel qua Quaternion
3. get_gravity_from_quaternion() - tính trọng lực chính xác ở mọi góc nghiêng
4. get_euler_from_quaternion() - chuyển Quaternion sang Pitch/Roll/Yaw
5. adaptive_recalibrate() - tự động cập nhật bias liên tục (EMA)
6. calc_velocity() - tích phân gia tốc ra vận tốc, có lọc nhiễu + ZUPT

### Thay đổi so với bản 5/7/2026:
- calc_angle() (dùng Accel thô) → thay bằng get_euler_from_quaternion() (dùng Madgwick Filter)
- Thêm góc Yaw (trước đây không đo được)
- Tần số đọc: 500ms → 20ms (50Hz) để phù hợp Madgwick Filter
------------------------------------------------------------
cập nhật code mới của a quy và BMI 

1. Phần đầu file
Dòng 1-5: Comment tiêu đề. Chỉ để ghi tên project và mô tả ngắn.

Dòng 7 #include <Arduino.h>: lấy thư viện Arduino để dùng setup(), loop(), Serial, millis(), analogRead()...

Dòng 8 #include <stdio.h>: dùng printf, snprintf.

Dòng 9 #include <math.h>: dùng sqrtf, atan2f, fabsf.

Dòng 10 #include "config.h": lấy các hằng số cấu hình như POT_PIN, RPM_IDLE, ADC_MAX_RAW...

Dòng 11 #include "sound_registry.h": danh sách âm thanh động cơ.

Dòng 12 #include "audio_engine.h": phần xử lý âm thanh.

Dòng 13 #include "ble_manager.h": phần BLE.

Dòng 15-17: comment + include driver I2C và timer của ESP32.

Dòng 16 #include "driver/i2c.h": dùng I2C kiểu ESP-IDF cũ.

Dòng 17 #include "esp_timer.h": dùng timer microsecond để đo thời gian chính xác.

2. Cấu hình chân I2C và thanh ghi BMI160
Dòng 19: comment, báo đây là phần cấu hình I2C.

Dòng 20 #define I2C_SDA_PIN GPIO_NUM_21: chân SDA là GPIO 21.

Dòng 21 #define I2C_SCL_PIN GPIO_NUM_22: chân SCL là GPIO 22.

Dòng 22 #define I2C_FREQ_HZ 400000: tốc độ I2C là 400kHz.

Dòng 23 #define BMI160_ADDR 0x69: địa chỉ I2C của BMI160.

Dòng 24 #define I2C_PORT_NUM I2C_NUM_0: dùng bus I2C số 0.

Dòng 26: comment nói về các thanh ghi của BMI160.

Dòng 27 REG_CHIP_ID: thanh ghi ID chip.

Dòng 28 REG_GYR_DATA: thanh ghi dữ liệu gyro.

Dòng 29 REG_ACC_RANGE: thanh ghi chọn dải đo accel.

Dòng 30 REG_GYR_RANGE: thanh ghi chọn dải đo gyro.

Dòng 31 REG_CMD: thanh ghi lệnh điều khiển BMI160.

Dòng 33 LPF_ALPHA 0.15f: hệ số lọc thông thấp.

Dòng 34 ACCEL_THRESHOLD 0.05f: ngưỡng phát hiện tăng tốc nhẹ.

Dòng 35 SHOCK_MAGNITUDE_THRESHOLD 1.8f: ngưỡng sốc mạnh.

Dòng 36 FALL_ANGLE_THRESHOLD 60.0f: góc nghiêng để coi là ngã.

Dòng 37 FALL_CONFIRM_TIME_MS 1500: cần nghiêng lâu 1.5s mới xác nhận ngã.

Dòng 38 BUMP_RECOVER_TIME_MS 500: sốc ngắn thì coi là va chạm thường.

3. Enum trạng thái
Dòng 40: comment báo phần enum trạng thái.

Dòng 41-45 EngineState: trạng thái động cơ.

ENG_OFF: tắt.
ENG_STARTING: đang đề máy.
ENG_RUNNING: đang chạy.
Dòng 47-51 SuKienVaCham: kiểu dữ liệu cho sự kiện va chạm.

SU_KIEN_BINH_THUONG: bình thường.
SU_KIEN_SOC_O_GA: sốc/va chạm ở ga.
SU_KIEN_TE_NGA: té ngã.
Dòng 53-57 TrangThaiTocDo: trạng thái tốc độ.

TOC_DO_DEU_GA: đều ga.
TOC_DO_TANG_TOC: tăng tốc.
TOC_DO_GIAM_TOC: giảm tốc/phanh.
4. Biến toàn cục
Dòng 59: comment cho biến toàn cục.

Dòng 60 s_currentRPM: RPM hiện tại.

Dòng 61 s_targetRPM: RPM mục tiêu.

Dòng 62 s_state: trạng thái động cơ hiện tại.

Dòng 64 lpf_forward_accel: giá trị gia tốc đã lọc.

Dòng 65 dang_theo_doi_soc: có đang theo dõi một cú sốc không.

Dòng 66 thoi_diem_bat_dau_soc: thời điểm bắt đầu sốc.

Dòng 68 throttle_bmi: ga lấy từ BMI160.

Dòng 70: comment giải thích fix cho té ngã.

Dòng 71 dang_bi_te_nga: cờ báo xe đang trong trạng thái ngã.

Dòng 72 thoi_diem_te_nga: thời điểm phát hiện ngã.

Dòng 73 FALL_SAFE_HOLD_MS 1000: giữ trạng thái ngã an toàn 1 giây.

Dòng 75: comment cho biến log.

Dòng 76 g_ax, g_ay, g_az: accel X/Y/Z.

Dòng 77 g_gx, g_gy, g_gz: gyro X/Y/Z.

Dòng 78 g_pitch, g_roll: góc nghiêng pitch/roll.

Dòng 79 g_su_kien: sự kiện va chạm hiện tại.

Dòng 80 g_trang_thai_toc: trạng thái tốc độ hiện tại.

5. Hàm tắt máy
Dòng 82-84 requestEngineOff(): hàm này chỉ làm một việc: ép trạng thái động cơ về ENG_OFF.
6. Hàm đổi enum sang chữ
Dòng 86: comment nói đây là hàm đổi mã thành chữ.

Dòng 87 impact_to_string(SuKienVaCham e): nhận một sự kiện va chạm, trả về chuỗi để in log.

Dòng 88 switch (e): rẽ nhánh theo giá trị enum.

Dòng 89: nếu là té ngã thì trả "TE NGA".

Dòng 90: nếu là sốc ở ga thì trả "SOC O GA".

Dòng 91: mặc định trả "Binh thuong".

Dòng 92-93: kết thúc switch/hàm.

Dòng 95 speed_to_string(TrangThaiTocDo s): đổi trạng thái tốc độ thành chữ.

Dòng 96 switch (s): rẽ nhánh theo tốc độ.

Dòng 97: tăng tốc -> "TANG TOC".

Dòng 98: giảm tốc -> "GIAM TOC".

Dòng 99: còn lại -> "DEU GA".

Dòng 100-101: kết thúc.

7. Hàm I2C ghi thanh ghi
Dòng 103: comment nói phần I2C driver cũ.
Dòng 104 bmi160_write_reg(...): ghi 1 byte vào thanh ghi BMI160.
Dòng 105 tạo chuỗi I2C command.
Dòng 106 bắt đầu giao dịch I2C.
Dòng 107 gửi địa chỉ chip + bit ghi.
Dòng 108 gửi địa chỉ thanh ghi.
Dòng 109 gửi dữ liệu cần ghi.
Dòng 110 kết thúc giao dịch.
Dòng 111 comment: timeout chỉ 50ms.
Dòng 112 thực thi lệnh I2C.
Dòng 113 xóa command link.
Dòng 114-115 trả kết quả.
8. Hàm I2C đọc thanh ghi
Dòng 117 bmi160_read_regs(...): đọc nhiều byte từ BMI160.
Dòng 118 nếu len == 0 thì trả OK ngay.
Dòng 119 tạo command I2C.
Dòng 120 start.
Dòng 121 gửi địa chỉ chip với chế độ ghi.
Dòng 122 gửi địa chỉ thanh ghi cần đọc.
Dòng 124 start lại để chuyển sang chế độ đọc.
Dòng 125 gửi địa chỉ chip với bit đọc.
Dòng 126-128 nếu cần đọc hơn 1 byte thì đọc phần đầu trước.
Dòng 129 đọc byte cuối cùng và NACK.
Dòng 130 stop.
Dòng 131 comment timeout 50ms.
Dòng 132 chạy command.
Dòng 133 xóa command.
Dòng 134-135 trả kết quả.
9. Hàm setup I2C
Dòng 137 i2c_master_setup_legacy(): cấu hình bus I2C.
Dòng 138 khai báo biến cấu hình conf.
Dòng 139 đặt chế độ master.
Dòng 140 set chân SDA.
Dòng 141 bật pull-up SDA.
Dòng 142 set chân SCL.
Dòng 143 bật pull-up SCL.
Dòng 144 set tốc độ xung I2C.
Dòng 145 set cờ clock.
Dòng 147 áp cấu hình I2C.
Dòng 148 cài driver I2C.
Dòng 149 kết thúc.
10. Hàm init BMI160
Dòng 151 bmi160_init_legacy(): khởi tạo cảm biến.
Dòng 152 biến lưu chip ID.
Dòng 153 đọc thanh ghi CHIP_ID.
Dòng 154 kiểm tra chip có đúng 0xD1 không.
Dòng 155 nếu sai thì in lỗi.
Dòng 156 trả false nếu sai.
Dòng 158 gửi lệnh bật accel, rồi delay.
Dòng 159 gửi lệnh bật gyro, rồi delay.
Dòng 160 đặt dải đo accel.
Dòng 161 đặt dải đo gyro.
Dòng 162 báo init thành công.
Dòng 163 trả true.
Dòng 164 kết thúc.
11. Hàm đọc dữ liệu BMI160
Dòng 166 comment nói thuật toán chia tỉ lệ.
Dòng 167 hàm đọc data thô từ BMI160.
Dòng 168 mảng raw 12 byte.
Dòng 169 đọc 12 byte từ thanh ghi gyro data.
Dòng 171-176 ghép 2 byte thành int16_t cho gyro và accel.
Dòng 178-180 đổi raw accel sang đơn vị g.
Dòng 181-183 đổi raw gyro sang dps.
Dòng 184 kết thúc.
12. Hàm lọc thông thấp
Dòng 186 apply_lpf(...): lọc nhiễu.
Dòng 187 công thức LPF: giá trị mới = phần mới * alpha + phần cũ * (1-alpha).
Dòng 188 trả giá trị đã lọc.
Dòng 189 kết thúc.
13. Phát hiện va chạm / té ngã
Dòng 191 detect_impact_event(...): phát hiện sự kiện va chạm.

Dòng 192 tính độ lớn vector gia tốc.

Dòng 193 lấy thời gian hiện tại (ms).

Dòng 194 kiểm tra góc nghiêng có lớn hơn ngưỡng không.

Dòng 196 nếu chưa theo dõi sốc:

Dòng 197 nếu magnitude vượt ngưỡng sốc:

Dòng 198 bật cờ theo dõi sốc.

Dòng 199 lưu thời điểm bắt đầu sốc.

Dòng 200 kết thúc if.

Dòng 201 trả bình thường ở chu kỳ đầu.

Dòng 202 nếu đang theo dõi sốc:

Dòng 203 tính thời gian đã trôi qua.

Dòng 204 nếu nghiêng lớn và đủ lâu:

Dòng 205 tắt theo dõi sốc.

Dòng 206 trả về té ngã.

Dòng 208 nếu không nghiêng lớn và sốc chỉ là va chạm ngắn:

Dòng 209 tắt theo dõi sốc.

Dòng 210 trả va chạm ở ga.

Dòng 212 nếu chưa đủ điều kiện thì vẫn bình thường.

Dòng 213-214 kết thúc hàm.

14. Nhận diện tăng/giảm tốc
Dòng 216 detect_motion_state(...): xác định xe đang tăng hay giảm tốc.
Dòng 217 nếu accel dương lớn hơn ngưỡng:
Dòng 218 trả TOC_DO_TANG_TOC.
Dòng 219 nếu accel âm nhỏ hơn âm ngưỡng:
Dòng 220 trả TOC_DO_GIAM_TOC.
Dòng 221 còn lại:
Dòng 222 trả TOC_DO_DEU_GA.
Dòng 223-224 kết thúc.
15. Đọc biến trở tay ga
Dòng 226 comment.

Dòng 227 readPotSmooth(): đọc analog biến trở.

Dòng 228 biến sum.

Dòng 229 lặp 8 lần để lấy mẫu mượt hơn.

Dòng 230 đọc ADC từ POT_PIN.

Dòng 231 delay rất ngắn giữa các lần đọc.

Dòng 233 lấy trung bình bằng dịch bit phải 3.

Dòng 234 trừ phần nhiễu thấp.

Dòng 235 nếu âm thì chặn về 0.

Dòng 236 tính giá trị dùng được tối đa.

Dòng 237 chặn không vượt quá max.

Dòng 238 trả kết quả.

Dòng 239 kết thúc.

Dòng 241 adcToThrottle(...): đổi ADC thành phần trăm ga.

Dòng 242 tính vùng ADC hữu dụng.

Dòng 243 nếu dưới deadband thì coi như 0 ga.

Dòng 244 chuẩn hóa sang 0..1.

Dòng 245 nếu quá 1 thì chặn lại.

Dòng 246 trả t.

Dòng 247 kết thúc.

16. Hàm setup
Dòng 249 comment.

Dòng 250 setup(): chạy 1 lần khi ESP32 khởi động.

Dòng 251 mở Serial 115200.

Dòng 252 delay để Serial ổn định.

Dòng 254 in dòng tiêu đề.

Dòng 255 in tên project.

Dòng 256 in số profile âm thanh.

Dòng 257 in dải RPM.

Dòng 258 in dấu ngăn cách.

Dòng 260 setup I2C.

Dòng 261 nếu BMI160 init fail:

Dòng 262 in lỗi nghiêm trọng.

Dòng 263 kết thúc if.

Dòng 265 init audio engine.

Dòng 267 báo đang init BLE.

Dòng 268 khởi tạo BLE với tên thiết bị.

Dòng 269 báo hệ thống sẵn sàng.

Dòng 270 kết thúc setup.

17. Hàm loop
Dòng 272 comment.
Dòng 273 loop(): chạy lặp mãi.
Dòng 274 biến lưu thời điểm log gần nhất.
Dòng 275 biến lưu thời điểm đọc BMI gần nhất.
Dòng 277 gọi AudioEngine_fillBuffer() để nạp buffer âm thanh.
18. Đọc BMI160 mỗi 20ms
Dòng 279 comment.

Dòng 280 nếu đã qua 20ms thì đọc tiếp.

Dòng 281 cập nhật mốc thời gian.

Dòng 282 đọc dữ liệu cảm biến vào các biến global.

Dòng 284 tính pitch.

Dòng 285 tính roll.

Dòng 287 lọc gia tốc forward.

Dòng 289 comment sửa lỗi dùng fabsf().

Dòng 290 chỉ nếu accel dương đủ lớn mới tính throttle BMI.

Dòng 291 chuẩn hóa ga BMI.

Dòng 292 chặn tối đa 1.0.

Dòng 293-294 nếu không đủ ngưỡng thì ga BMI = 0.

Dòng 295 kết thúc else.

Dòng 297 phát hiện sự kiện va chạm.

Dòng 298 phát hiện trạng thái tốc độ.

Dòng 300 comment fix té ngã.

Dòng 301 nếu phát hiện té ngã:

Dòng 302 bật cờ ngã.

Dòng 303 lưu thời điểm ngã.

Dòng 304 tắt động cơ.

Dòng 305 kết thúc if.

Dòng 307 comment kiểm tra hết thời gian an toàn chưa.

Dòng 308 nếu hết 1 giây:

Dòng 309 tắt cờ ngã.

Dòng 310 kết thúc if.

Dòng 311 kết thúc khối đọc BMI.

19. Xử lý lệnh Serial
Dòng 313 comment.
Dòng 314 nếu có dữ liệu trên Serial:
Dòng 315 đọc 1 dòng đến \n.
Dòng 316 xóa khoảng trắng.
Dòng 317 nếu chuỗi bắt đầu bằng S hoặc s:
Dòng 318 lấy số sau chữ S rồi đổi thành index.
Dòng 319 nếu index hợp lệ:
Dòng 320 chuyển âm thanh.
Dòng 321 tắt động cơ để áp sound mới.
Dòng 322-323 kết thúc if.
Dòng 324 kết thúc xử lý Serial.
20. Xử lý BLE
Dòng 326 comment.
Dòng 327 gọi BLEManager_process() để BLE hoạt động.
21. Trộn ga
Dòng 329 comment.
Dòng 330 đọc biến trở.
Dòng 331 đổi ADC sang throttle của biến trở.
Dòng 332 cộng ga biến trở + ga BMI.
Dòng 333 chặn tổng ga tối đa là 1.0.
22. State machine động cơ
Dòng 335 comment.
Dòng 336 switch (s_state): xử lý theo trạng thái động cơ.
Case ENG_OFF
Dòng 337 vào trạng thái tắt.
Dòng 338 tắt âm thanh.
Dòng 339 comment fix trạng thái ngã.
Dòng 340 nếu có ga và không đang ngã:
Dòng 341 reset RPM về 0.
Dòng 342 phát âm thanh đề máy.
Dòng 343 chuyển sang ENG_STARTING.
Dòng 344-345 kết thúc case.
Case ENG_STARTING
Dòng 347 đang đề máy.
Dòng 348 nếu tiếng đề đã xong:
Dòng 349 set RPM về idle.
Dòng 350 set target RPM về idle.
Dòng 351 cập nhật âm thanh ở idle.
Dòng 352 chuyển sang chạy bình thường.
Dòng 353-354 kết thúc case.
Case ENG_RUNNING
Dòng 356 đang chạy.
Dòng 357 tính RPM mục tiêu theo tổng ga.
Dòng 359 nếu RPM hiện tại nhỏ hơn mục tiêu:
Dòng 360 tính mức tăng RPM.
Dòng 361 cộng RPM.
Dòng 362 nếu vượt mục tiêu thì chặn lại.
Dòng 363 ngược lại nếu RPM đang cao hơn mục tiêu:
Dòng 364 giảm RPM.
Dòng 365 nếu thấp hơn mục tiêu thì chặn lại.
Dòng 366 kết thúc else.
Dòng 368 cập nhật engine sound theo RPM hiện tại.
Dòng 369-370 kết thúc case/switch.
23. In log mỗi 300ms
Dòng 372 comment.

Dòng 373 nếu đã 300ms thì in log.

Dòng 374 cập nhật thời điểm log.

Dòng 375-376 tạo chuỗi st là OFF/STARTING/RUNNING.

Dòng 378 comment.

Dòng 379 in tiêu đề khối BMI160.

Dòng 380 in accel X/Y/Z.

Dòng 381 in gyro X/Y/Z.

Dòng 382 in pitch/roll.

Dòng 383 in trạng thái tốc độ và giá trị LPF.

Dòng 384 in sự kiện va chạm.

Dòng 385 in dòng ngăn cách.

Dòng 387 comment.

Dòng 388 khai báo buffer log.

Dòng 389-392 format chuỗi log đầy đủ:

trạng thái động cơ
RPM
tổng ga
ga POT
ga IMU
volume
rev mix
số lần ISR
Dòng 394 in log ra Serial.

Dòng 395 gửi log qua BLE.

Dòng 396 kết thúc khối log.

24. Cuối loop
Dòng 398 delay 2ms để loop không chạy quá gắt.
Dòng 399 đóng hàm loop().




























































































































































































































