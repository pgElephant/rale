AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wvla -Wno-strict-prototypes -Wno-missing-field-initializers -DNDEBUG $(CJSON_CFLAGS)
AM_LDFLAGS = $(CJSON_LIBS)
AM_CPPFLAGS = -I$(top_srcdir)/librale/include -I$(srcdir)/include
ralectrl_CPPFLAGS = -DRALE_BINDIR=\"$(bindir)\" -I$(top_srcdir)/librale/include -I$(srcdir)/include

bin_PROGRAMS = ralectrl
ralectrl_SOURCES = src/ralectrl.c src/ralectrl_http_client.c
ralectrl_LDADD = $(top_builddir)/librale/librale.a
