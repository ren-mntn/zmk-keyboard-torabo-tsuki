if(CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY OR CONFIG_ZMK_INPUT_PROCESSOR_MOUSE_ACCEL OR CONFIG_ZMK_INPUT_PROCESSOR_SCROLL_MODE)
  zephyr_library()
  zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
  # typing_mode.c holds the shared global state used by the click, accel, and scroll modules.
  zephyr_library_sources(src/typing_mode.c)
endif()

if(CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY)
  zephyr_library_sources(src/behaviors/behavior_clk_or_key.c)
endif()

if(CONFIG_ZMK_INPUT_PROCESSOR_MOUSE_ACCEL)
  zephyr_library_sources_ifdef(CONFIG_ZMK_POINTING src/input_processors/input_processor_mouse_accel.c)
endif()

if(CONFIG_ZMK_INPUT_PROCESSOR_SCROLL_MODE)
  zephyr_library_sources_ifdef(CONFIG_ZMK_POINTING src/input_processors/input_processor_scroll_mode.c)
endif()
