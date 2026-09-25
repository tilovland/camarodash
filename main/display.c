#include "display.h"

#include <assert.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_rom_sys.h"
#include "lvgl.h"

#define LCD_H_RES              800
#define LCD_V_RES              480
#define LCD_PIXEL_CLOCK_HZ     (16 * 1000 * 1000)
#define LCD_BOUNCE_BUFFER_SIZE (LCD_H_RES * 10 * 16 / 8)

#define I2C_PORT               I2C_NUM_0
#define I2C_SDA_GPIO           GPIO_NUM_8
#define I2C_SCL_GPIO           GPIO_NUM_9
#define I2C_TIMEOUT_MS         1000
#define CH422G_MODE_ADDRESS    0x24
#define CH422G_OUTPUT_ADDRESS  0x38
#define CH422G_OUTPUT_MODE     0x01
#define CH422G_LCD_ON          0x1E
#define CH422G_TOUCH_RESET     0x2C
#define CH422G_TOUCH_RUN       0x2E
#define TOUCH_ADDRESS_GPIO     GPIO_NUM_4

static const char *TAG = "display";

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_ch422g_mode_dev;
static i2c_master_dev_handle_t s_ch422g_output_dev;

static esp_err_t i2c_init(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_i2c_bus), TAG,
                        "I2C bus init failed");

    const i2c_device_config_t mode_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_MODE_ADDRESS,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &mode_dev_config,
                                                  &s_ch422g_mode_dev),
                        TAG, "CH422G mode device add failed");

    const i2c_device_config_t output_dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CH422G_OUTPUT_ADDRESS,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(s_i2c_bus, &output_dev_config,
                                     &s_ch422g_output_dev);
}

static esp_err_t ch422g_write(i2c_master_dev_handle_t dev, uint8_t value)
{
    return i2c_master_transmit(dev, &value, 1, I2C_TIMEOUT_MS);
}

static esp_err_t backlight_on(void)
{
    ESP_RETURN_ON_ERROR(ch422g_write(s_ch422g_mode_dev, CH422G_OUTPUT_MODE),
                        TAG, "CH422G output mode failed");
    return ch422g_write(s_ch422g_output_dev, CH422G_LCD_ON);
}

static esp_err_t touch_init(esp_lcd_touch_handle_t *touch)
{
    const gpio_config_t address_gpio_config = {
        .pin_bit_mask = 1ULL << TOUCH_ADDRESS_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&address_gpio_config), TAG,
                        "Touch address GPIO config failed");
    ESP_RETURN_ON_ERROR(ch422g_write(s_ch422g_mode_dev, CH422G_OUTPUT_MODE),
                        TAG, "CH422G output mode failed");

    /* Waveshare/GT911 address-selection and reset sequence. */
    ESP_RETURN_ON_ERROR(ch422g_write(s_ch422g_output_dev, CH422G_TOUCH_RESET),
                        TAG, "Touch reset assert failed");
    esp_rom_delay_us(100 * 1000);
    ESP_RETURN_ON_ERROR(gpio_set_level(TOUCH_ADDRESS_GPIO, 0), TAG,
                        "Touch address select failed");
    esp_rom_delay_us(100 * 1000);
    ESP_RETURN_ON_ERROR(ch422g_write(s_ch422g_output_dev, CH422G_TOUCH_RUN),
                        TAG, "Touch reset release failed");
    esp_rom_delay_us(200 * 1000);

    esp_lcd_panel_io_handle_t touch_io = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_config, &touch_io),
                        TAG, "GT911 I2C IO init failed");

    const esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    return esp_lcd_touch_new_i2c_gt911(touch_io, &touch_config, touch);
}

esp_err_t display_init(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_touch_handle_t touch = NULL;
    const esp_lv_adapter_tear_avoid_mode_t tear_mode =
        ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB;
    const esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    const esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = LCD_PIXEL_CLOCK_HZ,
            .h_res = LCD_H_RES,
            .v_res = LCD_V_RES,
            .hsync_pulse_width = 4,
            .hsync_back_porch = 8,
            .hsync_front_porch = 8,
            .vsync_pulse_width = 4,
            .vsync_back_porch = 8,
            .vsync_front_porch = 8,
            .flags = {
                .pclk_active_neg = 1,
            },
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = esp_lv_adapter_get_required_frame_buffer_count(tear_mode,
                                                                   rotation),
        .bounce_buffer_size_px = LCD_BOUNCE_BUFFER_SIZE,
        .dma_burst_size = 64,
        .hsync_gpio_num = GPIO_NUM_46,
        .vsync_gpio_num = GPIO_NUM_3,
        .de_gpio_num = GPIO_NUM_5,
        .pclk_gpio_num = GPIO_NUM_7,
        .disp_gpio_num = GPIO_NUM_NC,
        .data_gpio_nums = {
            GPIO_NUM_14, GPIO_NUM_38, GPIO_NUM_18, GPIO_NUM_17,
            GPIO_NUM_10, GPIO_NUM_39, GPIO_NUM_0,  GPIO_NUM_45,
            GPIO_NUM_48, GPIO_NUM_47, GPIO_NUM_21, GPIO_NUM_1,
            GPIO_NUM_2,  GPIO_NUM_42, GPIO_NUM_41, GPIO_NUM_40,
        },
        .flags = {
            .fb_in_psram = 1,
        },
    };

    ESP_LOGI(TAG, "Initializing 800x480 RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &panel), TAG,
                        "RGB panel allocation failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "RGB panel init failed");
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "Shared I2C init failed");
    ESP_RETURN_ON_ERROR(touch_init(&touch), TAG, "GT911 touch init failed");
    ESP_RETURN_ON_ERROR(backlight_on(), TAG, "Backlight init failed");

    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = 12 * 1024;
    adapter_config.stack_in_psram = true;
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter_config), TAG,
                        "LVGL adapter init failed");

    esp_lv_adapter_display_config_t display_config =
        ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(panel, NULL, LCD_H_RES,
                                                   LCD_V_RES, rotation);
    display_config.profile.use_psram = true;
    lv_display_t *lv_display = esp_lv_adapter_register_display(&display_config);
    assert(lv_display != NULL);

    esp_lv_adapter_touch_config_t touch_config =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(lv_display, touch);
    assert(esp_lv_adapter_register_touch(&touch_config) != NULL);
    return esp_lv_adapter_start();
}
