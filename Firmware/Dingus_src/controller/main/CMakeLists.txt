idf_component_register(SRCS "main.c" "i2c_bus.c" "imu_mpu6050.c" "oled.c" "buttons.c" "gyro_cal.c" "orientation.c" "espnow_send.c"
                        INCLUDE_DIRS "."
                        REQUIRES esp_driver_i2c esp_driver_gpio esp_timer esp_wifi turret_common)