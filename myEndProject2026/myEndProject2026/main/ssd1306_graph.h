#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "i2c_bus.h"

esp_err_t ssd1306_graph_init(i2c_bus_handle_t bus);
void ssd1306_graph_deinit(void);
void ssd1306_graph_push_temperature(float temp_c);
bool ssd1306_graph_is_active(void);
