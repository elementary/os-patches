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

#include "utils.h"

/***
**** indicator_session_util_get_session_proxies()
***/

struct session_proxy_data
{
  Login1Manager * login1_manager;
  Login1Seat * login1_seat;
  DisplayManagerSeat * dm_seat;
  Accounts * account_manager;

  GCancellable * cancellable;
  int pending;

  indicator_session_util_session_proxies_func callback;
  gpointer user_data;
};


static void
on_proxy_ready_impl (struct session_proxy_data * data,
                     gsize                       member_offset,
                     GError                    * err,
                     gpointer                    proxy)
{
  if (err != NULL)
    {
      if (!g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s %s: %s", G_STRLOC, G_STRFUNC, err->message);

      g_error_free (err);
    }
  else
    {
      *((gpointer*)G_STRUCT_MEMBER_P(data, member_offset)) = proxy;
    }

  if (!--data->pending)
    {
      data->callback (data->login1_manager,
                      data->login1_seat,
                      data->dm_seat,
                      data->account_manager,
                      data->cancellable,
                      data->user_data);

      g_clear_object (&data->login1_manager);
      g_clear_object (&data->login1_seat);
      g_clear_object (&data->dm_seat);
      g_clear_object (&data->account_manager);
      g_clear_object (&data->cancellable);
      g_free (data);
    }
}
    
static void
on_display_manager_seat_proxy_ready (GObject      * o G_GNUC_UNUSED,
                                     GAsyncResult * res,
                                     gpointer       gdata)
{
  gsize offset = G_STRUCT_OFFSET (struct session_proxy_data, dm_seat);
  GError * err = NULL;
  gpointer proxy = display_manager_seat_proxy_new_for_bus_finish (res, &err);
  on_proxy_ready_impl (gdata, offset, err, proxy);
}

static void
on_login1_seat_ready (GObject      * o G_GNUC_UNUSED,
                      GAsyncResult * res,
                      gpointer       gdata)
{
  gsize offset = G_STRUCT_OFFSET (struct session_proxy_data, login1_seat);
  GError * err = NULL;
  gpointer proxy = login1_seat_proxy_new_for_bus_finish (res,  &err);
  on_proxy_ready_impl (gdata, offset, err, proxy);
}

static void
on_login1_manager_ready (GObject      * o G_GNUC_UNUSED,
                         GAsyncResult * res,
                         gpointer       gdata)
{
  gsize offset = G_STRUCT_OFFSET (struct session_proxy_data, login1_manager);
  GError * err = NULL;
  gpointer proxy = login1_manager_proxy_new_for_bus_finish (res, &err);
  on_proxy_ready_impl (gdata, offset, err, proxy);
}

static void
on_accounts_proxy_ready (GObject      * o G_GNUC_UNUSED,
                         GAsyncResult * res,
                         gpointer       gdata)
{
  gsize offset = G_STRUCT_OFFSET (struct session_proxy_data, account_manager);
  GError * err = NULL;
  gpointer proxy = accounts_proxy_new_for_bus_finish (res, &err);
  on_proxy_ready_impl (gdata, offset, err, proxy);
}

/* helper utility to get the dbus proxies used by the backend-dbus classes */
void
indicator_session_util_get_session_proxies (
                     indicator_session_util_session_proxies_func   func,
                     GCancellable                                * cancellable,
                     gpointer                                      user_data)
{
  struct session_proxy_data * data;
  const char * str;

  data = g_new0 (struct session_proxy_data, 1);
  data->callback = func;
  data->user_data = user_data;
  data->cancellable = g_object_ref (cancellable);

  /* login1 */
  data->pending++;
  login1_manager_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                    G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                    "org.freedesktop.login1",
                                    "/org/freedesktop/login1",
                                    data->cancellable,
                                    on_login1_manager_ready, data);

  /* login1 seat */
  if ((str = g_getenv ("XDG_SEAT")))
    {
      char * path;
      data->pending++;
      path = g_strconcat ("/org/freedesktop/login1/seat/", str, NULL);
      login1_seat_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                 G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                 "org.freedesktop.login1",
                                 path,
                                 data->cancellable,
                                 on_login1_seat_ready,
                                 data);
      g_free (path);
    }

  /* Accounts */
  data->pending++;
  accounts_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                              G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                              "org.freedesktop.Accounts",
                              "/org/freedesktop/Accounts",
                              data->cancellable,
                              on_accounts_proxy_ready, data);

  /* DisplayManager seat */
  if ((str = g_getenv ("XDG_SEAT_PATH")))
    {
      data->pending++;
      display_manager_seat_proxy_new_for_bus (
                               G_BUS_TYPE_SYSTEM,
                               G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                               "org.freedesktop.DisplayManager",
                               str,
                               data->cancellable,
                               on_display_manager_seat_proxy_ready, data);
    }
}
