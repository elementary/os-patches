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

#include "guest.h"

struct _IndicatorSessionGuestDbusPriv
{
  GCancellable * cancellable;

  Login1Manager * login1_manager;
  Login1Seat * login1_seat;
  DisplayManagerSeat * dm_seat;

  gboolean guest_is_active;
  gboolean guest_is_logged_in;
  gboolean guest_is_allowed;
};

typedef IndicatorSessionGuestDbusPriv priv_t;

G_DEFINE_TYPE (IndicatorSessionGuestDbus,
               indicator_session_guest_dbus,
               INDICATOR_TYPE_SESSION_GUEST)

/***
****
***/

static void
set_guest_is_allowed_flag (IndicatorSessionGuestDbus * self, gboolean b)
{
  priv_t * p = self->priv;

  if (p->guest_is_allowed != b)
    {
      p->guest_is_allowed = b;

      indicator_session_guest_notify_allowed (INDICATOR_SESSION_GUEST (self));
    }
}

static void
set_guest_is_logged_in_flag (IndicatorSessionGuestDbus * self, gboolean b)
{
  priv_t * p = self->priv;

  if (p->guest_is_logged_in != b)
    {
      p->guest_is_logged_in = b;

      indicator_session_guest_notify_logged_in (INDICATOR_SESSION_GUEST (self));
    }
}

static void
set_guest_is_active_flag (IndicatorSessionGuestDbus * self, gboolean b)
{
  priv_t * p = self->priv;

  if (p->guest_is_active != b)
    {
      p->guest_is_active = b;

      indicator_session_guest_notify_active (INDICATOR_SESSION_GUEST(self));
    }
}

/***
****
***/

/* walk the sessions to see if guest is logged in or active */
static void
on_login1_manager_session_list_ready (GObject      * o,
                                      GAsyncResult * res,
                                      gpointer       gself)
{
  GVariant * sessions;
  GError * err;

  sessions = NULL;
  err = NULL;
  login1_manager_call_list_sessions_finish (LOGIN1_MANAGER(o),
                                            &sessions,
                                            res,
                                            &err);
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s: %s", G_STRFUNC, err->message);

      g_error_free (err);
    }
  else
    {
      const gchar * const current_seat_id = g_getenv ("XDG_SEAT");
      const gchar * const current_session_id = g_getenv ("XDG_SESSION_ID");
      gboolean is_logged_in = FALSE;
      gboolean is_active = FALSE;
      const gchar * session_id = NULL;
      guint32 uid = 0;
      const gchar * user_name = NULL;
      const gchar * seat_id = NULL;
      GVariantIter iter;

      g_variant_iter_init (&iter, sessions);
      while (g_variant_iter_loop (&iter, "(&su&s&so)", &session_id,
                                                        &uid,
                                                        &user_name,
                                                        &seat_id,
                                                        NULL))
        {
          gboolean is_current_session;
          gboolean is_guest;

          is_current_session = !g_strcmp0 (current_seat_id, seat_id)
                            && !g_strcmp0 (current_session_id, session_id);

          is_guest = g_str_has_prefix (user_name, "guest-")
                  && (uid < 1000);

          if (is_guest)
            {
              is_logged_in = TRUE;

              is_active = is_current_session;
            }
        }

      set_guest_is_logged_in_flag (gself, is_logged_in);
      set_guest_is_active_flag (gself, is_active);

      g_variant_unref (sessions);
    }
}

static void
update_session_list (IndicatorSessionGuestDbus * self)
{
  priv_t * p = self->priv;

  if (p->login1_manager != NULL)
    {
      login1_manager_call_list_sessions (p->login1_manager,
                                         p->cancellable,
                                         on_login1_manager_session_list_ready,
                                         self);
    }
}

static void
set_login1_manager (IndicatorSessionGuestDbus * self,
                    Login1Manager             * login1_manager)
{
  priv_t * p = self->priv;

  if (p->login1_manager != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->login1_manager, self);

      g_clear_object (&p->login1_manager);
    }

  if (login1_manager != NULL)
    {
      p->login1_manager = g_object_ref (login1_manager);

      g_signal_connect_swapped (login1_manager, "session-new",
                                G_CALLBACK(update_session_list), self);
      g_signal_connect_swapped (login1_manager, "session-removed",
                                G_CALLBACK(update_session_list), self);
      update_session_list (self);
    }
}

static void
set_login1_seat (IndicatorSessionGuestDbus * self,
                 Login1Seat                * login1_seat)
{
  priv_t * p = self->priv;

  if (p->login1_seat != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->login1_seat, self);
      g_clear_object (&p->login1_seat);
    }

  if (login1_seat != NULL)
    {
      p->login1_seat = g_object_ref (login1_seat);

      g_signal_connect_swapped (login1_seat, "notify::active-session",
                                G_CALLBACK(update_session_list), self);
      update_session_list (self);
    }
}

/***
****
***/

static void
on_switch_to_guest_done (GObject      * o,
                         GAsyncResult * res,
                         gpointer       unused G_GNUC_UNUSED)
{
  GError * err;

  err = NULL;
  display_manager_seat_call_switch_to_guest_finish (DISPLAY_MANAGER_SEAT(o),
                                                    res,
                                                    &err);
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s %s: %s", G_STRLOC, G_STRFUNC, err->message);

      g_error_free (err);
    }
}

static void
my_switch_to_guest (IndicatorSessionGuest * guest)
{
  IndicatorSessionGuestDbus * self = INDICATOR_SESSION_GUEST_DBUS(guest);

  g_return_if_fail (self != NULL);
  g_return_if_fail (self->priv->dm_seat != NULL);

  display_manager_seat_call_switch_to_guest (self->priv->dm_seat,
                                             "",
                                             self->priv->cancellable,
                                             on_switch_to_guest_done,
                                             self);
}

static void
on_notify_has_guest_account (DisplayManagerSeat        * dm_seat,
                             GParamSpec                * pspec G_GNUC_UNUSED,
                             IndicatorSessionGuestDbus * self)
{
  gboolean guest_exists = display_manager_seat_get_has_guest_account (dm_seat);
  set_guest_is_allowed_flag (self, guest_exists);
}

static void
set_display_manager_seat (IndicatorSessionGuestDbus * self,
                          DisplayManagerSeat        * dm_seat)
{
  priv_t * p = self->priv;

  if (p->dm_seat != NULL)
    {
      g_signal_handlers_disconnect_by_data (p->dm_seat, self);

      g_clear_object (&p->dm_seat);
    }

  if (dm_seat != NULL)
    {
      p->dm_seat = g_object_ref (dm_seat);

      g_signal_connect (dm_seat, "notify::has-guest-account",
                        G_CALLBACK(on_notify_has_guest_account), self);
      on_notify_has_guest_account (dm_seat, NULL, self);
    }
}

/***
****  IndiatorSessionGuest Virtual Functions
***/

static gboolean
my_is_allowed (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST_DBUS(self), FALSE);

  return INDICATOR_SESSION_GUEST_DBUS(self)->priv->guest_is_allowed;
}

static gboolean
my_is_logged_in (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST_DBUS(self), FALSE);

  return INDICATOR_SESSION_GUEST_DBUS(self)->priv->guest_is_logged_in;
}

static gboolean
my_is_active (IndicatorSessionGuest * self)
{
  g_return_val_if_fail (INDICATOR_IS_SESSION_GUEST_DBUS(self), FALSE);

  return INDICATOR_SESSION_GUEST_DBUS(self)->priv->guest_is_active;
}

/***
****  GObject Virtual Functions
***/

static void
my_dispose (GObject * o)
{
  IndicatorSessionGuestDbus * self = INDICATOR_SESSION_GUEST_DBUS (o);

  if (self->priv->cancellable != NULL)
    {
      g_cancellable_cancel (self->priv->cancellable);
      g_clear_object (&self->priv->cancellable);
    }

  set_login1_seat (self, NULL);
  set_login1_manager (self, NULL);
  set_display_manager_seat (self, NULL);

  G_OBJECT_CLASS (indicator_session_guest_dbus_parent_class)->dispose (o);
}

/***
****  Instantiation
***/

static void
indicator_session_guest_dbus_class_init (IndicatorSessionGuestDbusClass * klass)
{
  GObjectClass * object_class;
  IndicatorSessionGuestClass * guest_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = my_dispose;

  guest_class = INDICATOR_SESSION_GUEST_CLASS (klass);
  guest_class->is_allowed = my_is_allowed;
  guest_class->is_logged_in = my_is_logged_in;
  guest_class->is_active = my_is_active;
  guest_class->switch_to_guest = my_switch_to_guest;

  g_type_class_add_private (klass, sizeof (IndicatorSessionGuestDbusPriv));
}

static void
indicator_session_guest_dbus_init (IndicatorSessionGuestDbus * self)
{
  priv_t * p;

  p = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                   INDICATOR_TYPE_SESSION_GUEST_DBUS,
                                   IndicatorSessionGuestDbusPriv);
  p->cancellable = g_cancellable_new ();
  self->priv = p;
}

/***
****  Public
***/

IndicatorSessionGuest *
indicator_session_guest_dbus_new (void)
{
  gpointer o = g_object_new (INDICATOR_TYPE_SESSION_GUEST_DBUS, NULL);

  return INDICATOR_SESSION_GUEST (o);
}

void
indicator_session_guest_dbus_set_proxies (IndicatorSessionGuestDbus * self,
                                          Login1Manager             * login1_manager,
                                          Login1Seat                * login1_seat,
                                          DisplayManagerSeat        * dm_seat)
{
  g_return_if_fail (INDICATOR_IS_SESSION_GUEST_DBUS(self));

  set_login1_manager (self, login1_manager);
  set_login1_seat (self, login1_seat);
  set_display_manager_seat (self, dm_seat);
}
