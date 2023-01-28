/*
 * Copyright (C) 2010-2011 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2 or version 3 of the License.
 * See http://www.gnu.org/copyleft/lgpl.html the full text of the license.
 */

#include <config.h>

#include <string.h>
#include <gio/gio.h>

#include "lightdm/power.h"

/**
 * SECTION:power
 * @title: Power Management
 * @short_description: Shutdown, restart, sleep the system
 * @include: lightdm.h
 *
 * Helper functions to perform power management operations.
 */

static GDBusProxy *upower_proxy = NULL;
static GDBusProxy *ck_proxy = NULL;
static GDBusProxy *login1_proxy = NULL;

static GVariant *
upower_call_function (const gchar *function, GError **error)
{
    if (!upower_proxy)
    {
        upower_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      NULL,
                                                      "org.freedesktop.UPower",
                                                      "/org/freedesktop/UPower",
                                                      "org.freedesktop.UPower",
                                                      NULL,
                                                      error);
        if (!upower_proxy)
            return NULL;
    }

    return g_dbus_proxy_call_sync (upower_proxy,
                                   function,
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   error);
}

static GVariant *
login1_call_function (const gchar *function, GVariant *parameters, GError **error)
{
    if (!login1_proxy)
    {
        login1_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                      NULL,
                                                      "org.freedesktop.login1",
                                                      "/org/freedesktop/login1",
                                                      "org.freedesktop.login1.Manager",
                                                      NULL,
                                                      error);
        if (!login1_proxy)
            return NULL;
    }

    return g_dbus_proxy_call_sync (login1_proxy,
                                   function,
                                   parameters,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   error);
}

static GVariant *
ck_call_function (const gchar *function, GVariant *parameters, GError **error)
{
    if (!ck_proxy)
    {
        ck_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ConsoleKit",
                                                  "/org/freedesktop/ConsoleKit/Manager",
                                                  "org.freedesktop.ConsoleKit.Manager",
                                                  NULL,
                                                  error);
        if (!ck_proxy)
            return FALSE;
    }

    return g_dbus_proxy_call_sync (ck_proxy,
                                   function,
                                   parameters,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   error);
}

/**
 * lightdm_get_can_suspend:
 *
 * Checks if authorized to do a system suspend.
 *
 * Return value: #TRUE if can suspend the system
 **/
gboolean
lightdm_get_can_suspend (void)
{
    g_autoptr(GVariant) r = login1_call_function ("CanSuspend", NULL, NULL);
    gboolean can_suspend = FALSE;
    if (r)
    {
        const gchar *result;
        if (g_variant_is_of_type (r, G_VARIANT_TYPE ("(s)")))
        {
            g_variant_get (r, "(&s)", &result);
            can_suspend = g_strcmp0 (result, "yes") == 0;
        }
    }
    if (!r)
    {
        const gchar *result;
        r = ck_call_function ("CanSuspend", NULL, NULL);
        if (r && g_variant_is_of_type (r, G_VARIANT_TYPE ("(s)")))
        {
            g_variant_get (r, "(&s)", &result);
            can_suspend = g_strcmp0 (result, "yes") == 0;
        }
    }
    if (!r)
    {
        r = upower_call_function ("SuspendAllowed", NULL);
        if (r && g_variant_is_of_type (r, G_VARIANT_TYPE ("(b)")))
            g_variant_get (r, "(b)", &can_suspend);
    }

    return can_suspend;
}

/**
 * lightdm_suspend:
 * @error: return location for a #GError, or %NULL
 *
 * Triggers a system suspend.
 *
 * Return value: #TRUE if suspend initiated.
 **/
gboolean
lightdm_suspend (GError **error)
{
    g_autoptr(GError) login1_error = NULL;
    g_autoptr(GVariant) login1_result = login1_call_function ("Suspend", g_variant_new("(b)", FALSE), &login1_error);
    if (login1_result)
        return TRUE;

    g_debug ("Can't suspend using logind; falling back to ConsoleKit: %s", login1_error->message);

    g_autoptr(GError) ck_error = NULL;
    g_autoptr(GVariant) ck_result = ck_call_function ("Suspend", g_variant_new ("(b)", FALSE), &ck_error);
    if (ck_result)
        return TRUE;

    g_debug ("Can't suspend using logind or ConsoleKit; falling back to UPower: %s", ck_error->message);

    g_autoptr(GVariant) upower_result = upower_call_function ("Suspend", error);
    return upower_result != NULL;
}

/**
 * lightdm_get_can_hibernate:
 *
 * Checks if is authorized to do a system hibernate.
 *
 * Return value: #TRUE if can hibernate the system
 **/
gboolean
lightdm_get_can_hibernate (void)
{
    g_autoptr(GVariant) r = login1_call_function ("CanHibernate", NULL, NULL);
    gboolean can_hibernate = FALSE;
    if (r)
    {
        const gchar *result;
        if (g_variant_is_of_type (r, G_VARIANT_TYPE ("(s)")))
        {
            g_variant_get (r, "(&s)", &result);
            can_hibernate = g_strcmp0 (result, "yes") == 0;
        }
    }
    if (!r)
    {
        const gchar *result;
        r = ck_call_function ("CanHibernate", NULL, NULL);
        if (r && g_variant_is_of_type (r, G_VARIANT_TYPE ("(s)")))
        {
            g_variant_get (r, "(&s)", &result);
            can_hibernate = g_strcmp0 (result, "yes") == 0;
        }
    }
    if (!r)
    {
        r = upower_call_function ("HibernateAllowed", NULL);
        if (r && g_variant_is_of_type (r, G_VARIANT_TYPE ("(b)")))
            g_variant_get (r, "(b)", &can_hibernate);
    }

    return can_hibernate;
}

/**
 * lightdm_hibernate:
 * @error: return location for a #GError, or %NULL
 *
 * Triggers a system hibernate.
 *
 * Return value: #TRUE if hibernate initiated.
 **/
gboolean
lightdm_hibernate (GError **error)
{
    g_autoptr(GError) login1_error = NULL;
    g_autoptr(GVariant) login1_result = login1_call_function ("Hibernate", g_variant_new("(b)", FALSE), &login1_error);
    if (login1_result)
        return TRUE;

    g_debug ("Can't hibernate using logind; falling back to ConsoleKit: %s", login1_error->message);

    g_autoptr(GError) ck_error = NULL;
    g_autoptr(GVariant) ck_result = ck_call_function ("Hibernate", g_variant_new ("(b)", FALSE), &ck_error);
    if (ck_result)
        return TRUE;

    g_debug ("Can't hibernate using logind or ConsoleKit; falling back to UPower: %s", ck_error->message);

    g_autoptr(GVariant) upower_result = upower_call_function ("Hibernate", error);
    return upower_result != NULL;
}

/**
 * lightdm_get_can_restart:
 *
 * Checks if is authorized to do a system restart.
 *
 * Return value: #TRUE if can restart the system
 **/
gboolean
lightdm_get_can_restart (void)
{
    g_autoptr(GVariant) r = login1_call_function ("CanReboot", NULL, NULL);
    gboolean can_restart = FALSE;
    if (r)
    {
        const gchar *result;
        if (g_variant_is_of_type (r, G_VARIANT_TYPE ("(s)")))
        {
            g_variant_get (r, "(&s)", &result);
            can_restart = g_strcmp0 (result, "yes") == 0;
        }
    }
    else
    {
        r = ck_call_function ("CanRestart", NULL, NULL);
        if (r && g_variant_is_of_type (r, G_VARIANT_TYPE ("(b)")))
            g_variant_get (r, "(b)", &can_restart);
    }

    return can_restart;
}

/**
 * lightdm_restart:
 * @error: return location for a #GError, or %NULL
 *
 * Triggers a system restart.
 *
 * Return value: #TRUE if restart initiated.
 **/
gboolean
lightdm_restart (GError **error)
{
    g_autoptr(GError) login1_error = NULL;
    g_autoptr(GVariant) login1_result = login1_call_function ("Reboot", g_variant_new("(b)", FALSE), &login1_error);
    if (login1_result)
        return TRUE;

    g_autoptr(GVariant) ck_result = ck_call_function ("Restart", NULL, error);
    return ck_result != NULL;
}

/**
 * lightdm_get_can_shutdown:
 *
 * Checks if is authorized to do a system shutdown.
 *
 * Return value: #TRUE if can shutdown the system
 **/
gboolean
lightdm_get_can_shutdown (void)
{
    g_autoptr(GVariant) r = login1_call_function ("CanPowerOff", NULL, NULL);
    gboolean can_shutdown = FALSE;
    if (r)
    {
        const gchar *result;
        if (g_variant_is_of_type (r, G_VARIANT_TYPE ("(s)")))
        {
            g_variant_get (r, "(&s)", &result);
            can_shutdown = g_strcmp0 (result, "yes") == 0;
        }
    }
    else
    {
        r = ck_call_function ("CanStop", NULL, NULL);
        if (r && g_variant_is_of_type (r, G_VARIANT_TYPE ("(b)")))
            g_variant_get (r, "(b)", &can_shutdown);
    }

    return can_shutdown;
}

/**
 * lightdm_shutdown:
 * @error: return location for a #GError, or %NULL
 *
 * Triggers a system shutdown.
 *
 * Return value: #TRUE if shutdown initiated.
 **/
gboolean
lightdm_shutdown (GError **error)
{
    g_autoptr(GError) login1_error = NULL;
    g_autoptr(GVariant) login1_result = login1_call_function ("PowerOff", g_variant_new("(b)", FALSE), &login1_error);
    if (login1_result)
        return TRUE;

    g_autoptr(GVariant) ck_result = ck_call_function ("Stop", NULL, error);
    return ck_result != NULL;
}
