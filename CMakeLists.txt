if(CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY OR CONFIG_ZMK_INPUT_PROCESSOR_MOUSE_ACCEL)
  zephyr_library()
  zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()

if(CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY)
  zephyr_library_sources(src/behaviors/behavior_clk_or_key.c)
  zephyr_library_sources(src/typing_mode.c)
endif()

if(CONFIG_ZMK_INPUT_PROCESSOR_MOUSE_ACCEL)
  zephyr_library_sources(src/input_processors/input_processor_mouse_accel.c)
  if(NOT CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY)
    # mouse_accel uses typing_mode.c symbols
    zephyr_library_sources(src/typing_mode.c)
  endif()
endif()
