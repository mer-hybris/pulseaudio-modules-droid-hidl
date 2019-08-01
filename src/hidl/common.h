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

#ifndef __HIDL_PASSTHROUGH_COMMON__
#define __HIDL_PASSTHROUGH_COMMON__

#include <stdlib.h>

#define HELPER_NAME                             "hidl-helper"

#define HIDL_PASSTHROUGH_PATH                   "/org/sailfishos/hidlpassthrough"
#define HIDL_PASSTHROUGH_IFACE                  "org.SailfishOS.HIDLPassthrough"

#define HIDL_PASSTHROUGH_METHOD_GET_PARAMETERS  "get_parameters"
#define HIDL_PASSTHROUGH_METHOD_SET_PARAMETERS  "set_parameters"

#define PULSE_ENV_LOG_LEVEL                     "PULSE_LOG"
#define PULSE_LOG_LEVEL_DEBUG                   (4)

static inline void log_init(unsigned int *level) {
    const char *e;

    if ((e = getenv(PULSE_ENV_LOG_LEVEL))) {
        *level = atoi(e);

        if (*level > PULSE_LOG_LEVEL_DEBUG)
            *level = PULSE_LOG_LEVEL_DEBUG;
    }
}

#endif
