/*
 * raled_config - Configuration information for raled
 *
 * This program provides information about the raled installation,
 * similar to PostgreSQL's pg_config utility.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Configuration values - these are configured by CMake */
#define RALED_VERSION "1.0.0"
#define RALED_VERSION_NUM 10000
#define RALED_MAJOR_VERSION "1"
#define RALED_MINOR_VERSION "0"
#define RALED_PATCH_VERSION "0"

/* Default paths - these are configured by CMake */
#define DEFAULT_PREFIX "/usr/local/raled"
#define DEFAULT_EXEC_PREFIX "/usr/local/raled"
#define DEFAULT_BINDIR "/usr/local/raled/bin"
#define DEFAULT_LIBDIR "/usr/local/raled/lib"
#define DEFAULT_INCLUDEDIR "/usr/local/raled/include"
#define DEFAULT_PKGLIBDIR "/usr/local/raled/lib"
#define DEFAULT_LOCALEDIR "/usr/local/raled/share/locale"
#define DEFAULT_MANDIR "/usr/local/raled/share/man"
#define DEFAULT_SHAREDIR "/usr/local/raled/share"
#define DEFAULT_SYSCONFDIR "/usr/local/raled/etc"
#define DEFAULT_CONFIGURE "--prefix=/usr/local/raled"
#define DEFAULT_CC "/usr/bin/cc"
#define DEFAULT_CPPFLAGS ""
#define DEFAULT_CFLAGS " -Wall -Wextra -Werror -Wpedantic -Wshadow -Wconversion -Wvla"
#define DEFAULT_LDFLAGS ""
#define DEFAULT_LIBS "-lpthread -lrale"

static void
print_usage(const char *progname)
{
	printf("Usage: %s [OPTION]...\n", progname);
	printf("\n");
	printf("Options:\n");
	printf("  --version          show raled version\n");
	printf("  --bindir           show binary directory\n");
	printf("  --docdir           show documentation directory\n");
	printf("  --htmldir          show html documentation directory\n");
	printf("  --includedir       show C header files directory\n");
	printf("  --libdir           show object code library directory\n");
	printf("  --pkglibdir        show raled object code library directory\n");
	printf("  --localedir        show locale files directory\n");
	printf("  --mandir           show manual page directory\n");
	printf("  --sharedir         show architecture-independent files directory\n");
	printf("  --sysconfdir       show system-wide configuration file directory\n");
	printf("  --configure        show options given to 'configure' script\n");
	printf("  --cc               show value of CC used when raled was built\n");
	printf("  --cppflags         show value of CPPFLAGS used when raled was built\n");
	printf("  --cflags           show value of CFLAGS used when raled was built\n");
	printf("  --ldflags          show value of LDFLAGS used when raled was built\n");
	printf("  --libs             show libraries required to link with raled\n");
	printf("  --version_num      show raled version as integer\n");
	printf("  --help             show this help, then exit\n");
	printf("\n");
	printf("With no arguments, show all configuration information.\n");
	printf("\n");
	printf("Report bugs to <raled-devel@example.com>.\n");
}

static void
print_config_info(void)
{
	printf("BINDIR = %s\n", DEFAULT_BINDIR);
	printf("DOCDIR = %s\n", DEFAULT_SHAREDIR "/doc/raled");
	printf("HTMLDIR = %s\n", DEFAULT_SHAREDIR "/doc/raled/html");
	printf("INCLUDEDIR = %s\n", DEFAULT_INCLUDEDIR);
	printf("LIBDIR = %s\n", DEFAULT_LIBDIR);
	printf("PKGLIBDIR = %s\n", DEFAULT_PKGLIBDIR);
	printf("LOCALEDIR = %s\n", DEFAULT_LOCALEDIR);
	printf("MANDIR = %s\n", DEFAULT_MANDIR);
	printf("SHAREDIR = %s\n", DEFAULT_SHAREDIR);
	printf("SYSCONFDIR = %s\n", DEFAULT_SYSCONFDIR);
	printf("CONFIGURE = %s\n", DEFAULT_CONFIGURE);
	printf("CC = %s\n", DEFAULT_CC);
	printf("CPPFLAGS = %s\n", DEFAULT_CPPFLAGS);
	printf("CFLAGS = %s\n", DEFAULT_CFLAGS);
	printf("LDFLAGS = %s\n", DEFAULT_LDFLAGS);
	printf("LIBS = %s\n", DEFAULT_LIBS);
	printf("VERSION = %s\n", RALED_VERSION);
	printf("VERSION_NUM = %d\n", RALED_VERSION_NUM);
}

static void
print_version(void)
{
	printf("raled %s\n", RALED_VERSION);
}

static void
print_version_num(void)
{
	printf("%d\n", RALED_VERSION_NUM);
}

static void
print_simple_option(const char *option, const char *value __attribute__((unused)))
{
	if (strcmp(option, "version") == 0)
		print_version();
	else if (strcmp(option, "version_num") == 0)
		print_version_num();
	else if (strcmp(option, "bindir") == 0)
		printf("%s\n", DEFAULT_BINDIR);
	else if (strcmp(option, "docdir") == 0)
		printf("%s\n", DEFAULT_SHAREDIR "/doc/raled");
	else if (strcmp(option, "htmldir") == 0)
		printf("%s\n", DEFAULT_SHAREDIR "/doc/raled/html");
	else if (strcmp(option, "includedir") == 0)
		printf("%s\n", DEFAULT_INCLUDEDIR);
	else if (strcmp(option, "libdir") == 0)
		printf("%s\n", DEFAULT_LIBDIR);
	else if (strcmp(option, "pkglibdir") == 0)
		printf("%s\n", DEFAULT_PKGLIBDIR);
	else if (strcmp(option, "localedir") == 0)
		printf("%s\n", DEFAULT_LOCALEDIR);
	else if (strcmp(option, "mandir") == 0)
		printf("%s\n", DEFAULT_MANDIR);
	else if (strcmp(option, "sharedir") == 0)
		printf("%s\n", DEFAULT_SHAREDIR);
	else if (strcmp(option, "sysconfdir") == 0)
		printf("%s\n", DEFAULT_SYSCONFDIR);
	else if (strcmp(option, "configure") == 0)
		printf("%s\n", DEFAULT_CONFIGURE);
	else if (strcmp(option, "cc") == 0)
		printf("%s\n", DEFAULT_CC);
	else if (strcmp(option, "cppflags") == 0)
		printf("%s\n", DEFAULT_CPPFLAGS);
	else if (strcmp(option, "cflags") == 0)
		printf("%s\n", DEFAULT_CFLAGS);
	else if (strcmp(option, "ldflags") == 0)
		printf("%s\n", DEFAULT_LDFLAGS);
	else if (strcmp(option, "libs") == 0)
		printf("%s\n", DEFAULT_LIBS);
	else
	{
		char error_msg[256];
		snprintf(error_msg, sizeof(error_msg), "Unknown option: %s\n", option);
		fputs(error_msg, stderr);
	}
}

int
main(int argc, char *argv[])
{
	int i;

	if (argc == 1) {
		/* No arguments - show all configuration info */
		print_config_info();
		return 0;
	}

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			print_usage(argv[0]);
			return 0;
		} else if (strncmp(argv[i], "--", 2) == 0) {
			/* Option with -- prefix */
			print_simple_option(argv[i] + 2, NULL);
		} else {
			char error_msg[256];
			snprintf(error_msg, sizeof(error_msg), "Unknown option: %s\n", argv[i]);
			fputs(error_msg, stderr);
			char help_msg[256];
			snprintf(help_msg, sizeof(help_msg), "Try '%s --help' for more information.\n", argv[0]);
			fputs(help_msg, stderr);
			return 1;
		}
	}

	return 0;
}
