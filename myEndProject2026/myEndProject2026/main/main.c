#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "bme280.h"
#include "ssd1306_graph.h"

#define I2C_MASTER_SDA_IO   CONFIG_BME280_I2C_SDA_GPIO
#define I2C_MASTER_SCL_IO   CONFIG_BME280_I2C_SCL_GPIO
#define I2C_MASTER_NUM      ((i2c_port_t)CONFIG_BME280_I2C_PORT_NUM)
#define I2C_MASTER_FREQ_HZ  CONFIG_BME280_I2C_FREQ_HZ

#define BME280_CONNECT_RETRY_MS  5000
/* Na init/start NORMAL-modus heeft de BME280 tijd nodig voor de eerste X16-meting;
 * te vroeg lezen geeft 0x800000 → ESP_FAIL in de driver. */
#define BME280_POST_INIT_MS      200
#define BME280_FIRST_READ_TRIES  12
#define BME280_FIRST_READ_GAP_MS 50

static const char *TAG = "BME280";

static bme280_handle_t create_bme280_with_auto_address(i2c_bus_handle_t bus, uint8_t *out_addr)
{
    const uint8_t possible_addresses[] = { BME280_I2C_ADDRESS_DEFAULT, 0x77 };

    for (size_t i = 0; i < sizeof(possible_addresses); i++) {
        uint8_t addr = possible_addresses[i];
        ESP_LOGI(TAG, "Trying BME280 address 0x%02X", addr);

        bme280_handle_t sensor = bme280_create(bus, addr);
        if (sensor == NULL) {
            ESP_LOGW(TAG, "bme280_create failed for address 0x%02X", addr);
            continue;
        }

        esp_err_t ret = bme280_default_init(sensor);
        if (ret == ESP_OK) {
            if (out_addr) {
                *out_addr = addr;
            }
            vTaskDelay(pdMS_TO_TICKS(BME280_POST_INIT_MS));
            return sensor;
        }

        ESP_LOGW(TAG, "bme280_default_init failed for 0x%02X: %s", addr, esp_err_to_name(ret));
        bme280_delete(&sensor);
    }

    return NULL;
}

static void log_wiring_checklist_once(void)
{
    static int done;

    if (done) {
        return;
    }
    done = 1;
    ESP_LOGE(TAG, "Geen BME280 op I2C (chip-ID). Controleer hardware:");
    ESP_LOGE(TAG, "  • SDA op GPIO%d, SCL op GPIO%d (of pas menuconfig \"BME280 I2C\" aan)", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    ESP_LOGE(TAG, "  • SDA/SCL niet omgedraaid; gemeenschappelijke GND; 3V3 naar module");
    ESP_LOGE(TAG, "  • 4k7 pull-up SDA+SCL naar 3V3 op breadboard indien nodig");
    ESP_LOGE(TAG, "  • idf.py menuconfig → BME280 I2C: andere GPIO's of 50000 Hz bij slecht contact");
}

void app_main(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Instellingen: SDA=GPIO%d SCL=GPIO%d port=%d %d Hz — wacht op sensor...",
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, (int)I2C_MASTER_NUM, I2C_MASTER_FREQ_HZ);

    for (;;) {
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = I2C_MASTER_FREQ_HZ,
        };

        i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
        if (i2c_bus == NULL) {
            ESP_LOGE(TAG, "I2C bus create failed — opnieuw over %d ms", BME280_CONNECT_RETRY_MS);
            vTaskDelay(pdMS_TO_TICKS(BME280_CONNECT_RETRY_MS));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(100));

        if (ssd1306_graph_init(i2c_bus) != ESP_OK) {
            ESP_LOGW(TAG, "SSD1306 niet gevonden of init mislukt — alleen seriële logging");
        }

        uint8_t used_addr = 0;
        bme280_handle_t bme280 = create_bme280_with_auto_address(i2c_bus, &used_addr);
        if (bme280 == NULL) {
            log_wiring_checklist_once();
            ESP_LOGW(TAG, "Opnieuw proberen over %d s (fix bedrading; herflash niet nodig)", BME280_CONNECT_RETRY_MS / 1000);
            ssd1306_graph_deinit();
            i2c_bus_delete(&i2c_bus);
            vTaskDelay(pdMS_TO_TICKS(BME280_CONNECT_RETRY_MS));
            continue;
        }

        ESP_LOGI(TAG, "BME280 ready at address 0x%02X", used_addr);

        float sanity = 0.0f;
        ret = ESP_FAIL;
        for (int attempt = 0; attempt < BME280_FIRST_READ_TRIES; attempt++) {
            ret = bme280_read_temperature(bme280, &sanity);
            if (ret == ESP_OK) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(BME280_FIRST_READ_GAP_MS));
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Eerste meting na %d pogingen: %s — bus opruimen en opnieuw",
                     BME280_FIRST_READ_TRIES, esp_err_to_name(ret));
            bme280_delete(&bme280);
            ssd1306_graph_deinit();
            i2c_bus_delete(&i2c_bus);
            vTaskDelay(pdMS_TO_TICKS(BME280_CONNECT_RETRY_MS));
            continue;
        }

        ESP_LOGI(TAG, "Sensor OK (eerste meting %.2f °C)", sanity);

        ssd1306_graph_push_temperature(sanity);

        while (1) {
            float temperature = 0.0f;
            float humidity = 0.0f;
            float pressure = 0.0f;

            ret = bme280_read_temperature(bme280, &temperature);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "read_temperature: %s — sensor weg? Herstart detectie.", esp_err_to_name(ret));
                break;
            }
            ret = bme280_read_humidity(bme280, &humidity);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "read_humidity: %s", esp_err_to_name(ret));
            }
            ret = bme280_read_pressure(bme280, &pressure);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "read_pressure: %s", esp_err_to_name(ret));
            }

            ESP_LOGI(TAG, "Temperatuur: %.2f °C, Luchtvochtigheid: %.2f %%, Luchtdruk: %.2f hPa",
                     temperature, humidity, pressure);

            ssd1306_graph_push_temperature(temperature);

            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        bme280_delete(&bme280);
        ssd1306_graph_deinit();
        i2c_bus_delete(&i2c_bus);
        ESP_LOGW(TAG, "Metingen afgebroken — opnieuw verbinden...");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
