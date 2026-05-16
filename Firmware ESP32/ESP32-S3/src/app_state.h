#pragma once
#include <stdint.h>
#include <string.h>

// ══════════════════════════════════════════════════════════════════════════════
// DSP Controller v4.2 — app_state.h
// Waveshare ESP32-S3-LCD-3.16 + ADAU1452 + CS42448 + PCF8574 x2
// ══════════════════════════════════════════════════════════════════════════════

// ── I²C ───────────────────────────────────────────────────────────────────────
#define I2C_SDA_PIN         15      // SH1.0 4PIN header
#define I2C_SCL_PIN         7
#define I2C_FREQ_HZ         100000

// ── Device addresses ──────────────────────────────────────────────────────────
#define ADAU1452_ADDR       0x38
#define CS42448_ADDR        0x48
#define PCF8574_ADDR_1      0x20    // A0=A1=A2=GND — encoder + relay TW/MID/WF/SEL/SPDIF
#define PCF8574_ADDR_2      0x21    // A0=VCC, A1=A2=GND — relay SUB L/R + espansione
#define PCF85063_ADDR       0x51    // onboard RTC (non cablare esternamente!)

// ── PCF8574 #1 (0x20) pin map ──────────────────────────────────────────────────
// Input (encoder): LOW = premuto / in movimento
#define PCF1_ENC_A          0x01    // P0 — Encoder canale A
#define PCF1_ENC_B          0x02    // P1 — Encoder canale B
#define PCF1_ENC_BTN        0x04    // P2 — Encoder pulsante (click)
// Output relay (open-drain, LOW = relay ON = speaker attivo)
#define PCF1_RELAY_TW       0x08    // P3 — Tweeter L+R
#define PCF1_RELAY_MID      0x10    // P4 — Mid L+R
#define PCF1_RELAY_WF       0x20    // P5 — Woofer L+R
#define PCF1_RELAY_SEL      0x40    // P6 — Selezione uscita
#define PCF1_SPDIF_SEL      0x80    // P7 — SPDIF: LOW=coax, HIGH=ottico (default)
// Default output: tutti speaker ON (relay LOW), SPDIF ottico (P7 HIGH)
#define PCF1_DEFAULT_OUT    (PCF1_SPDIF_SEL)  // altri relay ON = LOW = 0

// ── PCF8574 #2 (0x21) pin map ──────────────────────────────────────────────────
#define PCF2_RELAY_SUB      0x01    // P0 — Sub L+R
// P1..P7: liberi per espansione futura
#define PCF2_DEFAULT_OUT    (0xFF)  // SUB ON = bit0=0 → 0xFE; default OFF=0xFF

// ── ADAU1452 Parameter RAM base ───────────────────────────────────────────────
// Indirizzi assoluti (PARAM_BASE = 0) da ADAU1452_192tdm(SPI)_FIR_DSP_ver1
#define PARAM_BASE          0x0000  // assoluto, nessun offset
#define PARAM_VOLUME        31      // Interface Read1_2 (encoder → volume)

// Indirizzi gain per routing misura (vedi ADAU1452_PMAP in DSPxLab)
#define PARAM_GAIN_MID_L    308
#define PARAM_GAIN_MID_R    309
#define PARAM_GAIN_WF_L     312
#define PARAM_GAIN_WF_R     313
#define PARAM_GAIN_TW_L     304
#define PARAM_GAIN_TW_R     305
// SUB non ha gain separato (usa alignment gain via main volume)

// ── SafeLoad registers ────────────────────────────────────────────────────────
#define SAFELOAD_DATA_0     24
#define SAFELOAD_DATA_1     25
#define SAFELOAD_DATA_2     26
#define SAFELOAD_DATA_3     27
#define SAFELOAD_DATA_4     28
#define SAFELOAD_ADDR       29      // scrittura avvia il trasferimento

// ── Sorgenti audio ────────────────────────────────────────────────────────────
// Ordine = indice hardware nella Parameter RAM (Single1_3..14 mux volumes)
typedef enum {
    SOURCE_RADIO    = 0,  // CS42448 ADC ch0/1
    SOURCE_CD       = 1,  // CS42448 ADC ch2/3
    SOURCE_PHONO    = 2,  // CS42448 ADC ch4/5
    SOURCE_TAPE     = 3,  // PCM1808 ADC (I2S)
    SOURCE_TOSLINK  = 4,  // SPDIF → ASRC0 (relay P7=HIGH → ottico)
    SOURCE_SPDIF    = 5,  // SPDIF → ASRC0 (relay P7=LOW  → coax)
    SOURCE_USB      = 6,  // XMOS XU208 → TDM ch0/1
    SOURCE_BT       = 7,  // QCC5125   → TDM ch2/3
    SOURCE_COUNT    = 8
} AudioSource_t;

// Indirizzi mux volume per ogni sorgente (Single1_3..12 param RAM)
static const uint16_t SRC_MUX_ADDR[SOURCE_COUNT] = {
    34, 37, 40, 43, 46, 49, 52, 52  // nota: USB e BT condividono ultimo (TDM)
};

// ── Stato applicazione ────────────────────────────────────────────────────────
#define MAX_PRESETS         6
#define PRESET_NAME_LEN     17      // 16 char + null
#define PRESET_DESC_LEN     64      // descrizione/seconda riga preset + null

typedef struct {
    char    name[PRESET_NAME_LEN];  // prima riga / label breve
    char    relays[PRESET_DESC_LEN];// seconda riga / descrizione configurazione
                                   // nome storico mantenuto per compatibilità NVS
    bool    loaded;
    uint8_t relay1;                 // byte PCF8574 #1 (bit mask)
    uint8_t relay2;                 // byte PCF8574 #2 (bit mask)
} Preset_t;

typedef struct {
    // ── Audio ──────────────────────────────────────────────────────────────
    AudioSource_t   source;         // sorgente attiva
    int8_t          volume_db;      // -80..0 dB
    bool            muted;

    // ── Relay stato corrente ───────────────────────────────────────────────
    uint8_t         pcf1_out;       // byte corrente PCF8574 #1
    uint8_t         pcf2_out;       // byte corrente PCF8574 #2

    // ── ASRC lock ─────────────────────────────────────────────────────────
    bool            asrc_locked;    // true = segnale digitale locked
    uint8_t         asrc_sr_idx;    // 0=48k, 1=96k, 2=192k, 3=44.1k

    // ── RTC ───────────────────────────────────────────────────────────────
    uint8_t         rtc_h;
    uint8_t         rtc_m;
    uint8_t         rtc_s;
    uint8_t         rtc_day;
    uint8_t         rtc_month;
    uint16_t        rtc_year;

    // ── VU meter ──────────────────────────────────────────────────────────
    float           vu_l;           // 0..1 linear RMS
    float           vu_r;

    // ── Preset ────────────────────────────────────────────────────────────
    uint8_t         active_preset;  // 0..5
    Preset_t        presets[MAX_PRESETS];

    // ── WiFi ──────────────────────────────────────────────────────────────
    int8_t          wifi_rssi;      // dBm (0 = disconnesso)
    char            wifi_ssid[33];

    // ── DSPxLab connection / time sync ─────────────────────────────────
    bool            dspx_connected;
    uint32_t        dspx_last_ms;
} AppState_t;

// ── Encoder ───────────────────────────────────────────────────────────────────
#define ENC_DEBOUNCE_MS     2
#define ENC_ACCEL_MS        300     // finestra accelerazione (più giri = step maggiore)

// ── WebSocket opcodes ─────────────────────────────────────────────────────────
// Client → ESP32
#define WS_OP_ADAU_WRITE    0x01    // scrive param RAM ADAU1452
#define WS_OP_ADAU_READ     0x0A    // legge HW register ADAU1452
#define WS_OP_PCF_WRITE     0x10    // scrive byte PCF8574 (relay control)
#define WS_OP_SET_SOURCE    0x11    // cambia sorgente
#define WS_OP_SET_VOLUME    0x12    // imposta volume dB
#define WS_OP_PRESET_LOAD   0x13    // carica preset
#define WS_OP_PRESET_SAVE   0x14    // salva preset
                                      // legacy: [1]=idx,[2..17]=name,[18]=relay1,[19]=relay2
                                      // esteso: +[20..]=desc ASCII/NUL, max PRESET_DESC_LEN-1
#define WS_OP_NVS_SAVE      0x15    // forza salvataggio DSP params in NVS flash
#define WS_OP_NVS_CLEAR     0x16    // cancella DSP params NVS (factory reset)
#define WS_OP_NVS_STATUS    0x17    // risponde con stato NVS (params saved/not)
// ESP32 → Client (broadcast)
#define WS_OP_STATUS        0x20    // frame stato completo (display + DSPxLab)
#define WS_OP_NVS_ACK       0x21    // risposta a NVS_SAVE/NVS_STATUS

// ── NVS DSP params ────────────────────────────────────────────────────────────
// Namespace: "dsp_p" — parametri DSP scritti da DSPxLab (FIR/IIR/delay/gain)
// Separato da "dsp" (volume/source) per indipendenza logica.
// Struttura chiavi:
//   "valid"        → uint8  1=params salvati validi
//   "n_blk"        → uint16 numero di blocchi salvati
//   "b%04X_a"      → uint16 indirizzo inizio blocco
//   "b%04X_d"      → bytes  dati raw (max 28 byte per chunk Wire)
//   "b%04X_n"      → uint8  numero di word del blocco
#define NVS_DSP_PARAMS_NS   "dsp_p"
#define NVS_DSP_MAX_BLOCKS  512     // max blocchi param salvabili

#define WS_OP_WIFI_STATUS  0x18    // stato WiFi
#define WS_OP_WIFI_CONFIG  0x19    // salva rete WiFi STA
#define WS_OP_RTC_SET      0x1A    // imposta RTC da DSPxLab
