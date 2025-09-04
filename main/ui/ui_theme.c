#include "ui_theme.h"

void ui_theme_apply_button_style(lv_obj_t *btn, lv_obj_t *btn_label)
{
    // Style button with solid red background and white text
    lv_obj_set_style_bg_color(btn, lv_color_hex(PRIMARY_RED), 0); // Red background
    lv_obj_set_style_border_width(btn, 0, 0); // No border
    lv_obj_set_style_text_color(btn_label, lv_color_hex(WHITE), 0); // White text
}

void ui_theme_apply_button_pressed_style(lv_obj_t *btn, lv_obj_t *btn_label)
{
    // Change button to pressed state
    lv_obj_set_style_bg_color(btn, lv_color_hex(PRIMARY_RED), 0);
    lv_obj_set_style_text_color(btn_label, lv_color_hex(WHITE), 0); // White text on red
}

void ui_theme_apply_progress_bar_style(lv_obj_t *progress_bar)
{
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(BLACK), 0); // Black background
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(PRIMARY_RED), LV_PART_INDICATOR); // Red progress
}

void ui_theme_apply_border_style(lv_obj_t *border)
{
    lv_obj_set_style_border_color(border, lv_color_hex(PRIMARY_RED), 0);
}

void ui_theme_apply_corner_style(lv_obj_t *corner)
{
    lv_obj_set_style_bg_color(corner, lv_color_hex(PRIMARY_RED), 0);
}

// Color utility functions
lv_color_t ui_theme_get_primary_color(void)
{
    return lv_color_hex(PRIMARY_RED);
}

lv_color_t ui_theme_get_dark_color(void)
{
    return lv_color_hex(DARK_RED);
}

lv_color_t ui_theme_get_light_color(void)
{
    return lv_color_hex(LIGHT_RED);
}

lv_color_t ui_theme_get_background_color(void)
{
    return lv_color_hex(WHITE);  // Changed to white background
}

lv_color_t ui_theme_get_text_color(void)
{
    return lv_color_hex(BLACK);  // Changed to black text
}

// Semantic color functions for consistent black-on-white styling
lv_color_t ui_theme_get_title_text_color(void)
{
    return lv_color_hex(PRIMARY_RED);  // Indigo for titles (accent)
}

lv_color_t ui_theme_get_default_text_color(void)
{
    return lv_color_hex(BLACK);  // Black for body text
}

lv_color_t ui_theme_get_subtitle_text_color(void)
{
    return lv_color_hex(BLACK);  // Black for subtitles
}

lv_color_t ui_theme_get_status_text_color(void)
{
    return lv_color_hex(BLACK);  // Black for status info
}

lv_color_t ui_theme_get_error_text_color(void)
{
    return lv_color_hex(DARK_RED);  // Dark indigo for errors
}

lv_color_t ui_theme_get_success_text_color(void)
{
    return lv_color_hex(PRIMARY_RED);  // Indigo for success states
}

lv_color_t ui_theme_get_muted_text_color(void)
{
    return lv_color_hex(0x666666);  // Gray for secondary info
}

lv_color_t ui_theme_get_tile_bg_color(void)
{
    return lv_color_hex(WHITE);  // White tile background
}