cmake_minimum_required(VERSION 3.20.0)

# Register the out-of-tree VEML7700 driver as a Zephyr module so its
# zephyr_library() is collected into ZEPHYR_LIBS during the module sweep,
# before Zephyr snapshots that list for the final link. A plain
# add_subdirectory() after project() registers the library too late.
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/drivers/sensor/veml7700)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sap)

target_sources(app PRIVATE 
    src/main.c 
    src/ble.c 
    src/probe.c
    src/diode.c 
    src/button.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)