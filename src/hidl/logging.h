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

#ifndef _HIDL_HELPER_LOGGING_
#define _HIDL_HELPER_LOGGING_

#include <gutil_log.h>

extern gboolean _app_standalone;

#define DBGP(...)   do {                                                    \
                        printf(__VA_ARGS__);                                \
                        printf("\n");                                       \
                        fflush(stdout);                                     \
                    } while(0)

#define DBG(...)    do {                                                    \
                        if (gutil_log_default.level == GLOG_LEVEL_VERBOSE) {\
                            if (_app_standalone)                            \
                                GDEBUG(__VA_ARGS__);                        \
                            else                                            \
                                DBGP(__VA_ARGS__);                          \
                        }                                                   \
                    } while(0)

#define ERR(...)    do {                                                    \
                        if (_app_standalone)                                \
                            GERR(__VA_ARGS__);                              \
                        else                                                \
                            DBGP(__VA_ARGS__);                              \
                    } while(0)

#endif
