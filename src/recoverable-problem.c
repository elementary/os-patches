/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *     Ted Gould <ted.gould@canonical.com>
 */

#include "recoverable-problem.h"
#include <glib/gstdio.h>
#include <string.h>
#include <errno.h>

/* Helpers to ensure we write nicely */
static void 
write_string (int          fd,
              const gchar *string)
{
	int res; 
	do
		res = write (fd, string, strlen (string));
	while (G_UNLIKELY (res == -1 && errno == EINTR));
}

/* Make NULLs fast and fun! */
static void 
write_null (int fd)
{
	int res; 
	do
		res = write (fd, "", 1);
	while (G_UNLIKELY (res == -1 && errno == EINTR));
}

/* Child watcher */
static gboolean
apport_child_watch (GPid pid G_GNUC_UNUSED, gint status G_GNUC_UNUSED, gpointer user_data)
{
	g_main_loop_quit((GMainLoop *)user_data);
	return FALSE;
}

static gboolean
apport_child_timeout (gpointer user_data)
{
	g_warning("Recoverable Error Reporter Timeout");
	g_main_loop_quit((GMainLoop *)user_data);
	return FALSE;
}

/* Code to report an error */
void
report_recoverable_problem (const gchar * signature, GPid report_pid, gboolean wait, gchar * additional_properties[])
{
	GSpawnFlags flags;
	gboolean first;
	GError * error = NULL;
	gint error_stdin = 0;
	GPid pid = 0;
	gchar * pid_str = NULL;
	gchar ** argv = NULL;
	gchar * argv_nopid[2] = {
		"/usr/share/apport/recoverable_problem",
		NULL
	};
	gchar * argv_pid[4] = {
		"/usr/share/apport/recoverable_problem",
		"-p",
		NULL, /* put pid_str when allocated here */
		NULL
	};


	argv = (gchar **)argv_nopid;

	if (report_pid != 0) {
		pid_str = g_strdup_printf("%d", report_pid);
		argv_pid[2] = pid_str;
		argv = (gchar**)argv_pid;
	}

	flags = G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL;
	if (wait) {
		flags |= G_SPAWN_DO_NOT_REAP_CHILD;
	}

	g_spawn_async_with_pipes(NULL, /* cwd */
		argv,
		NULL, /* envp */
		flags,
		NULL, NULL, /* child setup func */
		&pid,
		&error_stdin,
		NULL, /* stdout */
		NULL, /* stderr */
		&error);

	if (error != NULL) {
		g_warning("Unable to report a recoverable error: %s", error->message);
		g_error_free(error);
	}

	first = TRUE;

	if (error_stdin != 0 && signature != NULL) {
		write_string(error_stdin, "DuplicateSignature");
		write_null(error_stdin);
		write_string(error_stdin, signature);

		first = FALSE;
	}

	if (error_stdin != 0 && additional_properties != NULL) {
		gint i;
		for (i = 0; additional_properties[i] != NULL; i++) {
			if (!first) {
				write_null(error_stdin);
			} else {
				first = FALSE;
			}

			write_string(error_stdin, additional_properties[i]);
		}
	}

	if (error_stdin != 0) {
		close(error_stdin);
	}

	if (wait && pid != 0) {
		GSource * child_source, * timeout_source;
		GMainContext * context = g_main_context_new();
		GMainLoop * loop = g_main_loop_new(context, FALSE);

		child_source = g_child_watch_source_new(pid);
		g_source_attach(child_source, context);
		g_source_set_callback(child_source, (GSourceFunc)apport_child_watch, loop, NULL);

		timeout_source = g_timeout_source_new_seconds(5);
		g_source_attach(timeout_source, context);
		g_source_set_callback(timeout_source, apport_child_timeout, loop, NULL);

		g_main_loop_run(loop);

		g_source_destroy(timeout_source);
		g_source_destroy(child_source);
		g_main_loop_unref(loop);
		g_main_context_unref(context);

		g_spawn_close_pid(pid);
	}

	g_free(pid_str);

	return;
}
