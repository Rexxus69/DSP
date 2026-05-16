// ══════════════════════════════════════════════════════════════════════════════
// DSP Controller v4.3 - display_task.cpp
// UI rework per display 820×320: font più grandi, nessuna top/bottom bar,
// informazioni ricollocate nell'area centrale.
// ══════════════════════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "app_state.h"
#include <lvgl.h>

extern AppState_t        state;
extern SemaphoreHandle_t stateMutex;

static AppState_t disp_state;

#define DISP_W          820
#define DISP_H          320
#define VU_W            72
#define CENTER_W        (DISP_W - 2 * VU_W)
#define SIDE_PANEL_W    136
#define VOL_ZONE_W      (CENTER_W - 2 * SIDE_PANEL_W)

extern bool       lcd_init();
extern lv_disp_t* lcd_lvgl_init();
extern void       lcd_set_brightness(uint8_t val);

static struct {
    lv_obj_t* lbl_clock;
    lv_obj_t* lbl_date;
    lv_obj_t* lbl_dspx;
    lv_obj_t* lbl_src;
    lv_obj_t* lbl_info;
    lv_obj_t* lbl_wifi;
    lv_obj_t* lbl_dsp;

    lv_obj_t* input_list;
    lv_obj_t* input_btns[SOURCE_COUNT];

    lv_obj_t* lbl_vol;        // fallback, hidden
    lv_obj_t* vol_seg_cont;   // big seven-segment numeric volume
    lv_obj_t* vol_seg[3][7];  // minus/tens/ones
    lv_obj_t* lbl_vol_unit;
    lv_obj_t* bar_vol;
    lv_obj_t* lbl_preset_name;

    lv_obj_t* relay_cont;
    lv_obj_t* relay_lbl[8];

    lv_obj_t* preset_list;
    lv_obj_t* preset_btns[MAX_PRESETS];

    lv_obj_t* vu_l_cont;
    lv_obj_t* vu_l_bar;
    lv_obj_t* vu_l_min_tick;
    lv_obj_t* vu_l_peak;
    lv_obj_t* vu_r_cont;
    lv_obj_t* vu_r_bar;
    lv_obj_t* vu_r_min_tick;
    lv_obj_t* vu_r_peak;
} ui;

#define COL_BG          lv_color_hex(0x07090E)
#define COL_BG2         lv_color_hex(0x0C1018)
#define COL_BG3         lv_color_hex(0x0A1420)
#define COL_BORDER      lv_color_hex(0x141820)
#define COL_ACC_BLUE    lv_color_hex(0x4A9EFF)
#define COL_ACC_GREEN   lv_color_hex(0x40E090)
#define COL_ACC_GOLD    lv_color_hex(0xF0C030)
#define COL_ACC_VIOLET  lv_color_hex(0xE060FF)
#define COL_TXT         lv_color_hex(0xD6E1F2)
#define COL_TXT2        lv_color_hex(0x5C6B86)
#define COL_TXT3        lv_color_hex(0x3A4658)
#define COL_TXT4        lv_color_hex(0x8EA3C5)

static const char* SRC_DISPLAY[SOURCE_COUNT] = {
    "RADIO", "CD", "PHONO", "TAPE", "TOSLINK", "S/PDIF", "USB", "BLUETOOTH"
};

static const char* SRC_INFO[SOURCE_COUNT] = {
    "Analog input", "Analog input", "Analog input", "Analog input",
    "Optical digital", "Coax digital", "USB audio", "Bluetooth audio"
};

static const char* relay_labels_from_preset(int preset_idx, int badge_idx) {
    static const char* labels[] = {
        "TWL", "MIDL", "WFL", "SWL",
        "TWR", "MIDR", "WFR", "SWR"
    };

    if (preset_idx >= MAX_PRESETS || badge_idx < 0 || badge_idx >= 8) return "";
    const Preset_t& p = disp_state.presets[preset_idx];
    if (!p.loaded) return "";

    bool tw  = !(p.relay1 & PCF1_RELAY_TW);
    bool mid = !(p.relay1 & PCF1_RELAY_MID);
    bool wf  = !(p.relay1 & PCF1_RELAY_WF);
    bool sw  = !(p.relay2 & PCF2_RELAY_SUB);

    if ((badge_idx == 0 || badge_idx == 4) && !tw)  return "";
    if ((badge_idx == 1 || badge_idx == 5) && !mid) return "";
    if ((badge_idx == 2 || badge_idx == 6) && !wf)  return "";
    if ((badge_idx == 3 || badge_idx == 7) && !sw)  return "";

    return labels[badge_idx];
}

static void update_relay_rows(int preset_idx, bool active) {
    for (int i = 0; i < 8; i++) {
        if (!ui.relay_lbl[i]) continue;
        const char* lbl = active ? relay_labels_from_preset(preset_idx, i) : "";
        lv_label_set_text(ui.relay_lbl[i], lbl);
        lv_obj_set_style_opa(ui.relay_lbl[i],
            (lbl && lbl[0]) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static const char* preset_description(const Preset_t& p, int idx) {
    if (p.loaded && p.relays[0]) return p.relays;
    if (idx == 0) return "3 vie stereo";
    if (p.loaded && p.name[0]) return p.name;

    static char fallback[18];
    snprintf(fallback, sizeof(fallback), "PRESET %d", idx + 1);
    return fallback;
}


static void style_panel(lv_obj_t* obj) {
    lv_obj_set_style_bg_color(obj, COL_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, COL_BORDER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}


static void seg_set(lv_obj_t* obj, bool on) {
    lv_obj_set_style_bg_opa(obj, on ? LV_OPA_COVER : LV_OPA_20, 0);
}

static void sevenseg_create_digit(lv_obj_t* parent, lv_obj_t* seg[7], int x, int y, int w, int h, int t) {
    // segment order: A,B,C,D,E,F,G
    const int hlen = w - 2 * t;
    const int vlen = (h - 3 * t) / 2;

    // A
    seg[0] = lv_obj_create(parent);
    lv_obj_set_pos(seg[0], x + t, y);
    lv_obj_set_size(seg[0], hlen, t);

    // B
    seg[1] = lv_obj_create(parent);
    lv_obj_set_pos(seg[1], x + w - t, y + t);
    lv_obj_set_size(seg[1], t, vlen);

    // C
    seg[2] = lv_obj_create(parent);
    lv_obj_set_pos(seg[2], x + w - t, y + 2 * t + vlen);
    lv_obj_set_size(seg[2], t, vlen);

    // D
    seg[3] = lv_obj_create(parent);
    lv_obj_set_pos(seg[3], x + t, y + h - t);
    lv_obj_set_size(seg[3], hlen, t);

    // E
    seg[4] = lv_obj_create(parent);
    lv_obj_set_pos(seg[4], x, y + 2 * t + vlen);
    lv_obj_set_size(seg[4], t, vlen);

    // F
    seg[5] = lv_obj_create(parent);
    lv_obj_set_pos(seg[5], x, y + t);
    lv_obj_set_size(seg[5], t, vlen);

    // G
    seg[6] = lv_obj_create(parent);
    lv_obj_set_pos(seg[6], x + t, y + t + vlen);
    lv_obj_set_size(seg[6], hlen, t);

    for (int i = 0; i < 7; i++) {
        lv_obj_set_style_bg_color(seg[i], COL_TXT, 0);
        lv_obj_set_style_border_width(seg[i], 0, 0);
        lv_obj_set_style_radius(seg[i], t / 2, 0);
        lv_obj_clear_flag(seg[i], LV_OBJ_FLAG_SCROLLABLE);
        seg_set(seg[i], false);
    }
}

static void sevenseg_set_digit(lv_obj_t* seg[7], int digit) {
    static const bool map[10][7] = {
        {1,1,1,1,1,1,0}, // 0
        {0,1,1,0,0,0,0}, // 1
        {1,1,0,1,1,0,1}, // 2
        {1,1,1,1,0,0,1}, // 3
        {0,1,1,0,0,1,1}, // 4
        {1,0,1,1,0,1,1}, // 5
        {1,0,1,1,1,1,1}, // 6
        {1,1,1,0,0,0,0}, // 7
        {1,1,1,1,1,1,1}, // 8
        {1,1,1,1,0,1,1}, // 9
    };

    if (digit < 0 || digit > 9) {
        for (int i = 0; i < 7; i++) seg_set(seg[i], false);
        return;
    }

    for (int i = 0; i < 7; i++) seg_set(seg[i], map[digit][i]);
}

static void sevenseg_set_minus(lv_obj_t* seg[7]) {
    for (int i = 0; i < 7; i++) seg_set(seg[i], i == 6);
}

static void update_volume_display(int vol_db) {
    int v = vol_db < 0 ? -vol_db : vol_db;
    if (v > 99) v = 99;
    sevenseg_set_minus(ui.vol_seg[0]);
    sevenseg_set_digit(ui.vol_seg[1], (v >= 10) ? ((v / 10) % 10) : -1);
    sevenseg_set_digit(ui.vol_seg[2], v % 10);
}


static void ui_create() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    auto mk_vu = [&](lv_obj_t** cont, lv_obj_t** bar, lv_obj_t** min_tick, lv_obj_t** peak, const char* name, bool isLeft) {
        *cont = lv_obj_create(scr);
        lv_obj_set_size(*cont, VU_W, DISP_H);
        lv_obj_set_pos(*cont, isLeft ? 0 : DISP_W - VU_W, 0);
        lv_obj_set_style_bg_color(*cont, lv_color_hex(0x06080C), 0);
        lv_obj_set_style_bg_opa(*cont, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(*cont, 0, 0);
        lv_obj_clear_flag(*cont, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ch = lv_label_create(*cont);
        lv_label_set_text(ch, name);
        lv_obj_set_style_text_font(ch, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(ch, COL_TXT4, 0);
        lv_obj_align(ch, LV_ALIGN_TOP_MID, 0, 8);

        *bar = lv_bar_create(*cont);
        lv_obj_set_size(*bar, 22, DISP_H - 68);
        lv_obj_align(*bar, LV_ALIGN_CENTER, 0, 10);
        lv_bar_set_range(*bar, 0, 100);
        lv_bar_set_value(*bar, 5, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(*bar, lv_color_hex(0x0D1018), 0);
        lv_obj_set_style_bg_color(*bar, lv_color_hex(0x0F3A20), LV_PART_INDICATOR);
        lv_obj_set_style_radius(*bar, 7, 0);

        // Tacca minima sempre visibile, indipendente dal valore del bar
        *min_tick = lv_obj_create(*cont);
        lv_obj_set_size(*min_tick, 26, 7);
        lv_obj_align_to(*min_tick, *bar, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_set_style_bg_color(*min_tick, COL_ACC_GREEN, 0);
        lv_obj_set_style_bg_opa(*min_tick, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(*min_tick, 0, 0);
        lv_obj_set_style_radius(*min_tick, 3, 0);
        lv_obj_clear_flag(*min_tick, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_foreground(*min_tick);

        *peak = lv_label_create(*cont);
        lv_label_set_text(*peak, "min");
        lv_obj_set_style_text_color(*peak, COL_TXT2, 0);
        lv_obj_set_style_text_font(*peak, &lv_font_montserrat_10, 0);
        lv_obj_align(*peak, LV_ALIGN_BOTTOM_MID, 0, -8);
    };

    mk_vu(&ui.vu_l_cont, &ui.vu_l_bar, &ui.vu_l_min_tick, &ui.vu_l_peak, "L", true);
    mk_vu(&ui.vu_r_cont, &ui.vu_r_bar, &ui.vu_r_min_tick, &ui.vu_r_peak, "R", false);

    lv_obj_t* center = lv_obj_create(scr);
    lv_obj_set_size(center, CENTER_W, DISP_H);
    lv_obj_set_pos(center, VU_W, 0);
    lv_obj_set_style_bg_color(center, COL_BG, 0);
    lv_obj_set_style_bg_opa(center, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center, 0, 0);
    lv_obj_set_style_pad_all(center, 0, 0);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);

    ui.input_list = lv_obj_create(center);
    lv_obj_set_size(ui.input_list, SIDE_PANEL_W, DISP_H);
    lv_obj_set_pos(ui.input_list, 0, 0);
    style_panel(ui.input_list);
    lv_obj_set_style_border_side(ui.input_list, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui.input_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.input_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(ui.input_list, 1, 0);

    for (int i = 0; i < SOURCE_COUNT; i++) {
        ui.input_btns[i] = lv_label_create(ui.input_list);
        lv_label_set_text(ui.input_btns[i], SRC_DISPLAY[i]);
        lv_obj_set_width(ui.input_btns[i], LV_PCT(100));
        lv_obj_set_style_pad_ver(ui.input_btns[i], 5, 0);
        lv_obj_set_style_pad_left(ui.input_btns[i], 8, 0);
        lv_obj_set_style_text_font(ui.input_btns[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(ui.input_btns[i], COL_TXT3, 0);
        lv_obj_set_style_border_width(ui.input_btns[i], 3, LV_PART_MAIN);
        lv_obj_set_style_border_side(ui.input_btns[i], LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.input_btns[i], COL_BORDER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.input_btns[i], LV_OPA_TRANSP, 0);
    }

    ui.preset_list = lv_obj_create(center);
    lv_obj_set_size(ui.preset_list, SIDE_PANEL_W, DISP_H);
    lv_obj_set_pos(ui.preset_list, CENTER_W - SIDE_PANEL_W, 0);
    style_panel(ui.preset_list);
    lv_obj_set_style_border_side(ui.preset_list, LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui.preset_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui.preset_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(ui.preset_list, 4, 0);

    for (int i = 0; i < MAX_PRESETS; i++) {
        ui.preset_btns[i] = lv_label_create(ui.preset_list);
        char buf[PRESET_NAME_LEN];
        snprintf(buf, sizeof(buf), "PRESET %d", i + 1);
        lv_label_set_text(ui.preset_btns[i], buf);
        lv_obj_set_width(ui.preset_btns[i], LV_PCT(100));
        lv_obj_set_style_pad_ver(ui.preset_btns[i], 10, 0);
        lv_obj_set_style_pad_left(ui.preset_btns[i], 10, 0);
        lv_obj_set_style_text_font(ui.preset_btns[i], &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ui.preset_btns[i], COL_TXT3, 0);
        lv_obj_set_style_border_width(ui.preset_btns[i], 3, LV_PART_MAIN);
        lv_obj_set_style_border_side(ui.preset_btns[i], LV_BORDER_SIDE_LEFT, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.preset_btns[i], COL_BORDER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui.preset_btns[i], LV_OPA_TRANSP, 0);
    }

    lv_obj_t* vol_zone = lv_obj_create(center);
    lv_obj_set_size(vol_zone, VOL_ZONE_W, DISP_H);
    lv_obj_set_pos(vol_zone, SIDE_PANEL_W, 0);
    lv_obj_set_style_bg_opa(vol_zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_zone, 0, 0);
    lv_obj_set_style_pad_all(vol_zone, 0, 0);
    lv_obj_clear_flag(vol_zone, LV_OBJ_FLAG_SCROLLABLE);

    // v65: niente data/ora in UI (rete locale senza Internet -> NTP spesso assente).
    // Redistribuzione header:
    // sx: WiFi / DSPxLab
    // centro: sorgente più grande, centrata verticalmente tra le due righe laterali
    // dx: tipo ingresso / sample rate
    ui.lbl_clock = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_clock, "");
    lv_obj_add_flag(ui.lbl_clock, LV_OBJ_FLAG_HIDDEN);

    ui.lbl_date = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_date, "");
    lv_obj_add_flag(ui.lbl_date, LV_OBJ_FLAG_HIDDEN);

    ui.lbl_wifi = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_wifi, "WiFi off");
    lv_obj_set_style_text_font(ui.lbl_wifi, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui.lbl_wifi, COL_TXT4, 0);
    lv_obj_align(ui.lbl_wifi, LV_ALIGN_TOP_LEFT, 10, 10);

    ui.lbl_dspx = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_dspx, "DSPx --");
    lv_obj_set_style_text_font(ui.lbl_dspx, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui.lbl_dspx, COL_TXT2, 0);
    lv_obj_align(ui.lbl_dspx, LV_ALIGN_TOP_LEFT, 10, 30);

    ui.lbl_src = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_src, "CD");
    lv_obj_set_style_text_font(ui.lbl_src, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(ui.lbl_src, COL_ACC_BLUE, 0);
    lv_obj_align(ui.lbl_src, LV_ALIGN_TOP_MID, 0, 12);

    ui.lbl_info = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_info, "Analog input");
    lv_obj_set_style_text_font(ui.lbl_info, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.lbl_info, COL_TXT2, 0);
    lv_obj_align(ui.lbl_info, LV_ALIGN_TOP_RIGHT, -10, 10);

    ui.lbl_dsp = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_dsp, "48kHz 24bit PCM");
    lv_obj_set_style_text_font(ui.lbl_dsp, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ui.lbl_dsp, COL_TXT2, 0);
    lv_obj_align(ui.lbl_dsp, LV_ALIGN_TOP_RIGHT, -10, 30);

    ui.lbl_vol = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_vol, "");
    lv_obj_add_flag(ui.lbl_vol, LV_OBJ_FLAG_HIDDEN);

    ui.vol_seg_cont = lv_obj_create(vol_zone);
    lv_obj_set_size(ui.vol_seg_cont, 238, 92);
    lv_obj_align(ui.vol_seg_cont, LV_ALIGN_CENTER, 0, -44);
    lv_obj_set_style_bg_opa(ui.vol_seg_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.vol_seg_cont, 0, 0);
    lv_obj_set_style_pad_all(ui.vol_seg_cont, 0, 0);
    lv_obj_clear_flag(ui.vol_seg_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Numero volume grande, stabile, indipendente dai font: '-' + due cifre
    sevenseg_create_digit(ui.vol_seg_cont, ui.vol_seg[0], 0,   25, 40, 50, 8);
    sevenseg_create_digit(ui.vol_seg_cont, ui.vol_seg[1], 58,  0,  66, 88, 10);
    sevenseg_create_digit(ui.vol_seg_cont, ui.vol_seg[2], 148, 0,  66, 88, 10);
    update_volume_display(-30);

    ui.lbl_vol_unit = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_vol_unit, "dB");
    lv_obj_set_style_text_font(ui.lbl_vol_unit, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui.lbl_vol_unit, COL_TXT2, 0);
    lv_obj_align(ui.lbl_vol_unit, LV_ALIGN_CENTER, 0, 20);

    ui.bar_vol = lv_bar_create(vol_zone);
    lv_obj_set_size(ui.bar_vol, (VOL_ZONE_W * 82) / 100, 18);
    lv_obj_align(ui.bar_vol, LV_ALIGN_CENTER, 0, 50);
    lv_bar_set_range(ui.bar_vol, 0, 80);
    lv_bar_set_value(ui.bar_vol, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.bar_vol, lv_color_hex(0x172033), 0);
    lv_obj_set_style_bg_color(ui.bar_vol, COL_ACC_BLUE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui.bar_vol, 7, 0);
    lv_obj_set_style_border_width(ui.bar_vol, 1, 0);
    lv_obj_set_style_border_color(ui.bar_vol, lv_color_hex(0x2A3A58), 0);

    ui.lbl_preset_name = lv_label_create(vol_zone);
    lv_label_set_text(ui.lbl_preset_name, "PRESET 1");
    lv_obj_set_style_text_font(ui.lbl_preset_name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ui.lbl_preset_name, COL_TXT4, 0);
    lv_obj_align(ui.lbl_preset_name, LV_ALIGN_CENTER, 0, 82);

    ui.relay_cont = lv_obj_create(vol_zone);
    lv_obj_set_size(ui.relay_cont, VOL_ZONE_W - 2, 72);
    lv_obj_align(ui.relay_cont, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_opa(ui.relay_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui.relay_cont, 0, 0);
    lv_obj_set_style_pad_all(ui.relay_cont, 0, 0);
    lv_obj_clear_flag(ui.relay_cont, LV_OBJ_FLAG_SCROLLABLE);

    static const lv_color_t badge_colors[8] = {
        lv_color_hex(0x60C8FF), lv_color_hex(0xF0C030),
        lv_color_hex(0x50E890), lv_color_hex(0xE060FF),
        lv_color_hex(0x60C8FF), lv_color_hex(0xF0C030),
        lv_color_hex(0x50E890), lv_color_hex(0xE060FF),
    };

    for (int i = 0; i < 8; i++) {
        ui.relay_lbl[i] = lv_label_create(ui.relay_cont);
        lv_label_set_text(ui.relay_lbl[i], "");
        lv_label_set_long_mode(ui.relay_lbl[i], LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(ui.relay_lbl[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(ui.relay_lbl[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(ui.relay_lbl[i], badge_colors[i], 0);
        lv_obj_set_style_border_width(ui.relay_lbl[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.relay_lbl[i], lv_color_mix(badge_colors[i], COL_BG, 48), LV_PART_MAIN);
        lv_obj_set_style_radius(ui.relay_lbl[i], 4, 0);
        lv_obj_set_style_pad_hor(ui.relay_lbl[i], 5, 0);
        lv_obj_set_style_pad_ver(ui.relay_lbl[i], 4, 0);
        lv_obj_set_style_bg_color(ui.relay_lbl[i], lv_color_mix(badge_colors[i], COL_BG, 22), 0);
        lv_obj_set_style_bg_opa(ui.relay_lbl[i], LV_OPA_20, 0);
        lv_obj_set_style_opa(ui.relay_lbl[i], LV_OPA_TRANSP, 0);

        // v49: posizione fissa 2x4.
        // Riga 0=L: TW MID WF SW; riga 1=R: TW MID WF SW.
        // Evita wrap errato tipo TW-R fuori colonna.
        const int bw = 56;
        const int bh = 22;
        const int gapX = 10;
        const int gapY = 10;
        const int totalW = 4*bw + 3*gapX;
        const int x0 = ((VOL_ZONE_W - 2) - totalW) / 2;
        const int y0 = 7;
        const int col = i % 4;
        const int row = i / 4;
        lv_obj_set_size(ui.relay_lbl[i], bw, bh);
        lv_obj_set_pos(ui.relay_lbl[i], x0 + col*(bw+gapX), y0 + row*(bh+gapY));
    }
}

static void refresh_ui() {
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        disp_state = state;
        xSemaphoreGive(stateMutex);
    } else {
        return;
    }

    update_volume_display((int)disp_state.volume_db);
    lv_bar_set_value(ui.bar_vol, 80 + disp_state.volume_db, LV_ANIM_OFF);

    uint8_t src = (uint8_t)disp_state.source;
    if (src < SOURCE_COUNT) {
        // v70b: la label centrale deve restare leggibile; BLUETOOTH diventa BT.
        lv_label_set_text(ui.lbl_src, (src == SOURCE_BT) ? "BT" : SRC_DISPLAY[src]);

        char info[40];
        snprintf(info, sizeof(info), "%s", SRC_INFO[src]);
        lv_label_set_text(ui.lbl_info, info);
    }

    for (int i = 0; i < SOURCE_COUNT; i++) {
        bool active = (i == (int)disp_state.source);
        lv_obj_set_style_text_color(ui.input_btns[i], active ? COL_ACC_BLUE : COL_TXT3, 0);
        lv_obj_set_style_border_color(ui.input_btns[i], active ? COL_ACC_BLUE : COL_BORDER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui.input_btns[i], active ? COL_BG3 : COL_BG, 0);
        lv_obj_set_style_bg_opa(ui.input_btns[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }

    uint8_t ap = disp_state.active_preset;
    for (int i = 0; i < MAX_PRESETS; i++) {
        bool active = (i == ap);
        const Preset_t& p = disp_state.presets[i];
        char preset_label[16];
        snprintf(preset_label, sizeof(preset_label), "PRESET %d", i + 1);
        lv_label_set_text(ui.preset_btns[i], preset_label);
        lv_obj_set_style_text_color(ui.preset_btns[i],
            active ? COL_ACC_BLUE : (p.loaded ? lv_color_hex(0xE4EAF5) : COL_TXT3), 0);
        lv_obj_set_style_border_color(ui.preset_btns[i], active ? COL_ACC_BLUE : COL_BORDER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ui.preset_btns[i], active ? COL_BG3 : COL_BG, 0);
        lv_obj_set_style_bg_opa(ui.preset_btns[i], active ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }

    if (ap < MAX_PRESETS) {
        char ps[64];
        const Preset_t& p = disp_state.presets[ap];
        snprintf(ps, sizeof(ps), "Preset attivo: %s", preset_description(p, ap));
        lv_label_set_text(ui.lbl_preset_name, ps);
    }

    update_relay_rows(ap, true);

    int vuL = max(10, (int)(disp_state.vu_l * 100.0f));
    int vuR = max(10, (int)(disp_state.vu_r * 100.0f));
    lv_bar_set_value(ui.vu_l_bar, vuL, LV_ANIM_OFF);
    lv_bar_set_value(ui.vu_r_bar, vuR, LV_ANIM_OFF);
    lv_obj_move_foreground(ui.vu_l_min_tick);
    lv_obj_move_foreground(ui.vu_r_min_tick);

    lv_color_t vu_col = vuL > 85 ? lv_color_hex(0x7A1A1A) :
                        vuL > 72 ? lv_color_hex(0x6B4B10) :
                                    lv_color_hex(0x0F3A20);
    lv_obj_set_style_bg_color(ui.vu_l_bar, vu_col, LV_PART_INDICATOR);
    vu_col = vuR > 85 ? lv_color_hex(0x7A1A1A) :
             vuR > 72 ? lv_color_hex(0x6B4B10) :
                        lv_color_hex(0x0F3A20);
    lv_obj_set_style_bg_color(ui.vu_r_bar, vu_col, LV_PART_INDICATOR);

    float pkL = disp_state.vu_l > 0.00001f ? 20.0f * log10f(disp_state.vu_l) : -99.0f;
    float pkR = disp_state.vu_r > 0.00001f ? 20.0f * log10f(disp_state.vu_r) : -99.0f;
    char dbs[16];
    if (pkL <= -90.0f) lv_label_set_text(ui.vu_l_peak, "min");
    else {
        snprintf(dbs, sizeof(dbs), "%ddB", (int)roundf(pkL));
        lv_label_set_text(ui.vu_l_peak, dbs);
    }
    if (pkR <= -90.0f) lv_label_set_text(ui.vu_r_peak, "min");
    else {
        snprintf(dbs, sizeof(dbs), "%ddB", (int)roundf(pkR));
        lv_label_set_text(ui.vu_r_peak, dbs);
    }

    if (disp_state.wifi_rssi != 0) {
        char wf[20];
        snprintf(wf, sizeof(wf), "WiFi %ddBm", (int)disp_state.wifi_rssi);
        lv_label_set_text(ui.lbl_wifi, wf);
    } else if (disp_state.wifi_ssid[0]) {
        lv_label_set_text(ui.lbl_wifi, disp_state.wifi_ssid);
    } else {
        lv_label_set_text(ui.lbl_wifi, "WiFi off");
    }

    if (ui.lbl_dspx) {
        lv_label_set_text(ui.lbl_dspx, disp_state.dspx_connected ? "DSPx OK" : "DSPx --");
        lv_obj_set_style_text_color(ui.lbl_dspx, disp_state.dspx_connected ? COL_ACC_GREEN : COL_TXT2, 0);
    }

    static const char* sr_str[] = {"48kHz", "96kHz", "192kHz", "44.1kHz"};
    char dsp_info[28];
    snprintf(dsp_info, sizeof(dsp_info), "%s 24bit PCM", sr_str[disp_state.asrc_sr_idx & 3]);
    lv_label_set_text(ui.lbl_dsp, dsp_info);
}

static void update_clock() {
    // v65: ora/data rimosse dalla UI.
    // Manteniamo la funzione per compatibilità ma senza mostrare placeholder NTP.
    if (ui.lbl_clock) lv_obj_add_flag(ui.lbl_clock, LV_OBJ_FLAG_HIDDEN);
    if (ui.lbl_date)  lv_obj_add_flag(ui.lbl_date, LV_OBJ_FLAG_HIDDEN);
}

void display_task(void* pv) {
#ifdef WOKWI_SIM
    Serial.println("[LCD] WOKWI_SIM: display task saltato");
    vTaskDelete(nullptr);
    return;
#endif

    lv_init();

    Serial.println("[LCDUI] display_task start");
    if (!lcd_init()) {
        Serial.println("[LCD] init fallita - display task terminato");
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[LCDUI] lcd_init OK");
    lv_disp_t* disp = lcd_lvgl_init();
    if (!disp) {
        Serial.println("[LCD] LVGL init fallita");
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[LCDUI] lvgl init OK, creating UI");
    ui_create();
    Serial.println("[LCDUI] ui_create OK");
    lcd_set_brightness(100);
    Serial.println("[LCDUI] backlight set, loop start");

    uint32_t lastFullRefresh = 0;
    uint32_t lastClock = 0;
    bool dspReady = false;

    for (;;) {
        uint32_t now = millis();
        lv_tick_inc(5);
        lv_timer_handler();

        if (now - lastFullRefresh >= 200) {
            if (!dspReady) {
                int8_t v = 0;
                if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    v = state.volume_db;
                    xSemaphoreGive(stateMutex);
                }
                if (v != 0) dspReady = true;
            }
            if (dspReady) refresh_ui();
            lastFullRefresh = now;
        }

        if (now - lastClock >= 1000) {
            update_clock();
            lastClock = now;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
