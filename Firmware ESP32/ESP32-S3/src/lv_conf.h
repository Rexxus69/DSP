/**
 * lv_conf.h — LVGL 8.4 configuration
 * DSP Controller v4.2 — Waveshare ESP32-S3-LCD-3.16
 */

#if 1 /* Set it to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16       /* RGB565 */
#define LV_COLOR_16_SWAP 0
#define LV_COLOR_SCREEN_TRANSP 0
#define LV_COLOR_MIX_ROUND_OFS 0
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*=========================
   MEMORY SETTINGS
 *=========================*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U)  /* 128KB per LVGL heap */
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC malloc
#define LV_MEM_POOL_FREE  free
#define LV_MEMCPY_MEMSET_STD 1

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 0        /* 0 = usa lv_tick_inc manuale */
#define LV_DPI_DEF 130

/*=================
 * DRAW BACKEND
 *================*/
#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_COUNT 4
#define LV_IMG_CACHE_DEF_SIZE 0
#define LV_LAYER_SIMPLE_BUF_SIZE (24 * 1024)
#define LV_IMG_USE_DRAW_CORE_MASKS 0

/*===================
 * GPU
 *==================*/
#define LV_USE_GPU_STM32_DMA2D 0
#define LV_USE_GPU_NXP_PXP     0
#define LV_USE_GPU_NXP_VG_LITE 0
#define LV_USE_GPU_SDL         0

/*==================
 * LOGGING
 * ==================*/
#define LV_USE_LOG 0

/*=================
 * ASSERT
 *================*/
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*==================
 * OTHERS
 * ==================*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_REFR_DEBUG   0
#define LV_SPRINTF_CUSTOM   0
#define LV_USE_USER_DATA    1
#define LV_ENABLE_GC        0

/*==================
 * COMPILER SETTINGS
 *==================*/
#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TASK_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN_SIZE 4
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_LARGE_RAM_ARRAY
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_DMA
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning
#define LV_USE_LARGE_COORD 0

/*==================
 * FONT USAGE
 *==================*/
/* Montserrat fonts — abilitate tutte quelle usate nel display */
#define LV_FONT_MONTSERRAT_8  1   /* badge relay, info piccole */
#define LV_FONT_MONTSERRAT_10 1   /* sorgenti, preset, bottombar */
#define LV_FONT_MONTSERRAT_12 1   /* sorgente attiva topbar */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1   /* volume principale */

/* Caratteri speciali */
#define LV_FONT_MONTSERRAT_12_SUBPX      0
#define LV_FONT_MONTSERRAT_28_COMPRESSED 0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_SIMSUN_16_CJK            0
#define LV_FONT_UNSCII_8                 1   /* font monospace base sempre presente */
#define LV_FONT_UNSCII_16                0

#define LV_FONT_CUSTOM_DECLARE
#define LV_FONT_DEFAULT &lv_font_montserrat_10

/*===================
 *  WIDGET USAGE
 *==================*/
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  0
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LABEL_LONG_TXT_HINT  0
#define LV_USE_LINE       0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     0
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0

/*==================
 * EXTRA COMPONENTS
 *==================*/
#define LV_USE_FLEX  1
#define LV_USE_GRID  0

#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*==================
 * THEME
 *==================*/
#define LV_USE_THEME_DEFAULT    0
#define LV_USE_THEME_BASIC      0
#define LV_USE_THEME_MONO       0

/*==================
 * LAYOUTS
 *==================*/
#define LV_USE_LAYOUT_FLEX  1
#define LV_USE_LAYOUT_GRID  0

#endif /*LV_CONF_H*/
#endif /*End of "Content enable"*/
