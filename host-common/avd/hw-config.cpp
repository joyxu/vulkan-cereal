/* Copyright (C) 2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#include "hw-config.h"
#include "base/export.h"
#include "util.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>

/* the global variable containing the hardware config for this device */
AndroidHwConfig   android_hw[1];

AEMU_EXPORT AndroidHwConfig* aemu_get_android_hw() {
    return (AndroidHwConfig*)(android_hw);
}
