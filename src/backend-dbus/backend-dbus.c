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

#include "actions.h"
#include "backend-dbus.h"
#include "guest.h"
#include "users.h"
#include "utils.h"

struct dbus_world_data
{
  GCancellable                * cancellable;
  IndicatorSessionActionsDbus * actions;
  IndicatorSessionUsersDbus   * users;
  IndicatorSessionGuestDbus   * guest;
};

static void 
on_proxies_ready (Login1Manager      * login1_manager,
                  Login1Seat         * login1_seat,
                  DisplayManagerSeat * display_manager_seat, 
                  Accounts           * account_manager,
                  GCancellable       * cancellable,
                  gpointer             gdata)
{
  struct dbus_world_data * data = gdata;

  if (!g_cancellable_is_cancelled (cancellable))
    {
      if (data->actions != NULL)
        indicator_session_actions_dbus_set_proxies (data->actions,
                                                    login1_manager,
                                                    login1_seat,
                                                    display_manager_seat);

      if (data->users != NULL)
        indicator_session_users_dbus_set_proxies (data->users,
                                                  login1_manager,
                                                  login1_seat,
                                                  display_manager_seat,
                                                  account_manager);

      if (data->guest != NULL)
        indicator_session_guest_dbus_set_proxies (data->guest,
                                                  login1_manager,
                                                  login1_seat,
                                                  display_manager_seat);
    }

  g_free (data);
}

/***
****
***/

void
backend_get (GCancellable             * cancellable,
             IndicatorSessionActions ** setme_actions,
             IndicatorSessionUsers   ** setme_users,
             IndicatorSessionGuest   ** setme_guest)
{
  struct dbus_world_data * data;

  data = g_new0 (struct dbus_world_data, 1);

  if (setme_actions != NULL)
    {
      IndicatorSessionActions * actions;
      actions = indicator_session_actions_dbus_new ();
      data->actions = INDICATOR_SESSION_ACTIONS_DBUS (actions);

      *setme_actions = actions;
    }

  if (setme_users != NULL)
    {
      IndicatorSessionUsers * users;
      users = indicator_session_users_dbus_new ();
      data->users = INDICATOR_SESSION_USERS_DBUS (users);

      *setme_users = users;
    }

  if (setme_guest != NULL)
    {
      IndicatorSessionGuest * guest;
      guest = indicator_session_guest_dbus_new ();
      data->guest = INDICATOR_SESSION_GUEST_DBUS (guest);

      *setme_guest = guest;
    }

  data->cancellable = g_object_ref (cancellable);

  indicator_session_util_get_session_proxies (on_proxies_ready,
                                              data->cancellable,
                                              data);
}
