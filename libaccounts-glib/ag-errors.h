/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012-2013 Canonical Ltd.
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

#ifndef _AG_ERRORS_H_
#define _AG_ERRORS_H_

#include <glib.h>

G_BEGIN_DECLS

GQuark ag_errors_quark (void);
GQuark ag_accounts_error_quark (void);

#define AG_ERRORS   ag_errors_quark ()

/**
 * AG_ACCOUNTS_ERROR:
 *
 * Error domain for libaccounts-glib errors. Errors in this domain will be from
 * the AgAccountsError enumeration.
 */
#define AG_ACCOUNTS_ERROR AG_ERRORS

typedef enum {
    AG_ERROR_DB,
    AG_ERROR_DISPOSED,
    AG_ERROR_DELETED,
    AG_ERROR_DB_LOCKED,
    AG_ERROR_ACCOUNT_NOT_FOUND,
} AgError;

/**
 * AgAccountsError:
 * @AG_ACCOUNTS_ERROR_DB: there was an error accessing the accounts database
 * @AG_ACCOUNTS_ERROR_DISPOSED: the account was in the process of being
 * disposed
 * @AG_ACCOUNTS_ERROR_DELETED: the account was in the process of being deleted
 * @AG_ACCOUNTS_ERROR_DB_LOCKED: the database was locked
 * @AG_ACCOUNTS_ERROR_ACCOUNT_NOT_FOUND: the requested account was not found
 * @AG_ACCOUNTS_ERROR_STORE_IN_PROGRESS: an asynchronous store operation is
 * already in progress. Since 1.4
 * @AG_ACCOUNTS_ERROR_READONLY: the accounts DB is in read-only mode. Since 1.4
 *
 * These identify the various errors that can occur with methods in
 * libaccounts-glib that return a #GError.
 */
typedef enum {
    AG_ACCOUNTS_ERROR_DB = AG_ERROR_DB,
    AG_ACCOUNTS_ERROR_DISPOSED = AG_ERROR_DISPOSED,
    AG_ACCOUNTS_ERROR_DELETED = AG_ERROR_DELETED,
    AG_ACCOUNTS_ERROR_DB_LOCKED = AG_ERROR_DB_LOCKED,
    AG_ACCOUNTS_ERROR_ACCOUNT_NOT_FOUND = AG_ERROR_ACCOUNT_NOT_FOUND,
    AG_ACCOUNTS_ERROR_STORE_IN_PROGRESS,
    AG_ACCOUNTS_ERROR_READONLY,
} AgAccountsError;

G_END_DECLS

#endif /* _AG_ERRORS_H_ */
