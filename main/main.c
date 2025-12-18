#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "appfs.h"
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/i2c.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "bsp/rtc.h"
#include "chakrapetchmedium.h"
#include "common/display.h"
#include "common/theme.h"
#include "custom_certificates.h"
#include "device_settings.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "gui_element_footer.h"
#include "gui_element_header.h"
#include "gui_menu.h"
#include "gui_style.h"
#include "hal/lcd_types.h"
#include "icons.h"
#include "ntp.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "portmacro.h"
#include "sdcard.h"
#include "sdkconfig.h"
#include "timezone.h"
#include "wifi_connection.h"
#include "wifi_remote.h"
#include "menu_ssh.h"

#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#endif

// Constants
static char const TAG[] = "tanmatsu-ssh";

// Global variables
static QueueHandle_t input_event_queue      = NULL;
static wl_handle_t   wl_handle              = WL_INVALID_HANDLE;
static bool          wifi_stack_initialized = false;
static bool          wifi_stack_task_done   = false;

static void fix_rtc_out_of_bounds(void) {
    time_t rtc_time = time(NULL);

    bool adjust = false;

    if (rtc_time < 1735689600) {  // 2025-01-01 00:00:00 UTC
        rtc_time = 1735689600;
        adjust   = true;
    }

    if (rtc_time > 4102441200) {  // 2100-01-01 00:00:00 UTC
        rtc_time = 4102441200;
        adjust   = true;
    }

    if (adjust) {
        struct timeval rtc_timeval = {
            .tv_sec  = rtc_time,
            .tv_usec = 0,
        };

        settimeofday(&rtc_timeval, NULL);
        bsp_rtc_set_time(rtc_time);
    }
}

void startup_screen(const char* text) {
    gui_theme_t* theme = get_theme();
    pax_buf_t*   fb    = display_get_buffer();
    pax_background(fb, theme->palette.color_background);
    gui_header_draw(fb, theme, ((gui_element_icontext_t[]){{NULL, (char*)text}}), 1, NULL, 0);
    gui_footer_draw(fb, theme, NULL, 0, NULL, 0);
    display_blit_buffer(fb);
}

bool wifi_stack_get_initialized(void) {
    return wifi_stack_initialized;
}

bool wifi_stack_get_task_done(void) {
    return wifi_stack_task_done;
}

static void wifi_task(void* pvParameters) {
    if (wifi_remote_initialize() == ESP_OK) {
        wifi_connection_init_stack();
        wifi_stack_initialized = true;
        // wifi_connect_try_all();
    } else {
        // bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_OFF);
        ESP_LOGE(TAG, "WiFi radio not responding, did you flash ESP-HOSTED firmware?");
    }
    wifi_stack_task_done = true;

    if (ntp_get_enabled() && wifi_stack_initialized) {
        if (wifi_connect_try_all() == ESP_OK) {
            esp_err_t res = ntp_start_service("pool.ntp.org");
            if (res == ESP_OK) {
                res = ntp_sync_wait();
                if (res != ESP_OK) {
                    ESP_LOGW(TAG, "NTP time sync failed: %s", esp_err_to_name(res));
                } else {
                    time_t rtc_time = time(NULL);
                    bsp_rtc_set_time(rtc_time);
                    ESP_LOGI(TAG, "NTP time sync succesful, RTC updated");
                }
            } else {
                ESP_LOGE(TAG, "Failed to initialize NTP service: %s", esp_err_to_name(res));
            }
        } else {
            ESP_LOGW(TAG, "Could not connect to network for NTP");
        }
    }

    while (1) {
        printf("free:%lu min-free:%lu lfb-dma:%u lfb-def:%u lfb-8bit:%u\n", esp_get_free_heap_size(),
               esp_get_minimum_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_DMA),
               heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    vTaskDelete(NULL);
}

void app_main(void) {
    gui_theme_t* theme = get_theme();
    pax_buf_t*   fb    = display_get_buffer();

    // Initialize the Non Volatile Storage service
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        res = nvs_flash_init();
    }
    ESP_ERROR_CHECK(res);

    // Initialize the Board Support Package
    const bsp_configuration_t bsp_configuration = {
        .display =
            {
                .requested_color_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
                .num_fbs                = 1,
            },
    };
    esp_err_t bsp_init_result = bsp_device_initialize(&bsp_configuration);

    if (bsp_init_result == ESP_OK) {
        ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));
        bsp_display_set_backlight_brightness(100);
        display_init();
    } else {
        ESP_LOGE(TAG, "Failed to initialize BSP, bailing out.");
        return;
    }

    // Apply settings
    startup_screen("Applying settings...");
    device_settings_apply();

    // Configure LEDs
    bsp_led_clear();
    bsp_led_set_mode(true);

    // Initialize filesystems
    startup_screen("Mounting FAT filesystem...");

    esp_vfs_fat_mount_config_t fat_mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = 10,
        .allocation_unit_size     = CONFIG_WL_SECTOR_SIZE,
        .disk_status_check_enable = false,
        .use_one_fat              = false,
    };

    res = esp_vfs_fat_spiflash_mount_rw_wl("/int", "locfd", &fat_mount_config, &wl_handle);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FAT filesystem: %s", esp_err_to_name(res));
        startup_screen("Error: Failed to mount FAT filesystem");
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 0, "Failed to initialize FAT filesystem");
        display_blit_buffer(buffer);
        return;
    }

    startup_screen("Mounting AppFS filesystem...");
    res = appfsInit(APPFS_PART_TYPE, APPFS_PART_SUBTYPE);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppFS: %s", esp_err_to_name(res));
        pax_buf_t* buffer = display_get_buffer();
        pax_background(buffer, 0xFFFF0000);
        pax_draw_text(buffer, 0xFFFFFFFF, pax_font_sky_mono, 16, 0, 0, "Failed to initialize app filesystem");
        display_blit_buffer(buffer);
        return;
    }

    bsp_rtc_update_time();
    if (timezone_nvs_apply("system", "timezone") != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply timezone, setting timezone to Etc/UTC");
        const timezone_t* zone = NULL;
        res                    = timezone_get_name("Etc/UTC", &zone);
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Timezone Etc/UTC not found");  // Should never happen
        } else {
            if (timezone_nvs_set("system", "timezone", zone->name) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save timezone to NVS");
            }
            if (timezone_nvs_set_tzstring("system", "tz", zone->tz) != ESP_OK) {
                ESP_LOGE(TAG, "Failed to save TZ string to NVS");
            }
        }
        timezone_apply_timezone(zone);
    }
    fix_rtc_out_of_bounds();

    bool sdcard_inserted = false;
    bsp_input_read_action(BSP_INPUT_ACTION_TYPE_SD_CARD, &sdcard_inserted);

    if (sdcard_inserted) {
        printf("SD card detected\r\n");
#if defined(CONFIG_BSP_TARGET_TANMATSU) || defined(CONFIG_BSP_TARGET_KONSOOL) || \
    defined(CONFIG_BSP_TARGET_HACKERHOTEL_2026)
        sd_pwr_ctrl_handle_t sd_pwr_handle = initialize_sd_ldo();
        sd_mount_spi(sd_pwr_handle);
        test_sd();
#endif
    }

    ESP_ERROR_CHECK(initialize_custom_ca_store());

    xTaskCreatePinnedToCore(wifi_task, TAG, 4096, NULL, 10, NULL, CONFIG_SOC_CPU_CORES_NUM - 1);

    load_icons();

    //bsp_power_set_usb_host_boost_enabled(true); -- might want to keep this?

    menu_ssh(fb, theme);
}
