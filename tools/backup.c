/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2011 Nokia Corporation.
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

#include <config.h>

#include <glib.h>
#include <sched.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
show_help ()
{
    printf ("\nUsage:\n"
            "   %1$s\n"
            "Backups the accounts from ~/.config/libaccounts-glib/accounts.db\n"
            "into ~/.config/libaccounts-glib/accounts.db.bak\n\n",
            g_get_prgname());
}

static gboolean
write_backup (sqlite3 *src, const gchar *filename)
{
    sqlite3_backup *backup;
    sqlite3 *dest;
    gint n_retries;
    int ret;

    ret = sqlite3_open (filename, &dest);
    if (ret != SQLITE_OK) return FALSE;

    backup = sqlite3_backup_init (dest, "main", src, "main");
    if (!backup)
    {
        g_warning ("Couldn't start backup");
        sqlite3_close (dest);
        return FALSE;
    }

    n_retries = 0;
    do
    {
        ret = sqlite3_backup_step (backup, -1);
        if (ret == SQLITE_BUSY || ret == SQLITE_LOCKED)
            sqlite3_sleep(250);
        n_retries++;
    }
    while ((ret == SQLITE_BUSY || ret == SQLITE_LOCKED) && n_retries < 5);

    sqlite3_backup_finish (backup);

    sqlite3_close (dest);
    return ret == SQLITE_OK;
}

static gboolean
backup ()
{
    gchar *filename, *filename_bak;
    sqlite3 *db;
    gint n_retries;
    int ret;
    gboolean success = FALSE;;

    filename = g_build_filename (g_get_user_config_dir (),
                                 DATABASE_DIR,
                                 "accounts.db",
                                 NULL);
    filename_bak = g_strdup_printf ("%s.bak", filename);

    g_debug ("Opening %s", filename);

    n_retries = 0;
    do
    {
        ret = sqlite3_open (filename, &db);
        if (ret == SQLITE_BUSY)
            sched_yield ();
        n_retries++;
    }
    while (ret == SQLITE_BUSY && n_retries < 5);

    if (G_UNLIKELY (ret != SQLITE_OK))
    {
        g_warning ("Couldn't open accounts DB: %s", sqlite3_errmsg (db));
        goto error_open;
    }

    n_retries = 0;
    do
    {
        ret = sqlite3_wal_checkpoint (db, NULL);
        if (ret == SQLITE_BUSY)
            sched_yield ();
        n_retries++;
    }
    while (ret == SQLITE_BUSY && n_retries < 5);

    if (G_UNLIKELY (ret != SQLITE_OK))
        g_warning ("Checkpoint failed: %s", sqlite3_errmsg (db));

    success = write_backup (db, filename_bak);

    sqlite3_close (db);
    success = TRUE;

error_open:
    g_free (filename_bak);
    g_free (filename);
    return success;
}

int
main (int argc, char **argv)
{
    gboolean success;

    g_set_prgname (g_path_get_basename (argv[0]));

    if (argc > 1)
    {
        show_help ();
        return 0;
    }

    success = backup ();

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

