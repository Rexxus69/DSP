// ══════════════════════════════════════════════════════════════════════════════
// DSP Controller v4.2 — lcd.cpp
// Waveshare ESP32-S3-LCD-3.16 fixed driver
// - ST7701S init via official 3-wire SPI sequence
// - RGB bus 320x820
// - Backlight ACTIVE LOW: GPIO6 LOW = ON
// - Exposes the same API expected by display_task.cpp:
//     bool lcd_init();
//     lv_disp_t* lcd_lvgl_init();
//     void lcd_set_brightness(uint8_t val);
// ══════════════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <string.h>
#include <lvgl.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "io_additions/esp_lcd_panel_io_additions.h"
}

// ── Official Waveshare ESP32-S3-LCD-3.16 pinout ─────────────────────────────
#define LCD_H_RES 320
#define LCD_V_RES 820
#define LVGL_H_RES LCD_V_RES   // logical landscape width: 820
#define LVGL_V_RES LCD_H_RES   // logical landscape height: 320

#define LCD_SPI_CS   GPIO_NUM_0
#define LCD_SPI_SCK  GPIO_NUM_2
#define LCD_SPI_SDA  GPIO_NUM_1

#define LCD_RST      GPIO_NUM_16
#define LCD_BL       6

#define LCD_DE       GPIO_NUM_40
#define LCD_PCLK     GPIO_NUM_41
#define LCD_VSYNC    GPIO_NUM_39
#define LCD_HSYNC    GPIO_NUM_38

#define LCD_R0       GPIO_NUM_17
#define LCD_R1       GPIO_NUM_46
#define LCD_R2       GPIO_NUM_3
#define LCD_R3       GPIO_NUM_8
#define LCD_R4       GPIO_NUM_18

#define LCD_G0       GPIO_NUM_14
#define LCD_G1       GPIO_NUM_13
#define LCD_G2       GPIO_NUM_12
#define LCD_G3       GPIO_NUM_11
#define LCD_G4       GPIO_NUM_10
#define LCD_G5       GPIO_NUM_9

#define LCD_B0       GPIO_NUM_21
#define LCD_B1       GPIO_NUM_5
#define LCD_B2       GPIO_NUM_45
#define LCD_B3       GPIO_NUM_48
#define LCD_B4       GPIO_NUM_47

#define LCD_PCLK_HZ  (10 * 1000 * 1000)
#define LCD_BUF_LINES 20
#define LCD_BUF_SIZE  (LVGL_H_RES * LCD_BUF_LINES)

static esp_lcd_panel_handle_t _panel = nullptr;
static esp_lcd_panel_io_handle_t _io_handle = nullptr;
static lv_disp_draw_buf_t _draw_buf;
static lv_disp_drv_t _disp_drv;
static lv_color_t *_buf1 = nullptr;
static lv_color_t *_buf2 = nullptr;
static lv_color_t *_rot_buf = nullptr;
static size_t _rot_buf_px = 0;

struct st7701_cmd_t {
    uint8_t cmd;
    const uint8_t *data;
    uint8_t len;
    uint16_t delay_ms;
};

#define U8A(...) (const uint8_t[]){__VA_ARGS__}

// Official Waveshare ST7701S sequence from 07_LVGL_V8_Test.
static const st7701_cmd_t init_cmds[] = {
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x13),5,0},
    {0xEF, U8A(0x08),1,0},
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x10),5,0},
    {0xC0, U8A(0xE5,0x02),2,0},
    {0xC1, U8A(0x15,0x0A),2,0},
    {0xC2, U8A(0x07,0x02),2,0},
    {0xCC, U8A(0x10),1,0},
    {0xB0, U8A(0x00,0x08,0x51,0x0D,0xCE,0x06,0x00,0x08,0x08,0x24,0x05,0xD0,0x0F,0x6F,0x36,0x1F),16,0},
    {0xB1, U8A(0x00,0x10,0x4F,0x0C,0x11,0x05,0x00,0x07,0x07,0x18,0x02,0xD3,0x11,0x6E,0x34,0x1F),16,0},
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x11),5,0},
    {0xB0, U8A(0x4D),1,0},
    {0xB1, U8A(0x37),1,0},
    {0xB2, U8A(0x87),1,0},
    {0xB3, U8A(0x80),1,0},
    {0xB5, U8A(0x4A),1,0},
    {0xB7, U8A(0x85),1,0},
    {0xB8, U8A(0x21),1,0},
    {0xB9, U8A(0x00,0x13),2,0},
    {0xC0, U8A(0x09),1,0},
    {0xC1, U8A(0x78),1,0},
    {0xC2, U8A(0x78),1,0},
    {0xD0, U8A(0x88),1,0},
    {0xE0, U8A(0x80,0x00,0x02),3,100},
    {0xE1, U8A(0x0F,0xA0,0x00,0x00,0x10,0xA0,0x00,0x00,0x00,0x60,0x60),11,0},
    {0xE2, U8A(0x30,0x30,0x60,0x60,0x45,0xA0,0x00,0x00,0x46,0xA0,0x00,0x00,0x00),13,0},
    {0xE3, U8A(0x00,0x00,0x33,0x33),4,0},
    {0xE4, U8A(0x44,0x44),2,0},
    {0xE5, U8A(0x0F,0x4A,0xA0,0xA0,0x11,0x4A,0xA0,0xA0,0x13,0x4A,0xA0,0xA0,0x15,0x4A,0xA0,0xA0),16,0},
    {0xE6, U8A(0x00,0x00,0x33,0x33),4,0},
    {0xE7, U8A(0x44,0x44),2,0},
    {0xE8, U8A(0x10,0x4A,0xA0,0xA0,0x12,0x4A,0xA0,0xA0,0x14,0x4A,0xA0,0xA0,0x16,0x4A,0xA0,0xA0),16,0},
    {0xEB, U8A(0x02,0x00,0x4E,0x4E,0xEE,0x44,0x00),7,0},
    {0xED, U8A(0xFF,0xFF,0x04,0x56,0x72,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x27,0x65,0x40,0xFF,0xFF),16,0},
    {0xEF, U8A(0x08,0x08,0x08,0x40,0x3F,0x64),6,0},
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x13),5,0},
    {0xE8, U8A(0x00,0x0E),2,0},
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x00),5,0},
    {0x11, nullptr,0,120},
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x13),5,0},
    {0xE8, U8A(0x00,0x0C),2,10},
    {0xE8, U8A(0x00,0x00),2,0},
    {0xFF, U8A(0x77,0x01,0x00,0x00,0x00),5,0},
    {0x3A, U8A(0x55),1,0},
    {0x36, U8A(0x00),1,0},
    {0x35, U8A(0x00),1,0},
    {0x29, nullptr,0,20},
    {0xC3, U8A(0x80),1,0},
};

static void backlight_on() {
    // ACTIVE LOW: LOW = ON, HIGH = OFF.
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, LOW);
}

static void backlight_off() {
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
}

static esp_err_t lcd_send_init_sequence() {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << LCD_RST;
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_line_config_t line_config = {};
    line_config.cs_io_type = IO_TYPE_GPIO;
    line_config.cs_gpio_num = LCD_SPI_CS;
    line_config.scl_io_type = IO_TYPE_GPIO;
    line_config.scl_gpio_num = LCD_SPI_SCK;
    line_config.sda_io_type = IO_TYPE_GPIO;
    line_config.sda_gpio_num = LCD_SPI_SDA;
    line_config.io_expander = NULL;

    esp_lcd_panel_io_3wire_spi_config_t io_config = {};
    io_config.line_config = line_config;
    io_config.expect_clk_speed = PANEL_IO_3WIRE_SPI_CLK_MAX;
    io_config.spi_mode = 0;
    io_config.lcd_cmd_bytes = 1;
    io_config.lcd_param_bytes = 1;
    io_config.flags.use_dc_bit = 1;
    io_config.flags.dc_zero_on_data = 0;
    io_config.flags.lsb_first = 0;
    io_config.flags.cs_high_active = 0;
    io_config.flags.del_keep_cs_inactive = 1;

    esp_err_t err = esp_lcd_new_panel_io_3wire_spi(&io_config, &_io_handle);
    Serial.printf("[LCD] 3-wire IO init: %s (%d)\n", esp_err_to_name(err), err);
    if (err != ESP_OK) return err;

    for (size_t i = 0; i < sizeof(init_cmds)/sizeof(init_cmds[0]); ++i) {
        const st7701_cmd_t &c = init_cmds[i];
        err = esp_lcd_panel_io_tx_param(_io_handle, c.cmd, c.data, c.len);
        if (err != ESP_OK) {
            Serial.printf("[LCD] ST7701 cmd 0x%02X failed: %s (%d)\n", c.cmd, esp_err_to_name(err), err);
            return err;
        }
        if (c.delay_ms) vTaskDelay(pdMS_TO_TICKS(c.delay_ms));
    }
    Serial.println("[LCD] ST7701S 3-wire init OK");
    return ESP_OK;
}

static void lcd_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *map) {
    if (!_panel) {
        lv_disp_flush_ready(drv);
        return;
    }

    // LVGL logical landscape: 820x320. Physical panel: 320x820.
    // Rotate flushed areas 90 degrees clockwise.
    const int32_t lx1 = area->x1;
    const int32_t ly1 = area->y1;
    const int32_t lx2 = area->x2;
    const int32_t ly2 = area->y2;
    const int32_t src_w = lx2 - lx1 + 1;
    const int32_t src_h = ly2 - ly1 + 1;
    const size_t need_px = (size_t)src_w * (size_t)src_h;

    if (need_px > _rot_buf_px) {
        if (_rot_buf) heap_caps_free(_rot_buf);
        _rot_buf = (lv_color_t*)heap_caps_malloc(need_px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!_rot_buf) _rot_buf = (lv_color_t*)heap_caps_malloc(need_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
        _rot_buf_px = _rot_buf ? need_px : 0;
    }
    if (!_rot_buf) {
        Serial.println("[LCD] rotate flush buffer alloc failed");
        lv_disp_flush_ready(drv);
        return;
    }

    const int32_t px1 = ly1;
    const int32_t py1 = LCD_V_RES - 1 - lx2;
    const int32_t px2 = ly2;
    const int32_t py2 = LCD_V_RES - 1 - lx1;
    const int32_t dst_w = src_h;

    for (int32_t sy = 0; sy < src_h; ++sy) {
        for (int32_t sx = 0; sx < src_w; ++sx) {
            const size_t src_i = (size_t)sy * src_w + sx;
            const int32_t dx = sy;
            const int32_t dy = src_w - 1 - sx;
            _rot_buf[(size_t)dy * dst_w + dx] = map[src_i];
        }
    }
    esp_lcd_panel_draw_bitmap(_panel, px1, py1, px2 + 1, py2 + 1, _rot_buf);
    lv_disp_flush_ready(drv);
}

bool lcd_init() {
    backlight_on();

    if (_panel) {
        Serial.println("[LCD] already initialized");
        return true;
    }

    esp_err_t err = lcd_send_init_sequence();
    if (err != ESP_OK) return false;

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_XTAL;
    cfg.data_width = 16;
    cfg.psram_trans_align = 64;
    cfg.disp_gpio_num = -1;
    cfg.de_gpio_num = LCD_DE;
    cfg.pclk_gpio_num = LCD_PCLK;
    cfg.vsync_gpio_num = LCD_VSYNC;
    cfg.hsync_gpio_num = LCD_HSYNC;

    // Official working mapping: BGR order on the RGB data bus.
    cfg.data_gpio_nums[0]  = LCD_B0;
    cfg.data_gpio_nums[1]  = LCD_B1;
    cfg.data_gpio_nums[2]  = LCD_B2;
    cfg.data_gpio_nums[3]  = LCD_B3;
    cfg.data_gpio_nums[4]  = LCD_B4;
    cfg.data_gpio_nums[5]  = LCD_G0;
    cfg.data_gpio_nums[6]  = LCD_G1;
    cfg.data_gpio_nums[7]  = LCD_G2;
    cfg.data_gpio_nums[8]  = LCD_G3;
    cfg.data_gpio_nums[9]  = LCD_G4;
    cfg.data_gpio_nums[10] = LCD_G5;
    cfg.data_gpio_nums[11] = LCD_R0;
    cfg.data_gpio_nums[12] = LCD_R1;
    cfg.data_gpio_nums[13] = LCD_R2;
    cfg.data_gpio_nums[14] = LCD_R3;
    cfg.data_gpio_nums[15] = LCD_R4;

    cfg.timings.pclk_hz = LCD_PCLK_HZ;
    cfg.timings.h_res = LCD_H_RES;
    cfg.timings.v_res = LCD_V_RES;
    cfg.timings.hsync_back_porch = 30;
    cfg.timings.hsync_front_porch = 30;
    cfg.timings.hsync_pulse_width = 6;
    cfg.timings.vsync_back_porch = 20;
    cfg.timings.vsync_front_porch = 20;
    cfg.timings.vsync_pulse_width = 40;
    cfg.timings.flags.hsync_idle_low = 0;
    cfg.timings.flags.vsync_idle_low = 0;
    cfg.timings.flags.de_idle_high = 0;
    cfg.timings.flags.pclk_active_neg = 0;
    cfg.flags.fb_in_psram = true;

    err = esp_lcd_new_rgb_panel(&cfg, &_panel);
    Serial.printf("[LCD] esp_lcd_new_rgb_panel: %s (%d), handle=%p\n", esp_err_to_name(err), err, _panel);
    if (err != ESP_OK) return false;

    // Do NOT call esp_lcd_panel_reset(_panel): ST7701S was already initialized by 3-wire SPI.
    err = esp_lcd_panel_init(_panel);
    Serial.printf("[LCD] esp_lcd_panel_init: %s (%d)\n", esp_err_to_name(err), err);
    if (err != ESP_OK) return false;

    backlight_on();
    Serial.printf("[LCD] init OK — %dx%d RGB565, PCLK %dMHz, BL GPIO%d active-low\n",
                  LCD_H_RES, LCD_V_RES, LCD_PCLK_HZ/1000000, LCD_BL);
    return true;
}

lv_disp_t* lcd_lvgl_init() {
    _buf1 = (lv_color_t*)heap_caps_malloc(LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    _buf2 = (lv_color_t*)heap_caps_malloc(LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!_buf1 || !_buf2) {
        Serial.printf("[LCD] LVGL internal buffer alloc failed buf1=%p buf2=%p — trying PSRAM\n", _buf1, _buf2);
        if (_buf1) heap_caps_free(_buf1);
        if (_buf2) heap_caps_free(_buf2);
        _buf1 = (lv_color_t*)heap_caps_malloc(LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
        _buf2 = (lv_color_t*)heap_caps_malloc(LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    }
    if (!_buf1 || !_buf2) {
        Serial.println("[LCD] LVGL buffer alloc FAILED completely");
        return nullptr;
    }

    lv_disp_draw_buf_init(&_draw_buf, _buf1, _buf2, LCD_BUF_SIZE);
    lv_disp_drv_init(&_disp_drv);
    _disp_drv.hor_res = LVGL_H_RES;
    _disp_drv.ver_res = LVGL_V_RES;
    _disp_drv.flush_cb = lcd_flush_cb;
    _disp_drv.draw_buf = &_draw_buf;
    _disp_drv.full_refresh = 0;
    Serial.println("[LCD] LVGL display driver registered - logical 820x320 rotated to physical 320x820");
    return lv_disp_drv_register(&_disp_drv);
}

void lcd_set_brightness(uint8_t val) {
    // Backlight is active-low. For now keep it simple and safe:
    // any non-zero brightness = ON, zero = OFF.
    if (val == 0) backlight_off();
    else backlight_on();
}
