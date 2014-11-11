AC_INIT([EML], [0.2.0], [cap@pcg.ull.es])
AC_CONFIG_SRCDIR([src/device.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile Doxyfile])
AC_CONFIG_MACRO_DIR([m4])
AS_IF([test "$program_prefix" = NONE],
	[program_prefix=$PACKAGE_TARNAME-])

AM_INIT_AUTOMAKE([1.14 -Wall -Werror -Wno-extra-portability subdir-objects check-news])

AC_PROG_CC([c99])
AC_PROG_CC_C99
AC_PROG_LN_S

AC_SEARCH_LIBS([clock_gettime], [rt])
AC_SEARCH_LIBS([dlopen], [dl])
AC_SEARCH_LIBS([pthread_create], [pthread])

PKG_CHECK_MODULES([CONFUSE], [libconfuse])

# Optional: Intel MIC support
AC_ARG_WITH([mic], AS_HELP_STRING([--without-mic],
	[Disable Intel Many Integrated Core support
		(default: enabled if miclib.h is available)]))
AS_IF([test "x$with_mic" != "xno"],
	[AC_CHECK_HEADER([miclib.h], [have_mic=yes])])
AM_CONDITIONAL([ENABLE_MIC], [test "x$have_mic" = "xyes"])

# Optional: Intel RAPL support
AC_ARG_WITH([rapl], AS_HELP_STRING([--without-rapl],
	[Disable Intel Running Average Power Limit support
		(default: enabled)]))
AS_IF([test "x$with_rapl" != "xno"], [have_rapl=yes])
AM_CONDITIONAL([ENABLE_RAPL], [test "x$have_rapl" = "xyes"])

# Optional: NVIDIA Management Library support
AC_ARG_WITH([nvml], AS_HELP_STRING([--without-nvml],
	[Disable NVIDIA Management Library support
		(default: enabled if nvml.h is available)]))
AS_IF([test "x$with_nvml" != "xno"],
	[AC_CHECK_HEADER([nvml.h], [have_nvml=yes])])
AM_CONDITIONAL([ENABLE_NVML], [test "x$have_nvml" = "xyes"])

# Optional: Schleifenbauer PDU support
AC_ARG_WITH([sb_pdu], AS_HELP_STRING([--without-sb-pdu],
	[Disable Schleifenbauer PDU support
		(default: enabled if libcrypto is available)]))
AS_IF([test "x$with_sb_pdu" != "xno"],
	[PKG_CHECK_MODULES([CRYPTO], [libcrypto], [have_sb_pdu=yes])])
AM_CONDITIONAL([ENABLE_SB_PDU], [test "x$have_sb_pdu" = "xyes"])

LT_INIT

# Doxygen documentation
DX_HTML_FEATURE(ON)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([AC_PACKAGE_NAME])

AC_OUTPUT
