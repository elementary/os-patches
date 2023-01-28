#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>

static gchar *
create_bus (const gchar *config_file, GPid *pid)
{
    int name_pipe[2];
    if (pipe (name_pipe) < 0)
    {
        g_warning ("Error creating pipe: %s", strerror (errno));
        exit (EXIT_FAILURE);
    }

    g_autofree gchar *command = g_strdup_printf ("dbus-daemon --config-file=%s --print-address=%d", config_file, name_pipe[1]);

    gchar **argv;
    g_autoptr(GError) error = NULL;
    if (!g_shell_parse_argv (command, NULL, &argv, &error))
    {
        g_warning ("Error parsing command line: %s", error->message);
        exit (EXIT_FAILURE);
    }

    if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_LEAVE_DESCRIPTORS_OPEN, NULL, NULL, pid, &error))
    {
        g_warning ("Error launching D-Bus: %s", error->message);
        exit (EXIT_FAILURE);
    }

    gchar address[1024];
    ssize_t n_read = read (name_pipe[0], address, 1023);
    if (n_read < 0)
    {
        g_warning ("Error reading D-Bus address: %s", strerror (errno));
        exit (EXIT_FAILURE);
    }
    address[n_read] = '\0';

    if (n_read > 0 && address[n_read - 1] == '\n')
        address[n_read - 1] = '\0';

    return g_strdup (address);
}

int
main (int argc, char **argv)
{
    g_autofree gchar *system_conf_file = g_build_filename (DATADIR, "system.conf", NULL);
    GPid system_bus_pid;
    g_autofree gchar *system_bus_address = create_bus (system_conf_file, &system_bus_pid);
    g_setenv ("DBUS_SYSTEM_BUS_ADDRESS", system_bus_address, TRUE);

    g_autofree gchar *session_conf_file = g_build_filename (DATADIR, "session.conf", NULL);
    GPid session_bus_pid;
    g_autofree gchar *session_bus_address = create_bus (session_conf_file, &session_bus_pid);
    g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_address, TRUE);

    GPid child_pid = fork ();
    if (child_pid == 0)
    {
        execvp (argv[1], argv + 1);
        _exit (EXIT_FAILURE);
    }
    int status;
    waitpid (child_pid, &status, 0);

    kill (session_bus_pid, SIGTERM);
    kill (system_bus_pid, SIGTERM);

    if (WIFEXITED (status))
        return WEXITSTATUS (status);
    else
        return EXIT_FAILURE;
}

