idf_component_register(
    SRCS "compile_time_request.c"
    INCLUDE_DIRS
        "${KLIPPER_ROOT}/include"
        "${KLIPPER_ROOT}/klipper_src/src"
        "${KLIPPER_ROOT}/klipper_src/src/esp32"
        "${KLIPPER_ROOT}/klipper_src/src/generic"
    LDFRAGMENTS
        "${CMAKE_CURRENT_LIST_DIR}/noflash.lf"
)
