// for wi-fi
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "secrets.h"
// for time
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include <time.h>
// for ssd1306
#include "font8x8_basic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1306.h"

#define TAG "wifi_time"
#define TAG1 "SSD1306"

void init_i2c_if_needed(SSD1306_t &dev) {
#if CONFIG_I2C_INTERFACE
    ESP_LOGI(TAG1, "INTERFACE is i2c");
    ESP_LOGI(TAG1, "CONFIG_SDA_GPIO=%d", CONFIG_SDA_GPIO);
    ESP_LOGI(TAG1, "CONFIG_SCL_GPIO=%d", CONFIG_SCL_GPIO);
    ESP_LOGI(TAG1, "CONFIG_RESET_GPIO=%d", CONFIG_RESET_GPIO);

    i2c_master_init(&dev, CONFIG_SDA_GPIO, CONFIG_SCL_GPIO, CONFIG_RESET_GPIO);
#endif // CONFIG_I2C_INTERFACE
}

void wifi_connect() {
    // Инициализация NVS
    ESP_ERROR_CHECK(nvs_flash_init());
    // Инициализация сетевого стека
    ESP_ERROR_CHECK(esp_netif_init());
    // Цикл обработки событий
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Создание wi-fi STA интерфейса
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    esp_wifi_connect();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void on_got_ip(void *, esp_event_base_t, int32_t, void *) {
    // Настройка SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.google.com");
    esp_sntp_init();
    // часовой пояс
    setenv("TZ", "MSK-3", 1);
    tzset();
}

void setup_time() {

    // прочитать время
    time_t now;
    struct tm t;
    time(&now);
    localtime_r(&now, &t);

    // преобразование timestamp в строку
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &t);
    ESP_LOGI(TAG, "Local time: %s", buf);
}

extern "C" void app_main() {
    wifi_connect();
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, nullptr,
                                        nullptr);

    SSD1306_t dev;

    init_i2c_if_needed(dev);
    ssd1306_init(&dev, 128, 64);
    char lineChar[20];

    static int prev_sec = -1, prev_yday = -1;

    ssd1306_clear_screen(&dev, false);
    while (true) {
        time_t now;
        struct tm t{};
        time(&now);
        localtime_r(&now, &t);

        if (t.tm_sec != prev_sec) {
            char s[17];
            snprintf(s, sizeof(s), "%02d:%02d:%02d      ", t.tm_hour, t.tm_min, t.tm_sec);
            ssd1306_display_text(&dev, 4, s, 16, false);
            prev_sec = t.tm_sec;
        }

        if (t.tm_mday != prev_yday) {
            char d[33];
            snprintf(d, sizeof(d), "%02d.%02d.%04d      ", t.tm_mday, t.tm_mon + 1,
                     t.tm_year + 1900);
            ssd1306_display_text(&dev, 2, d, 16, false);
            prev_yday = t.tm_yday;
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}