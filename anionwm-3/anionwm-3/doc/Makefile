# Settings
######################################

TOPDIR=..

include $(TOPDIR)/build/system-inc.mk

L2H=latex2html -show_section_numbers -short_index -local_icons -noaddress \
    -up_url http://iki.fi/tuomov/ion/ -up_title "Ion homepage" -nofootnode\
##    -style greyviolet.css


# Function documentation to build
######################################

DOCS=ionconf ionnotes

FNTEXES=ioncore.exports mod_tiling.exports \
	mod_query.exports de.exports mod_menu.exports \
	mod_dock.exports mod_sp.exports mod_statusbar.exports

TEXSOURCES=conf-bindings.tex confintro.tex conf-menus.tex conf-statusbar.tex \
	conf.tex conf-winprops.tex cstyle.tex designnotes.tex de.tex fdl.tex \
	fnref.tex fullhierarchy.tex hookref.tex ionconf.tex ionnotes.tex luaif.tex \
	macros.tex miscref.tex objectsimpl.tex objects.tex prelim.tex statusd.tex tricks.tex

RUBBER_DVI=rubber
RUBBER_PS=rubber -p
RUBBER_PDF=rubber -d

TARGETS = ionconf ionnotes


# Generic rules
######################################

default: all

%-dvi: $(TEXSOURCES)
	$(RUBBER_DVI) $*
	touch $@

%-ps: $(TEXSOURCES)
	$(RUBBER_PS) $*
	touch $@

%-pdf: $(TEXSOURCES)
	$(RUBBER_PDF) $*
	touch $@

# Install
######################################

install: all
	$(INSTALLDIR) $(DOCDIR); \
	for d in $(DOCS); do \
	    for e in ps pdf dvi; do \
	      test -f $$d.$$e && $(INSTALL) -m $(DATA_MODE) $$d.$$e $(DOCDIR); \
	    done; \
	    $(INSTALLDIR) $(DOCDIR)/$$d; \
	    for i in $$d/*; do \
	        $(INSTALL) -m $(DATA_MODE) $$i $(DOCDIR)/$$i; \
	    done; \
	    rm -f $(DOCDIR)/$$d/*.log; \
	    rm -f $(DOCDIR)/$$d/WARNINGS; \
	    rm -f $(DOCDIR)/$$d/*.aux; \
	    rm -f $(DOCDIR)/$$d/*.idx; \
	    rm -f $(DOCDIR)/$$d/*.tex; \
	    rm -f $(DOCDIR)/$$d/*.pl; \
	done

# ionconf rules
######################################

ionconf-dvi: fnlist.tex $(TEXSOURCES)
ionconf-ps: fnlist.tex $(TEXSOURCES)
ionconf-pdf: fnlist.tex $(TEXSOURCES)

ionconf-html: $(FNTEXES) $(TEXSOURCES)
	$(L2H) -split 3 ionconf
	touch ionconf-html

# ionnotes rules
######################################

ionnotes-html: $(TEXSOURCES)
	$(L2H) -split 4 ionnotes
	touch ionnotes-html

# More generic rules
######################################

.PHONY: all all-dvi all-ps all-pdf all-html

all: all-dvi all-ps all-pdf all-html

all-dvi: $(patsubst %,%-dvi,$(TARGETS))

all-ps: $(patsubst %, %-ps, $(TARGETS))

all-pdf: $(patsubst %, %-pdf, $(TARGETS))

all-html: $(patsubst %, %-html, $(TARGETS))

# Clean
######################################

clean:
	rm -f $(FNTEXES) fnlist.tex
	rm -f *.aux *.toc *.log *.out
	rm -f *.idx *.ild *.ilg *.ind
	rm -f *.ps *.ps.gz *.dvi *.dvi.gz *.pdf
	rm -rf ionconf ionnotes
	rm -f ionconf-{dvi,html,ps,pdf}
	rm -f ionnotes-{dvi,html,ps,pdf}

realclean: clean
	rm -f *.ps *.pdf *.dvi
	rm -rf $(DOCS)

# Function reference rules
######################################

include $(TOPDIR)/libmainloop/rx.mk

$(TOPDIR)/%/exports.tex:
	$(MAKE) -C $$(dirname $@) _exports_doc

%.exports: $(TOPDIR)/%/exports.tex
	cp $< $@

# Function list
######################################

fnlist.tex: $(FNTEXES)
	grep hyperlabel $+ | \
	sed 's/.*fn:\([^}]*\).*/\\fnlisti{\1}/;'|sort -d -f \
	> $@
