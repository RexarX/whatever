idf_component_register(
    SRCS "servo_controller.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer driver freertos
)
