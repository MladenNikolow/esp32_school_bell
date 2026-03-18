/* ================================================================== */
/* ui_strings.c — BG/EN string tables for touch screen UI              */
/* ================================================================== */
#include "ui_strings.h"
#include "NVS_API.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_STRINGS";

static ui_language_t s_language = UI_LANG_BG;  /* Default: Bulgarian */

/* ------------------------------------------------------------------ */
/* Bulgarian strings                                                   */
/* ------------------------------------------------------------------ */
static const char * const s_strings_bg[STR_COUNT] = {
    /* Navbar */
    [STR_NAV_DASHBOARD]       = "Начало",
    [STR_NAV_SCHEDULE]        = "Програма",
    [STR_NAV_SETTINGS]        = "Настройки",
    [STR_NAV_INFO]            = "Инфо",

    /* Dashboard: bell status */
    [STR_BELL_STATUS]         = "Звънец",
    [STR_BELL_IDLE]           = "Неактивен",
    [STR_BELL_RINGING]        = "Звъни",
    [STR_BELL_PANIC]          = "ПАНИКА",
    [STR_NEXT_BELL]           = "Следващ звънец",
    [STR_NO_UPCOMING_BELLS]   = "Няма предстоящи звънци",

    /* Dashboard: info card */
    [STR_DATE]                = "Дата",
    [STR_NO_TIME_SYNC]        = "Няма синхр. на часа",
    [STR_DAY_TYPE]            = "Тип на деня",
    [STR_DAY_OFF]             = "Почивен ден",
    [STR_WORKING_DAY]         = "Работен ден",
    [STR_HOLIDAY]             = "Празник",
    [STR_EXCEPTION_WORKING]   = "Изключение (работен)",
    [STR_EXCEPTION_OFF]       = "Изключение (почивка)",
    [STR_UNKNOWN]             = "Неизвестно",

    /* Dashboard: action buttons */
    [STR_RING]                = "Звъни",
    [STR_PANIC]               = "ПАНИКА",
    [STR_STOP]                = "СТОП",
    [STR_DAY_OFF_BTN]         = "Почивен",
    [STR_DAY_ON_BTN]          = "Работен",
    [STR_CANCEL_OFF]          = "Отмени почивка",
    [STR_CANCEL_ON]           = "Отмени работен",

    /* Dashboard: warning banner */
    [STR_WARN_WIFI_DISCONNECTED] = "WiFi не е свързан",
    [STR_WARN_TIME_NOT_SYNCED]   = "Часът не е синхронизиран",
    [STR_WARN_WIFI_AND_TIME]     = "WiFi не е свързан | Часът не е синхр.",
    [STR_WARN_TIME_STALE]        = "Часът е остарял (последна синхр. преди %lum)",

    /* Schedule view */
    [STR_1ST_SHIFT]           = "1-ва смяна",
    [STR_2ND_SHIFT]           = "2-ра смяна",
    [STR_SHIFT_DISABLED]      = "Смяната е изключена",
    [STR_NO_BELLS_TODAY]      = "Няма звънци днес",
    [STR_CLASS]               = "Час",
    [STR_START]               = "начало",
    [STR_END]                 = "край",
    [STR_SEC_SUFFIX]          = "с",
    [STR_BREAK]               = "Междучасие",

    /* Settings */
    [STR_NETWORK]             = "Мрежа",
    [STR_WIFI_NETWORK]        = "WiFi мрежа",
    [STR_IP_ADDRESS]          = "IP адрес",
    [STR_NOT_CONNECTED]       = "Не е свързан",
    [STR_SCHEDULE]            = "Програма",
    [STR_WORKING_DAYS]        = "Работни дни",
    [STR_TIMEZONE]            = "Часова зона",
    [STR_SYSTEM]              = "Система",
    [STR_NTP_SYNC]            = "NTP синхр.",
    [STR_SYNCED_JUST_NOW]     = "Синхронизиран (току-що)",
    [STR_NOT_SYNCED]          = "Не е синхронизиран",
    [STR_NONE]                = "Няма",
    [STR_LANGUAGE]            = "Език / Language",

    /* Day abbreviations: Sun(0) .. Sat(6) */
    [STR_DAY_SUN]             = "Нед",
    [STR_DAY_MON]             = "Пон",
    [STR_DAY_TUE]             = "Вт",
    [STR_DAY_WED]             = "Ср",
    [STR_DAY_THU]             = "Чет",
    [STR_DAY_FRI]             = "Пет",
    [STR_DAY_SAT]             = "Съб",

    /* Month abbreviations: Jan(0) .. Dec(11) */
    [STR_MON_JAN]             = "Яну",
    [STR_MON_FEB]             = "Фев",
    [STR_MON_MAR]             = "Мар",
    [STR_MON_APR]             = "Апр",
    [STR_MON_MAY]             = "Май",
    [STR_MON_JUN]             = "Юни",
    [STR_MON_JUL]             = "Юли",
    [STR_MON_AUG]             = "Авг",
    [STR_MON_SEP]             = "Сеп",
    [STR_MON_OCT]             = "Окт",
    [STR_MON_NOV]             = "Ное",
    [STR_MON_DEC]             = "Дек",

    /* Info screen */
    [STR_DEVICE]              = "Устройство",
    [STR_FIRMWARE_VERSION]    = "Версия на фърмуера",
    [STR_IDF_VERSION]         = "ESP-IDF версия",
    [STR_CHIP]                = "Чип",
    [STR_MAC_ADDRESS]         = "MAC адрес",
    [STR_WEB_INTERFACE]       = "Уеб интерфейс",
    [STR_SCAN_QR]             = "Сканирайте QR кода за уеб интерфейса",
    [STR_WIFI_NOT_CONNECTED]  = "WiFi не е свързан",
    [STR_ABOUT]               = "Относно",
    [STR_ABOUT_DESC]          = "Автоматичен училищен звънец с\nтъчскрийн и уеб управление.",
    [STR_BOARD_INFO]          = "Платка: Waveshare ESP32-S3-Touch-LCD-4",

    /* PIN entry */
    [STR_ENTER_PIN]           = "Въведете ПИН",
    [STR_ENTER_PIN_SUBTITLE]  = "Въведете 4-цифрен ПИН",
    [STR_WRONG_PIN]           = "Грешен ПИН",
    [STR_CANCEL]              = "Отказ",

    /* WiFi setup */
    [STR_WIFI_SETUP]          = "WiFi настройка",
    [STR_SCAN]                = "Сканирай",
    [STR_SCANNING]            = "Сканиране...",
    [STR_TAP_SCAN]            = "Натиснете Сканирай за търсене",
    [STR_SCAN_FAILED]         = "Сканирането не успя",
    [STR_SCAN_FAILED_RETRY]   = "Сканирането не успя.\nОпитайте отново.",
    [STR_SCANNING_NETWORKS]   = "Сканиране на мрежи...",
    [STR_NO_NETWORKS_SCANNED] = "Няма сканирани мрежи.\nНатиснете Сканирай.",
    [STR_NO_NETWORKS_FOUND]   = "Не са намерени мрежи.\nОпитайте отново.",
    [STR_ENTER_SSID_MANUALLY] = "Въведете SSID ръчно",
    [STR_SKIP]                = "Пропусни",
    [STR_BACK]                = "Назад",
    [STR_CONNECT_TO]          = "Свързване с:",
    [STR_PASSWORD]            = "Парола",
    [STR_ENTER_PASSWORD]      = "Въведете парола",
    [STR_SHOW_PASSWORD]       = "Покажи парола",
    [STR_CONNECT]             = "Свържи",
    [STR_NETWORK_NAME_SSID]   = "Име на мрежа (SSID)",
    [STR_PASSWORD_OR_EMPTY]   = "Парола (или оставете празно)",
    [STR_CONNECTING_TO]       = "Свързване с\n%.32s",
    [STR_PLEASE_WAIT]         = "Моля, изчакайте...",
    [STR_CONNECTED]           = "Свързан!",
    [STR_CONNECTING_BG]       = "Свързване...",
    [STR_LOCKED_FOR]          = "Заключен за %lus",
};

/* ------------------------------------------------------------------ */
/* English strings                                                     */
/* ------------------------------------------------------------------ */
static const char * const s_strings_en[STR_COUNT] = {
    /* Navbar */
    [STR_NAV_DASHBOARD]       = "Dashboard",
    [STR_NAV_SCHEDULE]        = "Schedule",
    [STR_NAV_SETTINGS]        = "Settings",
    [STR_NAV_INFO]            = "Info",

    /* Dashboard: bell status */
    [STR_BELL_STATUS]         = "Bell Status",
    [STR_BELL_IDLE]           = "Idle",
    [STR_BELL_RINGING]        = "Ringing",
    [STR_BELL_PANIC]          = "PANIC",
    [STR_NEXT_BELL]           = "Next Bell",
    [STR_NO_UPCOMING_BELLS]   = "No upcoming bells",

    /* Dashboard: info card */
    [STR_DATE]                = "Date",
    [STR_NO_TIME_SYNC]        = "No time sync",
    [STR_DAY_TYPE]            = "Day Type",
    [STR_DAY_OFF]             = "Day Off",
    [STR_WORKING_DAY]         = "Working Day",
    [STR_HOLIDAY]             = "Holiday",
    [STR_EXCEPTION_WORKING]   = "Exception (Working)",
    [STR_EXCEPTION_OFF]       = "Exception (Off)",
    [STR_UNKNOWN]             = "Unknown",

    /* Dashboard: action buttons */
    [STR_RING]                = "Ring",
    [STR_PANIC]               = "PANIC",
    [STR_STOP]                = "STOP",
    [STR_DAY_OFF_BTN]         = "Day Off",
    [STR_DAY_ON_BTN]          = "Day On",
    [STR_CANCEL_OFF]          = "Cancel Off",
    [STR_CANCEL_ON]           = "Cancel On",

    /* Dashboard: warning banner */
    [STR_WARN_WIFI_DISCONNECTED] = "WiFi disconnected",
    [STR_WARN_TIME_NOT_SYNCED]   = "Time not synchronized",
    [STR_WARN_WIFI_AND_TIME]     = "WiFi disconnected | Time not synced",
    [STR_WARN_TIME_STALE]        = "Time stale (last sync %lum ago)",

    /* Schedule view */
    [STR_1ST_SHIFT]           = "1st Shift",
    [STR_2ND_SHIFT]           = "2nd Shift",
    [STR_SHIFT_DISABLED]      = "Shift disabled",
    [STR_NO_BELLS_TODAY]      = "No bells today",
    [STR_CLASS]               = "Class",
    [STR_START]               = "start",
    [STR_END]                 = "end",
    [STR_SEC_SUFFIX]          = "s",
    [STR_BREAK]               = "Break",

    /* Settings */
    [STR_NETWORK]             = "Network",
    [STR_WIFI_NETWORK]        = "WiFi Network",
    [STR_IP_ADDRESS]          = "IP Address",
    [STR_NOT_CONNECTED]       = "Not connected",
    [STR_SCHEDULE]            = "Schedule",
    [STR_WORKING_DAYS]        = "Working Days",
    [STR_TIMEZONE]            = "Timezone",
    [STR_SYSTEM]              = "System",
    [STR_NTP_SYNC]            = "NTP Sync",
    [STR_SYNCED_JUST_NOW]     = "Synced (just now)",
    [STR_NOT_SYNCED]          = "Not synced",
    [STR_NONE]                = "None",
    [STR_LANGUAGE]            = "Език / Language",

    /* Day abbreviations */
    [STR_DAY_SUN]             = "Sun",
    [STR_DAY_MON]             = "Mon",
    [STR_DAY_TUE]             = "Tue",
    [STR_DAY_WED]             = "Wed",
    [STR_DAY_THU]             = "Thu",
    [STR_DAY_FRI]             = "Fri",
    [STR_DAY_SAT]             = "Sat",

    /* Month abbreviations */
    [STR_MON_JAN]             = "Jan",
    [STR_MON_FEB]             = "Feb",
    [STR_MON_MAR]             = "Mar",
    [STR_MON_APR]             = "Apr",
    [STR_MON_MAY]             = "May",
    [STR_MON_JUN]             = "Jun",
    [STR_MON_JUL]             = "Jul",
    [STR_MON_AUG]             = "Aug",
    [STR_MON_SEP]             = "Sep",
    [STR_MON_OCT]             = "Oct",
    [STR_MON_NOV]             = "Nov",
    [STR_MON_DEC]             = "Dec",

    /* Info screen */
    [STR_DEVICE]              = "Device",
    [STR_FIRMWARE_VERSION]    = "Firmware Version",
    [STR_IDF_VERSION]         = "ESP-IDF Version",
    [STR_CHIP]                = "Chip",
    [STR_MAC_ADDRESS]         = "MAC Address",
    [STR_WEB_INTERFACE]       = "Web Interface",
    [STR_SCAN_QR]             = "Scan QR code to open the web interface",
    [STR_WIFI_NOT_CONNECTED]  = "WiFi not connected",
    [STR_ABOUT]               = "About",
    [STR_ABOUT_DESC]          = "Automated school bell system with\ntouchscreen control and web management.",
    [STR_BOARD_INFO]          = "Board: Waveshare ESP32-S3-Touch-LCD-4",

    /* PIN entry */
    [STR_ENTER_PIN]           = "Enter PIN",
    [STR_ENTER_PIN_SUBTITLE]  = "Enter 4-digit PIN to continue",
    [STR_WRONG_PIN]           = "Wrong PIN",
    [STR_CANCEL]              = "Cancel",

    /* WiFi setup */
    [STR_WIFI_SETUP]          = "WiFi Setup",
    [STR_SCAN]                = "Scan",
    [STR_SCANNING]            = "Scanning...",
    [STR_TAP_SCAN]            = "Tap Scan to find networks",
    [STR_SCAN_FAILED]         = "Scan failed",
    [STR_SCAN_FAILED_RETRY]   = "Scan failed.\nTap Scan to try again.",
    [STR_SCANNING_NETWORKS]   = "Scanning for networks...",
    [STR_NO_NETWORKS_SCANNED] = "No networks scanned yet.\nPress Scan to search.",
    [STR_NO_NETWORKS_FOUND]   = "No networks found.\nTry scanning again.",
    [STR_ENTER_SSID_MANUALLY] = "Enter SSID manually",
    [STR_SKIP]                = "Skip",
    [STR_BACK]                = "Back",
    [STR_CONNECT_TO]          = "Connect to:",
    [STR_PASSWORD]            = "Password",
    [STR_ENTER_PASSWORD]      = "Enter password",
    [STR_SHOW_PASSWORD]       = "Show password",
    [STR_CONNECT]             = "Connect",
    [STR_NETWORK_NAME_SSID]   = "Network Name (SSID)",
    [STR_PASSWORD_OR_EMPTY]   = "Password (or leave empty)",
    [STR_CONNECTING_TO]       = "Connecting to\n%.32s",
    [STR_PLEASE_WAIT]         = "Please wait...",
    [STR_CONNECTED]           = "Connected!",
    [STR_CONNECTING_BG]       = "Connecting in background...",
    [STR_LOCKED_FOR]          = "Locked for %lus",
};

/* ------------------------------------------------------------------ */
/* String table lookup array                                           */
/* ------------------------------------------------------------------ */
static const char * const *s_tables[UI_LANG_COUNT] = {
    [UI_LANG_BG] = s_strings_bg,
    [UI_LANG_EN] = s_strings_en,
};

/* ================================================================== */
/* API implementation                                                  */
/* ================================================================== */

const char *ui_str(ui_string_id_t id)
{
    if (id >= STR_COUNT) return "";
    const char *s = s_tables[s_language][id];
    return s ? s : "";
}

ui_language_t ui_get_language(void)
{
    return s_language;
}

void ui_set_language(ui_language_t lang)
{
    if (lang >= UI_LANG_COUNT) lang = UI_LANG_BG;
    s_language = lang;

    /* Persist to NVS */
    nvs_handle_t h;
    if (NVS_Open("touch_cfg", NVS_READWRITE, &h) == ESP_OK) {
        uint8_t val = (uint8_t)lang;
        NVS_Write(h, "lang", &val, sizeof(val));
        NVS_Commit(h);
        NVS_Close(h);
        ESP_LOGI(TAG, "Language saved: %s", lang == UI_LANG_EN ? "EN" : "BG");
    }
}

void ui_strings_init(void)
{
    nvs_handle_t h;
    if (NVS_Open("touch_cfg", NVS_READONLY, &h) == ESP_OK) {
        uint8_t val = 0;
        size_t len = sizeof(val);
        if (NVS_Read(h, "lang", &val, &len) == ESP_OK && val < UI_LANG_COUNT) {
            s_language = (ui_language_t)val;
            ESP_LOGI(TAG, "Language loaded: %s", val == UI_LANG_EN ? "EN" : "BG");
        } else {
            s_language = UI_LANG_BG;
            ESP_LOGI(TAG, "No saved language, defaulting to BG");
        }
        NVS_Close(h);
    } else {
        s_language = UI_LANG_BG;
        ESP_LOGI(TAG, "NVS open failed, defaulting to BG");
    }
}
