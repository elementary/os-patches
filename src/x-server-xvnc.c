/*
 * Copyright (C) 2010-2011 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#include <config.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib/gstdio.h>

#include "x-server-xvnc.h"
#include "configuration.h"
#include "process.h"

typedef struct
{
    /* File descriptor to use for standard input */
    gint socket_fd;

    /* Geometry and colour depth */
    gint width, height, depth;
} XServerXVNCPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XServerXVNC, x_server_xvnc, X_SERVER_LOCAL_TYPE)

XServerXVNC *
x_server_xvnc_new (void)
{
    XServerXVNC *server = g_object_new (X_SERVER_XVNC_TYPE, NULL);

    x_server_local_set_command (X_SERVER_LOCAL (server), "Xvnc");

    return server;
}
void
x_server_xvnc_set_socket (XServerXVNC *server, int fd)
{
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);
    g_return_if_fail (server != NULL);
    priv->socket_fd = fd;
}

int
x_server_xvnc_get_socket (XServerXVNC *server)
{
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);
    g_return_val_if_fail (server != NULL, 0);
    return priv->socket_fd;
}

void
x_server_xvnc_set_geometry (XServerXVNC *server, gint width, gint height)
{
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);
    g_return_if_fail (server != NULL);
    priv->width = width;
    priv->height = height;
}

void
x_server_xvnc_set_depth (XServerXVNC *server, gint depth)
{
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);
    g_return_if_fail (server != NULL);
    priv->depth = depth;
}

static void
x_server_xvnc_run (Process *process, gpointer user_data)
{
    XServerXVNC *server = user_data;
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);

    /* Connect input */
    dup2 (priv->socket_fd, STDIN_FILENO);
    dup2 (priv->socket_fd, STDOUT_FILENO);
    close (priv->socket_fd);

    /* Set SIGUSR1 to ignore so the X server can indicate it when it is ready */
    signal (SIGUSR1, SIG_IGN);
}

static ProcessRunFunc
x_server_xvnc_get_run_function (XServerLocal *server)
{
    return x_server_xvnc_run;
}

static gboolean
x_server_xvnc_get_log_stdout (XServerLocal *server)
{
    return FALSE;
}

static gboolean
x_server_xvnc_get_can_share (DisplayServer *server)
{
    return TRUE;
}

static void
x_server_xvnc_add_args (XServerLocal *x_server, GString *command)
{
    XServerXVNC *server = X_SERVER_XVNC (x_server);
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);

    g_string_append (command, " -inetd");

    if (priv->width > 0 && priv->height > 0)
        g_string_append_printf (command, " -geometry %dx%d", priv->width, priv->height);

    if (priv->depth > 0)
        g_string_append_printf (command, " -depth %d", priv->depth);
}

static void
x_server_xvnc_init (XServerXVNC *server)
{
    XServerXVNCPrivate *priv = x_server_xvnc_get_instance_private (server);
    priv->width = 1024;
    priv->height = 768;
    priv->depth = 8;
}

static void
x_server_xvnc_class_init (XServerXVNCClass *klass)
{
    XServerLocalClass *x_server_local_class = X_SERVER_LOCAL_CLASS (klass);
    DisplayServerClass *display_server_class = DISPLAY_SERVER_CLASS (klass);

    x_server_local_class->get_run_function = x_server_xvnc_get_run_function;
    x_server_local_class->get_log_stdout = x_server_xvnc_get_log_stdout;
    x_server_local_class->add_args = x_server_xvnc_add_args;
    display_server_class->get_can_share = x_server_xvnc_get_can_share;
}
