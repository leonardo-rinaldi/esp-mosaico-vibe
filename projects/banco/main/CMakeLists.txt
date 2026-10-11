idf_component_register(
    SRCS "main.c" "game.c" "calc.c" "i18n.c" "keypad.c" "screen_cable.c" "screen_color.c"
         "screen_battery.c" "power_status.c" "screen_conv.c" "screen_level.c" "screen_lens.c" "lens_present_device.c" "lens_camera.c" "level.c" "screen_menu.c" "screen_ohm.c" "ui_font.c" "ui_widgets.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_game_app mosaico_game_save mosaico_raylib_fast esp-gsp esp_timer esp-mosaico-bsp
             mosaico_raylib_port mosaico_module_camera esp_driver_ppa esp_mm esp_hw_support)

# GSP needs an image to bind the Raylib framebuffer. This generated placeholder
# is embedded in the application; no external game resource partition is used.
set(canvas_png "${CMAKE_CURRENT_BINARY_DIR}/canvas.png")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_canvas_png.py"
    "${canvas_png}" RESULT_VARIABLE canvas_result)
if(NOT canvas_result EQUAL 0)
    message(FATAL_ERROR "Could not generate the game canvas placeholder")
endif()
set(CANVAS_IMAGE "${canvas_png}")
configure_file("scene.json.in" "${CMAKE_CURRENT_BINARY_DIR}/scene.json" @ONLY)
gsp_add_bundle(${COMPONENT_LIB}
    SCENES "${CMAKE_CURRENT_BINARY_DIR}/scene.json"
    PIXEL_FORMAT rgb565)
