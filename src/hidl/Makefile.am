AM_LIBADD = \
	$(PULSEAUDIO_LIBS) \
	$(DBUS_LIBS) \
	$(DROIDUTIL_LIBS)
AM_CFLAGS = \
	$(HELPER_LOCATION_CFLAGS) \
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

pulselibexecdir=$(libexecdir)/pulse

pulselibexec_PROGRAMS = hidl-helper

hidl_helper_SOURCES = hidl-helper.c dbus-comms.c impl-af.c impl-hidl.c
hidl_helper_LDADD = $(LIBGBINDER_LIBS) $(GLIB_LIBS) $(GIO_LIBS)
hidl_helper_CFLAGS = $(DROIDHEADERS_CFLAGS) $(LIBGBINDER_CFLAGS) $(GLIB_CFLAGS) $(GIO_CFLAGS)
