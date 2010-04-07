##
## Ion Makefile
##

# System-specific configuration is in system.mk
include build/system-inc.mk

# List of modules
include modulelist.mk

######################################

INSTALL_SUBDIRS=\
	$(MODULE_LIST) \
	ioncore ion pwm etc utils man po doc \
	

SUBDIRS = $(LIBS_SUBDIRS) $(INSTALL_SUBDIRS)

DOCS = README LICENSE ChangeLog RELNOTES

TO_REALCLEAN = build/ac/system-ac.mk

POTFILE=po/ion.pot

######################################

include build/rules.mk

######################################

_contrib-install:
	mkdir -p $(ETCDIR)
	find contrib -name '*.lua' -exec install -D -m0644 {} $(ETCDIR) \;

_install: _contrib-install
	$(INSTALLDIR) $(DOCDIR)
	for i in $(DOCS); do \
		$(INSTALL) -m $(DATA_MODE) $$i $(DOCDIR); \
	done

relocatable_build:
	$(MAKE) RELOCATABLE=1 PREFIX=
