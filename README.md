PulseAudio Droid HIDL module
============================

module-droid-hidl
-----------------

The purpose of this module is to forward calls made to HIDL API to active
hw module. This means that the module cannot be loaded independently, it
needs to have module-droid-card or module-droid-{sink,source} or whatever
module loaded beforehand that parses droid configuration and loads the hw
module to PulseAudio global object.
