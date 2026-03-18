#ifndef TOUCHSCREEN_UI_STRINGS_H
#define TOUCHSCREEN_UI_STRINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =========================================================================
 * Language enum
 * ========================================================================= */
typedef enum {
    UI_LANG_BG = 0,   /* Bulgarian (default) */
    UI_LANG_EN = 1,   /* English */
    UI_LANG_COUNT
} ui_language_t;

/* =========================================================================
 * String IDs — every translatable UI string
 * ========================================================================= */
typedef enum {
    /* --- Navbar --- */
    STR_NAV_DASHBOARD = 0,
    STR_NAV_SCHEDULE,
    STR_NAV_SETTINGS,
    STR_NAV_INFO,

    /* --- Dashboard: bell status card --- */
    STR_BELL_STATUS,
    STR_BELL_IDLE,
    STR_BELL_RINGING,
    STR_BELL_PANIC,
    STR_NEXT_BELL,
    STR_NO_UPCOMING_BELLS,

    /* --- Dashboard: info card --- */
    STR_DATE,
    STR_NO_TIME_SYNC,
    STR_DAY_TYPE,
    STR_DAY_OFF,
    STR_WORKING_DAY,
    STR_HOLIDAY,
    STR_EXCEPTION_WORKING,
    STR_EXCEPTION_OFF,
    STR_UNKNOWN,

    /* --- Dashboard: action buttons --- */
    STR_RING,
    STR_PANIC,
    STR_STOP,
    STR_DAY_OFF_BTN,
    STR_DAY_ON_BTN,
    STR_CANCEL_OFF,
    STR_CANCEL_ON,

    /* --- Dashboard: warning banner --- */
    STR_WARN_WIFI_DISCONNECTED,
    STR_WARN_TIME_NOT_SYNCED,
    STR_WARN_WIFI_AND_TIME,
    STR_WARN_TIME_STALE,

    /* --- Schedule view --- */
    STR_1ST_SHIFT,
    STR_2ND_SHIFT,
    STR_SHIFT_DISABLED,
    STR_NO_BELLS_TODAY,
    STR_CLASS,          /* "Class" / "Час" */
    STR_START,          /* "start" / "начало" */
    STR_END,            /* "end"   / "край" */
    STR_SEC_SUFFIX,     /* "s"     / "с" */
    STR_BREAK,          /* "Break" / "Междучасие" */

    /* --- Settings --- */
    STR_NETWORK,
    STR_WIFI_NETWORK,
    STR_IP_ADDRESS,
    STR_NOT_CONNECTED,
    STR_SCHEDULE,
    STR_WORKING_DAYS,
    STR_TIMEZONE,
    STR_SYSTEM,
    STR_NTP_SYNC,
    STR_SYNCED_JUST_NOW,
    STR_NOT_SYNCED,
    STR_NONE,
    STR_LANGUAGE,

    /* --- Day name abbreviations (Sun=0 ... Sat=6) --- */
    STR_DAY_SUN,
    STR_DAY_MON,
    STR_DAY_TUE,
    STR_DAY_WED,
    STR_DAY_THU,
    STR_DAY_FRI,
    STR_DAY_SAT,

    /* --- Month abbreviations (0-based: Jan=0 ... Dec=11) --- */
    STR_MON_JAN,
    STR_MON_FEB,
    STR_MON_MAR,
    STR_MON_APR,
    STR_MON_MAY,
    STR_MON_JUN,
    STR_MON_JUL,
    STR_MON_AUG,
    STR_MON_SEP,
    STR_MON_OCT,
    STR_MON_NOV,
    STR_MON_DEC,

    /* --- Info screen --- */
    STR_DEVICE,
    STR_FIRMWARE_VERSION,
    STR_IDF_VERSION,
    STR_CHIP,
    STR_MAC_ADDRESS,
    STR_WEB_INTERFACE,
    STR_SCAN_QR,
    STR_WIFI_NOT_CONNECTED,
    STR_ABOUT,
    STR_ABOUT_DESC,
    STR_BOARD_INFO,

    /* --- PIN entry --- */
    STR_ENTER_PIN,
    STR_ENTER_PIN_SUBTITLE,
    STR_WRONG_PIN,
    STR_CANCEL,

    /* --- WiFi setup --- */
    STR_WIFI_SETUP,
    STR_SCAN,
    STR_SCANNING,
    STR_TAP_SCAN,
    STR_SCAN_FAILED,
    STR_SCAN_FAILED_RETRY,
    STR_SCANNING_NETWORKS,
    STR_NO_NETWORKS_SCANNED,
    STR_NO_NETWORKS_FOUND,
    STR_ENTER_SSID_MANUALLY,
    STR_SKIP,
    STR_BACK,
    STR_CONNECT_TO,
    STR_PASSWORD,
    STR_ENTER_PASSWORD,
    STR_SHOW_PASSWORD,
    STR_CONNECT,
    STR_NETWORK_NAME_SSID,
    STR_PASSWORD_OR_EMPTY,
    STR_CONNECTING_TO,
    STR_PLEASE_WAIT,
    STR_CONNECTED,
    STR_CONNECTING_BG,
    STR_LOCKED_FOR,

    STR_COUNT   /* Must be last */
} ui_string_id_t;

/* =========================================================================
 * API
 * ========================================================================= */

/**
 * @brief Get the translated string for the current language.
 * @param id  String ID from ui_string_id_t
 * @return  Pointer to a UTF-8 string (never NULL — returns "" on invalid id)
 */
const char *ui_str(ui_string_id_t id);

/**
 * @brief Set the active UI language and persist to NVS.
 */
void ui_set_language(ui_language_t lang);

/**
 * @brief Get the current UI language.
 */
ui_language_t ui_get_language(void);

/**
 * @brief Load language preference from NVS. Call once at startup.
 */
void ui_strings_init(void);

#ifdef __cplusplus
}
#endif

#endif /* TOUCHSCREEN_UI_STRINGS_H */
