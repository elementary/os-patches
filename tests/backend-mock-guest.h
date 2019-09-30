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

#ifndef __GUEST_MOCK_H__
#define __GUEST_MOCK_H__

#include <glib.h>
#include <glib-object.h>

#include "guest.h" /* parent class */

G_BEGIN_DECLS

#define INDICATOR_TYPE_SESSION_GUEST_MOCK          (indicator_session_guest_mock_get_type())
#define INDICATOR_SESSION_GUEST_MOCK(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), INDICATOR_TYPE_SESSION_GUEST_MOCK, IndicatorSessionGuestMock))
#define INDICATOR_SESSION_GUEST_MOCK_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), INDICATOR_TYPE_SESSION_GUEST_MOCK, IndicatorSessionGuestMockClass))
#define INDICATOR_IS_SESSION_GUEST_MOCK(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), INDICATOR_TYPE_SESSION_GUEST_MOCK))

typedef struct _IndicatorSessionGuestMock        IndicatorSessionGuestMock;
typedef struct _IndicatorSessionGuestMockPriv    IndicatorSessionGuestMockPriv;
typedef struct _IndicatorSessionGuestMockClass   IndicatorSessionGuestMockClass;

/**
 * An implementation of IndicatorSessionGuest that lies about everything.
 */
struct _IndicatorSessionGuestMock
{
  /*< private >*/
  IndicatorSessionGuest parent;
  IndicatorSessionGuestMockPriv * priv;
};

struct _IndicatorSessionGuestMockClass
{
  IndicatorSessionGuestClass parent_class;
};

GType indicator_session_guest_mock_get_type (void);

IndicatorSessionGuest * indicator_session_guest_mock_new (void);

G_END_DECLS

#endif
