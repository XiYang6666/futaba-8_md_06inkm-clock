file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")

idf_component_register(
        SRCS ${SOURCES}
        INCLUDE_DIRS "src"
        REQUIRES ftb-8-md esp_timer esp_driver_gpio nvs_flash esp_wifi esp_netif esp_event esp_http_server spiffs dns_server lwip
)

spiffs_create_partition_image(storage fs FLASH_IN_PROJECT)