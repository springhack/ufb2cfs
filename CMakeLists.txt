cmake_minimum_required(VERSION 3.16)

set(KLIPPER_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(PROJECT_VER "platformio")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(klipper_esp32_platformio)
