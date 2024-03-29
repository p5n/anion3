##
## Ion manual page Makefile
##

# System-specific configuration is in system.mk
TOPDIR=..
include $(TOPDIR)/build/system-inc.mk

######################################

TRANSLATIONS=fi cs

WELCOME_TARGETS=\
	welcome.txt \
	$(foreach tr, $(TRANSLATIONS), welcome.$(tr).txt)

TARGETS=anion3.1 $(foreach tr, $(TRANSLATIONS), anion3.$(tr).1) \
	anion-pwm3.1 $(foreach tr, $(TRANSLATIONS), anion-pwm3.$(tr).1) \
	$(WELCOME_TARGETS)

MKMAN=$(LUA) ../build/mkman.lua $(MKMAN_DEFS)
MKMAN_DEFS=-D ETCDIR $(REL)$(ETCDIR) -D DOCDIR $(REL)$(DOCDIR)

ifeq ($(RELOCATABLE),1)
REL="/..."
endif

NROFF=nroff -man -Tlatin1
#FILTERCRAP=perl -p -i -e 's/.\10//g'
FILTERCRAP=$(LUA) -e 'io.write((string.gsub(io.read("*a"), ".\8", "")))'

CONFIGS=../etc/cfg_anioncore.lua \
	../etc/cfg_tiling.lua \
	../etc/cfg_query.lua \
	../etc/cfg_menu.lua

# TODO: PWM configuration file is undocumented
PWM_CONFIGS=\
	../etc/cfg_anioncore.lua \
	../etc/cfg_menu.lua \
	../pwm/cfg_pwm.lua

TO_CLEAN=*.1

######################################

include $(TOPDIR)/build/rules.mk

######################################

anion3.1: anion3.in $(CONFIGS)
	$(MKMAN) -i $< -o $@ $(CONFIGS)

anion-pwm3.1: anion-pwm3.in $(PWM_CONFIGS)
	$(MKMAN) -i $< -o $@ $(PWM_CONFIGS)

anion3.%.1: anion3.%.in $(CONFIGS) ../po/%.po
	$(MKMAN) -po ../po/$*.po -i $< -o $@ $(CONFIGS)

anion-pwm3.%.1: anion-pwm3.%.in $(PWM_CONFIGS) ../po/%.po
	$(MKMAN) -po ../po/$*.po -i $< -o $@ $(PWM_CONFIGS)

welcome%txt: welcome%head anion3%1
	(cat welcome$*head; \
	$(NROFF) anion3$*1 | $(FILTERCRAP)) > $@

_install:
	$(INSTALLDIR) $(MANDIR)/man1
	$(INSTALL) -m $(DATA_MODE) anion3.1 $(MANDIR)/man1
	$(INSTALL) -m $(DATA_MODE) anion-pwm3.1 $(MANDIR)/man1
	for tr in $(TRANSLATIONS); do \
		$(INSTALLDIR) $(MANDIR)/$$tr/man1 ; \
		$(INSTALL) -m $(DATA_MODE) anion3.$$tr.1 $(MANDIR)/$$tr/man1/anion3.1 ; \
		$(INSTALL) -m $(DATA_MODE) anion-pwm3.$$tr.1 $(MANDIR)/$$tr/man1/anion-pwm3.1 ; \
	done
	$(INSTALLDIR) $(SHAREDIR)
	for i in $(WELCOME_TARGETS); do \
		$(INSTALL) -m $(DATA_MODE) $$i $(SHAREDIR) ; \
	done
