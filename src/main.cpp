#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
/* #include <lvgl.h>
#include <demos/lv_demos.h> */

TFT_eSPI tft = TFT_eSPI();

// ======= 触摸屏引脚定义 =======
#define TFT_TOUCH_XM 3  // -X
#define TFT_TOUCH_YM 8  // -Y
#define TFT_TOUCH_XP 18 // +X
#define TFT_TOUCH_YP 17 // +Y

// 校准参数
int touch_x_min = 200;
int touch_x_max = 3800;
int touch_y_min = 200;
int touch_y_max = 3800;
bool swap_xy = false;

const int EDGE_MARGIN = 20;

// ====== 1. 核心改进：用物理拉低的方法绝对可靠地检测是否发生触摸 ======
bool isTouched() {
    bool touched = false;
    // 将 X+ 内部上拉为高电平 (HIGH)，将 Y- 强制拉低 (LOW)
    // 没按时，X+ 是 HIGH。按下去后，上下层导通，Y- 会把 X+ 扯到 LOW。
    pinMode(TFT_TOUCH_XP, INPUT_PULLUP);
    pinMode(TFT_TOUCH_XM, INPUT);
    pinMode(TFT_TOUCH_YP, INPUT);
    pinMode(TFT_TOUCH_YM, OUTPUT);
    digitalWrite(TFT_TOUCH_YM, LOW);

    delay(2);
    // 判断 X+ 是否被拉低
    if (digitalRead(TFT_TOUCH_XP) == LOW) {
        touched = true;
    }

    // 恢复原有防干扰状态
    pinMode(TFT_TOUCH_XP, INPUT);
    pinMode(TFT_TOUCH_YM, INPUT);
    return touched;
}

// ====== 2. 读取坐标并引入物理按压校验 ======
bool getRawTouch(int &rawX, int &rawY) {
    if (!isTouched()) return false; // 如果物理上没被按下，直接返回无触摸！杜绝漂移

    // ---- 1. 读取 X 轴 ----
    pinMode(TFT_TOUCH_YM, INPUT);
    pinMode(TFT_TOUCH_YP, INPUT);
    
    pinMode(TFT_TOUCH_XP, OUTPUT);
    digitalWrite(TFT_TOUCH_XP, HIGH);
    pinMode(TFT_TOUCH_XM, OUTPUT);
    digitalWrite(TFT_TOUCH_XM, LOW);
    
    delay(2);
    int rx = analogRead(TFT_TOUCH_YP);

    // ---- 2. 读取 Y 轴 ----
    pinMode(TFT_TOUCH_XM, INPUT);
    pinMode(TFT_TOUCH_XP, INPUT);
    
    pinMode(TFT_TOUCH_YP, OUTPUT);
    digitalWrite(TFT_TOUCH_YP, HIGH);
    pinMode(TFT_TOUCH_YM, OUTPUT);
    digitalWrite(TFT_TOUCH_YM, LOW);
    
    delay(2);
    int ry = analogRead(TFT_TOUCH_XP);

    pinMode(TFT_TOUCH_YP, INPUT);
    pinMode(TFT_TOUCH_YM, INPUT);

    if (rx > 20 && rx < 4095 && ry > 20 && ry < 4095) {
        rawX = rx;
        rawY = ry;
        return true;
    }
    return false;
}

void drawCross(int x, int y, uint16_t color) {
    tft.drawLine(x - 10, y, x + 10, y, color);
    tft.drawLine(x, y - 10, x, y + 10, color);
}

void waitTouch(int &recordX, int &recordY) {
    int rx, ry;
    // 等待按下
    while (!getRawTouch(rx, ry)) {
        delay(10);
    }
    delay(50); // 稳定防抖

    // 读取15次平均值
    long sumX = 0, sumY = 0;
    int count = 0;
    for (int i = 0; i < 15; i++) {
        if (getRawTouch(rx, ry)) {
            sumX += rx;
            sumY += ry;
            count++;
        }
        delay(10);
    }
    if (count > 0) {
        recordX = sumX / count;
        recordY = sumY / count;
    }

    // 等待松开 (这里使用纯物理引脚检测，不纠结模拟值)
    while (isTouched()) {
        delay(10);
    }
    delay(200); // 离开防抖
}

void calibrateTouch() {
    tft.fillScreen(TFT_BLACK);

    // 左上角、右上角、右下角
    int cal_pts_x[3] = { EDGE_MARGIN, tft.width() - EDGE_MARGIN, tft.width() - EDGE_MARGIN };
    int cal_pts_y[3] = { EDGE_MARGIN, EDGE_MARGIN, tft.height() - EDGE_MARGIN };
    int raw_x[3] = {0}, raw_y[3] = {0};

    for (int i = 0; i < 3; i++) {
        // 画十字（由于你的屏幕RGB顺序可能不太一样，我这里直接用白色，最醒目）
        drawCross(cal_pts_x[i], cal_pts_y[i], TFT_WHITE);
        
        waitTouch(raw_x[i], raw_y[i]);
        
        // 你点击松开后，它会被涂成黑色（消失掉）。然后进入下一个点的校准
        drawCross(cal_pts_x[i], cal_pts_y[i], TFT_BLACK); 
        delay(300);
    }

    // 计算校准逻辑
    int dx = abs(raw_x[0] - raw_x[1]);
    int dy = abs(raw_y[0] - raw_y[1]);
    swap_xy = (dy > dx);

    if (swap_xy) {
        for (int i = 0; i < 3; i++) {
            int temp = raw_x[i];
            raw_x[i] = raw_y[i];
            raw_y[i] = temp;
        }
    }

    touch_x_min = raw_x[0];
    touch_x_max = raw_x[1];
    touch_y_min = raw_y[1];
    touch_y_max = raw_y[2];

    Serial.println("\n--- Touch Calibration Results ---");
    Serial.printf("touch_x_min = %d\n", touch_x_min);
    Serial.printf("touch_x_max = %d\n", touch_x_max);
    Serial.printf("touch_y_min = %d\n", touch_y_min);
    Serial.printf("touch_y_max = %d\n", touch_y_max);
    Serial.printf("swap_xy = %s\n", swap_xy ? "true" : "false");
    Serial.println("---------------------------------");
    
    Serial.println("--- Screen Corners (Raw X, Y) ---");
    Serial.printf("Top-Left:     X=%d, Y=%d\n", touch_x_min, touch_y_min);
    Serial.printf("Top-Right:    X=%d, Y=%d\n", touch_x_max, touch_y_min);
    Serial.printf("Bottom-Left:  X=%d, Y=%d\n", touch_x_min, touch_y_max);
    Serial.printf("Bottom-Right: X=%d, Y=%d\n", touch_x_max, touch_y_max);
    Serial.println("---------------------------------\n");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("OK!", 10, 10);
    delay(1000);
    tft.fillScreen(TFT_BLACK);
}

bool readTouchMapped(int &x, int &y) {
    int rawX, rawY;
    if (getRawTouch(rawX, rawY)) {
        if (swap_xy) {
            int temp = rawX;
            rawX = rawY;
            rawY = temp;
        }

        x = map(rawX, touch_x_min, touch_x_max, EDGE_MARGIN, tft.width() - EDGE_MARGIN);
        y = map(rawY, touch_y_min, touch_y_max, EDGE_MARGIN, tft.height() - EDGE_MARGIN);

        x = constrain(x, 0, tft.width());
        y = constrain(y, 0, tft.height());
        return true;
    }
    return false;
}

/* LVGL 显示刷新回调 */
/* void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
} */

/* LVGL 触摸读取回调 */
/* void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    int x, y;
    if (readTouchMapped(x, y)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
} */

/* // 屏幕尺寸，需根据你的屏幕实际情况确认
static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10]; // 缓冲区大小可根据内存调整 */

void setup() {
    Serial.begin(115200);

    // 1. 初始化 TFT
    tft.init();
    tft.setRotation(1);
    tft.invertDisplay(false); // 修复 ST7789 屏幕反色问题
    
    // 校准触摸（如果你之前已经记下校准值，可以直接跳过这里节省开机时间）
    calibrateTouch();

/*     // 2. 初始化 LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    // 3. 注册 LVGL 显示驱动
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = tft.width();
    disp_drv.ver_res = tft.height();
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 4. 注册 LVGL 输入设备驱动
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // ================== LVGL 组件演示 ==================
    // 注释掉之前的手动组件，启动 benchmark
    // lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x222222), LV_PART_MAIN); 
    // lv_obj_t *btn1 = lv_btn_create(lv_scr_act());
    // lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -50);
    // lv_obj_t *label1 = lv_label_create(btn1);
    // lv_label_set_text(label1, "Hello LVGL!");
    // lv_obj_t *slider = lv_slider_create(lv_scr_act());
    // lv_obj_align(slider, LV_ALIGN_CENTER, 0, 10);
    // lv_obj_set_width(slider, 200);
    // lv_obj_t *sw = lv_switch_create(lv_scr_act());
    // lv_obj_align(sw, LV_ALIGN_CENTER, 0, 60);
    
    // 运行 Benchmark 演示 
    //lv_demo_benchmark();
    
    // 运行音乐播放器演示（非常华丽）
    //lv_demo_music();
    
    // 或者运行组件概览演示（展示所有基础组件的样式）
    //lv_demo_widgets();
    
    // 或者运行压力测试
     lv_demo_stress(); */
}

void loop() {
    /* lv_timer_handler(); // 处理 LVGL 的绘图和事件 */
    delay(5);           // 让出一点时间
}
