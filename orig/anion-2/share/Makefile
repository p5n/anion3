##
## Ion share/ Makefile
##

# System-specific configuration is in system.mk
TOPDIR=..
include $(TOPDIR)/system-inc.mk

######################################

SUBDIRS=ion-completefile
INSTALL_SUBDIRS=$(SUBDIRS)

LUA_SOURCES = ioncorelib.lua ioncorelib-mplexfns.lua ioncore-efbb.lua

SHELLSCRIPTS = ion-edit ion-man ion-runinxterm ion-ssh ion-view

MISC_SHARE = welcome_message.txt

TARGETS=ion-ssh ion-man welcome_message.txt

######################################

include $(TOPDIR)/rules.mk

######################################

_install:
	$(INSTALLDIR) $(SHAREDIR)
	for i in $(SHELLSCRIPTS); do \
		$(INSTALL) -m $(BIN_MODE) $$i $(SHAREDIR); \
	done
	for i in $(LUA_SOURCES) $(MISC_SHARE); do \
		$(INSTALL) -m $(DATA_MODE) $$i $(SHAREDIR); \
	done
	$(INSTALLDIR) $(LCDIR)
	for i in $(LUA_COMPILED); do \
		$(INSTALL) -m $(DATA_MODE) $$i $(LCDIR); \
	done

%: %.in
	sed 's#SHAREDIR#$(SHAREDIR)#g' $< > $@
	chmod $(BIN_MODE) $@

#
# Awful hack that is not even supposed to work on the user's platform.
# Therefore these commands should be executed before packaging Ion.
#

welcome_message.txt: ../man/ion.1.in.txt welcome_message.head
	(cat welcome_message.head; \
         sed 's#ETCDIR#$(ETCDIR)#g; s#DOCDIR#$(DOCDIR)#g;' \
	 ../man/ion.1.in.txt) > welcome_message.txt
