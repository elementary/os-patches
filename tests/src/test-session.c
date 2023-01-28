#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>
#include <xcb/xcb.h>
#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <glib/gstdio.h>
#include <lightdm/greeter.h>

#include "status.h"

static gchar *session_id;

static GMainLoop *loop;

static GString *open_fds;

static GKeyFile *config;

static xcb_connection_t *connection;

static LightDMGreeter *greeter = NULL;

static gboolean
sigint_cb (gpointer user_data)
{
    status_notify ("%s TERMINATE SIGNAL=%d", session_id, SIGINT);
    g_main_loop_quit (loop);
    return TRUE;
}

static gboolean
sigterm_cb (gpointer user_data)
{
    status_notify ("%s TERMINATE SIGNAL=%d", session_id, SIGTERM);
    g_main_loop_quit (loop);
    return TRUE;
}

static void
show_message_cb (LightDMGreeter *greeter, const gchar *text, LightDMMessageType type)
{
    status_notify ("%s GREETER-SHOW-MESSAGE TEXT=\"%s\"", session_id, text);
}

static void
show_prompt_cb (LightDMGreeter *greeter, const gchar *text, LightDMPromptType type)
{
    status_notify ("%s GREETER-SHOW-PROMPT TEXT=\"%s\"", session_id, text);
}

static void
authentication_complete_cb (LightDMGreeter *greeter)
{
    if (lightdm_greeter_get_authentication_user (greeter))
        status_notify ("%s GREETER-AUTHENTICATION-COMPLETE USERNAME=%s AUTHENTICATED=%s",
                       session_id,
                       lightdm_greeter_get_authentication_user (greeter),
                       lightdm_greeter_get_is_authenticated (greeter) ? "TRUE" : "FALSE");
    else
        status_notify ("%s GREETER-AUTHENTICATION-COMPLETE AUTHENTICATED=%s",
                       session_id,
                       lightdm_greeter_get_is_authenticated (greeter) ? "TRUE" : "FALSE");
}

static void
request_cb (const gchar *name, GHashTable *params)
{
    if (!name)
    {
        g_main_loop_quit (loop);
        return;
    }

    if (strcmp (name, "LOGOUT") == 0)
        exit (EXIT_SUCCESS);

    else if (strcmp (name, "CRASH") == 0)
        kill (getpid (), SIGSEGV);

    else if (strcmp (name, "LOCK-SEAT") == 0)
    {
        status_notify ("%s LOCK-SEAT", session_id);
        g_dbus_connection_call_sync (g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL),
                                     "org.freedesktop.DisplayManager",
                                     getenv ("XDG_SEAT_PATH"),
                                     "org.freedesktop.DisplayManager.Seat",
                                     "Lock",
                                     g_variant_new ("()"),
                                     G_VARIANT_TYPE ("()"),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     1000,
                                     NULL,
                                     NULL);
    }

    else if (strcmp (name, "LOCK-SESSION") == 0)
    {
        status_notify ("%s LOCK-SESSION", session_id);
        g_dbus_connection_call_sync (g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL),
                                     "org.freedesktop.DisplayManager",
                                     getenv ("XDG_SESSION_PATH"),
                                     "org.freedesktop.DisplayManager.Session",
                                     "Lock",
                                     g_variant_new ("()"),
                                     G_VARIANT_TYPE ("()"),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     1000,
                                     NULL,
                                     NULL);
    }

    else if (strcmp (name, "LIST-GROUPS") == 0)
    {
        int n_groups = getgroups (0, NULL);
        if (n_groups < 0)
        {
            g_printerr ("Failed to get groups: %s", strerror (errno));
            n_groups = 0;
        }
        gid_t *groups = malloc (sizeof (gid_t) * n_groups);
        n_groups = getgroups (n_groups, groups);
        g_autoptr(GString) group_list = g_string_new ("");
        for (int i = 0; i < n_groups; i++)
        {
            struct group *group;

            if (i != 0)
                g_string_append (group_list, ",");
            group = getgrgid (groups[i]);
            if (group)
                g_string_append (group_list, group->gr_name);
            else
                g_string_append_printf (group_list, "%d", groups[i]);
        }
        status_notify ("%s LIST-GROUPS GROUPS=%s", session_id, group_list->str);
        free (groups);
    }

    else if (strcmp (name, "READ-ENV") == 0)
    {
        const gchar *name = g_hash_table_lookup (params, "NAME");
        const gchar *value = g_getenv (name);
        status_notify ("%s READ-ENV NAME=%s VALUE=%s", session_id, name, value ? value : "");
    }

    else if (strcmp (name, "WRITE-STDOUT") == 0)
        g_print ("%s", (const gchar *) g_hash_table_lookup (params, "TEXT"));

    else if (strcmp (name, "WRITE-STDERR") == 0)
        g_printerr ("%s", (const gchar *) g_hash_table_lookup (params, "TEXT"));

    else if (strcmp (name, "READ") == 0)
    {
        const gchar *name = g_hash_table_lookup (params, "FILE");

        g_autofree gchar *contents = NULL;
        g_autoptr(GError) error = NULL;
        if (g_file_get_contents (name, &contents, NULL, &error))
            status_notify ("%s READ FILE=%s TEXT=%s", session_id, name, contents);
        else
            status_notify ("%s READ FILE=%s ERROR=%s", session_id, name, error->message);
    }

    else if (strcmp (name, "LIST-UNKNOWN-FILE-DESCRIPTORS") == 0)
        status_notify ("%s LIST-UNKNOWN-FILE-DESCRIPTORS FDS=%s", session_id, open_fds->str);

    else if (strcmp (name, "CHECK-X-AUTHORITY") == 0)
    {
        g_autofree gchar *xauthority = g_strdup (g_getenv ("XAUTHORITY"));
        if (!xauthority)
            xauthority = g_build_filename (g_get_home_dir (), ".Xauthority", NULL);

        GStatBuf file_info;
        g_stat (xauthority, &file_info);

        g_autoptr(GString) mode_string = g_string_new ("");
        g_string_append_c (mode_string, file_info.st_mode & S_IRUSR ? 'r' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IWUSR ? 'w' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IXUSR ? 'x' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IRGRP ? 'r' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IWGRP ? 'w' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IXGRP ? 'x' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IROTH ? 'r' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IWOTH ? 'w' : '-');
        g_string_append_c (mode_string, file_info.st_mode & S_IXOTH ? 'x' : '-');
        status_notify ("%s CHECK-X-AUTHORITY MODE=%s", session_id, mode_string->str);
    }

    else if (strcmp (name, "WRITE-SHARED-DATA") == 0)
    {
        const gchar *data = g_hash_table_lookup (params, "DATA");

        const gchar *dir = getenv ("XDG_GREETER_DATA_DIR");
        if (dir)
        {
            g_autofree gchar *path = NULL;
            FILE *f;

            path = g_build_filename (dir, "data", NULL);
            if (!(f = fopen (path, "w")) || fprintf (f, "%s", data) < 0)
                status_notify ("%s WRITE-SHARED-DATA ERROR=%s", session_id, strerror (errno));
            else
                status_notify ("%s WRITE-SHARED-DATA RESULT=TRUE", session_id);

            if (f)
                fclose (f);
        }
        else
            status_notify ("%s WRITE-SHARED-DATA ERROR=NO_XDG_GREETER_DATA_DIR", session_id);
    }

    else if (strcmp (name, "READ-SHARED-DATA") == 0)
    {
        const gchar *dir = getenv ("XDG_GREETER_DATA_DIR");
        if (dir)
        {
            g_autofree gchar *path = g_build_filename (dir, "data", NULL);
            g_autofree gchar *contents = NULL;
            g_autoptr(GError) error = NULL;
            if (g_file_get_contents (path, &contents, NULL, &error))
                status_notify ("%s READ-SHARED-DATA DATA=%s", session_id, contents);
            else
                status_notify ("%s WRITE-SHARED-DATA ERROR=%s", session_id, error->message);
        }
        else
            status_notify ("%s WRITE-SHARED-DATA ERROR=NO_XDG_GREETER_DATA_DIR", session_id);
    }

    else if (strcmp (name, "GREETER-START") == 0)
    {
        g_assert (greeter == NULL);
        greeter = lightdm_greeter_new ();
        g_signal_connect (greeter, LIGHTDM_GREETER_SIGNAL_SHOW_MESSAGE, G_CALLBACK (show_message_cb), NULL);
        g_signal_connect (greeter, LIGHTDM_GREETER_SIGNAL_SHOW_PROMPT, G_CALLBACK (show_prompt_cb), NULL);
        g_signal_connect (greeter, LIGHTDM_GREETER_SIGNAL_AUTHENTICATION_COMPLETE, G_CALLBACK (authentication_complete_cb), NULL);
        g_autoptr(GError) error = NULL;
        if (lightdm_greeter_connect_to_daemon_sync (greeter, &error))
            status_notify ("%s GREETER-STARTED", session_id);
        else
            status_notify ("%s GREETER-FAILED ERROR=%s", session_id, error->message);

        if (lightdm_greeter_get_select_user_hint (greeter))
            status_notify ("%s GREETER-SELECT-USER-HINT USERNAME=%s", session_id, lightdm_greeter_get_select_user_hint (greeter));
        if (lightdm_greeter_get_select_guest_hint (greeter))
            status_notify ("%s GREETER-SELECT-GUEST-HINT", session_id);
        if (lightdm_greeter_get_lock_hint (greeter))
            status_notify ("%s GREETER-LOCK-HINT", session_id);
        if (!lightdm_greeter_get_has_guest_account_hint (greeter))
            status_notify ("%s GREETER-HAS-GUEST-ACCOUNT-HINT=FALSE", session_id);
        if (lightdm_greeter_get_hide_users_hint (greeter))
            status_notify ("%s GREETER-HIDE-USERS-HINT", session_id);
        if (lightdm_greeter_get_show_manual_login_hint (greeter))
            status_notify ("%s GREETER-SHOW-MANUAL-LOGIN-HINT", session_id);
        if (!lightdm_greeter_get_show_remote_login_hint (greeter))
            status_notify ("%s GREETER-SHOW-REMOTE-LOGIN-HINT=FALSE", session_id);
        int timeout = lightdm_greeter_get_autologin_timeout_hint (greeter);
        if (lightdm_greeter_get_autologin_user_hint (greeter))
        {
            if (timeout != 0)
                status_notify ("%s GREETER-AUTOLOGIN-USER USERNAME=%s TIMEOUT=%d", session_id, lightdm_greeter_get_autologin_user_hint (greeter), timeout);
            else
                status_notify ("%s GREETER-AUTOLOGIN-USER USERNAME=%s", session_id, lightdm_greeter_get_autologin_user_hint (greeter));
        }
        else if (lightdm_greeter_get_autologin_guest_hint (greeter))
        {
            if (timeout != 0)
                status_notify ("%s GREETER-AUTOLOGIN-GUEST TIMEOUT=%d", session_id, timeout);
            else
                status_notify ("%s GREETER-AUTOLOGIN-GUEST", session_id);
        }     
    }

    else if (strcmp (name, "GREETER-LOG-DEFAULT-SESSION") == 0)
        status_notify ("%s GREETER-LOG-DEFAULT-SESSION SESSION=%s", session_id, lightdm_greeter_get_default_session_hint (greeter));

    else if (strcmp (name, "GREETER-AUTHENTICATE") == 0)
    {
        g_autoptr(GError) error = NULL;

        if (!lightdm_greeter_authenticate (greeter, g_hash_table_lookup (params, "USERNAME"), &error))
            status_notify ("%s FAIL-AUTHENTICATE ERROR=%s", session_id, error->message);
    }

    else if (strcmp (name, "GREETER-RESPOND") == 0)
    {
        g_autoptr(GError) error = NULL;
        if (!lightdm_greeter_respond (greeter, g_hash_table_lookup (params, "TEXT"), &error))
            status_notify ("%s FAIL-RESPOND ERROR=%s", session_id, error->message);
    }

    else if (strcmp (name, "GREETER-START-SESSION") == 0)
    {
        g_autoptr(GError) error = NULL;

        if (!lightdm_greeter_start_session_sync (greeter, g_hash_table_lookup (params, "SESSION"), &error))
            status_notify ("%s FAIL-START-SESSION ERROR=%s", session_id, error->message);
    }

    else if (strcmp (name, "GREETER-STOP") == 0)
    {
        g_assert (greeter != NULL);
        g_clear_object (&greeter);
    }
}

int
main (int argc, char **argv)
{
    const gchar *display = getenv ("DISPLAY");
    const gchar *xdg_seat = getenv ("XDG_SEAT");
    const gchar *xdg_vtnr = getenv ("XDG_VTNR");
    const gchar *xdg_current_desktop = getenv ("XDG_CURRENT_DESKTOP");
    const gchar *xdg_greeter_data_dir = getenv ("XDG_GREETER_DATA_DIR");
    const gchar *xdg_session_cookie = getenv ("XDG_SESSION_COOKIE");
    const gchar *xdg_session_class = getenv ("XDG_SESSION_CLASS");
    const gchar *xdg_session_type = getenv ("XDG_SESSION_TYPE");
    const gchar *xdg_session_desktop = getenv ("XDG_SESSION_DESKTOP");
    const gchar *mir_server_host_socket = getenv ("MIR_SERVER_HOST_SOCKET");
    const gchar *mir_vt = getenv ("MIR_SERVER_VT");
    const gchar *mir_id = getenv ("MIR_SERVER_NAME");
    if (display)
    {
        if (display[0] == ':')
            session_id = g_strdup_printf ("SESSION-X-%s", display + 1);
        else
            session_id = g_strdup_printf ("SESSION-X-%s", display);
    }
    else if (mir_id)
        session_id = g_strdup_printf ("SESSION-MIR-%s", mir_id);
    else if (mir_server_host_socket || mir_vt)
        session_id = g_strdup ("SESSION-MIR");
    else if (g_strcmp0 (xdg_session_type, "wayland") == 0)
        session_id = g_strdup ("SESSION-WAYLAND");
    else
        session_id = g_strdup ("SESSION-UNKNOWN");

    open_fds = g_string_new ("");
    int open_max = sysconf (_SC_OPEN_MAX);
    for (int fd = STDERR_FILENO + 1; fd < open_max; fd++)
    {
        if (fcntl (fd, F_GETFD) >= 0)
            g_string_append_printf (open_fds, "%d,", fd);
    }
    if (g_str_has_suffix (open_fds->str, ","))
        open_fds->str[strlen (open_fds->str) - 1] = '\0';

#if !defined(GLIB_VERSION_2_36)
    g_type_init ();
#endif

    loop = g_main_loop_new (NULL, FALSE);

    g_unix_signal_add (SIGINT, sigint_cb, NULL);
    g_unix_signal_add (SIGTERM, sigterm_cb, NULL);

    status_connect (request_cb, session_id);

    g_autoptr(GString) status_text = g_string_new ("");
    g_string_printf (status_text, "%s START", session_id);
    if (xdg_seat)
        g_string_append_printf (status_text, " XDG_SEAT=%s", xdg_seat);
    if (xdg_vtnr)
        g_string_append_printf (status_text, " XDG_VTNR=%s", xdg_vtnr);
    if (xdg_current_desktop)
        g_string_append_printf (status_text, " XDG_CURRENT_DESKTOP=%s", xdg_current_desktop);
    if (xdg_greeter_data_dir)
        g_string_append_printf (status_text, " XDG_GREETER_DATA_DIR=%s", xdg_greeter_data_dir);
    if (xdg_session_cookie)
        g_string_append_printf (status_text, " XDG_SESSION_COOKIE=%s", xdg_session_cookie);
    if (xdg_session_class)
        g_string_append_printf (status_text, " XDG_SESSION_CLASS=%s", xdg_session_class);
    if (xdg_session_type)
        g_string_append_printf (status_text, " XDG_SESSION_TYPE=%s", xdg_session_type);
    if (xdg_session_desktop)
        g_string_append_printf (status_text, " XDG_SESSION_DESKTOP=%s", xdg_session_desktop);
    if (mir_vt > 0)
        g_string_append_printf (status_text, " MIR_SERVER_VT=%s", mir_vt);
    if (argc > 1)
        g_string_append_printf (status_text, " NAME=%s", argv[1]);
    g_string_append_printf (status_text, " USER=%s", getenv ("USER"));
    status_notify ("%s", status_text->str);

    config = g_key_file_new ();
    g_key_file_load_from_file (config, g_build_filename (g_getenv ("LIGHTDM_TEST_ROOT"), "script", NULL), G_KEY_FILE_NONE, NULL);

    if (display)
    {
        connection = xcb_connect (NULL, NULL);
        if (xcb_connection_has_error (connection))
        {
            status_notify ("%s CONNECT-XSERVER-ERROR", session_id);
            return EXIT_FAILURE;
        }
        status_notify ("%s CONNECT-XSERVER", session_id);
    }

    g_main_loop_run (loop);

    return EXIT_SUCCESS;
}
