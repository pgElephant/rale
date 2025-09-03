AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wvla -Wno-strict-prototypes -Wno-missing-field-initializers -DNDEBUG $(CJSON_CFLAGS)
AM_LDFLAGS = $(CJSON_LIBS)
AM_CPPFLAGS = -I$(top_srcdir)/librale/include -I$(srcdir)/include

bin_PROGRAMS = raled raled_config
raled_SOURCES = \
  src/raled.c src/raled_args.c src/raled_comm.c src/raled_command.c \
  src/raled_configfile.c src/raled_guc.c src/raled_logger.c src/raled_main.c \
  src/raled_response.c src/raled_signal.c

raled_LDADD = $(top_builddir)/librale/librale.a

raled_config_SOURCES = src/raled_config.c

EXTRA_DIST = src/raled_config.c.in include/*.h
