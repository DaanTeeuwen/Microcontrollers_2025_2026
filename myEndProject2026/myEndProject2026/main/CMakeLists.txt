idf_component_register(SRCS "main.c" "ssd1306_graph.c"
                    INCLUDE_DIRS "."
                    REQUIRES espressif__bme280 driver)
