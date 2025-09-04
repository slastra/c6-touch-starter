#pragma once

#include "lvgl.h"
#include "../app_config.h"

// Theme application functions
void ui_theme_apply_button_style(lv_obj_t *btn, lv_obj_t *btn_label);
void ui_theme_apply_button_pressed_style(lv_obj_t *btn, lv_obj_t *btn_label);
void ui_theme_apply_progress_bar_style(lv_obj_t *progress_bar);
void ui_theme_apply_border_style(lv_obj_t *border);
void ui_theme_apply_corner_style(lv_obj_t *corner);

// Color utility functions - Black-on-White Design
lv_color_t ui_theme_get_primary_color(void);           // PRIMARY_RED - for accents/titles
lv_color_t ui_theme_get_dark_color(void);              // DARK_RED - for errors
lv_color_t ui_theme_get_light_color(void);             // LIGHT_RED - for loading states
lv_color_t ui_theme_get_background_color(void);        // WHITE - tile backgrounds
lv_color_t ui_theme_get_text_color(void);              // BLACK - default text

// Semantic color functions for consistent styling
lv_color_t ui_theme_get_title_text_color(void);        // PRIMARY_RED - tile titles
lv_color_t ui_theme_get_default_text_color(void);      // BLACK - body text
lv_color_t ui_theme_get_subtitle_text_color(void);     // BLACK - subtitles
lv_color_t ui_theme_get_status_text_color(void);       // BLACK - status info
lv_color_t ui_theme_get_error_text_color(void);        // DARK_RED - errors
lv_color_t ui_theme_get_success_text_color(void);      // PRIMARY_RED - success states
lv_color_t ui_theme_get_muted_text_color(void);        // GRAY - secondary info
lv_color_t ui_theme_get_tile_bg_color(void);           // WHITE - tile background