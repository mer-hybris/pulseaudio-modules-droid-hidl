PulseAudio Droid HIDL module
============================

module-droid-hidl
-----------------

The purpose of this module is to forward calls made to HIDL API or
AudioFlinger service to active hw module. This means that the module
cannot be loaded independently, it needs to have module-droid-card
or module-droid-{sink,source} or whatever module loaded beforehand
that parses droid configuration and loads the hw module to
PulseAudio global object.

Normally just compiling the package against your adaptation should
provide working module, but for testing one can also run the helper
binary by hand.

In pa configuration disable helper binary for the module,

    load-module module-droid-hidl helper=false

Run the helper in standalone mode (type can be either hidl or af),

    /usr/libexec/pulse/hidl-helper unix:path=.../pulse/dbus-socket -s -v -t <type>
