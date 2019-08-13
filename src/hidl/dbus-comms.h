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

#ifndef _HIDL_HELPER_DBUS_COMMS_
#define _HIDL_HELPER_DBUS_COMMS_

typedef struct dbus_comms DBusComms;

DBusComms*
dbus_comms_new(
        const gchar *address);

void
dbus_comms_done(
        DBusComms *c);

typedef void (*dbus_comms_connected_cb)(DBusComms *c, gboolean connected, void *userdata);

void
dbus_comms_init_delayed(
        DBusComms *c,
        dbus_comms_connected_cb cb,
        void *userdata);

gint
dbus_comms_get_parameters(
        DBusComms *c,
        const gchar *keys,
        gchar **reply_values);

gint
dbus_comms_set_parameters(
        DBusComms *c,
        const gchar *key_value_pairs);


#endif
