if(CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY
   OR CONFIG_ZMK_BEHAVIOR_BT_TOGGLE_01
   OR CONFIG_ZMK_INPUT_PROCESSOR_MOUSE_ACCEL
   OR CONFIG_ZMK_INPUT_PROCESSOR_SCROLL_MODE)
  zephyr_library()
  zephyr_library_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
  # typing_mode.c holds the shared global state used by the click, accel, and scroll modules.
  zephyr_library_sources(src/typing_mode.c)

  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_CLK_OR_KEY
    src/behaviors/behavior_clk_or_key.c)

  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_BT_TOGGLE_01
    src/behaviors/behavior_bt_toggle_01.c)

  if(CONFIG_ZMK_POINTING)
    zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_MOUSE_ACCEL
      src/input_processors/input_processor_mouse_accel.c)
    zephyr_library_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_SCROLL_MODE
      src/input_processors/input_processor_scroll_mode.c)
  endif()
endif()
