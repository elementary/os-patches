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

#ifndef __USERS_DBUS_H__
#define __USERS_DBUS_H__

#include <glib.h>
#include <glib-object.h>

#include "../users.h" /* parent class */
#include "dbus-accounts.h"
#include "dbus-login1-manager.h"
#include "dbus-login1-seat.h"
#include "dbus-display-manager.h"

G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_USERS_DBUS          (indicator_session_users_dbus_get_type())
#define INDICATOR_SESSION_USERS_DBUS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_USERS_DBUS, IndicatorSessionUsersDbus))
#define INDICATOR_SESSION_USERS_DBUS_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_USERS_DBUS, IndicatorSessionUsersDbusClass))
#define INDICATOR_IS_SESSION_USERS_DBUS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_USERS_DBUS))

typedef struct _IndicatorSessionUsersDbus        IndicatorSessionUsersDbus;
typedef struct _IndicatorSessionUsersDbusPriv    IndicatorSessionUsersDbusPriv;
typedef struct _IndicatorSessionUsersDbusClass   IndicatorSessionUsersDbusClass;

/**
 * An implementation of IndicatorSessionUsers that gets its user information
 * from org.freedesktop.login1 and org.freedesktop.Accounts over DBus.
 */
struct _IndicatorSessionUsersDbus
{
  /*< private >*/
  IndicatorSessionUsers parent;
  IndicatorSessionUsersDbusPriv * priv;
};

struct _IndicatorSessionUsersDbusClass
{
  IndicatorSessionUsersClass parent_class;
};

GType indicator_session_users_dbus_get_type (void);

IndicatorSessionUsers * indicator_session_users_dbus_new (void);

void indicator_session_users_dbus_set_proxies (IndicatorSessionUsersDbus *,
                                               Login1Manager             *,
                                               Login1Seat                *,
                                               DisplayManagerSeat        *,
                                               Accounts                  *);



G_END_DECLS

#endif
