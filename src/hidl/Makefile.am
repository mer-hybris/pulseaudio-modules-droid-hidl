AM_LIBADD = \
	$(PULSEAUDIO_LIBS) \
	$(DBUS_LIBS) \
	$(DROIDUTIL_LIBS)
AM_CFLAGS = \
	$(PULSEAUDIO_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(DROIDHEADERS_CFLAGS) \
	$(DROIDUTIL_CFLAGS) \
	-DPULSEAUDIO_VERSION=@PA_MAJOR@ \
	-I$(top_srcdir)/src/hidl


modlibexec_LTLIBRARIES = module-droid-hidl.la

noinst_HEADERS = module-droid-hidl-symdef.h

module_droid_hidl_la_SOURCES = module-droid-hidl.c
module_droid_hidl_la_LDFLAGS = -module -avoid-version -Wl,-no-undefined -Wl,-z,noexecstack
module_droid_hidl_la_LIBADD = $(AM_LIBADD) -lm
module_droid_hidl_la_CFLAGS = $(AM_CFLAGS)
