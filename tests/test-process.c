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

#include "libaccounts-glib/ag-manager.h"
#include "libaccounts-glib/ag-account.h"
#include "libaccounts-glib/ag-service.h"
#include "libaccounts-glib/ag-errors.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static GMainLoop *main_loop = NULL;
static AgAccount *account = NULL;
static AgManager *manager = NULL;
static AgService *service = NULL;
static sqlite3 *sqldb = NULL;
int lock_file = 0;

#define PROVIDER    "dummyprovider"

typedef struct {
    gint argc;
    gchar **argv;
} TestArgs;

static void
lock_db(gboolean lock)
{
    static sqlite3_stmt *begin_stmt = NULL;
    static sqlite3_stmt *commit_stmt = NULL;

    /* this lock is to synchronize with the main test application */
    if (!lock)
    {
        int ret = lockf(lock_file, F_ULOCK, 0);
        g_assert(ret == 0);
    }

    if (!begin_stmt)
    {
        sqlite3_prepare_v2 (sqldb, "BEGIN EXCLUSIVE;", -1, &begin_stmt, NULL);
        sqlite3_prepare_v2 (sqldb, "COMMIT;", -1, &commit_stmt, NULL);
    }
    else
    {
        sqlite3_reset (begin_stmt);
        sqlite3_reset (commit_stmt);
    }

    if (lock)
        sqlite3_step (begin_stmt);
    else
        sqlite3_step (commit_stmt);

    /* this lock is to synchronize with the main test application */
    if (lock)
    {
        int ret = lockf(lock_file, F_LOCK, 0);
        g_assert(ret == 0);
    }
}

static void
end_test ()
{
    if (account)
    {
        g_object_unref (account);
        account = NULL;
    }
    if (manager)
    {
        g_object_unref (manager);
        manager = NULL;
    }
    if (service)
    {
        ag_service_unref (service);
        service = NULL;
    }

    if (main_loop)
    {
        g_main_loop_quit (main_loop);
        g_main_loop_unref (main_loop);
        main_loop = NULL;
    }
}

void account_store_cb (AgAccount *account, const GError *error,
                       gpointer user_data)
{
    if (error)
        g_warning ("Got error: %s", error->message);

    end_test ();
}

gboolean test_create (TestArgs *args)
{
    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, args->argv[0]);

    if (args->argc > 1)
    {
        ag_account_set_display_name (account, args->argv[1]);
    }

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_delete (TestArgs *args)
{
    AgAccountId id;

    manager = ag_manager_new ();
    id = atoi(args->argv[0]);
    account = ag_manager_get_account (manager, id);
    ag_account_delete (account);

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_create2 (TestArgs *args)
{
    GValue value = { 0 };
    const gchar *numbers[] = {
        "one",
        "two",
        "three",
        NULL
    };

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, args->argv[0]);

    if (args->argc > 1)
    {
        ag_account_set_display_name (account, args->argv[1]);
    }

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, -12345);
    ag_account_set_value (account, "integer", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "a string");
    ag_account_set_value (account, "string", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRV);
    g_value_set_boxed (&value, numbers);
    ag_account_set_value (account, "numbers", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, TRUE);

    /* also set some keys in one service */
    service = ag_manager_get_service (manager, "MyService");
    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_UINT);
    g_value_set_uint (&value, 54321);
    ag_account_set_value (account, "unsigned", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_CHAR);
#if GLIB_CHECK_VERSION(2,32,0)
    g_value_set_schar (&value, 'z');
#else
    g_value_set_char (&value, 'z');
#endif
    ag_account_set_value (account, "character", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, TRUE);
    ag_account_set_value (account, "boolean", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, FALSE);

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_create3 (TestArgs *args)
{
    GValue value = { 0 };

    manager = ag_manager_new ();

    account = ag_manager_create_account (manager, args->argv[0]);

    if (args->argc > 1)
    {
        ag_account_set_display_name (account, args->argv[1]);
    }

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, -12345);
    ag_account_set_value (account, "integer", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "a string");
    ag_account_set_value (account, "string", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, TRUE);

    /* also set some keys in one service */
    service = ag_manager_get_service (manager, "MyService");
    ag_account_select_service (account, service);

    g_value_init (&value, G_TYPE_UINT);
    g_value_set_uint (&value, 54321);
    ag_account_set_value (account, "unsigned", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_CHAR);
#if GLIB_CHECK_VERSION(2,32,0)
    g_value_set_schar (&value, 'z');
#else
    g_value_set_char (&value, 'z');
#endif
    ag_account_set_value (account, "character", &value);
    g_value_unset (&value);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, TRUE);
    ag_account_set_value (account, "boolean", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, TRUE);

    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_change (TestArgs *args)
{
    GValue value = { 0 };

    AgAccountId id;

    manager = ag_manager_new ();
    id = atoi(args->argv[0]);
    account = ag_manager_get_account (manager, id);

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_static_string (&value, "another string");
    ag_account_set_value (account, "string", &value);
    g_value_unset (&value);

    service = ag_manager_get_service (manager, "MyService");
    ag_account_select_service (account, service);

    ag_account_set_value (account, "character", NULL);

    g_value_init (&value, G_TYPE_BOOLEAN);
    g_value_set_boolean (&value, FALSE);
    ag_account_set_value (account, "boolean", &value);
    g_value_unset (&value);

    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_enabled_event (TestArgs *args)
{
    AgAccountId id;

    manager = ag_manager_new ();
    id = atoi(args->argv[0]);
    account = ag_manager_get_account (manager, id);
    service = ag_manager_get_service (manager, "MyService");
    ag_account_select_service (account, service);
    ag_account_set_enabled (account, TRUE);
    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean test_enabled_event2 (TestArgs *args)
{
    AgAccountId id;

    manager = ag_manager_new ();
    id = atoi (args->argv[0]);
    account = ag_manager_get_account (manager, id);
    ag_account_select_service (account, NULL);
    ag_account_set_enabled (account, FALSE);
    ag_account_store (account, account_store_cb, NULL);

    return FALSE;
}

gboolean unlock_and_exit()
{
    lock_db(FALSE);
    end_test ();
    return FALSE;
}

gboolean test_lock_db (TestArgs *args)
{
    const gchar *basedir;
    gchar *filename;
    gint ms;

    ms = atoi(args->argv[0]);
    lock_file = open(args->argv[1], O_RDWR | O_APPEND);

    basedir = g_getenv ("ACCOUNTS");
    if (G_LIKELY (!basedir))
        basedir = g_get_home_dir ();
    filename = g_build_filename (basedir, "accounts.db", NULL);
    sqlite3_open (filename, &sqldb);
    g_free (filename);

    lock_db(TRUE);

    g_timeout_add (ms, (GSourceFunc)unlock_and_exit, NULL);

    return FALSE;
}

int main(int argc, char **argv)
{
    TestArgs args;

    if (argc >= 2)
    {
        const gchar *test_name = argv[1];

        argc -= 2;
        argv += 2;
        args.argc = argc;
        args.argv = argv;

        if (strcmp (test_name, "create") == 0)
        {
            g_idle_add ((GSourceFunc)test_create, &args);
        }
        else if (strcmp (test_name, "delete") == 0)
        {
            g_idle_add ((GSourceFunc)test_delete, &args);
        }
        else if (strcmp (test_name, "create2") == 0)
        {
            g_idle_add ((GSourceFunc)test_create2, &args);
        }
        else if (strcmp (test_name, "create3") == 0)
        {
            g_idle_add ((GSourceFunc)test_create3, &args);
        }
        else if (strcmp (test_name, "change") == 0)
        {
            g_idle_add ((GSourceFunc)test_change, &args);
        }
        else if (strcmp (test_name, "lock_db") == 0)
        {
            g_idle_add ((GSourceFunc)test_lock_db, &args);
        }
        else if (strcmp (test_name, "enabled_event") == 0)
        {
            g_idle_add ((GSourceFunc)test_enabled_event, &args);
        }
        else if (strcmp (test_name, "enabled_event2") == 0)
        {
            g_idle_add ((GSourceFunc)test_enabled_event2, &args);
        }

        main_loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (main_loop);
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

