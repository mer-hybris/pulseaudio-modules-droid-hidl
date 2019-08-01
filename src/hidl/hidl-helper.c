/*
 * Copyright (C) 2019 Jolla Ltd.
 *               2019 Slava Monich <slava.monich@jolla.com>
 *
 * Contact: Juho Hämäläinen <juho.hamalainen@jolla.com>
 *
 * This application is free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This application is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this application; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 */

#include <gbinder.h>
#include <glib-unix.h>
#include <gutil_log.h>
#include <gio/gio.h>

#include "common.h"

#define RET_OK                      (0)
#define RET_INVARG                  (2)

#define BINDER_DEVICE               GBINDER_DEFAULT_HWBINDER
#define QCRIL_IFACE_1_0(x)          "vendor.qti.hardware.radio.am@1.0::" x
#define QCRIL_AUDIO_1_0             QCRIL_IFACE_1_0("IQcRilAudio")
#define QCRIL_AUDIO_CALLBACK_1_0    QCRIL_IFACE_1_0("IQcRilAudioCallback")

#define OFONO_RIL_SUBSCRIPTION_CONF "/etc/ofono/ril_subscription.conf"
#define OFONO_RIL_SUBSCRIPTION_D    "/etc/ofono/ril_subscription.d"
#define OFONO_RIL_SLOTS_MAX         (4)
#define CONNECT_RETRY_TIMEOUT_S     (1)

#define DBGP(...)   do {                                                    \
                        printf(__VA_ARGS__);                                \
                        printf("\n");                                       \
                        fflush(stdout);                                     \
                    } while(0)

#define DBG(...)    do {                                                    \
                        if (gutil_log_default.level == GLOG_LEVEL_VERBOSE) {\
                            if (standalone)                                 \
                                GDEBUG(__VA_ARGS__);                        \
                            else                                            \
                                DBGP(__VA_ARGS__);                          \
                        }                                                   \
                    } while(0)

#define ERR(...)    do {                                                    \
                        if (standalone)                                     \
                            GERR(__VA_ARGS__);                              \
                        else                                                \
                            DBGP(__VA_ARGS__);                              \
                    } while(0)


enum qcril_audio_methods {
    QCRIL_AUDIO_SET_CALLBACK = GBINDER_FIRST_CALL_TRANSACTION,
    QCRIL_AUDIO_SET_ERROR
};

enum qcril_audio_callback_methods {
    QCRIL_AUDIO_CALLBACK_GET_PARAMETERS = GBINDER_FIRST_CALL_TRANSACTION,
    QCRIL_AUDIO_CALLBACK_SET_PARAMETERS
};

static gboolean standalone = FALSE;
static const char pname[] = HELPER_NAME;

typedef struct app App;

typedef struct am_client {
    App *app;
    char* fqname;
    gchar* slot;
    GBinderServiceManager* sm;
    GBinderLocalObject* local;
    GBinderRemoteObject* remote;
    GBinderClient* client;
    gulong wait_id;
    gulong death_id;
} AmClient;

struct app {
    GMainLoop* loop;
    int ret;
    GBinderServiceManager* sm;
    GSList* clients;
    guint connect_source;
    GDBusConnection *dbus;
    gchar *address;
};

static gint
dbus_get_parameters(
        App *app,
        const gchar *keys,
        gchar **reply_values);

static gint
dbus_set_parameters(
        App *app,
        const gchar *key_value_pairs);

static void
am_client_registration_handler(
        GBinderServiceManager* sm,
        const char* name,
        void* user_data);

static void
am_remote_died(
        GBinderRemoteObject* obj,
        void* user_data)
{
    AmClient* am = user_data;

    DBG("%s has died", am->fqname);
    gbinder_remote_object_unref(am->remote);
    am->remote = NULL;

    /* Wait for it to re-appear */
    am->wait_id = gbinder_servicemanager_add_registration_handler(am->sm,
        am->fqname, am_client_registration_handler, am);
}

/* IQcRilAudioCallback::getParameters(string str) generates (string) */
static gboolean
am_client_callback_get_parameters(
        AmClient* am,
        const char* str,
        GBinderLocalReply* reply)
{
    if (str) {
        gchar* result = NULL;
        dbus_get_parameters(am->app, str, &result);

        if (result) {
            GBinderWriter writer;
            gbinder_local_reply_init_writer(reply, &writer);
            gbinder_writer_append_int32(&writer, 0 /* OK */);
            gbinder_writer_append_hidl_string(&writer, result);

            return TRUE;
        }
    }

    return FALSE;
}

/* IQcRilAudioCallback::setParameters(string str) generates (int32_t) */
static gboolean
am_client_callback_set_parameters(
        AmClient* am,
        const char* str,
        GBinderLocalReply* reply)
{
    if (str) {
        GBinderWriter writer;
        guint32 result = 0;

        result = dbus_set_parameters(am->app, str);
        gbinder_local_reply_init_writer(reply, &writer);
        gbinder_writer_append_int32(&writer, 0 /* OK */);
        gbinder_writer_append_int32(&writer, result);

        return TRUE;
    }

    return FALSE;
}

static GBinderLocalReply*
am_client_callback(
        GBinderLocalObject* obj,
        GBinderRemoteRequest* req,
        guint code,
        guint flags,
        int* status,
        void* user_data)
{
    AmClient* am = user_data;
    const char* iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, QCRIL_AUDIO_CALLBACK_1_0)) {
        GBinderReader reader;
        GBinderLocalReply* reply = gbinder_local_object_new_reply(obj);
        const char* str;

        gbinder_remote_request_init_reader(req, &reader);
        str = gbinder_reader_read_hidl_string_c(&reader);
        switch (code) {
        case QCRIL_AUDIO_CALLBACK_GET_PARAMETERS:
            DBG("IQcRilAudioCallback::getParameters %s %s", am->slot, str);
            if (am_client_callback_get_parameters(am, str, reply)) {
                return reply;
            }
            break;
        case QCRIL_AUDIO_CALLBACK_SET_PARAMETERS:
            DBG("IQcRilAudioCallback::setParameters %s %s", am->slot, str);
            if (am_client_callback_set_parameters(am, str, reply)) {
                return reply;
            }
            break;
        }
        /* We haven't used the reply */
        gbinder_local_reply_unref(reply);
    }
    ERR("Unexpected callback %s %u", iface, code);
    *status = GBINDER_STATUS_FAILED;
    return NULL;
}

static gboolean
am_client_connect(
        AmClient* am)
{
    int status = 0;
    am->remote = gbinder_servicemanager_get_service_sync(am->sm,
        am->fqname, &status); /* auto-released reference */

    if (am->remote) {
        GBinderLocalRequest* req;

        DBG("Connected to %s", am->fqname);
        gbinder_remote_object_ref(am->remote);
        am->client = gbinder_client_new(am->remote, QCRIL_AUDIO_1_0);
        am->death_id = gbinder_remote_object_add_death_handler(am->remote,
            am_remote_died, am);
        am->local = gbinder_servicemanager_new_local_object(am->sm,
            QCRIL_AUDIO_CALLBACK_1_0, am_client_callback, am);

        /* oneway IQcRilAudio::setCallback(IQcRilAudioCallback) */
        req = gbinder_client_new_request(am->client);
        gbinder_local_request_append_local_object(req, am->local);
        status = gbinder_client_transact_sync_oneway(am->client,
            QCRIL_AUDIO_SET_CALLBACK, req);
        gbinder_local_request_unref(req);
        DBG("setCallback %s status %d", am->slot, status);
        return TRUE;
    }
    return FALSE;
}

static void
am_client_registration_handler(
        GBinderServiceManager* sm,
        const char* name,
        void* user_data)
{
    AmClient* am = user_data;

    if (!strcmp(name, am->fqname) && am_client_connect(am)) {
        DBG("%s has reanimated", am->fqname);
        gbinder_servicemanager_remove_handler(am->sm, am->wait_id);
        am->wait_id = 0;
    } else {
        DBG("%s appeared", name);
    }
}

static AmClient*
am_client_new(
        App *app,
        const char* slot)
{
    AmClient* am = g_new0(AmClient, 1);

    am->app = app;
    am->slot = g_strdup(slot);
    am->fqname = g_strconcat(QCRIL_AUDIO_1_0, "/", slot, NULL);
    am->sm = gbinder_servicemanager_ref(app->sm);
    return am;
}

static void
am_client_connect_all(
        GSList *clients)
{
    GSList *i;

    for (i = clients; i; i = i->next) {
        AmClient *am = i->data;

        if (!am_client_connect(am)) {
            DBG("Waiting for %s", am->fqname);
            am->wait_id = gbinder_servicemanager_add_registration_handler(am->sm,
                am->fqname, am_client_registration_handler, am);
        }
    }
}

static void
am_client_free(
        gpointer data)
{
    AmClient* am = data;

    if (am->remote) {
        gbinder_remote_object_remove_handler(am->remote, am->death_id);
        gbinder_remote_object_unref(am->remote);
    }
    if (am->local) {
        gbinder_local_object_drop(am->local);
        gbinder_client_unref(am->client);
    }
    gbinder_servicemanager_remove_handler(am->sm, am->wait_id);
    gbinder_servicemanager_unref(am->sm);
    g_free(am->fqname);
    g_free(am->slot);
    g_free(am);
}

static gboolean
app_signal(
        gpointer user_data)
{
    App* app = user_data;

    DBG("Caught signal, %s shutting down...", pname);
    g_main_loop_quit(app->loop);
    return G_SOURCE_CONTINUE;
}

static void
app_run(
        App* app)
{
    guint sigtrm = g_unix_signal_add(SIGTERM, app_signal, app);
    guint sigint = g_unix_signal_add(SIGINT, app_signal, app);

    am_client_connect_all(app->clients);
    g_main_loop_run(app->loop);
    g_source_remove(sigtrm);
    g_source_remove(sigint);
}

static gint
dbus_call(
        App *app,
        const gchar *method,
        const gchar *args,
        gchar **reply_str)
{
    gint ret = 1;
    GDBusMessage *msg = NULL;
    GDBusMessage *reply = NULL;
    GError *error = NULL;

    g_assert(app);
    g_assert(method);
    g_assert(args);

    if (reply_str)
        *reply_str = NULL;

    if (!app->dbus) {
        ERR("No connection (%s)", app->address);
        goto out;
    }

    msg = g_dbus_message_new_method_call(NULL,
                                         HIDL_PASSTHROUGH_PATH,
                                         HIDL_PASSTHROUGH_IFACE,
                                         method);
    g_dbus_message_set_body(msg, g_variant_new("(s)", args));
    reply = g_dbus_connection_send_message_with_reply_sync(app->dbus,
                                                           msg,
                                                           G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                                           -1,
                                                           NULL, /* out_serial */
                                                           NULL, /* cancellable */
                                                           &error);
    if (!reply) {
        ERR("Failed to call %s(): %s", method, error->message);
        goto out;
    }

    if (g_dbus_message_get_message_type(reply) == G_DBUS_MESSAGE_TYPE_ERROR) {
        ERR("Failed to call %s()", method);
        goto out;
    }

    if (reply_str)
        *reply_str = g_strdup(g_dbus_message_get_arg0(reply));
    ret = 0;

out:
    if (error)
        g_error_free(error);
    if (msg)
        g_object_unref(msg);
    if (reply)
        g_object_unref(reply);

    return ret;
}

static gint
dbus_set_parameters(
        App *app,
        const gchar *key_value_pairs)
{
    g_assert(app);
    g_assert(key_value_pairs);

    return dbus_call(app, HIDL_PASSTHROUGH_METHOD_SET_PARAMETERS, key_value_pairs, NULL);
}

static gint
dbus_get_parameters(
        App *app,
        const gchar *keys,
        gchar **reply_values)
{
    g_assert(app);
    g_assert(keys);
    g_assert(reply_values);

    return dbus_call(app, HIDL_PASSTHROUGH_METHOD_GET_PARAMETERS, keys, reply_values);
}

static gboolean
dbus_init_cb(
        gpointer user_data)
{
    App *app = user_data;
    GError *error = NULL;

    app->dbus = g_dbus_connection_new_for_address_sync(app->address,
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                       NULL,    /* observer */
                                                       NULL,    /* cancellable */
                                                       &error);


    if (!app->dbus) {
        DBG("Could not connect to %s, try again in %d seconds...", app->address, CONNECT_RETRY_TIMEOUT_S);
        return G_SOURCE_CONTINUE;
    }

    if (error) {
        DBG("Could not connect to %s: %s", app->address, error->message);
        g_error_free(error);
        return G_SOURCE_CONTINUE;
    }

    DBG("Connected to DBus socket %s", app->address);
    app->connect_source = 0;
    return G_SOURCE_REMOVE;
}

static void
dbus_deinit(
        App *app)
{
    if (app->connect_source) {
        g_source_remove(app->connect_source);
        app->connect_source = 0;
    }

    if (app->dbus) {
        g_object_unref(app->dbus);
        app->dbus= NULL;
    }
}

static void
dbus_init_delayed(
        App *app)
{
    dbus_deinit(app);
    DBG("Using address: %s", app->address);
    app->connect_source = g_timeout_add_seconds(1, dbus_init_cb, app);
}

static void
am_client_remove_slot(
        App *app,
        const gchar *slot_name)
{
    GSList *i;

    for (i = app->clients; i; i = i->next) {
        AmClient *am = i->data;

        if (!g_strcmp0(slot_name, am->slot)) {
            app->clients = g_slist_delete_link(app->clients, i);
            am_client_free(am);
            break;
        }
    }
}

static void
parse_key(
        App *app,
        GKeyFile *config,
        const char *key)
{
    if (g_key_file_has_key(config, key, "transport", NULL)) {
        gchar *value;
        gchar *name;

        value = g_key_file_get_value(config, key, "transport", NULL);
        if (g_str_has_prefix(value, "binder:name")) {
            name = g_strrstr(value, "=");
            if (name && strlen(name) > 1) {
                name++;
                am_client_remove_slot(app, name);
                app->clients = g_slist_append(app->clients,
                                              am_client_new(app, name));
            }
        }
    }
}

static void
parse_slots_from_file(
        App *app,
        const gchar *filename)
{
    GKeyFile *config;

    config = g_key_file_new();
    if (g_key_file_load_from_file(config,
                                  filename,
                                  G_KEY_FILE_NONE,
                                  NULL)) {
        gint i;
        for (i = 0; i < OFONO_RIL_SLOTS_MAX; i++) {
            gchar *key = g_strdup_printf("ril_%d", i);
            parse_key(app, config, key);
            g_free(key);
        }
    }

    g_key_file_unref(config);
}

static void
app_parse_all_slots(
        App *app)
{
    GDir *config_dir;

    parse_slots_from_file(app, OFONO_RIL_SUBSCRIPTION_CONF);
    if ((config_dir = g_dir_open(OFONO_RIL_SUBSCRIPTION_D, 0, NULL))) {
        const gchar *filename;
        while ((filename = g_dir_read_name(config_dir))) {
            if (g_str_has_suffix(filename, ".conf")) {
                gchar *path = g_strdup_printf(OFONO_RIL_SUBSCRIPTION_D "/%s", filename);
                parse_slots_from_file(app, path);
                g_free(path);
            }
        }
        g_dir_close(config_dir);
    }
}

static gboolean
app_init(
        App* app,
        int argc,
        char* argv[])
{
    guint level = 0;
    gboolean ok = FALSE;
    GError* error = NULL;
    GOptionContext* options;
    gboolean verbose = FALSE;

    GOptionEntry entries[] = {
        { "standalone", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
          &standalone, "Standalone execution.", NULL },
        { "verbose", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
          &verbose, "Enable verbose output", NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    options = g_option_context_new("<PulseAudio DBus address>");

    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDOUT, pname);
    gutil_log_default.level = GLOG_LEVEL_ERR;
    log_init(&level);

    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        if (verbose || level == PULSE_LOG_LEVEL_DEBUG)
            gutil_log_default.level = GLOG_LEVEL_VERBOSE;

        app->loop = g_main_loop_new(NULL, TRUE);
        app->sm = gbinder_servicemanager_new(BINDER_DEVICE);

        if (argc > 1) {
            app->address = g_strdup(argv[1]);
            app->ret = RET_OK;
            dbus_init_delayed(app);
            app_parse_all_slots(app);
            ok = TRUE;
        }
    } else {
        ERR("Options: %s", error->message);
        g_error_free(error);
    }
    g_option_context_free(options);

    if (!app->address)
        ERR("Address is not defined for %s", pname);

    return ok;
}

static void
app_deinit(
        App *app)
{
    dbus_deinit(app);
    g_free(app->address);
}

int main(int argc, char* argv[])
{
    App app;

    memset(&app, 0, sizeof(app));
    app.ret = RET_INVARG;

    if (app_init(&app, argc, argv)) {
        if (gbinder_servicemanager_wait(app.sm, -1))
            app_run(&app);

        g_main_loop_unref(app.loop);
        g_slist_free_full(app.clients, am_client_free);
        gbinder_servicemanager_unref(app.sm);
    }

    app_deinit(&app);
    return app.ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
