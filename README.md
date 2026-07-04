# Esp32-bmi160-i2c
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



































































































































































































































