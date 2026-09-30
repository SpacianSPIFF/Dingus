idf_component_register(SRCS "main.c" "espnow_recv.c" "servo.c"
                        INCLUDE_DIRS "."
                        REQUIRES esp_wifi esp_timer esp_driver_ledc turret_common)