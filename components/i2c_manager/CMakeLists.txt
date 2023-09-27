set(SOURCES "i2c_manager/i2c_manager.c")
set(INCLUDES "i2c_manager")

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDES}
    REQUIRES driver
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
