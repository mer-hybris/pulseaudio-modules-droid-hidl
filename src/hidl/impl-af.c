/*
 * Copyright (C) 2019 Jolla Ltd.
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

#include "common.h"
#include "impl.h"
#include "logging.h"
#include "dbus-comms.h"

#define BINDER_DEVICE               GBINDER_DEFAULT_BINDER
#define SERVICE_NAME                "media.audio_flinger"
#define SERVICE_IFACE               "android.media.IAudioFlinger"


enum af_methods {
    AF_SET_PARAMETERS = GBINDER_FIRST_CALL_TRANSACTION, /* + starting index */
    AF_GET_PARAMETERS,
    AF_REGISTER_CLIENT,
};

typedef struct af_app AfApp;

struct af_app {
    GMainLoop* loop;
    const AppConfig *config;
    gulong presence_id;
    GBinderServiceManager* sm;
    GBinderLocalObject *local;
    DBusComms *dbus;
};

static AfApp _app;

static guint
binder_idx(
        AfApp *app,
        guint idx)
{
    return app->config->binder_index + idx;
}

static GBinderLocalReply *app_reply(GBinderLocalObject* obj,
                                    GBinderRemoteRequest* req,
                                    guint code,
                                    guint flags,
                                    int* status,
                                    void* user_data)
{
    AfApp* app = user_data;
    const char* iface;
    GBinderLocalReply* reply = NULL;

    iface = gbinder_remote_request_interface(req);
    if (g_strcmp0(iface, SERVICE_IFACE)) {
        ERR("Unexpected interface \"%s\"", iface);
        *status = -1;
        return NULL;
    }

    if (code == binder_idx(app, AF_SET_PARAMETERS)) {
        GBinderReader reader;
        int token;
        int iohandle;
        const char *key_value_pairs;

        gbinder_remote_request_init_reader(req, &reader);
        gbinder_reader_read_int32(&reader, &token);
        gbinder_reader_read_int32(&reader, &iohandle);
        key_value_pairs = gbinder_reader_read_string8(&reader);

        DBG("(%d) setParameters(%d, \"%s\")", token, iohandle, key_value_pairs);
        dbus_comms_set_parameters(app->dbus, key_value_pairs);

        reply = gbinder_local_object_new_reply(obj);
        gbinder_local_reply_append_int32(reply, 0);
        *status = 0;
    } else if (code == binder_idx(app, AF_GET_PARAMETERS)) {
        GBinderReader reader;
        int token;
        int iohandle;
        const char *keys;
        char *key_value_pairs = NULL;

        gbinder_remote_request_init_reader(req, &reader);
        gbinder_reader_read_int32(&reader, &token);
        gbinder_reader_read_int32(&reader, &iohandle);
        keys = gbinder_reader_read_string8(&reader);

        dbus_comms_get_parameters(app->dbus, keys, &key_value_pairs);
        DBG("(%d) getParameters(%d, \"%s\"): \"%s\"", token, iohandle, keys, key_value_pairs);

        reply = gbinder_local_object_new_reply(obj);
        gbinder_local_reply_append_string8(reply, key_value_pairs ? key_value_pairs : "");
        g_free(key_value_pairs);
        *status = 0;
    } else if (code == binder_idx(app, AF_REGISTER_CLIENT)) {
            DBG("register client");
            *status = 0;
    } else {
        ERR("Unknown code (%u)", code);
        *status = 0;
    }

    return reply;
}

static void app_add_service_done(GBinderServiceManager* sm,
                                 int status,
                                 void *user_data)
{
    AfApp *app = user_data;

    if (status == GBINDER_STATUS_OK) {
        DBG("Added " SERVICE_NAME);
    } else {
        ERR("Failed to add " SERVICE_NAME " (%d)", status);
        g_main_loop_quit(app->loop);
    }
}

static void sm_presence_handler(GBinderServiceManager* sm,
                                void* user_data)
{
    AfApp* app = user_data;

    if (gbinder_servicemanager_is_present(app->sm)) {
        DBG("Service manager has reappeared.");
        gbinder_servicemanager_add_service(app->sm, SERVICE_NAME, app->local,
                                           app_add_service_done, app);
    } else {
        DBG("Service manager has died.");
    }
}

static void
dbus_connected_cb(
        DBusComms *c,
        gboolean connected,
        void *userdata)
{
    AfApp *app = userdata;

    if (connected) {
        DBG("DBus up, connect service");
        gbinder_servicemanager_add_service(app->sm, SERVICE_NAME, app->local,
                                           app_add_service_done, app);
    }
}

gboolean
app_af_init(
        GMainLoop *mainloop,
        const AppConfig *config)
{
    memset(&_app, 0, sizeof(_app));
    _app.loop = mainloop;
    _app.config = config;
    _app.sm = gbinder_servicemanager_new(BINDER_DEVICE);
    _app.local = gbinder_servicemanager_new_local_object(_app.sm,
                                                         SERVICE_IFACE,
                                                         app_reply,
                                                         &_app);
    _app.presence_id = gbinder_servicemanager_add_presence_handler(_app.sm,
                                                                   sm_presence_handler,
                                                                   &_app);
    _app.dbus = dbus_comms_new(_app.config->address);
    dbus_comms_init_delayed(_app.dbus, dbus_connected_cb, &_app);

    return TRUE;
}

gboolean
app_af_wait(
        void)
{
    return gbinder_servicemanager_wait(_app.sm, -1);
}

gint
app_af_done(
        void)
{
    if (_app.dbus)
        dbus_comms_done(_app.dbus);
    if (_app.sm)
        gbinder_servicemanager_unref(_app.sm);

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
