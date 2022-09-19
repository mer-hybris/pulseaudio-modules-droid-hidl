module_droid_hidl = shared_module('module-droid-hidl',
  'module-droid-hidl.c',
  include_directories : [configinc],
  c_args : [pa_c_args, '-DPA_MODULE_NAME=module_droid_hidl'],
  install : true,
  install_dir : modlibexecdir,
  dependencies : [
    audiosystem_passthrough_dep,
    dbus_dep,
    droid_headers_dep,
    pulsecore_dep
  ],
  name_prefix : ''
)
