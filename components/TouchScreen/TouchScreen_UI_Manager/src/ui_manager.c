#include "ui_manager_internal.h"
#include "ui_theme.h"
#include "ui_statusbar.h"
#include "ui_navbar.h"
#include "ui_strings.h"
#include "ui_config.h"
#include "../screens/splash/splash_screen_internal.h"
#include "../screens/wifi_setup/wifi_setup_screen_internal.h"
#include "../screens/pin_entry/pin_entry_screen_internal.h"
#include "../screens/dashboard/dashboard_screen_internal.h"
#include "../screens/schedule_view/schedule_view_screen_internal.h"
#include "../screens/settings/settings_screen_internal.h"
#include "../screens/info/info_screen_internal.h"
#include "../screens/setup_wizard/setup_wizard_screen_internal.h"
#include "TouchScreen_UI_Manager.h"
#include "TouchScreen_Services.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

#include <stdio.h>
#include <time.h>

static const char *TAG = "TOUCHSCREEN_UI_MGR";

/* Global UI state */
touchscreen_ui_manager_state_t g_ui_state = {
    .current_screen = TOUCHSCREEN_UI_SCREEN_NONE,
    .overlay_screen = TOUCHSCREEN_UI_SCREEN_NONE,
    .wifi_callback = NULL,
    .screen_obj = NULL,
    .statusbar = NULL,
    .navbar = NULL,
    .update_timer = NULL,
    .initialized = false,
};

/* Forward declarations for screen create/destroy wrappers */
static void screen_splash_create(void);
static void screen_splash_destroy(void);
static void screen_wifi_setup_create(void);
static void screen_wifi_setup_destroy(void);
static void screen_wifi_setup_update(void);
static void screen_dashboard_create(void);
static void screen_dashboard_destroy(void);
static void screen_dashboard_update(void);
static void screen_schedule_create(void);
static void screen_schedule_destroy(void);
static void screen_schedule_update(void);
static void screen_settings_create(void);
static void screen_settings_destroy(void);
static void screen_settings_update(void);
static void screen_info_create(void);
static void screen_info_destroy(void);
static void screen_info_update(void);
static void screen_pin_entry_create(void);
static void screen_pin_entry_destroy(void);
static void screen_setup_wizard_create(void);
static void screen_setup_wizard_destroy(void);
static void screen_setup_wizard_update(void);

/* Screen definition registry — indexed by TouchScreen_UI_Screen_t */
static const TouchScreen_Screen_Def_t s_screen_defs[TOUCHSCREEN_UI_SCREEN_MAX] = {
    [TOUCHSCREEN_UI_SCREEN_SPLASH] = {
        .create = screen_splash_create,
        .destroy = screen_splash_destroy,
        .update = NULL,
        .show_chrome = false,
    },
    [TOUCHSCREEN_UI_SCREEN_WIFI_SETUP] = {
        .create = screen_wifi_setup_create,
        .destroy = screen_wifi_setup_destroy,
        .update = screen_wifi_setup_update,
        .show_chrome = false,
    },
    [TOUCHSCREEN_UI_SCREEN_DASHBOARD] = {
        .create = screen_dashboard_create,
        .destroy = screen_dashboard_destroy,
        .update = screen_dashboard_update,
        .show_chrome = true,
    },
    [TOUCHSCREEN_UI_SCREEN_SCHEDULE_VIEW] = {
        .create = screen_schedule_create,
        .destroy = screen_schedule_destroy,
        .update = screen_schedule_update,
        .show_chrome = true,
    },
    [TOUCHSCREEN_UI_SCREEN_SETTINGS] = {
        .create = screen_settings_create,
        .destroy = screen_settings_destroy,
        .update = screen_settings_update,
        .show_chrome = true,
    },
    [TOUCHSCREEN_UI_SCREEN_INFO] = {
        .create = screen_info_create,
        .destroy = screen_info_destroy,
        .update = screen_info_update,
        .show_chrome = true,
    },
    [TOUCHSCREEN_UI_SCREEN_PIN_ENTRY] = {
        .create = screen_pin_entry_create,
        .destroy = screen_pin_entry_destroy,
        .update = NULL,
        .show_chrome = false,  /* Overlay — doesn't need its own chrome */
    },
    [TOUCHSCREEN_UI_SCREEN_SETUP_WIZARD] = {
        .create = screen_setup_wizard_create,
        .destroy = screen_setup_wizard_destroy,
        .update = screen_setup_wizard_update,
        .show_chrome = false,  /* Wizard has its own navigation */
    },
};

/* Idle tracking for brightness dimming */
static bool     s_dimmed = false;

/* Forward declaration for statusbar refresh */
static void ui_manager_refresh_statusbar(void);

/* === Navbar tab callback === */
static void ui_manager_nav_tab_cb(uint8_t tab_index)
{
    if (tab_index == UI_NAV_TAB_DASHBOARD) {
        TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_DASHBOARD);
    } else if (tab_index == UI_NAV_TAB_SCHEDULE) {
        TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SCHEDULE_VIEW);
    } else if (tab_index == UI_NAV_TAB_SETTINGS) {
        TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SETTINGS);
    } else if (tab_index == UI_NAV_TAB_INFO) {
        TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_INFO);
    }
}

/* === Runtime WiFi setup callback (used when WiFi icon is tapped) === */
static void
ui_manager_runtime_wifi_cb(TouchScreen_WiFi_Setup_Result_t result,
                           const char *ssid, const char *password)
{
    if (result == TOUCHSCREEN_WIFI_SETUP_RESULT_SUCCESS && ssid && ssid[0] != '\0') {
        ESP_LOGI(TAG, "Runtime WiFi setup: connecting to '%s'", ssid);
        TS_WiFi_Connect(ssid, password ? password : "", NULL);
    } else {
        ESP_LOGI(TAG, "Runtime WiFi setup cancelled");
        /* Safety net: if the scan disconnected us, reconnect using the
         * existing STA config still stored in the WiFi driver. */
        if (!TS_WiFi_IsConnected()) {
            ESP_LOGI(TAG, "Reconnecting to previous network after cancel");
            esp_wifi_connect();
        }
    }

    /* Always return to dashboard */
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_DASHBOARD);
}

/* === Helper: navigate to WiFi setup (runtime mode) === */
static void
ui_manager_open_wifi_setup_runtime(void)
{
    ESP_LOGI(TAG, "Opening WiFi setup (runtime)");
    g_ui_state.wifi_callback = ui_manager_runtime_wifi_cb;
    g_ui_state.wifi_is_initial_setup = false;  /* Runtime reconfiguration */
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_WIFI_SETUP);
}

/* === PIN callback for WiFi reconfiguration when connected === */
static void
ui_manager_wifi_reconfig_pin_cb(bool success)
{
    if (success) {
        ui_manager_open_wifi_setup_runtime();
    }
    /* If PIN cancelled/failed, stay on current screen */
}

/* === WiFi icon click handler (statusbar) === */
static void
ui_manager_wifi_icon_clicked(lv_event_t *e)
{
    (void)e;

    if (TS_WiFi_IsConnected()) {
        /* Connected — require PIN before allowing reconfiguration */
        ESP_LOGI(TAG, "WiFi icon tapped (connected) — requesting PIN");
        TouchScreen_UI_ShowPinEntry(ui_manager_wifi_reconfig_pin_cb);
    } else {
        /* Disconnected — go directly to WiFi setup (user needs immediate fix) */
        ESP_LOGI(TAG, "WiFi icon tapped (disconnected) — opening WiFi setup");
        ui_manager_open_wifi_setup_runtime();
    }
}

/* === Periodic update timer callback (runs in LVGL context) === */
static void ui_manager_update_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* Refresh status bar icons and clock */
    ui_manager_refresh_statusbar();

    /* Track idle time for brightness dimming */
    lv_display_t *disp = lv_display_get_default();
    uint32_t idle_ms = disp ? lv_display_get_inactive_time(disp) : 0;

    if (!s_dimmed && idle_ms >= TOUCHSCREEN_UI_DIM_TIMEOUT_MS) {
        /* CH32V003 IO expander may NACK first I2C attempt — send twice */
        bsp_display_brightness_set(TOUCHSCREEN_UI_DIM_BRIGHTNESS);
        bsp_display_brightness_set(TOUCHSCREEN_UI_DIM_BRIGHTNESS);
        s_dimmed = true;
    } else if (s_dimmed && idle_ms < TOUCHSCREEN_UI_DIM_TIMEOUT_MS) {
        bsp_display_brightness_set(TOUCHSCREEN_UI_FULL_BRIGHTNESS);
        bsp_display_brightness_set(TOUCHSCREEN_UI_FULL_BRIGHTNESS);
        s_dimmed = false;
    }

    /* Call the active screen's update */
    TouchScreen_UI_Screen_t active = g_ui_state.current_screen;
    if (active >= 0 && active < TOUCHSCREEN_UI_SCREEN_MAX) {
        if (s_screen_defs[active].update) {
            s_screen_defs[active].update();
        }
    }
}

/* === Chrome (status bar + navbar) management === */
static void ui_manager_show_chrome(lv_obj_t *scr)
{
    bsp_display_lock(0);
    g_ui_state.statusbar = ui_statusbar_create(scr);
    g_ui_state.navbar = ui_navbar_create(scr, ui_manager_nav_tab_cb);

    /* Register WiFi icon click handler — opens WiFi setup when disconnected */
    ui_statusbar_set_wifi_click_cb(ui_manager_wifi_icon_clicked, NULL);

    /* Highlight active nav tab */
    if (g_ui_state.current_screen == TOUCHSCREEN_UI_SCREEN_SCHEDULE_VIEW) {
        ui_navbar_set_active(UI_NAV_TAB_SCHEDULE);
    } else if (g_ui_state.current_screen == TOUCHSCREEN_UI_SCREEN_SETTINGS) {
        ui_navbar_set_active(UI_NAV_TAB_SETTINGS);
    } else if (g_ui_state.current_screen == TOUCHSCREEN_UI_SCREEN_INFO) {
        ui_navbar_set_active(UI_NAV_TAB_INFO);
    } else {
        ui_navbar_set_active(UI_NAV_TAB_DASHBOARD);
    }
    bsp_display_unlock();
}

/* === Public API === */

bool TouchScreen_UI_ManagerInit(void)
{
    ESP_LOGI(TAG, "Initializing UI Manager (LVGL 9, Material theme)");

    /* Load saved language preference from NVS before any UI is created */
    ui_strings_init();

    bsp_display_lock(0);
    ui_theme_init();
    bsp_display_unlock();

    /* Create periodic update timer (runs in LVGL thread context) */
    bsp_display_lock(0);
    g_ui_state.update_timer = lv_timer_create(ui_manager_update_timer_cb,
                                               TOUCHSCREEN_UI_UPDATE_PERIOD_MS, NULL);
    if (g_ui_state.update_timer) {
        lv_timer_pause(g_ui_state.update_timer);  /* Start paused; resume when dashboard is shown */
    }
    bsp_display_unlock();

    g_ui_state.current_screen = TOUCHSCREEN_UI_SCREEN_NONE;
    g_ui_state.overlay_screen = TOUCHSCREEN_UI_SCREEN_NONE;
    g_ui_state.initialized = true;

    ESP_LOGI(TAG, "UI Manager initialized");
    return true;
}

void TouchScreen_UI_ManagerDeinit(void)
{
    ESP_LOGI(TAG, "Deinitializing UI Manager");

    if (g_ui_state.update_timer) {
        bsp_display_lock(0);
        lv_timer_delete(g_ui_state.update_timer);
        bsp_display_unlock();
        g_ui_state.update_timer = NULL;
    }

    g_ui_state.initialized = false;
}

void TouchScreen_UI_NavigateTo(TouchScreen_UI_Screen_t screen)
{
    if (screen < 0 || screen >= TOUCHSCREEN_UI_SCREEN_MAX) {
        ESP_LOGE(TAG, "Invalid screen ID: %d", screen);
        return;
    }

    TouchScreen_UI_Screen_t prev_screen = g_ui_state.current_screen;
    ESP_LOGI(TAG, "Navigating from screen %d to %d", prev_screen, screen);

    /* Pause the update timer immediately to prevent callbacks during transition */
    if (g_ui_state.update_timer) {
        bsp_display_lock(0);
        lv_timer_pause(g_ui_state.update_timer);
        bsp_display_unlock();
    }

    /* Destroy current screen's logical state (widgets will be deleted with old screen obj) */
    if (prev_screen >= 0 && prev_screen < TOUCHSCREEN_UI_SCREEN_MAX) {
        if (s_screen_defs[prev_screen].destroy) {
            s_screen_defs[prev_screen].destroy();
        }
    }

    /* Dismiss any overlay */
    if (g_ui_state.overlay_screen != TOUCHSCREEN_UI_SCREEN_NONE) {
        if (s_screen_defs[g_ui_state.overlay_screen].destroy) {
            s_screen_defs[g_ui_state.overlay_screen].destroy();
        }
        g_ui_state.overlay_screen = TOUCHSCREEN_UI_SCREEN_NONE;
    }

    /* Clear chrome pointers (they belonged to old screen and will be auto-deleted) */
    g_ui_state.statusbar = NULL;
    g_ui_state.navbar = NULL;

    /* Create a fresh screen object — BEFORE loading it, so widgets are added first.
     * lv_screen_load_anim does NOT make the screen active immediately; the switch
     * happens in its start_cb on the next timer tick.  If we called lv_scr_act()
     * between lv_screen_load_anim and that tick we would get the OLD screen,
     * causing all widgets to be created on the wrong screen and then deleted by
     * auto_del.  To avoid this, we store new_scr in g_ui_state.screen_obj and
     * have create functions use that instead of lv_scr_act(). */
    bsp_display_lock(0);
    lv_obj_t *new_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(new_scr, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(new_scr, LV_OPA_COVER, 0);
    bsp_display_unlock();

    /* Publish the new screen object and screen ID BEFORE create, so create
     * functions (and any helper they call) can find the correct parent. */
    g_ui_state.screen_obj = new_scr;
    g_ui_state.current_screen = screen;

    /* Create screen content on the new (not-yet-loaded) screen */
    if (s_screen_defs[screen].create) {
        s_screen_defs[screen].create();
    }

    /* Show chrome if needed */
    if (s_screen_defs[screen].show_chrome) {
        ui_manager_show_chrome(new_scr);
    }

    /* NOW load the fully-populated screen with the transition animation */
    bsp_display_lock(0);
    if (prev_screen == TOUCHSCREEN_UI_SCREEN_NONE || prev_screen == TOUCHSCREEN_UI_SCREEN_SPLASH) {
        lv_screen_load(new_scr);
    } else {
        lv_screen_load_anim(new_scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, true);
    }
    bsp_display_unlock();

    /* Resume/pause periodic timer based on whether screen has an update callback */
    if (g_ui_state.update_timer) {
        bsp_display_lock(0);
        if (s_screen_defs[screen].update) {
            lv_timer_resume(g_ui_state.update_timer);
        }
        /* else: already paused at the top of this function */
        bsp_display_unlock();
    }
}

void TouchScreen_UI_PushOverlay(TouchScreen_UI_Screen_t screen)
{
    if (screen < 0 || screen >= TOUCHSCREEN_UI_SCREEN_MAX) return;

    ESP_LOGI(TAG, "Pushing overlay screen %d on top of %d", screen, g_ui_state.current_screen);

    g_ui_state.overlay_screen = screen;

    /* Create overlay on top of current screen (does NOT clean the screen) */
    if (s_screen_defs[screen].create) {
        s_screen_defs[screen].create();
    }
}

void TouchScreen_UI_PopOverlay(void)
{
    if (g_ui_state.overlay_screen == TOUCHSCREEN_UI_SCREEN_NONE) return;

    ESP_LOGI(TAG, "Popping overlay screen %d", g_ui_state.overlay_screen);

    if (s_screen_defs[g_ui_state.overlay_screen].destroy) {
        s_screen_defs[g_ui_state.overlay_screen].destroy();
    }

    g_ui_state.overlay_screen = TOUCHSCREEN_UI_SCREEN_NONE;
}

TouchScreen_UI_Screen_t TouchScreen_UI_GetCurrentScreen(void)
{
    return g_ui_state.current_screen;
}

void TouchScreen_UI_Update(void)
{
    TouchScreen_UI_Screen_t active = g_ui_state.current_screen;
    if (active >= 0 && active < TOUCHSCREEN_UI_SCREEN_MAX) {
        if (s_screen_defs[active].update) {
            s_screen_defs[active].update();
        }
    }
}

/* === Legacy convenience wrappers === */

void TouchScreen_UI_ShowSplash(uint32_t duration_ms)
{
    ESP_LOGI(TAG, "Showing splash screen for %lu ms", (unsigned long)duration_ms);
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SPLASH);

    /* Block for the duration (splash is a timed screen) */
    uint32_t remaining = duration_ms;
    const uint32_t chunk = 100;
    while (remaining > 0) {
        uint32_t delay = (remaining > chunk) ? chunk : remaining;
        vTaskDelay(pdMS_TO_TICKS(delay));
        remaining -= delay;
    }
}

void TouchScreen_UI_ShowWiFiSetup(TouchScreen_WiFi_Setup_Callback_t callback)
{
    ESP_LOGI(TAG, "Showing WiFi setup screen");
    g_ui_state.wifi_callback = callback;
    g_ui_state.wifi_is_initial_setup = true;  /* Boot-time initial setup */
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_WIFI_SETUP);
}

void TouchScreen_UI_ShowDashboard(void)
{
    ESP_LOGI(TAG, "Showing dashboard");
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_DASHBOARD);
}

void TouchScreen_UI_ShowPinEntry(TouchScreen_PIN_Result_Callback_t callback)
{
    ESP_LOGI(TAG, "Showing PIN entry overlay");
    g_ui_state.pin_callback = callback;
    TouchScreen_UI_PushOverlay(TOUCHSCREEN_UI_SCREEN_PIN_ENTRY);
}

void TouchScreen_UI_ShowSetupWizard(TouchScreen_Setup_Wizard_Callback_t callback)
{
    ESP_LOGI(TAG, "Showing setup wizard");
    g_ui_state.wizard_callback = callback;
    TouchScreen_UI_NavigateTo(TOUCHSCREEN_UI_SCREEN_SETUP_WIZARD);
}

/* =========================================================================
 * Screen create/destroy wrappers
 * These bridge between the registry pattern and the actual screen implementations.
 * Screens that are not yet implemented have stub wrappers.
 * ========================================================================= */

static void screen_splash_create(void)
{
    touchscreen_splash_screen_create(TOUCHSCREEN_UI_SPLASH_DEFAULT_DURATION);
}

static void screen_splash_destroy(void)
{
    touchscreen_splash_screen_destroy();
}

static void screen_wifi_setup_create(void)
{
    touchscreen_wifi_setup_screen_create(g_ui_state.wifi_callback, g_ui_state.wifi_is_initial_setup);
}

static void screen_wifi_setup_destroy(void)
{
    touchscreen_wifi_setup_screen_destroy();
}

static void screen_wifi_setup_update(void)
{
    touchscreen_wifi_setup_screen_update();
}

/* === Dashboard === */
static void screen_dashboard_create(void)
{
    touchscreen_dashboard_screen_create();
}

static void screen_dashboard_destroy(void)
{
    touchscreen_dashboard_screen_destroy();
}

static void screen_dashboard_update(void)
{
    touchscreen_dashboard_screen_update();
}

/* === Schedule View === */
static void screen_schedule_create(void)
{
    touchscreen_schedule_view_screen_create();
}

static void screen_schedule_destroy(void)
{
    touchscreen_schedule_view_screen_destroy();
}

static void screen_schedule_update(void)
{
    touchscreen_schedule_view_screen_update();
}

/* === Settings === */
static void screen_settings_create(void)
{
    touchscreen_settings_screen_create();
}

static void screen_settings_destroy(void)
{
    touchscreen_settings_screen_destroy();
}

static void screen_settings_update(void)
{
    touchscreen_settings_screen_update();
}

/* === Info / About === */
static void screen_info_create(void)
{
    touchscreen_info_screen_create();
}

static void screen_info_destroy(void)
{
    touchscreen_info_screen_destroy();
}

static void screen_info_update(void)
{
    touchscreen_info_screen_update();
}

/* === PIN Entry overlay === */
static void screen_pin_entry_create(void)
{
    touchscreen_pin_entry_screen_create(g_ui_state.pin_callback);
}

static void screen_pin_entry_destroy(void)
{
    touchscreen_pin_entry_screen_destroy();
}

/* === Setup Wizard === */
static void screen_setup_wizard_create(void)
{
    touchscreen_setup_wizard_screen_create();
}

static void screen_setup_wizard_destroy(void)
{
    touchscreen_setup_wizard_screen_destroy();
}

static void screen_setup_wizard_update(void)
{
    touchscreen_setup_wizard_screen_update();
}

/* =========================================================================
 * Statusbar live refresh — called every 1s from the update timer
 * ========================================================================= */
static void ui_manager_refresh_statusbar(void)
{
    /* Only refresh if chrome (statusbar) is visible */
    if (!g_ui_state.statusbar) return;

    /* Clock — from scheduler status which wraps TimeSync */
    SCHEDULER_STATUS_T tStatus = {0};
    esp_err_t err = TS_Schedule_GetStatus(&tStatus);
    if (err == ESP_OK && tStatus.bTimeSynced) {
        char time_buf[16];
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d",
                 tStatus.tCurrentTime.tm_hour,
                 tStatus.tCurrentTime.tm_min,
                 tStatus.tCurrentTime.tm_sec);
        ui_statusbar_update_time(time_buf);
        ui_statusbar_set_ntp_synced(true);
    } else if (TS_WiFi_IsConnected()) {
        ui_statusbar_update_time("Syncing...");
        ui_statusbar_set_ntp_synced(false);
    } else {
        ui_statusbar_update_time("--:--:--");
        ui_statusbar_set_ntp_synced(false);
    }

    /* WiFi connected status */
    ui_statusbar_set_wifi_connected(TS_WiFi_IsConnected());

    /* Bell state (idle / ringing / panic) */
    BELL_STATE_E bell_state = TS_Bell_GetState();
    ui_statusbar_set_bell_state((uint8_t)bell_state);
}
