##
## Query module Makefile
##

# System-specific configuration is in system.mk
TOPDIR=..
include $(TOPDIR)/system-inc.mk

######################################

INCLUDES += -I../libtu/include $(X11_INCLUDES) -I..
CFLAGS += $(XOPEN_SOURCE) $(C99_SOURCE)

SOURCES=complete.c edln.c input.c listing.c main.c wedln.c \
        wedln-wrappers.c wmessage.c query.c fwarn.c

LUA_SOURCES=querylib.lua

MAKE_EXPORTS=query_module

MODULE=query

######################################

include $(TOPDIR)/rules.mk

######################################

_install: module_install
	$(INSTALLDIR) $(SHAREDIR)
	$(INSTALL) -m $(DATA_MODE) querylib.lua $(SHAREDIR)
	$(INSTALLDIR) $(LCDIR)
	$(INSTALL) -m $(DATA_MODE) querylib.lc $(LCDIR)
