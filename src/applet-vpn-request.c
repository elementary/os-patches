/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright 2004 - 2017 Red Hat, Inc.
 */

#include "nm-default.h"

#include "applet-vpn-request.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "nm-utils/nm-compat.h"

/*****************************************************************************/

typedef struct {
	char *uuid;
	char *id;
	char *service_type;

	guint watch_id;
	GPid pid;

	GSList *lines;
	int child_stdin;
	int child_stdout;
	int num_newlines;
	GIOChannel *channel;
	guint channel_eventid;
} RequestData;

typedef struct {
	SecretsRequest req;
	RequestData *req_data;
} VpnSecretsInfo;

/*****************************************************************************/

static void request_data_free (RequestData *req_data);

/*****************************************************************************/

size_t
applet_vpn_request_get_secrets_size (void)
{
	return sizeof (VpnSecretsInfo);
}

/*****************************************************************************/

static void
child_finished_cb (GPid pid, gint status, gpointer user_data)
{
	SecretsRequest *req = user_data;
	VpnSecretsInfo *info = (VpnSecretsInfo *) req;
	RequestData *req_data = info->req_data;
	gs_free_error GError *error = NULL;
	gs_unref_variant GVariant *settings = NULL;
	GVariantBuilder settings_builder, vpn_builder, secrets_builder;

	if (status == 0) {
		GSList *iter;

		g_variant_builder_init (&settings_builder, NM_VARIANT_TYPE_CONNECTION);
		g_variant_builder_init (&vpn_builder, NM_VARIANT_TYPE_SETTING);
		g_variant_builder_init (&secrets_builder, G_VARIANT_TYPE ("a{ss}"));

		/* The length of 'lines' must be divisible by 2 since it must contain
		 * key:secret pairs with the key on one line and the associated secret
		 * on the next line.
		 */
		for (iter = req_data->lines; iter; iter = g_slist_next (iter)) {
			if (!iter->next)
				break;
			g_variant_builder_add (&secrets_builder, "{ss}", iter->data, iter->next->data);
			iter = iter->next;
		}

		g_variant_builder_add (&vpn_builder, "{sv}",
		                       NM_SETTING_VPN_SECRETS,
		                       g_variant_builder_end (&secrets_builder));
		g_variant_builder_add (&settings_builder, "{sa{sv}}",
		                       NM_SETTING_VPN_SETTING_NAME,
		                       &vpn_builder);
		settings = g_variant_builder_end (&settings_builder);
	} else {
		error = g_error_new (NM_SECRET_AGENT_ERROR,
		                     NM_SECRET_AGENT_ERROR_USER_CANCELED,
		                     "%s.%d (%s): canceled", __FILE__, __LINE__, __func__);
	}

	/* Complete the secrets request */
	applet_secrets_request_complete (req, settings, error);
	applet_secrets_request_free (req);
}

/*****************************************************************************/

static gboolean
child_stdout_data_cb (GIOChannel *source, GIOCondition condition, gpointer user_data)
{
	VpnSecretsInfo *info = user_data;
	RequestData *req_data = info->req_data;
	char *str;
	int len;

	if (!(condition & G_IO_IN))
		return TRUE;

	if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
		len = strlen (str);
		if (len == 1 && str[0] == '\n') {
			/* on second line with a newline newline */
			if (++req_data->num_newlines == 2) {
				const char *buf = "QUIT\n\n";

				/* terminate the child */
				if (write (req_data->child_stdin, buf, strlen (buf)) == -1)
					return TRUE;
			}
		} else if (len > 0) {
			/* remove terminating newline */
			str[len - 1] = '\0';
			req_data->lines = g_slist_append (req_data->lines, str);
		}
	}
	return TRUE;
}

/*****************************************************************************/

static void
_str_append (GString *str,
             const char *tag,
             const char *val)
{
	const char *s;
	gsize i;

	nm_assert (str);
	nm_assert (tag && tag[0]);
	nm_assert (val);

	g_string_append (str, tag);
	g_string_append_c (str, '=');

	s = strchr (val, '\n');
	if (s) {
		gs_free char *val2 = g_strdup (val);

		for (i = 0; val2[i]; i++) {
			if (val2[i] == '\n')
				val2[i] = ' ';
		}
		g_string_append (str, val2);
	} else
		g_string_append (str, val);
	g_string_append_c (str, '\n');
}

static char *
connection_to_data (NMConnection *connection,
                    gsize *out_length,
                    GError **error)
{
	NMSettingVpn *s_vpn;
	GString *buf;
	const char **keys;
	guint i, len;

	g_return_val_if_fail (NM_IS_CONNECTION (connection), NULL);

	s_vpn = nm_connection_get_setting_vpn (connection);
	if (!s_vpn) {
		g_set_error_literal (error,
		                     NM_SECRET_AGENT_ERROR,
		                     NM_SECRET_AGENT_ERROR_FAILED,
		                     _("Connection had no VPN setting"));
		return NULL;
	}

	buf = g_string_new_len (NULL, 100);

	keys = nm_setting_vpn_get_data_keys (s_vpn, &len);
	for (i = 0; i < len; i++) {
		_str_append (buf, "DATA_KEY", keys[i]);
		_str_append (buf, "DATA_VAL", nm_setting_vpn_get_data_item (s_vpn, keys[i]));
	}
	nm_clear_g_free (&keys);

	keys = nm_setting_vpn_get_secret_keys (s_vpn, &len);
	for (i = 0; i < len; i++) {
		_str_append (buf, "SECRET_KEY", keys[i]);
		_str_append (buf, "SECRET_VAL", nm_setting_vpn_get_secret (s_vpn, keys[i]));
	}
	nm_clear_g_free (&keys);

	g_string_append (buf, "DONE\n\n");
	NM_SET_OUT (out_length, buf->len);
	return g_string_free (buf, FALSE);
}

/*****************************************************************************/

static gboolean
connection_to_fd (NMConnection *connection,
                  int fd,
                  GError **error)
{
	gs_free char *data = NULL;
	gsize data_len;
	gssize w;
	int errsv;

	data = connection_to_data (connection, &data_len, error);
	if (!data)
		return FALSE;

again:
	w = write (fd, data, data_len);
	if (w < 0) {
		errsv = errno;
		if (errsv == EINTR)
			goto again;
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             _("Failed to write connection to VPN UI: %s (%d)"), g_strerror (errsv), errsv);
		return FALSE;
	}

	if ((gsize) w != data_len) {
		g_set_error_literal (error,
		                     NM_SECRET_AGENT_ERROR,
		                     NM_SECRET_AGENT_ERROR_FAILED,
		                     _("Failed to write connection to VPN UI: incomplete write"));
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static void
vpn_child_setup (gpointer user_data)
{
	/* We are in the child process at this point */
	pid_t pid = getpid ();
	setpgid (pid, pid);
}

static gboolean
auth_dialog_spawn (const char *con_id,
                   const char *con_uuid,
                   const char *const*hints,
                   const char *auth_dialog,
                   const char *service_type,
                   gboolean supports_hints,
                   guint32 flags,
                   GPid *out_pid,
                   int *out_stdin,
                   int *out_stdout,
                   GError **error)
{
	gsize hints_len;
	gsize i, j;
	gs_free const char **argv = NULL;
	gs_free const char **envp = NULL;
	gsize environ_len;

	g_return_val_if_fail (con_id, FALSE);
	g_return_val_if_fail (con_uuid, FALSE);
	g_return_val_if_fail (auth_dialog, FALSE);
	g_return_val_if_fail (service_type, FALSE);
	g_return_val_if_fail (out_pid, FALSE);
	g_return_val_if_fail (out_stdin, FALSE);
	g_return_val_if_fail (out_stdout, FALSE);

	hints_len = NM_PTRARRAY_LEN (hints);
	argv = g_new (const char *, 10 + (2 * hints_len));
	i = 0;
	argv[i++] = auth_dialog;
	argv[i++] = "-u";
	argv[i++] = con_uuid;
	argv[i++] = "-n";
	argv[i++] = con_id;
	argv[i++] = "-s";
	argv[i++] = service_type;
	if (flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_ALLOW_INTERACTION)
		argv[i++] = "-i";
	if (flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_REQUEST_NEW)
		argv[i++] = "-r";
	for (j = 0; supports_hints && (j < hints_len); j++) {
		argv[i++] = "-t";
		argv[i++] = hints[j];
	}
	nm_assert (i <= 10 + (2 * hints_len));
	argv[i++] = NULL;

	environ_len = NM_PTRARRAY_LEN (environ);
	envp = g_new (const char *, environ_len + 1);
	memcpy (envp, environ, sizeof (const char *) * environ_len);
	for (i = 0, j = 0; i < environ_len; i++) {
		const char *e = environ[i];

		if (g_str_has_prefix (e, "G_MESSAGES_DEBUG=")) {
			/* skip this environment variable. We interact with the auth-dialog via stdout.
			 * G_MESSAGES_DEBUG may enable additional debugging messages from GTK. */
			continue;
		}
		envp[j++] = e;
	}
	envp[j] = NULL;

	if (!g_spawn_async_with_pipes (NULL,
	                               (char **) argv,
	                               (char **) envp,
	                               G_SPAWN_DO_NOT_REAP_CHILD,
	                               vpn_child_setup,
	                               NULL,
	                               out_pid,
	                               out_stdin,
	                               out_stdout,
	                               NULL,
	                               error))
		return FALSE;

	return TRUE;
}

/*****************************************************************************/

static void
free_vpn_secrets_info (SecretsRequest *req)
{
	request_data_free (((VpnSecretsInfo *) req)->req_data);
}

gboolean
applet_vpn_request_get_secrets (SecretsRequest *req, GError **error)
{
	VpnSecretsInfo *info = (VpnSecretsInfo *) req;
	RequestData *req_data;
	NMSettingConnection *s_con;
	NMSettingVpn *s_vpn;
	const char *connection_type;
	const char *service_type;
	const char *auth_dialog;
	gs_unref_object NMVpnPluginInfo *plugin = NULL;

	applet_secrets_request_set_free_func (req, free_vpn_secrets_info);

	s_con = nm_connection_get_setting_connection (req->connection);
	s_vpn = nm_connection_get_setting_vpn (req->connection);

	connection_type = nm_setting_connection_get_connection_type (s_con);
	g_return_val_if_fail (nm_streq0 (connection_type, NM_SETTING_VPN_SETTING_NAME), FALSE);

	service_type = nm_setting_vpn_get_service_type (s_vpn);
	g_return_val_if_fail (service_type, FALSE);

	plugin = nm_vpn_plugin_info_new_search_file (NULL, service_type);
	auth_dialog = plugin ? nm_vpn_plugin_info_get_auth_dialog (plugin) : NULL;
	if (!auth_dialog) {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             "Could not find the authentication dialog for VPN connection type '%s'",
		             service_type);
		return FALSE;
	}

	info->req_data = g_slice_new0 (RequestData);
	if (!info->req_data) {
		g_set_error_literal (error,
		                     NM_SECRET_AGENT_ERROR,
		                     NM_SECRET_AGENT_ERROR_FAILED,
		                     "Could not create VPN secrets request object");
		return FALSE;
	}
	req_data = info->req_data;

	if (!auth_dialog_spawn (nm_setting_connection_get_id (s_con),
	                        nm_setting_connection_get_uuid (s_con),
	                        (const char *const*) req->hints,
	                        auth_dialog,
	                        service_type,
	                        nm_vpn_plugin_info_supports_hints (plugin),
	                        req->flags,
	                        &req_data->pid,
	                        &req_data->child_stdin,
	                        &req_data->child_stdout,
	                        error))
		return FALSE;

	/* catch when child is reaped */
	req_data->watch_id = g_child_watch_add (req_data->pid, child_finished_cb, info);

	/* listen to what child has to say */
	req_data->channel = g_io_channel_unix_new (req_data->child_stdout);
	req_data->channel_eventid = g_io_add_watch (req_data->channel, G_IO_IN, child_stdout_data_cb, info);
	g_io_channel_set_encoding (req_data->channel, NULL, NULL);

	/* Dump parts of the connection to the child */
	return connection_to_fd (req->connection, req_data->child_stdin, error);
}

/*****************************************************************************/

static gboolean
ensure_killed (gpointer data)
{
	pid_t pid = GPOINTER_TO_INT (data);

	if (kill (pid, 0) == 0)
		kill (pid, SIGKILL);
	/* ensure the child is reaped */
	waitpid (pid, NULL, 0);
	return FALSE;
}

static void
request_data_free (RequestData *req_data)
{
	if (!req_data)
		return;

	g_free (req_data->uuid);
	g_free (req_data->id);
	g_free (req_data->service_type);

	nm_clear_g_source (&req_data->watch_id);

	nm_clear_g_source (&req_data->channel_eventid);
	if (req_data->channel)
		g_io_channel_unref (req_data->channel);

	if (req_data->pid) {
		g_spawn_close_pid (req_data->pid);
		if (kill (req_data->pid, SIGTERM) == 0)
			g_timeout_add_seconds (2, ensure_killed, GINT_TO_POINTER (req_data->pid));
		else {
			kill (req_data->pid, SIGKILL);
			/* ensure the child is reaped */
			waitpid (req_data->pid, NULL, 0);
		}
	}

	g_slist_foreach (req_data->lines, (GFunc) g_free, NULL);
	g_slist_free (req_data->lines);

	g_slice_free (RequestData, req_data);
}
