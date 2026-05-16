v24b UI tweak fixed

Fix:
- removed lv_font_montserrat_56 because LVGL stock Montserrat in this project does not include 56 px.
- restored lv_font_montserrat_48, currently the largest enabled stock font.
- preserved input list compact spacing to avoid the last input being clipped.

Next option for bigger volume:
- add a custom 64/72 px numeric font, or
- draw the volume digits as graphics.
