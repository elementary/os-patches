/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#define AG_DISABLE_DEPRECATION_WARNINGS

#include "libaccounts-glib/ag-account.h"
#include "libaccounts-glib/ag-manager.h"
#include "libaccounts-glib/ag-provider.h"
#include "libaccounts-glib/ag-service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if GLIB_CHECK_VERSION (2, 30, 0)
#else
#define G_VALUE_INIT { 0, { { 0 } } }
#endif

static gchar *gl_app_name = NULL;

enum
{
    ERROR_GENERIC,
    INVALID_ACC_ID,
    INVALID_SERVICE_NAME,
    INVALID_INPUT
};

static void
show_error (gint err)
{

    switch (err)
    {
    case ERROR_GENERIC:
        printf ("\nUnable to process the request\n\n");
        break;
    case INVALID_ACC_ID:
        printf ("\nAccount does not exist. Check account ID entered\n\n");
        break;
    case INVALID_SERVICE_NAME:
        printf ("\nService does not exist. Enter valid service name\n\n");
        break;
    case INVALID_INPUT:
        printf ("\nRequest is not processed. Check the command parameters\n\n");
        break;
    default:
        printf ("\nUnknown problem in processing the request\n\n");
        break;
    }
}

static void
show_help ()
{

    printf ("\nOptions:\n"
            "   * Creates an account\n"
            "   %1$s create-account <provider name> [<display name>] [<enable|disable>] \n\n"
            "   * Updates/Adds key to account and sets a value to key\n"
            "   %1$s update-account <account id> (int|uint|bool|string):<key>=<value> \n\n"
            "   * Updates/Adds key to service of an account and sets a value to the key\n"
            "   %1$s update-service <accound id> <service name>\n"
            "                       (int|uint|bool|string):<key>=<value> \n\n"
            "   * Enables an account\n"
            "   %1$s enable-account <account id>\n\n"
            "   * Enables service of the account\n"
            "   %1$s enable-service <account id> <service name>\n\n"
            "   * Disables an account\n"
            "   %1$s disable-account <accound id>\n\n"
            "   * Disables service of an account\n"
            "   %1$s disable-service <accound id> <service name>\n\n"
            "   * Gets the value of a key of an account\n"
            "   %1$s get-account <accound id> <(int|uint|bool|string):key>\n\n"
            "   * Gets the value of a key of a service\n"
            "   %1$s get-service <account id> <service name>\n\t\t       <(int|uint|bool|string):<key>=<value>\n\n"
            "   * Deletes all accounts is <all> keyword is used or deletes specified account\n"
            "   %1$s delete-account <account id>/<all>\n\n"
            "   * Lists all providers\n"
            "   %1$s list-providers\n\n"
            "   * Lists all services or services that can be associated with an account\n"
            "   %1$s list-services [<account id>]\n\n"
            "   * Lists all accounts\n"
            "   %1$s list-accounts\n\n"
            "   * List all enabled accounts\n"
            "     If account ID is specified lists services enabled on the given account\n"
            "   %1$s list-enabled [<account id>]\n\n"
            "   * Lists settings associated with account\n"
            "   %1$s list-settings <account id>\n", gl_app_name);

    printf ("\nParameters in square braces '[param]' are optional\n");
}

static void
show_help_text (gchar *command)
{
    /* TODO: Show individal command help text if needed */
    show_help ();
}


static gchar *
get_string_value (const GValue *value)
{
    gchar *str = NULL;

    if (G_VALUE_HOLDS_STRING (value))
    {
        str = g_value_dup_string (value);
    }
    else if (G_VALUE_HOLDS_UINT (value))
    {
        str = g_strdup_printf ("%u", g_value_get_uint (value));
    }
    else if (G_VALUE_HOLDS_INT (value))
    {
        str = g_strdup_printf ("%i", g_value_get_int (value));
    }
    else if (G_VALUE_HOLDS_BOOLEAN (value))
    {
        str = g_strdup (g_value_get_boolean (value) ? "true" : "false");
    }
    else
    {
        str = g_strdup_value_contents (value);
    }
    return str;
}

static void
get_account (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GValue value = G_VALUE_INIT;
    GType type = 0;
    gchar *str = NULL;
    gchar **param = NULL;

    if (argv[2] == NULL || argv[3] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    /* param[0] = type, param[1] = key. Both separated by ':' */
    param = g_strsplit (argv[3], ":", 2);
    if (param[0] == NULL || param[1] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        return;
    }

    if (strcmp (param[0], "int") == 0)
    {
        g_value_init (&value, G_TYPE_INT);
        type = G_TYPE_INT;
    }
    else if (strcmp (param[0], "uint") == 0)
    {
        g_value_init (&value, G_TYPE_UINT);
        type = G_TYPE_UINT;
    }
    else if (strcmp (param[0], "bool") == 0 ||
             strcmp (param[0], "boolean") == 0)
    {
        g_value_init (&value, G_TYPE_BOOLEAN);
        type = G_TYPE_BOOLEAN;
    }
    else if (strcmp (param[0], "string") == 0)
    {
        g_value_init (&value, G_TYPE_STRING);
        type = G_TYPE_STRING;
    }
    else
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        g_strfreev (param);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_strfreev (param);
        g_object_unref (manager);
        return;
    }

    if (ag_account_get_value (account, param[1],
                              &value) == AG_SETTING_SOURCE_NONE)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        g_object_unref (account);
        g_object_unref (manager);
        return;
    }

    switch (type)
    {
    case G_TYPE_INT:
        str = g_strdup_printf ("%i", g_value_get_int (&value));
        break;
    case G_TYPE_UINT:
        str = g_strdup_printf ("%u", g_value_get_uint (&value));
        break;
    case G_TYPE_BOOLEAN:
        str = g_strdup_printf ("%i", g_value_get_boolean (&value));
        break;
    case G_TYPE_STRING:
        str = g_value_dup_string (&value);
        break;
    default:
        break;
    }

    if (G_IS_VALUE (&value))
        g_value_unset (&value);
    g_object_unref (account);
    g_object_unref (manager);

    printf ("%s = %s\n", param[1], str);

    g_strfreev (param);
    g_free(str);
}

static void
list_service_settings (AgAccount *account)
{
    GList *list = NULL;
    GList *tmp = NULL;
    AgAccountSettingIter iter;
    const gchar *key = NULL;
    const GValue *val = NULL;
    gchar *str = NULL;

    list = ag_account_list_services (account);
    if (list == NULL || g_list_length (list) == 0)
    {
        return;
    }

    for (tmp = list; tmp != NULL; tmp = g_list_next (tmp))
    {
        printf ("\t\t%s\n", ag_service_get_name (tmp->data));
        ag_account_select_service (account, (AgService *) tmp->data);

        ag_account_settings_iter_init (account, &iter, NULL);
        while (ag_account_settings_iter_next (&iter, &key, &val))
        {
            str = get_string_value (val);
            printf ("%s = %s\n", key, str);
            g_free (str);
            str = NULL;
        }
    }

    ag_service_list_free (list);
}

static void
list_settings (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    AgAccountSettingIter iter;
    const gchar *key = NULL;
    const GValue *val = NULL;
    gchar *str = NULL;

    if (argv[2] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_object_unref (manager);
        return;
    }

    ag_account_settings_iter_init (account, &iter, NULL);
    while (ag_account_settings_iter_next (&iter, &key, &val))
    {
        str = get_string_value (val);
        printf ("%s = %s\n", key, str);
        g_free (str);
        str = NULL;
    }

    list_service_settings (account);

    g_object_unref (account);
    g_object_unref (manager);
}

static void
get_service (gchar **argv)
{
    AgManager *manager = NULL;
    AgService *service = NULL;
    AgAccount *account = NULL;
    GValue value = G_VALUE_INIT;
    GType type = 0;
    gchar *str = NULL;
    gchar **param = NULL;

    if (argv[2] == NULL || argv[3] == NULL || argv[4] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    /* argv[4] = type:key */
    param = g_strsplit (argv[4], ":", 2);
    if (param[0] == NULL || param[1] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        return;
    }

    if (strcmp (param[0], "int") == 0)
    {
        g_value_init (&value, G_TYPE_INT);
        type = G_TYPE_INT;
    }
    else if (strcmp (param[0], "uint") == 0)
    {
        g_value_init (&value, G_TYPE_UINT);
        type = G_TYPE_UINT;
    }
    else if (strcmp (param[0], "bool") == 0 ||
             strcmp (param[0], "boolean") == 0)
    {
        g_value_init (&value, G_TYPE_BOOLEAN);
        type = G_TYPE_BOOLEAN;
    }
    else if (strcmp (param[0], "string") == 0)
    {
        g_value_init (&value, G_TYPE_STRING);
        type = G_TYPE_STRING;
    }
    else
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        g_strfreev (param);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_strfreev (param);
        g_object_unref (manager);
        return;
    }

    service = ag_manager_get_service (manager, argv[3]);
    if (service == NULL)
    {
        show_error (INVALID_SERVICE_NAME);
        g_strfreev (param);
        g_object_unref (account);
        g_object_unref (manager);
        return;
    }

    ag_account_select_service (account, service);
    if (ag_account_get_value (account, param[1],
                              &value) == AG_SETTING_SOURCE_NONE)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        ag_service_unref (service);
        g_object_unref (account);
        g_object_unref (manager);
        return;
    }

    switch (type)
    {
    case G_TYPE_INT:
        str = g_strdup_printf ("%i", g_value_get_int (&value));
        break;
    case G_TYPE_UINT:
        str = g_strdup_printf ("%u", g_value_get_uint (&value));
        break;
    case G_TYPE_BOOLEAN:
        str = g_strdup_printf ("%i", g_value_get_boolean (&value));
        break;
    case G_TYPE_STRING:
        str = g_value_dup_string (&value);
        break;
    default:
        break;
    }

    if (G_IS_VALUE (&value))
        g_value_unset (&value);
    ag_service_unref (service);
    g_object_unref (account);
    g_object_unref (manager);

    printf ("%s = %s\n", param[1], str);

    g_strfreev (param);
    g_free (str);
}

static void
update_service (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GValue *gvalue = NULL;
    gchar **param = NULL;
    gchar **keytype = NULL;
    AgService *service = NULL;
    GError *error = NULL;

    if (argv[2] == NULL || argv[3] == NULL || argv[4] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    param = g_strsplit (argv[4], "=", 2);
    if (param[0] == NULL || param[1] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        return;
    }

    keytype = g_strsplit (param[0], ":", 2);
    if (keytype[0] == NULL || keytype[1] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        g_strfreev (keytype);
        return;
    }

    gvalue = g_new0 (GValue, 1);
    if (strcmp (keytype[0], "int") == 0)
    {
        g_value_init (gvalue, G_TYPE_INT);
        g_value_set_int (gvalue, strtol (param[1], NULL, 10));
    }
    else if (strcmp (keytype[0], "uint") == 0)
    {
        g_value_init (gvalue, G_TYPE_UINT);
        g_value_set_uint (gvalue, strtoul (param[1], NULL, 10));
    }
    else if (strcmp (keytype[0], "bool") == 0 || strcmp (keytype[0],
                                                         "boolean") == 0)
    {
        g_value_init (gvalue, G_TYPE_BOOLEAN);
        g_value_set_boolean (gvalue, atoi (param[1]));
    }
    else if (strcmp (keytype[0], "string") == 0)
    {
        g_value_init (gvalue, G_TYPE_STRING);
        g_value_set_string (gvalue, param[1]);
    }
    else
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        g_strfreev (keytype);
        g_value_unset (gvalue);
        g_free (gvalue);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        g_strfreev (param);
        g_strfreev (keytype);
        g_value_unset (gvalue);
        g_free (gvalue);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_strfreev (param);
        g_strfreev (keytype);
        g_object_unref (manager);
        g_value_unset (gvalue);
        g_free (gvalue);
        return;
    }

    service = ag_manager_get_service (manager, argv[3]);
    if (service == NULL)
    {
        show_error (INVALID_SERVICE_NAME);
        g_strfreev (param);
        g_strfreev (keytype);
        g_object_unref (account);
        g_object_unref (manager);
        g_value_unset (gvalue);
        g_free (gvalue);
        return;
    }

    ag_account_select_service (account, service);
    ag_account_set_value (account, keytype[1], gvalue);
    ag_account_store_blocking (account, &error);
    if (error)
    {
        show_error (ERROR_GENERIC);
        g_error_free (error);
    }

    g_strfreev (param);
    g_strfreev (keytype);
    g_value_unset (gvalue);
    g_free (gvalue);
    ag_service_unref (service);
    g_object_unref (account);
    g_object_unref (manager);

    return;
}

static void
update_account (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GValue *gvalue = NULL;
    gchar **param = NULL;
    gchar **keytype = NULL;
    GError *error = NULL;

    /* Input parameter will be argv[2] = <account Id>
     * argv[3] = <keytype:key=value>
     */
    if ((argv[2] == NULL) || (argv[3] == NULL))
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    /* param[0] = <keytype:key>, param[1] = value */
    param = g_strsplit (argv[3], "=", 2);
    if (param[0] == NULL || param[1] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        return;
    }

    /* keytype[0] = type, keytype[1] = key */
    keytype = g_strsplit (param[0], ":", 2);
    if (keytype[0] == NULL || keytype[1] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        g_strfreev (keytype);
        return;
    }

    gvalue = g_new0 (GValue, 1);
    if (strcmp (keytype[0], "int") == 0)
    {
        g_value_init (gvalue, G_TYPE_INT);
        g_value_set_int (gvalue, strtol (param[1], NULL, 10));
    }
    else if (strcmp (keytype[0], "uint") == 0)
    {
        g_value_init (gvalue, G_TYPE_UINT);
        g_value_set_uint (gvalue, strtoul (param[1], NULL, 10));
    }
    else if (strcmp (keytype[0], "bool") == 0 || strcmp (keytype[0],
                                                         "boolean") == 0)
    {
        g_value_init (gvalue, G_TYPE_BOOLEAN);
        g_value_set_boolean (gvalue, atoi (param[1]));
    }
    else if (strcmp (keytype[0], "string") == 0)
    {
        g_value_init (gvalue, G_TYPE_STRING);
        g_value_set_string (gvalue, param[1]);
    }
    else
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        g_strfreev (param);
        g_strfreev (keytype);
        g_free (gvalue);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        g_strfreev (param);
        g_strfreev (keytype);
        g_free (gvalue);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_strfreev (param);
        g_strfreev (keytype);
        g_value_unset (gvalue);
        g_free (gvalue);
        g_object_unref (manager);
        return;
    }

    ag_account_set_value (account, keytype[1], gvalue);

    ag_account_store_blocking (account, &error);
    if (error)
    {
        show_error (ERROR_GENERIC);
        g_error_free (error);
    }

    g_strfreev (param);
    g_strfreev (keytype);
    g_value_unset (gvalue);
    g_free (gvalue);
    g_object_unref (account);
    g_object_unref (manager);
}

static void
create_account (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GError *error = NULL;

    if (argv[2] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    account = ag_manager_create_account (manager, argv[2]);
    if (account == NULL)
    {
        show_error (ERROR_GENERIC);
        g_object_unref (manager);
        return;
    }

    if (argv[3] != NULL)
        ag_account_set_display_name (account, argv[3]);

    if (argv[4] != NULL)
    {
        if (strcmp (argv[4], "enable") == 0)
            ag_account_set_enabled (account, TRUE);
        if (strcmp (argv[4], "disable") == 0)
            ag_account_set_enabled (account, FALSE);
    }

    ag_account_store_blocking (account, &error);
    if (error)
    {
        show_error (ERROR_GENERIC);
        g_error_free (error);
    }

    g_object_unref (account);
    g_object_unref (manager);
}

static void
enable_disable_service (gchar **argv, gboolean enable)
{

    AgManager *manager = NULL;
    AgService *service = NULL;
    AgAccount *account = NULL;
    GError *error = NULL;

    if ((argv[2] == NULL) || (argv[3] == NULL))
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_object_unref (manager);
        return;
    }

    service = ag_manager_get_service (manager, argv[3]);
    if (service == NULL)
    {
        show_error (INVALID_SERVICE_NAME);
        g_object_unref (account);
        g_object_unref (manager);
        return;
    }

    ag_account_select_service (account, service);
    ag_account_set_enabled (account, enable);

    ag_account_store_blocking (account, &error);
    if (error)
    {
        show_error (ERROR_GENERIC);
        g_error_free (error);
    }

    ag_service_unref (service);
    g_object_unref (account);
    g_object_unref (manager);
}

static void
delete_account (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    gint id = 0;
    GList *list = NULL;
    GList *iter = NULL;
    GError *error = NULL;

    if (argv[2] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    if (strcmp (argv[2], "all") == 0)
        list = ag_manager_list (manager);
    else
        list = g_list_prepend (list, GUINT_TO_POINTER (atoi (argv[2])));

    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        id = GPOINTER_TO_UINT (iter->data);
        account = ag_manager_get_account (manager, id);
        if (account == NULL)
        {
            show_error (INVALID_ACC_ID);
            continue;
        }

        ag_account_delete (account);

        ag_account_store_blocking (account, &error);
        if (error)
        {
            show_error (ERROR_GENERIC);
            g_error_free (error);
            error = NULL;
        }

        g_object_unref (account);
        account = NULL;
    }

    g_object_unref (manager);
    ag_manager_list_free (list);
}

static void
list_providers ()
{
    AgManager *manager = NULL;
    GList *list = NULL;
    GList *iter = NULL;
    const gchar *name = NULL;

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    list = ag_manager_list_providers (manager);
    if ((list == NULL) || (g_list_length (list) == 0))
    {
        printf ("No providers are available\n");
        return;
    }

    printf ("\nProvider Name\n-------------\n");
    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        name = ag_provider_get_name ((AgProvider *) (iter->data));
        printf ("%s\n", name);
    }

    ag_provider_list_free (list);
    g_object_unref (manager);
}

static void
list_services (gchar **argv)
{
    AgManager *manager = NULL;
    GList *list = NULL;
    GList *iter = NULL;
    const gchar *name = NULL;
    AgAccount *account = NULL;
    const gchar *type = NULL;

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    /* If account Id is not specified, list all services */
    if (argv[2] == NULL)
        list = ag_manager_list_services (manager);
    else
    {
        account = ag_manager_get_account (manager, atoi (argv[2]));
        if (account == NULL)
        {
            show_error (INVALID_ACC_ID);
            g_object_unref (manager);
            return;
        }

        list = ag_account_list_services (account);
    }

    if (list == NULL || g_list_length (list) == 0)
    {
        printf ("No services available\n");

        if (account)
            g_object_unref (account);

        g_object_unref (manager);
        return;
    }

    printf ("%-35s %s\n", "Service type", "Service name");
    printf ("%-35s %s\n", "------------", "------------");

    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        name = ag_service_get_name ((AgService *) (iter->data));
        type = ag_service_get_service_type ((AgService *) (iter->data));
        printf ("%-35s %s\n", type, name);
    }

    ag_service_list_free (list);

    if (account)
        g_object_unref (account);

    g_object_unref (manager);
}

static void
list_accounts ()
{
    AgManager *manager = NULL;
    GList *list = NULL;
    GList *iter = NULL;
    const gchar *name = NULL;
    const gchar *provider = NULL;
    AgAccount *account = NULL;

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    list = ag_manager_list (manager);
    if (list == NULL || g_list_length (list) == 0)
    {
        printf ("\nNo accounts configured\n");
        g_object_unref (manager);
        return;
    }

    printf ("%-10s %-30s %s\n", "ID", "Provider", "Name");
    printf ("%-10s %-30s %s\n", "--", "--------", "----");

    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        printf ("%-10d ", GPOINTER_TO_UINT (iter->data));

        account = ag_manager_get_account (manager,
                                          GPOINTER_TO_UINT (iter->data));
        if (account == NULL)
        {
            continue;
        }

        provider = ag_account_get_provider_name (account);
        if (provider != NULL)
            printf ("%-30s ", provider);
        else
            printf ("%-30s ", " ");

        name = ag_account_get_display_name (account);
        if (name != NULL)
            printf ("%s\n", name);
        else
            printf ("\n");

        g_object_unref (account);
        account = NULL;
    }

    ag_manager_list_free (list);
    g_object_unref (manager);
}

static void
enable_disable_account (gchar **argv, gboolean enable)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GError *error = NULL;

    if (argv[2] == NULL)
    {
        show_error (INVALID_INPUT);
        show_help_text (argv[1]);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    account = ag_manager_get_account (manager, atoi (argv[2]));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_object_unref (manager);
        return;
    }

    ag_account_set_enabled (account, enable);
    ag_account_store_blocking (account, &error);
    if (error)
    {
        show_error (ERROR_GENERIC);
        g_error_free (error);
    }

    g_object_unref (account);
    g_object_unref (manager);
}

static void
list_enabled_services (gchar *id)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GList *list = NULL;
    GList *iter = NULL;
    const gchar *name = NULL;
    const gchar *type = NULL;

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    account = ag_manager_get_account (manager, atoi (id));
    if (account == NULL)
    {
        show_error (INVALID_ACC_ID);
        g_object_unref (manager);
        return;
    }

    list = ag_account_list_enabled_services (account);
    if (list == NULL || g_list_length (list) == 0)
    {
        printf ("No services enabled for account\n");
        g_object_unref (account);
        g_object_unref (manager);
        return;
    }

    printf ("%-35s%s\n", "Type", "Service Name");
    printf ("%-35s%s\n", "----", "------------");
    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        name = ag_service_get_name ((AgService *) (iter->data));
        type = ag_service_get_service_type ((AgService *) (iter->data));
        printf ("%-35s", type);
        printf ("%s\n", name);
    }

    ag_service_list_free (list);
    g_object_unref (account);
    g_object_unref (manager);
}

static void
list_enabled (gchar **argv)
{
    AgManager *manager = NULL;
    AgAccount *account = NULL;
    GList *list = NULL;
    GList *iter = NULL;
    const gchar *provider = NULL;
    const gchar *name = NULL;

    if (argv[2] != NULL)
    {
        list_enabled_services (argv[2]);
        return;
    }

    manager = ag_manager_new ();
    if (manager == NULL)
    {
        show_error (ERROR_GENERIC);
        return;
    }

    list = ag_manager_list_enabled (manager);
    if (list == NULL || g_list_length (list) == 0)
    {
        printf ("No accounts enabled\n");
        g_object_unref (manager);
        return;
    }

    printf ("%-10s %-30s %s\n", "ID", "Provider", "Name");
    printf ("%-10s %-30s %s\n", "--", "--------", "----");


    for (iter = list; iter != NULL; iter = g_list_next (iter))
    {
        printf ("%-10d ", (AgAccountId) GPOINTER_TO_UINT (iter->data));

        account = ag_manager_get_account (manager,
                                          GPOINTER_TO_UINT (iter->data));
        if (account == NULL)
        {
            continue;
        }

        provider = ag_account_get_provider_name (account);
        if (provider != NULL)
            printf ("%-30s ", provider);
        else
            printf ("%-30s ", " ");

        name = ag_account_get_display_name (account);
        if (name != NULL)
            printf ("%s\n", name);
        else
            printf ("\n");

        g_object_unref (account);
        account = NULL;

    }
    ag_manager_list_free (list);
    g_object_unref (manager);
}

static int
parse (int argc, char **argv)
{
    int i = 0;

    if (strcmp (argv[1], "create-account") == 0)
    {
        create_account (argv);
        return 0;
    }
    else if (strcmp (argv[1], "delete-account") == 0)
    {
        delete_account (argv);
        return 0;
    }
    else if (strcmp (argv[1], "list-providers") == 0)
    {
        list_providers ();
        return 0;
    }
    else if (strcmp (argv[1], "list-services") == 0)
    {
        list_services (argv);
        return 0;
    }
    else if (strcmp (argv[1], "list-accounts") == 0)
    {
        list_accounts ();
        return 0;
    }
    else if (strcmp (argv[1], "enable-account") == 0)
    {
        enable_disable_account (argv, TRUE);
        return 0;
    }
    else if (strcmp (argv[1], "disable-account") == 0)
    {
        enable_disable_account (argv, FALSE);
        return 0;
    }
    else if (strcmp (argv[1], "list-enabled") == 0)
    {
        list_enabled (argv);
        return 0;
    }
    else if (strcmp (argv[1], "enable-service") == 0)
    {
        enable_disable_service (argv, TRUE);
        return 0;
    }
    else if (strcmp (argv[1], "disable-service") == 0)
    {
        enable_disable_service (argv, FALSE);
        return 0;
    }
    else if (strcmp (argv[1], "update-account") == 0)
    {
        update_account (argv);
        return 0;
    }
    else if (strcmp (argv[1], "update-service") == 0)
    {
        update_service (argv);
        return 0;
    }
    else if (strcmp (argv[1], "get-service") == 0)
    {
        get_service (argv);
        return 0;
    }
    else if (strcmp (argv[1], "get-account") == 0)
    {
        get_account (argv);
        return 0;
    }
    else if (strcmp (argv[1], "list-settings") == 0)
    {
        list_settings (argv);
        return 0;
    }

    return -1;
}

gint
main (int argc, char **argv)
{
    gl_app_name = g_path_get_basename (argv[0]);

    if (argc < 2)
    {
        show_help ();
        return 0;
    }

    if (parse (argc, argv))
        show_help ();
}
