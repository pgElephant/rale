AM_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wvla -Wno-strict-prototypes -Wno-missing-field-initializers -DNDEBUG $(CJSON_CFLAGS)

noinst_LIBRARIES = librale.a
librale_a_SOURCES = \
    src/assert.c src/cluster.c src/db.c src/dlog.c src/dstore.c \
    src/hash.c src/librale.c src/node.c src/rale_proto.c \
    src/shutdown.c src/tcp_client.c src/tcp_server.c src/udp.c \
    src/system_detect.c src/util.c src/validation.c src/watchdog.c src/rale_error.c

noinst_HEADERS = $(wildcard include/*.h)

AM_CPPFLAGS = -I$(srcdir)/include
