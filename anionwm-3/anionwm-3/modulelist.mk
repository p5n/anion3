##
## List of modules to build
##

MODULE_LIST = mod_tiling mod_query mod_menu \
	      mod_dock mod_sp mod_sm mod_statusbar \
	      de mod_xkb mod_xrandr mod_xinerama

# Modules to -dlpreload into pwm if statically linking.

PWM_MODULE_LIST := $(MODULE_LIST)
