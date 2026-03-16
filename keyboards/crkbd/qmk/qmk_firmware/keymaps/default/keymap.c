#include QMK_KEYBOARD_H

enum custom_keycodes {
    // Homerow mods - balanced flavor (activate on other key press or tapping term)
    HM_EXLM = SAFE_RANGE, // LGUI on hold, ! on tap
    HM_AT,                 // LALT on hold, @ on tap
    HM_HASH,               // LSFT on hold, # on tap
    HM_DLR,                // LCTL on hold, $ on tap
    HM_AMPR,               // LCTL on hold, & on tap
    HM_ASTR,               // LSFT on hold, * on tap
    HM_LPRN,               // LALT on hold, ( on tap
    // Clipboard keys - tap-preferred (hold sends Ctrl+key once)
    CT_X,                  // tap=X, hold=Ctrl+X
    CT_C,                  // tap=C, hold=Ctrl+C
    CT_V,                  // tap=V, hold=Ctrl+Ins
    // Auto-shift keys - tap-preferred (hold sends shifted key)
    AS_GRV,                // tap=`, hold=~
    AS_EQL,                // tap==, hold=+
    AS_QUOT,               // tap=', hold="
    AS_MINS,               // tap=-, hold=_
    AS_SCLN,               // tap=;, hold=:
    AS_COMM,               // tap=,, hold=<
    AS_SLSH,               // tap=/, hold=?
    AS_DOT,                // tap=., hold=>
};

// --- Homerow mods (balanced) ---

typedef struct {
    uint8_t  mod;
    uint16_t tap;
} custom_mod_tap_t;

#define NUM_CUSTOM_MT 7

static const custom_mod_tap_t custom_mt_defs[NUM_CUSTOM_MT] = {
    [HM_EXLM - SAFE_RANGE] = {MOD_LGUI, KC_EXLM},
    [HM_AT   - SAFE_RANGE] = {MOD_LALT, KC_AT},
    [HM_HASH - SAFE_RANGE] = {MOD_LSFT, KC_HASH},
    [HM_DLR  - SAFE_RANGE] = {MOD_LCTL, KC_DLR},
    [HM_AMPR - SAFE_RANGE] = {MOD_LCTL, KC_AMPR},
    [HM_ASTR - SAFE_RANGE] = {MOD_LSFT, KC_ASTR},
    [HM_LPRN - SAFE_RANGE] = {MOD_LALT, KC_LPRN},
};

static uint16_t custom_mt_timer[NUM_CUSTOM_MT];
static bool     custom_mt_pressed[NUM_CUSTOM_MT];
static bool     custom_mt_held[NUM_CUSTOM_MT];

// --- Tap-preferred keys (clipboard + auto-shift) ---

typedef struct {
    uint16_t tap_kc;
    uint16_t hold_kc;
    bool     hold_register; // true = register/unregister, false = tap once
} tap_hold_key_t;

#define TP_FIRST CT_X
#define TP_LAST  AS_DOT
#define NUM_TP_KEYS (TP_LAST - TP_FIRST + 1)

static const tap_hold_key_t tp_defs[NUM_TP_KEYS] = {
    [CT_X    - TP_FIRST] = {KC_X,    LCTL(KC_X), false},
    [CT_C    - TP_FIRST] = {KC_C,    LCTL(KC_C), false},
    [CT_V    - TP_FIRST] = {KC_V,    S(KC_INS),  false},
    [AS_GRV  - TP_FIRST] = {KC_GRV,  S(KC_GRV),  true},
    [AS_EQL  - TP_FIRST] = {KC_EQL,  S(KC_EQL),  true},
    [AS_QUOT - TP_FIRST] = {KC_QUOT, S(KC_QUOT), true},
    [AS_MINS - TP_FIRST] = {KC_MINS, S(KC_MINS), true},
    [AS_SCLN - TP_FIRST] = {KC_SCLN, S(KC_SCLN), true},
    [AS_COMM - TP_FIRST] = {KC_COMM, S(KC_COMM), true},
    [AS_SLSH - TP_FIRST] = {KC_SLSH, S(KC_SLSH), true},
    [AS_DOT  - TP_FIRST] = {KC_DOT,  S(KC_DOT),  true},
};

static uint16_t tp_timer[NUM_TP_KEYS];
static bool     tp_pressed[NUM_TP_KEYS];
static bool     tp_held[NUM_TP_KEYS];
static bool     tp_tapped[NUM_TP_KEYS];

// --- Key processing ---

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        // Resolve pending tap-preferred keys as taps when any other key is pressed
        for (uint8_t i = 0; i < NUM_TP_KEYS; i++) {
            if (tp_pressed[i] && !tp_held[i] && !tp_tapped[i] &&
                keycode != (TP_FIRST + i)) {
                tap_code16(tp_defs[i].tap_kc);
                tp_tapped[i] = true;
            }
        }

        // Activate held homerow mods when any non-homerow key is pressed
        if (!(keycode >= HM_EXLM && keycode <= HM_LPRN)) {
            for (uint8_t i = 0; i < NUM_CUSTOM_MT; i++) {
                if (custom_mt_pressed[i] && !custom_mt_held[i]) {
                    register_mods(custom_mt_defs[i].mod);
                    custom_mt_held[i] = true;
                }
            }
        }
    }

    // Handle homerow mod keys (balanced)
    if (keycode >= HM_EXLM && keycode <= HM_LPRN) {
        uint8_t idx = keycode - SAFE_RANGE;
        if (record->event.pressed) {
            custom_mt_timer[idx]   = timer_read();
            custom_mt_pressed[idx] = true;
            custom_mt_held[idx]    = false;
        } else {
            custom_mt_pressed[idx] = false;
            if (custom_mt_held[idx]) {
                unregister_mods(custom_mt_defs[idx].mod);
            } else {
                tap_code16(custom_mt_defs[idx].tap);
            }
            custom_mt_held[idx] = false;
        }
        return false;
    }

    // Handle tap-preferred keys (clipboard + auto-shift)
    if (keycode >= TP_FIRST && keycode <= TP_LAST) {
        uint8_t idx = keycode - TP_FIRST;
        if (record->event.pressed) {
            tp_timer[idx]   = timer_read();
            tp_pressed[idx] = true;
            tp_held[idx]    = false;
            tp_tapped[idx]  = false;
        } else {
            tp_pressed[idx] = false;
            if (tp_held[idx]) {
                if (tp_defs[idx].hold_register) {
                    unregister_code16(tp_defs[idx].hold_kc);
                }
            } else if (!tp_tapped[idx]) {
                tap_code16(tp_defs[idx].tap_kc);
            }
            tp_held[idx]   = false;
            tp_tapped[idx] = false;
        }
        return false;
    }

    return true;
}

void matrix_scan_user(void) {
    // Homerow mods: activate after tapping term
    for (uint8_t i = 0; i < NUM_CUSTOM_MT; i++) {
        if (custom_mt_pressed[i] && !custom_mt_held[i] &&
            timer_elapsed(custom_mt_timer[i]) > TAPPING_TERM) {
            register_mods(custom_mt_defs[i].mod);
            custom_mt_held[i] = true;
        }
    }
    // Tap-preferred keys: activate hold action after tapping term
    for (uint8_t i = 0; i < NUM_TP_KEYS; i++) {
        if (tp_pressed[i] && !tp_held[i] && !tp_tapped[i] &&
            timer_elapsed(tp_timer[i]) > TAPPING_TERM) {
            if (tp_defs[i].hold_register) {
                register_code16(tp_defs[i].hold_kc);
            } else {
                tap_code16(tp_defs[i].hold_kc);
            }
            tp_held[i] = true;
        }
    }
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  // Colemak-DH with homerow mods
  [0] = LAYOUT_split_3x6_3_ex2(
  //,-----------------------------------------------------------.                    ,-----------------------------------------------------------.
       AS_GRV,    KC_Q,    KC_W,    KC_F,    KC_P,    KC_B, XXXXXXX,        XXXXXXX,     KC_J,    KC_L,    KC_U,    KC_Y, AS_SCLN,  AS_EQL,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
       KC_ESC,LGUI_T(KC_A),LALT_T(KC_R),LSFT_T(KC_S),LCTL_T(KC_T),KC_G,XXXXXXX, XXXXXXX, KC_M,LCTL_T(KC_N),LSFT_T(KC_E),LALT_T(KC_I),RGUI_T(KC_O),AS_QUOT,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      CW_TOGG,    KC_Z,    CT_X,    CT_C,    KC_D,    CT_V,                         KC_K,    KC_H, AS_COMM,  AS_DOT, AS_SLSH, AS_MINS,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          KC_BSPC,  KC_SPC,   MO(1),     MO(2),  KC_ENT,  KC_TAB
                                      //`--------------------------'  `--------------------------'
  ),

  // Lower (Navigation + Symbols with homerow mods)
  [1] = LAYOUT_split_3x6_3_ex2(
  //,-----------------------------------------------------------.                    ,-----------------------------------------------------------.
      _______, _______, _______, _______, KC_PIPE, KC_BSLS, QK_BOOT,        _______,  KC_DOWN, KC_RGHT,  KC_END, KC_LBRC, KC_RBRC, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
      _______, HM_EXLM,   HM_AT, HM_HASH,  HM_DLR, KC_PERC, _______,       _______, KC_CIRC, HM_AMPR, HM_ASTR, HM_LPRN, KC_RPRN, _______,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      _______, _______, _______, _______, _______, _______,                        KC_UP, KC_LEFT, KC_HOME, KC_LCBR, KC_RCBR, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          _______, _______, _______,      MO(3), _______, _______
                                      //`--------------------------'  `--------------------------'
  ),

  // Raise (Numbers)
  [2] = LAYOUT_split_3x6_3_ex2(
  //,-----------------------------------------------------------.                    ,-----------------------------------------------------------.
      _______, _______, _______, _______, _______, _______, _______,        QK_BOOT, _______, _______, _______, _______, _______, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
      _______,LGUI_T(KC_1),LALT_T(KC_2),LSFT_T(KC_3),LCTL_T(KC_4),KC_5,_______, _______,KC_6,LCTL_T(KC_7),LSFT_T(KC_8),LALT_T(KC_9),RGUI_T(KC_0),_______,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      _______, _______, _______, _______, _______, _______,                      _______, _______, _______, _______, _______, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          _______, _______,   MO(3),    _______, _______, _______
                                      //`--------------------------'  `--------------------------'
  ),

  // Settings (tri-layer: Lower+Raise)
  [3] = LAYOUT_split_3x6_3_ex2(
  //,-----------------------------------------------------------.                    ,-----------------------------------------------------------.
      _______, _______, _______, _______, _______, _______, _______,        _______, _______, _______, _______, _______, _______, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
       KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,  QK_RBT,    LSA(KC_4),  KC_F7,   KC_F8,   KC_F9,  KC_F10,  KC_F11,  KC_F12,
  //|--------+--------+--------+--------+--------+--------|                    |--------+--------+--------+--------+--------+--------|
      RGB_TOG, RGB_MOD, RGB_HUI, RGB_SAI, RGB_VAI, _______,                      _______, RGB_VAD, RGB_SAD, RGB_HUD, _______, _______,
  //|--------+--------+--------+--------+--------+--------+--------|  |--------+--------+--------+--------+--------+--------+--------|
                                          _______, _______, _______,    _______, _______, _______
                                      //`--------------------------'  `--------------------------'
  )
};
