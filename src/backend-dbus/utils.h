/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DBUS_UTILS_H__
#define __DBUS_UTILS_H__

#include <glib.h>
#include <glib-object.h>

#include "dbus-accounts.h"
#include "dbus-display-manager.h"
#include "dbus-login1-manager.h"
#include "dbus-login1-seat.h"

typedef void (*indicator_session_util_session_proxies_func)(
                   Login1Manager      * login1_manager,
                   Login1Seat         * login1_seat,
                   DisplayManagerSeat * display_manager_seat,
                   Accounts           * account_manager,
                   GCancellable       * cancellable,
                   gpointer             user_data);

/**
 * Both users-dbus and guest-dbus need some of these proxies.
 * Getting them all involves a lot of steps, so instead of repeating
 * ourselves, the common dbus steps are extracted to this func.
 */
void indicator_session_util_get_session_proxies (
                   indicator_session_util_session_proxies_func   func,
                   GCancellable                                * cancellable,
                   gpointer                                      user_data);

#endif
