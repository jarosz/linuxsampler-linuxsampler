INCLUDES = $(all_includes)
METASOURCES = AUTO

AM_CXXFLAGS = -Wreturn-type -ffast-math $(CXX_CPU_SWITCH)

noinst_LTLIBRARIES = liblinuxsamplernetwork.la
liblinuxsamplernetwork_la_SOURCES = \
	lscp.h \
	lscpparser.cpp lscpparser.h \
	lscpserver.cpp lscpserver.h \
	lscpsymbols.h \
	lscpresultset.cpp lscpresultset.h \
	lscpevent.cpp lscpevent.h

yacc_sources = lscp.y

EXTRA_DIST = $(yacc_sources)

# automatically (re)generate lscpsymbols.h with bison / yacc if the
# yacc source file(s) have been changed
lscpsymbols.h: $(yacc_sources)
	$(top_builddir)/scripts/generate_parser.sh

# automatically (re)generate lscpparser.cpp with bison / yacc if the
# yacc source file(s) have been changed
lscpparser.cpp: $(yacc_sources)
	$(top_builddir)/scripts/generate_parser.sh


.PHONY: parser

# "make parser" was explicitly requested
parser:
	$(top_builddir)/scripts/generate_parser.sh
