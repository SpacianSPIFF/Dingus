idf_component_register(SRCS "espnow_common.c"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_wifi esp_event nvs_flash)