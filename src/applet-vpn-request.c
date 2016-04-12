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
 * Copyright 2004 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include <NetworkManager.h>

#include "applet-vpn-request.h"

#define APPLET_TYPE_VPN_REQUEST            (applet_vpn_request_get_type ())
#define APPLET_VPN_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), APPLET_TYPE_VPN_REQUEST, AppletVpnRequest))
#define APPLET_VPN_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), APPLET_TYPE_VPN_REQUEST, AppletVpnRequestClass))
#define APPLET_IS_VPN_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), APPLET_TYPE_VPN_REQUEST))
#define APPLET_IS_VPN_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), APPLET_TYPE_VPN_REQUEST))
#define APPLET_VPN_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), APPLET_TYPE_VPN_REQUEST, AppletVpnRequestClass))

typedef struct {
	GObject parent;
} AppletVpnRequest;

typedef struct {
	GObjectClass parent;
} AppletVpnRequestClass;

GType applet_vpn_request_get_type (void);

G_DEFINE_TYPE (AppletVpnRequest, applet_vpn_request, G_TYPE_OBJECT)

#define APPLET_VPN_REQUEST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                           APPLET_TYPE_VPN_REQUEST, \
                                           AppletVpnRequestPrivate))

typedef struct {
	gboolean disposed;

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
} AppletVpnRequestPrivate;

/****************************************************************/

typedef struct {
	SecretsRequest req;
	AppletVpnRequest *vpn;
} VpnSecretsInfo;

static void 
child_finished_cb (GPid pid, gint status, gpointer user_data)
{
	SecretsRequest *req = user_data;
	VpnSecretsInfo *info = (VpnSecretsInfo *) req;
	AppletVpnRequest *self = info->vpn;
	AppletVpnRequestPrivate *priv = APPLET_VPN_REQUEST_GET_PRIVATE (self);
	GError *error = NULL;
	GVariant *settings = NULL;
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
		for (iter = priv->lines; iter; iter = g_slist_next (iter)) {
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

	if (settings)
		g_variant_unref (settings);
	g_clear_error (&error);
}

static gboolean 
child_stdout_data_cb (GIOChannel *source, GIOCondition condition, gpointer user_data)
{
	VpnSecretsInfo *info = user_data;
	AppletVpnRequest *self = info->vpn;
	AppletVpnRequestPrivate *priv = APPLET_VPN_REQUEST_GET_PRIVATE (self);
	const char *buf = "QUIT\n\n";
	char *str;
	int len;

	if (!(condition & G_IO_IN))
		return TRUE;

	if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) == G_IO_STATUS_NORMAL) {
		len = strlen (str);
		if (len == 1 && str[0] == '\n') {
			/* on second line with a newline newline */
			if (++priv->num_newlines == 2) {
				/* terminate the child */
				if (write (priv->child_stdin, buf, strlen (buf)) == -1)
					return TRUE;
			}
		} else if (len > 0) {
			/* remove terminating newline */
			str[len - 1] = '\0';
			priv->lines = g_slist_append (priv->lines, str);
		}
	}
	return TRUE;
}

static char *
find_auth_dialog_binary (const char *service,
                         gboolean *out_hints_supported,
                         GError **error)
{
	GDir *dir;
	char *prog = NULL;
	const char *f;
	gboolean hints_supported = FALSE;

	dir = g_dir_open (VPN_NAME_FILES_DIR, 0, NULL);
	if (!dir) {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             "Failed to open VPN plugin file configuration directory " VPN_NAME_FILES_DIR);
		return NULL;
	}

	while (prog == NULL && (f = g_dir_read_name (dir)) != NULL) {
		char *path;
		GKeyFile *keyfile;

		if (!g_str_has_suffix (f, ".name"))
			continue;

		path = g_strdup_printf ("%s/%s", VPN_NAME_FILES_DIR, f);

		keyfile = g_key_file_new ();
		if (g_key_file_load_from_file (keyfile, path, 0, NULL)) {
			char *thisservice;

			thisservice = g_key_file_get_string (keyfile, "VPN Connection", "service", NULL);
			if (g_strcmp0 (thisservice, service) == 0) {
				prog = g_key_file_get_string (keyfile, "GNOME", "auth-dialog", NULL);
				hints_supported = g_key_file_get_boolean (keyfile, "GNOME", "supports-hints", NULL);
			}
			g_free (thisservice);
		}
		g_key_file_free (keyfile);
		g_free (path);
	}
	g_dir_close (dir);

	if (prog == NULL) {
		g_set_error (error,
		             NM_SECRET_AGENT_ERROR,
		             NM_SECRET_AGENT_ERROR_FAILED,
		             "Could not find the authentication dialog for VPN connection type '%s'",
		             service);
	} else if (!g_path_is_absolute (prog)) {
		char *prog_basename;

		/* Remove any path component, then reconstruct path to the auth
		 * dialog in LIBEXECDIR.
		 */
		prog_basename = g_path_get_basename (prog);
		g_free (prog);
		prog = g_strdup_printf ("%s/%s", LIBEXECDIR, prog_basename);
		g_free (prog_basename);

		*out_hints_supported = hints_supported;
	}

	return prog;
}

static void
free_vpn_secrets_info (SecretsRequest *req)
{
	VpnSecretsInfo *info = (VpnSecretsInfo *) req;

	if (info->vpn)
		g_object_unref (info->vpn);
}

size_t
applet_vpn_request_get_secrets_size (void)
{
	return sizeof (VpnSecretsInfo);
}

typedef struct {
	int fd;
	gboolean secret;
	GError **error;
} WriteItemInfo;

static const char *data_key_tag = "DATA_KEY=";
static const char *data_val_tag = "DATA_VAL=";
static const char *secret_key_tag = "SECRET_KEY=";
static const char *secret_val_tag = "SECRET_VAL=";

static gboolean
write_item (int fd, const char *item, GError **error)
{
	size_t item_len = strlen (item);

	errno = 0;
	if (write (fd, item, item_len) != item_len) {
		g_set_error (error,
			         NM_SECRET_AGENT_ERROR,
			         NM_SECRET_AGENT_ERROR_FAILED,
			         "Failed to write connection to VPN UI: errno %d", errno);
		return FALSE;
	}
	return TRUE;
}

static void
write_one_key_val (const char *key, const char *value, gpointer user_data)
{
	WriteItemInfo *info = user_data;
	const char *tag;

	if (info->error && *(info->error))
		return;

	/* Write the key name */
	tag = info->secret ? secret_key_tag : data_key_tag;
	if (!write_item (info->fd, tag, info->error))
		return;
	if (!write_item (info->fd, key, info->error))
		return;
	if (!write_item (info->fd, "\n", info->error))
		return;

	/* Write the key value */
	tag = info->secret ? secret_val_tag : data_val_tag;
	if (!write_item (info->fd, tag, info->error))
		return;
	if (!write_item (info->fd, value ? value : "", info->error))
		return;
	if (!write_item (info->fd, "\n\n", info->error))
		return;
}

static gboolean
write_connection_to_child (int fd, NMConnection *connection, GError **error)
{
	NMSettingVpn *s_vpn;
	WriteItemInfo info = { .fd = fd, .secret = FALSE, .error = error };

	s_vpn = nm_connection_get_setting_vpn (connection);
	if (!s_vpn) {
		g_set_error_literal (error,
		                     NM_SECRET_AGENT_ERROR,
		                     NM_SECRET_AGENT_ERROR_FAILED,
		                     "Connection had no VPN setting");
		return FALSE;
	}

	nm_setting_vpn_foreach_data_item (s_vpn, write_one_key_val, &info);
	if (error && *error)
		return FALSE;

	info.secret = TRUE;
	nm_setting_vpn_foreach_secret (s_vpn, write_one_key_val, &info);
	if (error && *error)
		return FALSE;

	if (!write_item (fd, "DONE\n\n", error))
		return FALSE;

	return TRUE;
}

static void
vpn_child_setup (gpointer user_data G_GNUC_UNUSED)
{
	/* We are in the child process at this point */
	pid_t pid = getpid ();
	setpgid (pid, pid);
}

gboolean
applet_vpn_request_get_secrets (SecretsRequest *req, GError **error)
{
	VpnSecretsInfo *info = (VpnSecretsInfo *) req;
	AppletVpnRequestPrivate *priv;
	NMSettingConnection *s_con;
	NMSettingVpn *s_vpn;
	const char *connection_type;
	const char *service_type;
	char *bin_path;
	char **argv = NULL;
	gboolean success = FALSE;
	guint i = 0, u, hints_len;
	gboolean supports_hints = FALSE;

	applet_secrets_request_set_free_func (req, free_vpn_secrets_info);

	s_con = nm_connection_get_setting_connection (req->connection);
	g_return_val_if_fail (s_con != NULL, FALSE);

	connection_type = nm_setting_connection_get_connection_type (s_con);
	g_return_val_if_fail (connection_type != NULL, FALSE);
	g_return_val_if_fail (strcmp (connection_type, NM_SETTING_VPN_SETTING_NAME) == 0, FALSE);

	s_vpn = nm_connection_get_setting_vpn (req->connection);
	g_return_val_if_fail (s_vpn != NULL, FALSE);

	service_type = nm_setting_vpn_get_service_type (s_vpn);
	g_return_val_if_fail (service_type != NULL, FALSE);

	/* find the auth-dialog binary */
	bin_path = find_auth_dialog_binary (service_type, &supports_hints, error);
	if (!bin_path)
		return FALSE;

	info->vpn = (AppletVpnRequest *) g_object_new (APPLET_TYPE_VPN_REQUEST, NULL);
	if (!info->vpn) {
		g_set_error_literal (error,
		                     NM_SECRET_AGENT_ERROR,
		                     NM_SECRET_AGENT_ERROR_FAILED,
		                     "Could not create VPN secrets request object");
		goto out;
	}

	priv = APPLET_VPN_REQUEST_GET_PRIVATE (info->vpn);

	hints_len = g_strv_length (req->hints);
	argv = g_new0 (char *, 10 + (2 * hints_len));
	argv[i++] = bin_path;
	argv[i++] = "-u";
	argv[i++] = (char *) nm_setting_connection_get_uuid (s_con);
	argv[i++] = "-n";
	argv[i++] = (char *) nm_setting_connection_get_id (s_con);
	argv[i++] = "-s";
	argv[i++] = (char *) service_type;
	if (req->flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_ALLOW_INTERACTION)
		argv[i++] = "-i";
	if (req->flags & NM_SECRET_AGENT_GET_SECRETS_FLAG_REQUEST_NEW)
		argv[i++] = "-r";

	/* add hints */
	for (u = 0; supports_hints && (u < hints_len); u++) {
		argv[i++] = "-t";
		argv[i++] = req->hints[u];
	}

	if (!g_spawn_async_with_pipes (NULL,                       /* working_directory */
	                               argv,                       /* argv */
	                               NULL,                       /* envp */
	                               G_SPAWN_DO_NOT_REAP_CHILD,  /* flags */
	                               vpn_child_setup,            /* child_setup */
	                               NULL,                       /* user_data */
	                               &priv->pid,                 /* child_pid */
	                               &priv->child_stdin,         /* standard_input */
	                               &priv->child_stdout,        /* standard_output */
	                               NULL,                       /* standard_error */
	                               error))                     /* error */
		goto out;

	/* catch when child is reaped */
	priv->watch_id = g_child_watch_add (priv->pid, child_finished_cb, info);

	/* listen to what child has to say */
	priv->channel = g_io_channel_unix_new (priv->child_stdout);
	priv->channel_eventid = g_io_add_watch (priv->channel, G_IO_IN, child_stdout_data_cb, info);
	g_io_channel_set_encoding (priv->channel, NULL, NULL);

	/* Dump parts of the connection to the child */
	success = write_connection_to_child (priv->child_stdin, req->connection, error);

out:
	g_free (argv);
	g_free (bin_path);
	return success;
}

static void
applet_vpn_request_init (AppletVpnRequest *self)
{
}

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
dispose (GObject *object)
{
	AppletVpnRequest *self = APPLET_VPN_REQUEST (object);
	AppletVpnRequestPrivate *priv = APPLET_VPN_REQUEST_GET_PRIVATE (self);

	if (priv->disposed)
		goto done;

	priv->disposed = TRUE;

	g_free (priv->uuid);
	g_free (priv->id);
	g_free (priv->service_type);

	if (priv->watch_id)
		g_source_remove (priv->watch_id);

	if (priv->channel_eventid)
		g_source_remove (priv->channel_eventid);
	if (priv->channel)
		g_io_channel_unref (priv->channel);

	if (priv->pid) {
		g_spawn_close_pid (priv->pid);
		if (kill (priv->pid, SIGTERM) == 0)
			g_timeout_add_seconds (2, ensure_killed, GINT_TO_POINTER (priv->pid));
		else {
			kill (priv->pid, SIGKILL);
			/* ensure the child is reaped */
			waitpid (priv->pid, NULL, 0);
		}
	}

	g_slist_foreach (priv->lines, (GFunc) g_free, NULL);
	g_slist_free (priv->lines);

done:
	G_OBJECT_CLASS (applet_vpn_request_parent_class)->dispose (object);
}

static void
applet_vpn_request_class_init (AppletVpnRequestClass *req_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (req_class);

	g_type_class_add_private (req_class, sizeof (AppletVpnRequestPrivate));

	/* virtual methods */
	object_class->dispose = dispose;
}

