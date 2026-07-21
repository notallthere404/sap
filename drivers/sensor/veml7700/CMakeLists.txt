zephyr_library()

zephyr_library_sources(veml7700.c)
zephyr_library_sources_ifdef(CONFIG_SENSOR_ASYNC_API  veml7700_async.c veml7700_decoder.c)
