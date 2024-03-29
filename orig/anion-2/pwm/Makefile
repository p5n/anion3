##
## PWM Makefile
##

# System-specific configuration is in system.mk
TOPDIR=..
include $(TOPDIR)/system-inc.mk

# List of modules to possibly preload
include $(TOPDIR)/modulelist.mk

######################################

INCLUDES += $(X11_INCLUDES)
INCLUDES += -I.. -I../libtu/include 

LIBS += -lm
LIBS += $(X11_LIBS) $(XINERAMA_LIBS)
LIBS += $(LUA_LIBS)
LIBS += $(LTDL_LIBS)
EXT_OBJS += ../ioncore/ioncore.a ../luaextl/luaextl.a
EXT_OBJS += ../libtu/libtu.a

DEFINES += -DETCDIR=\"$(ETCDIR)\" -DSHAREDIR=\"$(SHAREDIR)\" \
	   -DEXTRABINDIR=\"$(EXTRABINDIR)\" -DMODULEDIR=\"$(MODULEDIR)\" \
	   -DLCDIR=\"$(LCDIR)\"

ifndef PWM_ETCDIR
PWM_ETCDIR = $(ETCDIR)
else
DEFINES += -DPWM_ETCDIR=\"$(PWM_ETCDIR)\"
endif

CFLAGS += $(XOPEN_SOURCE) $(C99_SOURCE)

ifeq ($(PRELOAD_MODULES),1)
LDFLAGS += $(foreach mod, $(PWM_MODULE_LIST), -dlpreopen ../$(mod)/$(mod).la)
LINKOPTS = -R$(MODULEDIR)
else
LINKOPTS = -export-dynamic -R$(MODULEDIR)
endif

SOURCES = pwm.c
ETC = pwm.lua pwm-menus.lua pwm-bindings.lua
TARGETS = pwm

######################################

include $(TOPDIR)/rules.mk

######################################

pwm: $(OBJS) $(EXT_OBJS)
	$(LIBTOOL) --mode=link $(CC) $(LINKOPTS) $(OBJS) $(EXT_OBJS) $(LDFLAGS) -o $@

_install:
	$(INSTALLDIR) $(BINDIR)
	$(INSTALL) -s -m $(BIN_MODE) pwm $(BINDIR)
	# $(STRIP) $(BINDIR)/pwm
	$(INSTALLDIR) $(PWM_ETCDIR)
	for i in $(ETC); do \
		$(INSTALL) -m $(DATA_MODE) $$i $(PWM_ETCDIR); \
	done
