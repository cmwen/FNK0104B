#ifndef LV_CONF_H
#define LV_CONF_H

/* Keep the LVGL build focused on the widgets used by this diagnostic app. */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_USE_OS LV_OS_NONE
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_BUTTON 1
#define LV_USE_LABEL 1
#define LV_USE_FLEX 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif
