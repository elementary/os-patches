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

#ifndef __INDICATOR_SESSION_ACTIONS_DBUS_H__
#define __INDICATOR_SESSION_ACTIONS_DBUS_H__

#include <glib.h>
#include <glib-object.h>

#include "../actions.h" /* parent class */
#include "dbus-login1-manager.h"
#include "dbus-login1-seat.h"
#include "dbus-display-manager.h"


G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_ACTIONS_DBUS          (indicator_session_actions_dbus_get_type())
#define INDICATOR_SESSION_ACTIONS_DBUS(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_ACTIONS_DBUS, IndicatorSessionActionsDbus))
#define INDICATOR_SESSION_ACTIONS_DBUS_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_ACTIONS_DBUS, IndicatorSessionActionsDbusClass))
#define INDICATOR_IS_SESSION_ACTIONS_DBUS(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_ACTIONS_DBUS))

typedef struct _IndicatorSessionActionsDbus        IndicatorSessionActionsDbus;
typedef struct _IndicatorSessionActionsDbusPriv    IndicatorSessionActionsDbusPriv;
typedef struct _IndicatorSessionActionsDbusClass   IndicatorSessionActionsDbusClass;

/**
 * An implementation of IndicatorSessionActions that gets its user information
 * from org.freedesktop.login1 org.freedesktop.DisplayManager over DBus.
 */
struct _IndicatorSessionActionsDbus
{
  /*< private >*/
  IndicatorSessionActions parent;
  IndicatorSessionActionsDbusPriv * priv;
};

struct _IndicatorSessionActionsDbusClass
{
  IndicatorSessionActionsClass parent_class;
};

GType indicator_session_actions_dbus_get_type (void);

IndicatorSessionActions * indicator_session_actions_dbus_new (void);

void indicator_session_actions_dbus_set_proxies (IndicatorSessionActionsDbus * self,
                                                 Login1Manager               * login1_manager,
                                                 Login1Seat                  * login1_seat,
                                                 DisplayManagerSeat          * dm_seat);
                                                 

G_END_DECLS

#endif /* __INDICATOR_SESSION_ACTIONS_DBUS_H__ */
