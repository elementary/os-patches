/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2019 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include "config.h"

#define PAM_SM_ACCOUNT

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <libmalcontent/malcontent.h>
#include <pwd.h>
#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <syslog.h>


/* Example usage:
 *
 * Here’s an example of a PAM file which uses `pam_malcontent.so`. Note
 * that `pam_malcontent.so` must be listed before `pam_systemd.so`, and it must
 * have type `account`.
 *
 * ```
 * auth     sufficient pam_unix.so nullok try_first_pass
 * auth     required   pam_deny.so
 *
 * account  required   pam_nologin.so
 * account  sufficient pam_unix.so
 * account  required   pam_permit.so
 * -account required pam_malcontent.so
 *
 * password sufficient pam_unix.so nullok sha512 shadow try_first_pass try_authtok
 * password required   pam_deny.so
 *
 * -session optional   pam_keyinit.so revoke
 * -session optional   pam_loginuid.so
 * -session optional   pam_systemd.so
 * session  sufficient pam_unix.so
 * ```
*/

/* @pw_out is (transfer none) (out) (not optional) */
static int
get_user_data (pam_handle_t         *handle,
               const char          **username_out,
               const struct passwd **pw_out)
{
  const char *username = NULL;
  struct passwd *pw = NULL;
  int r;

  g_return_val_if_fail (handle != NULL, PAM_AUTH_ERR);
  g_return_val_if_fail (username_out != NULL, PAM_AUTH_ERR);
  g_return_val_if_fail (pw_out != NULL, PAM_AUTH_ERR);

  r = pam_get_user (handle, &username, NULL);
  if (r != PAM_SUCCESS)
    {
      pam_syslog (handle, LOG_ERR, "Failed to get user name.");
      return r;
    }

  if (username == NULL || *username == '\0')
    {
      pam_syslog (handle, LOG_ERR, "User name not valid.");
      return PAM_AUTH_ERR;
    }

  pw = pam_modutil_getpwnam (handle, username);
  if (pw == NULL)
    {
      pam_syslog (handle, LOG_ERR, "Failed to get user data.");
      return PAM_USER_UNKNOWN;
    }

  *pw_out = pw;
  *username_out = username;

  return PAM_SUCCESS;
}

static void
runtime_max_sec_free (pam_handle_t *handle,
                      void         *data,
                      int           error_status)
{
  g_return_if_fail (data != NULL);

  g_free (data);
}

PAM_EXTERN int
pam_sm_acct_mgmt (pam_handle_t  *handle,
                  int            flags,
                  int            argc,
                  const char   **argv)
{
  int retval;
  const char *username = NULL;
  const struct passwd *pw = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(MctManager) manager = NULL;
  g_autoptr(MctSessionLimits) limits = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *runtime_max_sec_str = NULL;
  guint64 now = g_get_real_time ();
  guint64 time_remaining_secs = 0;
  gboolean time_limit_enabled = FALSE;

  /* Look up the user data from the handle. */
  retval = get_user_data (handle, &username, &pw);
  if (retval != PAM_SUCCESS)
    {
      /* The error has already been logged. */
      return retval;
    }

  if (pw->pw_uid == 0)
    {
      /* Always allow root, to avoid a situation where this PAM module prevents
       * all users logging in with no way of recovery. */
      pam_info (handle, _("User ‘%s’ has no time limits enabled"), "root");
      return PAM_SUCCESS;
    }

  /* Connect to the system bus. */
  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &local_error);
  if (connection == NULL)
    {
      pam_error (handle,
                 _("Error getting session limits for user ‘%s’: %s"),
                 username, local_error->message);
      return PAM_SERVICE_ERR;
    }

  /* Get the time limits on this user’s session usage. */
  manager = mct_manager_new (connection);
  limits = mct_manager_get_session_limits (manager, pw->pw_uid,
                                           MCT_MANAGER_GET_VALUE_FLAGS_NONE,
                                           NULL, &local_error);

  if (limits == NULL)
    {
      if (g_error_matches (local_error, MCT_MANAGER_ERROR,
                           MCT_MANAGER_ERROR_DISABLED))
        {
          return PAM_SUCCESS;
        }
      else
        {
          pam_error (handle,
                     _("Error getting session limits for user ‘%s’: %s"),
                     username, local_error->message);
          return PAM_SERVICE_ERR;
        }
    }

  /* Check if there’s time left. */
  if (!mct_session_limits_check_time_remaining (limits, now, &time_remaining_secs,
                                                &time_limit_enabled))
    {
      pam_error (handle, _("User ‘%s’ has no time remaining"), username);
      return PAM_AUTH_ERR;
    }

  if (!time_limit_enabled)
    {
      pam_info (handle, _("User ‘%s’ has no time limits enabled"), username);
      return PAM_SUCCESS;
    }

  /* Propagate the remaining time to the `pam_systemd.so` module, which will
   * end the user’s session when it runs out. */
  runtime_max_sec_str = g_strdup_printf ("%" G_GUINT64_FORMAT, time_remaining_secs);
  retval = pam_set_data (handle, "systemd.runtime_max_sec",
                         g_steal_pointer (&runtime_max_sec_str), runtime_max_sec_free);

  if (retval != PAM_SUCCESS)
    {
      pam_error (handle, _("Error setting time limit on login session: %s"),
                 pam_strerror (handle, retval));
      return retval;
    }

  return PAM_SUCCESS;
}
