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

#ifndef _HIDL_HELPER_IMPL_
#define _HIDL_HELPER_IMPL_

enum app_type {
    APP_HIDL,
    APP_AF,
    APP_MAX
};

typedef struct app_config {
    gchar *address;
    gboolean verbose;
    gint binder_index;
} AppConfig;

typedef gboolean (*app_init_cb)(GMainLoop *mainloop, const AppConfig *config);
typedef gboolean (*app_wait_cb)(void);
typedef gint     (*app_done_cb)(void);

typedef struct app_implementation {
    const char *name;
    app_init_cb init;
    app_wait_cb wait;
    app_done_cb done;
} AppImplementation;

gboolean
app_af_init(
        GMainLoop *mainloop,
        const AppConfig *config);

gboolean
app_af_wait(
        void);

gint
app_af_done(
        void);


gboolean
app_hidl_init(
        GMainLoop *mainloop,
        const AppConfig *config);

gboolean
app_hidl_wait(
        void);

gint
app_hidl_done(
        void);

#endif
