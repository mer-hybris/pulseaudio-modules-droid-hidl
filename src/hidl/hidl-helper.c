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

#include <android-config.h>
#if !defined(ANDROID_VERSION_MAJOR)
#error "ANDROID_VERSION_MAJOR not defined. Did you get your headers via extract-headers.sh?"
#endif

#if ANDROID_VERSION_MAJOR <= 7
#define DEFAULT_TYPE_STR    "af"
#define DEFAULT_BIND_IDX    (17)
#elif ANDROID_VERSION_MAJOR <= 8
#define DEFAULT_TYPE_STR    "af"
#define DEFAULT_BIND_IDX    (18)
#else
#define DEFAULT_TYPE_STR    "hidl"
#define DEFAULT_BIND_IDX    (18)
#endif

#include "common.h"
#include "impl.h"
#include "logging.h"
gboolean _app_standalone = FALSE;

#define RET_OK                      (0)
#define RET_INVARG                  (2)

static const char pname[] = HELPER_NAME;

typedef struct app App;

struct app {
    GMainLoop* loop;
    int ret;
    gint type;
    AppConfig config;
};

static AppImplementation app_implementations[APP_MAX] = {
    { "hidl",   app_hidl_init,  app_hidl_wait,  app_hidl_done   },
    { "af",     app_af_init,    app_af_wait,    app_af_done     },
};


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

    g_main_loop_run(app->loop);

    g_source_remove(sigtrm);
    g_source_remove(sigint);
}

static gboolean
parse_app_type(
        App *app,
        const gchar *type_str)
{
    guint i;

    if (!type_str)
        type_str = DEFAULT_TYPE_STR;

    for (i = 0; i < sizeof(app_implementations) / sizeof(app_implementations[0]); i++) {
        if (!g_strcmp0(type_str, app_implementations[i].name)) {
            DBG("Using %s implementation", app_implementations[i].name);
            app->type = i;
            break;
        }
    }

    return app->type >= 0;
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
    gchar *type_str = NULL;
    GOptionContext* options = NULL;

    GOptionEntry entries[] = {
        { "type", 't', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING,
          &type_str, "Passthrough type, af/hidl (default " DEFAULT_TYPE_STR ")", NULL },
        { "standalone", 's', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
          &_app_standalone, "Standalone execution.", NULL },
        { "verbose", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE,
          &app->config.verbose, "Enable verbose output", NULL },
        { NULL, 0, 0, 0, NULL, NULL, NULL }
    };

    options = g_option_context_new("<PulseAudio DBus address>");

    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDOUT, pname);
    gutil_log_default.level = GLOG_LEVEL_ERR;
    log_init(&level);

    g_option_context_add_main_entries(options, entries, NULL);
    if (!g_option_context_parse(options, &argc, &argv, &error))
        goto fail;

    if (app->config.verbose || level == PULSE_LOG_LEVEL_DEBUG)
        gutil_log_default.level = GLOG_LEVEL_VERBOSE;

    if (argc <= 1)
        goto fail;

    app->config.address = g_strdup(argv[1]);
    app->config.binder_index = DEFAULT_BIND_IDX;

    if (!parse_app_type(app, type_str)) {
        ERR("Unknown type '%s'", type_str ? type_str : DEFAULT_TYPE_STR);
        goto fail;
    }
    g_free(type_str);

    app->loop = g_main_loop_new(NULL, TRUE);
    if (!app_implementations[app->type].init(app->loop, &app->config))
        goto fail;

    ok = TRUE;
    app->ret = RET_OK;
    g_option_context_free(options);

    return ok;

fail:
    g_free(type_str);

    if (options)
        g_option_context_free(options);

    if (error) {
        ERR("Options: %s", error->message);
        g_error_free(error);
    }

    if (!app->config.address)
        ERR("Address is not defined for %s", pname);

    return ok;
}

static void
app_deinit(
        App *app)
{
    if (app->type >= 0 && app->ret == RET_OK)
        app->ret = app_implementations[app->type].done();
    g_free(app->config.address);
}

int main(int argc, char* argv[])
{
    App app;

    memset(&app, 0, sizeof(app));
    app.ret = RET_INVARG;
    app.type = -1;

    if (app_init(&app, argc, argv)) {
        if (app_implementations[app.type].wait())
            app_run(&app);
        g_main_loop_unref(app.loop);
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
