##
## Ion Makefile
##

# System-specific configuration is in system.mk
include system-inc.mk

# List of modules
include modulelist.mk

######################################

INSTALL_SUBDIRS = $(MODULE_LIST) ion pwm man etc share
SUBDIRS = libtu ioncore luaextl $(INSTALL_SUBDIRS)

DOCS = README LICENSE ChangeLog

TO_REALCLEAN = system-ac.mk

######################################

include rules.mk

######################################

_install:
	$(INSTALLDIR) $(DOCDIR)
	for i in $(DOCS); do \
		$(INSTALL) -m $(DATA_MODE) $$i $(DOCDIR); \
	done
