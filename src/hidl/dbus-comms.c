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

#include <glib.h>
#include <gio/gio.h>

#include "common.h"
#include "logging.h"
#include "dbus-comms.h"

#define CONNECT_RETRY_TIMEOUT_S (1)

struct dbus_comms {
    gchar *address;
    guint connect_source;
    GDBusConnection *dbus;
    dbus_comms_connected_cb cb;
    void *userdata;
};

static gint
dbus_call(
        DBusComms *c,
        const gchar *method,
        const gchar *args,
        gchar **reply_str)
{
    gint ret = 1;
    GDBusMessage *msg = NULL;
    GDBusMessage *reply = NULL;
    GError *error = NULL;

    g_assert(c);
    g_assert(method);
    g_assert(args);

    if (reply_str)
        *reply_str = NULL;

    if (!c->dbus) {
        ERR("No connection (%s)", c->address);
        goto out;
    }

    msg = g_dbus_message_new_method_call(NULL,
                                         HIDL_PASSTHROUGH_PATH,
                                         HIDL_PASSTHROUGH_IFACE,
                                         method);
    g_dbus_message_set_body(msg, g_variant_new("(s)", args));
    reply = g_dbus_connection_send_message_with_reply_sync(c->dbus,
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

gint
dbus_comms_set_parameters(
        DBusComms *c,
        const gchar *key_value_pairs)
{
    g_assert(c);
    g_assert(key_value_pairs);

    return dbus_call(c, HIDL_PASSTHROUGH_METHOD_SET_PARAMETERS, key_value_pairs, NULL);
}

gint
dbus_comms_get_parameters(
        DBusComms *c,
        const gchar *keys,
        gchar **reply_values)
{
    g_assert(c);
    g_assert(keys);
    g_assert(reply_values);

    return dbus_call(c, HIDL_PASSTHROUGH_METHOD_GET_PARAMETERS, keys, reply_values);
}

static gboolean
dbus_init_cb(
        gpointer user_data)
{
    DBusComms *c = user_data;
    GError *error = NULL;

    c->dbus = g_dbus_connection_new_for_address_sync(c->address,
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                       NULL,    /* observer */
                                                       NULL,    /* cancellable */
                                                       &error);


    if (!c->dbus) {
        DBG("Could not connect to %s, try again in %d seconds...", c->address, CONNECT_RETRY_TIMEOUT_S);
        return G_SOURCE_CONTINUE;
    }

    if (error) {
        DBG("Could not connect to %s: %s", c->address, error->message);
        g_error_free(error);
        return G_SOURCE_CONTINUE;
    }

    DBG("Connected to DBus socket %s", c->address);
    c->connect_source = 0;
    if (c->cb) {
        c->cb(c, TRUE, c->userdata);
    }
    return G_SOURCE_REMOVE;
}

static void
dbus_deinit(
        DBusComms *c)
{
    if (c->connect_source) {
        g_source_remove(c->connect_source);
        c->connect_source = 0;
    }

    if (c->dbus) {
        g_object_unref(c->dbus);
        c->dbus= NULL;
    }
}

void
dbus_comms_init_delayed(
        DBusComms *c,
        dbus_comms_connected_cb cb,
        void *userdata)
{
    dbus_deinit(c);
    DBG("Using address: %s", c->address);
    c->cb = cb;
    c->userdata = userdata;
    c->connect_source = g_timeout_add_seconds(1, dbus_init_cb, c);
}

DBusComms*
dbus_comms_new(
        const gchar *address)
{
    DBusComms *c;

    c = g_new0(DBusComms, 1);
    c->address = g_strdup(address);

    return c;
}

void
dbus_comms_done(
        DBusComms *c)
{
    dbus_deinit(c);
    g_free(c);
}
