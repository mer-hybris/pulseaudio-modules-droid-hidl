/*
 * Copyright (C) 2019 Jolla Ltd.
 *
 * Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>
 *
 * These PulseAudio Modules are free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include <pulse/rtclock.h>
#include <pulse/timeval.h>
#include <pulse/xmalloc.h>
#include <pulse/mainloop-api.h>

#include <pulsecore/core.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/i18n.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/start-child.h>
#include <pulsecore/shared.h>

#include <android-version.h>
#include <audiosystem-passthrough/common.h>
#include "module-droid-hidl-symdef.h"

PA_MODULE_AUTHOR("Juho Hämäläinen");
PA_MODULE_DESCRIPTION("Droid AudioSystem passthrough");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_USAGE(
        "module_id=<unused> "
        "helper=<spawn helper binary, default true>"
);

static const char* const valid_modargs[] = {
    "module_id",
    "helper",
    NULL,
};

#define HELPER_BINARY       PASSTHROUGH_HELPER_DIR "/" PASSTHROUGH_HELPER_EXE
#define BUFFER_MAX          (512)

#if ANDROID_VERSION_MAJOR <= 7
#define DEFAULT_BINDER_IDX  "17"
#elif ANDROID_VERSION_MAJOR <= 8
#define DEFAULT_BINDER_IDX  "18"
#else
#define DEFAULT_BINDER_IDX  "18"
#endif

#define QTI_INTERFACE_NAME  "IQcRilAudio"

#define DROID_HW_HANDLE         "droid.handle.v1"
#define DROID_SET_PARAMETERS    "droid.set_parameters.v1"
#define DROID_GET_PARAMETERS    "droid.get_parameters.v1"

struct userdata {
    pa_core *core;
    pa_module *module;

    pa_dbus_protocol* dbus_protocol;

    void   *hw_handle;
    int   (*set_parameters)(void *handle, const char *key_value_pairs);
    char* (*get_parameters)(void *handle, const char *keys);

    /* Helper */
    pid_t pid;
    int fd;
    pa_io_event *io_event;
};

static pa_log_level_t _log_level = PA_LOG_ERROR;

static bool log_level_debug(void) {
    if (PA_UNLIKELY(_log_level == PA_LOG_DEBUG))
        return true;
    return false;
}

static void get_parameters(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void set_parameters(DBusConnection *conn, DBusMessage *msg, void *userdata);

enum audiosystem_passthrough_methods {
    PASSTHROUGH_GET_PARAMETERS,
    PASSTHROUGH_SET_PARAMETERS,
    PASSTHROUGH_METHOD_MAX
};

static pa_dbus_arg_info get_parameters_args[] = {
    { "keys", "s", "in" }
};

static pa_dbus_arg_info set_parameters_args[] = {
    { "key_value_pairs", "s", "in" }
};

static pa_dbus_method_handler passthrough_method_handlers[PASSTHROUGH_METHOD_MAX] = {
    [PASSTHROUGH_GET_PARAMETERS] = {
        .method_name = AUDIOSYSTEM_PASSTHROUGH_GET_PARAMETERS,
        .arguments = get_parameters_args,
        .n_arguments = sizeof(get_parameters_args) / sizeof(get_parameters_args[0]),
        .receive_cb = get_parameters
    },
    [PASSTHROUGH_SET_PARAMETERS] = {
        .method_name = AUDIOSYSTEM_PASSTHROUGH_SET_PARAMETERS,
        .arguments = set_parameters_args,
        .n_arguments = sizeof(set_parameters_args) / sizeof(set_parameters_args[0]),
        .receive_cb = set_parameters
    },
};

static pa_dbus_interface_info passthrough_info = {
    .name = AUDIOSYSTEM_PASSTHROUGH_IFACE,
    .method_handlers = passthrough_method_handlers,
    .n_method_handlers = PASSTHROUGH_METHOD_MAX,
    .property_handlers = NULL,
    .n_property_handlers = 0,
    .get_all_properties_cb = NULL,
    .signals = NULL,
    .n_signals = 0
};

static void dbus_init(struct userdata *u) {
    pa_assert(u);
    pa_assert(u->core);

    u->dbus_protocol = pa_dbus_protocol_get(u->core);

    pa_dbus_protocol_add_interface(u->dbus_protocol, AUDIOSYSTEM_PASSTHROUGH_PATH, &passthrough_info, u);
    pa_dbus_protocol_register_extension(u->dbus_protocol, AUDIOSYSTEM_PASSTHROUGH_IFACE);
}

static void dbus_done(struct userdata *u) {
    pa_assert(u);

    pa_dbus_protocol_unregister_extension(u->dbus_protocol, AUDIOSYSTEM_PASSTHROUGH_IFACE);
    pa_dbus_protocol_remove_interface(u->dbus_protocol, AUDIOSYSTEM_PASSTHROUGH_PATH, passthrough_info.name);
    pa_dbus_protocol_unref(u->dbus_protocol);
    u->dbus_protocol = NULL;
}

static void get_parameters(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct userdata *u;
    DBusMessage *reply;
    DBusError error;
    char *keys = NULL;
    char *key_value_pairs = NULL;

    pa_assert_se((u = userdata));
    dbus_error_init(&error);

    if (dbus_message_get_args(msg,
                              &error,
                              DBUS_TYPE_STRING,
                              &keys,
                              DBUS_TYPE_INVALID)) {

        key_value_pairs = u->get_parameters(u->hw_handle, keys);

        reply = dbus_message_new_method_return(msg);
        dbus_message_append_args(reply,
                                 DBUS_TYPE_STRING,
                                 &key_value_pairs,
                                 DBUS_TYPE_INVALID);

        pa_assert_se(dbus_connection_send(conn, reply, NULL));
        dbus_message_unref(reply);
        return;
    }

    pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Fail: %s", error.message);
    dbus_error_free(&error);
}

static void set_parameters(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct userdata *u;
    DBusError error;
    char *key_value_pairs = NULL;
    int ret;

    pa_assert_se((u = userdata));
    dbus_error_init(&error);

    if (dbus_message_get_args(msg,
                              &error,
                              DBUS_TYPE_STRING,
                              &key_value_pairs,
                              DBUS_TYPE_INVALID)) {

        ret = u->set_parameters(u->hw_handle, key_value_pairs);

        if (ret != 0)
            pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Failed to set parameters.");
        else
            pa_dbus_send_empty_reply(conn, msg);
        return;
    }

    pa_dbus_send_error(conn, msg, DBUS_ERROR_FAILED, "Fail: %s", error.message);
    dbus_error_free(&error);
}

static void io_free(struct userdata *u) {
    if (u->io_event) {
        u->core->mainloop->io_free(u->io_event);
        u->io_event = NULL;
    }

    if (u->fd >= 0) {
        pa_close(u->fd);
        u->fd = -1;
    }
}

static void io_event_cb(pa_mainloop_api*a, pa_io_event* e, int fd, pa_io_event_flags_t events, void *userdata) {
    struct userdata *u = userdata;
    char buffer[BUFFER_MAX];
    ssize_t r;

    pa_assert(u);

    if (events & PA_IO_EVENT_INPUT) {
        memset(buffer, 0, BUFFER_MAX);
        if ((r = pa_read(u->fd, buffer, BUFFER_MAX, NULL)) > 0) {
            if (log_level_debug())
                pa_log_debug("[" PASSTHROUGH_HELPER_EXE "] %s", buffer);
            else
                pa_log("[" PASSTHROUGH_HELPER_EXE "] %s", buffer);
        } else if (r < 0) {
            pa_log("failed read");
            io_free(u);
        }
    } else if (events & PA_IO_EVENT_HANGUP) {
        pa_log_debug("helper disappeared");
        io_free(u);
    } else if (events & PA_IO_EVENT_ERROR) {
        pa_log("io error");
        io_free(u);
    }
}

static bool file_exists(const char *path) {
    return access(path, F_OK) == 0 ? true : false;
}

static bool string_in_file(const char *path, const char *string) {
    char line[512];
    FILE *f = NULL;
    bool found = false;

    pa_assert(path);

    if (!file_exists(path))
        goto done;

    if (!(f = pa_fopen_cloexec(path, "r"))) {
        pa_log_warn("open('%s') failed: %s", path, pa_cstrerror(errno));
        goto done;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, string)) {
            found = true;
            break;
        }
    }

done:
    if (f)
        fclose(f);

    return found;
}

static void helper_setenv(const char *dbus_address, const char *impl_type, const char *idx) {
    setenv(ENV_AUDIOSYSTEM_PASSTHROUGH_ADDRESS, dbus_address, 1);
    setenv(ENV_AUDIOSYSTEM_PASSTHROUGH_TYPE, impl_type, 0);
    setenv(ENV_AUDIOSYSTEM_PASSTHROUGH_IDX, idx, 0);
}

static void helper_unsetenv(void) {
    unsetenv(ENV_AUDIOSYSTEM_PASSTHROUGH_ADDRESS);
    unsetenv(ENV_AUDIOSYSTEM_PASSTHROUGH_TYPE);
    unsetenv(ENV_AUDIOSYSTEM_PASSTHROUGH_IDX);
}

int pa__init(pa_module *m) {
    pa_modargs *ma = NULL;
    bool helper = true;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    log_init(&_log_level);

    struct userdata *u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    m->userdata = u;
    u->pid = (pid_t) -1;
    u->fd = -1;
    u->io_event = NULL;

    if (pa_modargs_get_value_boolean(ma, "helper", &helper) < 0) {
        pa_log("helper is boolean argument");
        goto fail;
    }

    if (!(u->hw_handle = pa_shared_get(u->core, DROID_HW_HANDLE)) ||
        !(u->set_parameters = pa_shared_get(u->core, DROID_SET_PARAMETERS)) ||
        !(u->get_parameters = pa_shared_get(u->core, DROID_GET_PARAMETERS))) {
        pa_log("Couldn't get hw module functions, is module-droid-card loaded?");
        goto fail;
    }

    dbus_init(u);

    if (helper) {
        const char *const manifest_locations[] = {
            "/vendor/etc/vintf/manifest.xml",
            "/vendor/manifest.xml",
        };
        const char *impl_str = NULL;
        const char *idx_str = NULL;
        char *dbus_address = NULL;
        bool qti_found = false;
        unsigned i;

        for (i = 0; i < PA_ELEMENTSOF(manifest_locations); i++) {
            if (string_in_file(manifest_locations[i], QTI_INTERFACE_NAME)) {
                pa_log_debug("Detected " AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_QTI " implementation.");
                qti_found = true;
                break;
            }
        }

        if (qti_found)
            impl_str = AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_QTI;
        else
            impl_str = AUDIOSYSTEM_PASSTHROUGH_IMPL_STR_AF;

        idx_str = DEFAULT_BINDER_IDX;
        dbus_address = pa_get_dbus_address_from_server_type(u->core->server_type);

        helper_setenv(dbus_address, impl_str, idx_str);
        pa_xfree(dbus_address);

        if ((u->fd = pa_start_child_for_read(HELPER_BINARY,
                                             "--module", &u->pid)) < 0) {
            pa_log("Failed to spawn " PASSTHROUGH_HELPER_EXE);
            goto fail;
        }

        pa_log_info("Helper running with pid %d", u->pid);

        u->io_event = u->core->mainloop->io_new(u->core->mainloop,
                                                u->fd,
                                                PA_IO_EVENT_INPUT | PA_IO_EVENT_ERROR | PA_IO_EVENT_HANGUP,
                                                io_event_cb,
                                                u);
        helper_unsetenv();
    }

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (helper)
        helper_unsetenv();

    pa__done(m);

    return -1;
}

void pa__done(pa_module *m) {
    struct userdata *u;

    pa_assert(m);

    if ((u = m->userdata)) {
        dbus_done(u);

        if (u->pid != (pid_t) -1) {
            kill(u->pid, SIGTERM);

            for (;;) {
                if (waitpid(u->pid, NULL, 0) >= 0)
                    break;

                if (errno != EINTR) {
                    pa_log("waitpid() failed: %s", pa_cstrerror(errno));
                    break;
                }
            }
        }

        io_free(u);

        pa_xfree(u);
    }
}
