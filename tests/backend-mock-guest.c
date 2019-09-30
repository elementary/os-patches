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

#include <glib.h>

#include "backend-mock-guest.h"

struct _IndicatorSessionGuestMockPriv
{
  gboolean guest_is_active;
  gboolean guest_is_logged_in;
  gboolean guest_is_allowed;
};

typedef IndicatorSessionGuestMockPriv priv_t;

G_DEFINE_TYPE (IndicatorSessionGuestMock,
               indicator_session_guest_mock,
               INDICATOR_TYPE_SESSION_GUEST)

/**
***  IndicatorSessionGuest virtual functions
**/

static gboolean
my_is_allowed (IndicatorSessionGuest * self)
{
  return INDICATOR_SESSION_GUEST_MOCK(self)->priv->guest_is_allowed;
}

static gboolean
my_is_logged_in (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST_MOCK(self), FALSE);

  return INDICATOR_SESSION_GUEST_MOCK(self)->priv->guest_is_logged_in;
}

static gboolean
my_is_active (IndicatorSessionGuest * self)
{
  return INDICATOR_SESSION_GUEST_MOCK(self)->priv->guest_is_active;
}

static void
my_switch_to_guest (IndicatorSessionGuest * self G_GNUC_UNUSED)
{
  g_message ("%s %s FIXME", G_STRLOC, G_STRFUNC);
}

/***
****  GObject virtual Functions
***/

static void
my_dispose (GObject * o)
{
  G_OBJECT_CLASS (indicator_session_guest_mock_parent_class)->dispose (o);
}

static void
my_finalize (GObject * o)
{
  G_OBJECT_CLASS (indicator_session_guest_mock_parent_class)->finalize (o);
}

/***
****  GObject Boilerplate
***/

static void
/* cppcheck-suppress unusedFunction */
indicator_session_guest_mock_class_init (IndicatorSessionGuestMockClass * klass)
{
  GObjectClass * object_class;
  IndicatorSessionGuestClass * guest_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;
  object_class->finalize = my_finalize;

  guest_class = INDICATOR_SESSION_GUEST_CLASS (klass);
  guest_class->is_allowed = my_is_allowed;
  guest_class->is_logged_in = my_is_logged_in;
  guest_class->is_active = my_is_active;
  guest_class->switch_to_guest = my_switch_to_guest;

  g_type_class_add_private (klass, sizeof (IndicatorSessionGuestMockPriv));
}

static void
/* cppcheck-suppress unusedFunction */
indicator_session_guest_mock_init (IndicatorSessionGuestMock * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_SESSION_GUEST_MOCK,
                                   IndicatorSessionGuestMockPriv);
  self->priv = p;

  p->guest_is_allowed = TRUE;
  p->guest_is_active = FALSE;
  p->guest_is_logged_in = FALSE;
}

/***
****  Public
***/

IndicatorSessionGuest *
indicator_session_guest_mock_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_SESSION_GUEST_MOCK, NULL);

  return INDICATOR_SESSION_GUEST (o);
}
