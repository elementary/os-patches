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

#ifndef __GUEST_DBUS_H__
#define __GUEST_DBUS_H__

#include <glib.h>
#include <glib-object.h>

#include "../guest.h" /* parent class */
#include "dbus-login1-manager.h"
#include "dbus-login1-seat.h"
#include "dbus-display-manager.h"


G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_GUEST_DBUS          (indicator_session_guest_dbus_get_type())
#define INDICATOR_SESSION_GUEST_DBUS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_GUEST_DBUS, IndicatorSessionGuestDbus))
#define INDICATOR_SESSION_GUEST_DBUS_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_GUEST_DBUS, IndicatorSessionGuestDbusClass))
#define INDICATOR_IS_SESSION_GUEST_DBUS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_GUEST_DBUS))

typedef struct _IndicatorSessionGuestDbus        IndicatorSessionGuestDbus;
typedef struct _IndicatorSessionGuestDbusPriv    IndicatorSessionGuestDbusPriv;
typedef struct _IndicatorSessionGuestDbusClass   IndicatorSessionGuestDbusClass;

/**
 * An implementation of IndicatorSessionGuest that gets its user information
 * from org.freedesktop.login1 and org.freedesktop.Accounts over DBus.
 */
struct _IndicatorSessionGuestDbus
{
  /*< private >*/
  IndicatorSessionGuest parent;
  IndicatorSessionGuestDbusPriv * priv;
};

struct _IndicatorSessionGuestDbusClass
{
  IndicatorSessionGuestClass parent_class;
};

GType indicator_session_guest_dbus_get_type (void);

IndicatorSessionGuest * indicator_session_guest_dbus_new (void);

void indicator_session_guest_dbus_set_proxies (IndicatorSessionGuestDbus * self,
                                               Login1Manager             * login1_manager,
                                               Login1Seat                * login1_seat,
                                               DisplayManagerSeat        * display_manager_seat);

G_END_DECLS

#endif
