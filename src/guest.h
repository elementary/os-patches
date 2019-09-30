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

#ifndef __GUEST_H__
#define __GUEST_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_GUEST          (indicator_session_guest_get_type())
#define INDICATOR_SESSION_GUEST(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_GUEST, IndicatorSessionGuest))
#define INDICATOR_SESSION_GUEST_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_GUEST, IndicatorSessionGuestClass))
#define INDICATOR_SESSION_GUEST_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST ((k), INDICATOR_TYPE_SESSION_GUEST, IndicatorSessionGuestClass))
#define INDICATOR_IS_SESSION_GUEST(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_GUEST))

typedef struct _IndicatorSessionGuest        IndicatorSessionGuest;
typedef struct _IndicatorSessionGuestClass   IndicatorSessionGuestClass;

GType indicator_session_guest_get_type (void);

/**
 * A base class for getting state information about the system's guest user.
 * Use backend.h's get_backend() to get an instance.
 */
struct _IndicatorSessionGuest
{
  /*< private >*/
  GObject parent;
};

/* properties */
#define INDICATOR_SESSION_GUEST_PROPERTY_ALLOWED   "guest-is-allowed"
#define INDICATOR_SESSION_GUEST_PROPERTY_LOGGED_IN "guest-is-logged-in"
#define INDICATOR_SESSION_GUEST_PROPERTY_ACTIVE    "guest-is-active-session"

struct _IndicatorSessionGuestClass
{
  GObjectClass parent_class;

  /* virtual functions */
  gboolean (* is_allowed)      (IndicatorSessionGuest * self);
  gboolean (* is_logged_in)    (IndicatorSessionGuest * self);
  gboolean (* is_active)       (IndicatorSessionGuest * self);
  void     (* switch_to_guest) (IndicatorSessionGuest * self);
};

gboolean indicator_session_guest_is_allowed   (IndicatorSessionGuest * self);
gboolean indicator_session_guest_is_logged_in (IndicatorSessionGuest * self);
gboolean indicator_session_guest_is_active    (IndicatorSessionGuest * self);

void indicator_session_guest_switch_to_guest  (IndicatorSessionGuest * self);

/**
 * Emit 'notify' signals for the corresponding properties.
 * These functions should only be called by IndicatorSessionGuest implementations.
 */
void indicator_session_guest_notify_allowed   (IndicatorSessionGuest * self);
void indicator_session_guest_notify_logged_in (IndicatorSessionGuest * self);
void indicator_session_guest_notify_active    (IndicatorSessionGuest * self);


G_END_DECLS

#endif
