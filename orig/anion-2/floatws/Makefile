##
## Floating workspace module Makefile
##

# System-specific configuration is in system.mk
TOPDIR=..
include $(TOPDIR)/system-inc.mk

######################################

INCLUDES += -I../libtu/include $(X11_INCLUDES) -I..
CFLAGS += $(XOPEN_SOURCE) $(C99_SOURCE)

SOURCES=main.c floatws.c floatframe.c placement.c resize.c

MAKE_EXPORTS=floatws_module

MODULE=floatws

######################################

include $(TOPDIR)/rules.mk

######################################

_install: module_install
