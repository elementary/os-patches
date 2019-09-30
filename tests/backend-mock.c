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

#include "backend-mock.h"
#include "backend-mock-actions.h"
#include "backend-mock-guest.h"
#include "backend-mock-users.h"

GSettings               * mock_settings = NULL;
IndicatorSessionActions * mock_actions  = NULL;
IndicatorSessionUsers   * mock_users    = NULL;
IndicatorSessionGuest   * mock_guest    = NULL;

void
backend_get (GCancellable             * cancellable G_GNUC_UNUSED,
             IndicatorSessionActions ** setme_actions,
             IndicatorSessionUsers   ** setme_users,
             IndicatorSessionGuest   ** setme_guest)
{
  if (setme_actions != NULL)
    *setme_actions = g_object_ref (mock_actions);

  if (setme_users != NULL)
    *setme_users = g_object_ref (mock_users);

  if (setme_guest != NULL)
    *setme_guest = g_object_ref (mock_guest);
}
