/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2019 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include "config.h"

#include <glib.h>
#include <gio/gio.h>
#include <libmalcontent/session-limits.h>
#include <libmalcontent/manager.h>
#include <libglib-testing/dbus-queue.h>
#include <locale.h>
#include <string.h>
#include "accounts-service-iface.h"
#include "accounts-service-extension-iface.h"


/* Helper function to convert a constant time in seconds to microseconds,
 * avoiding issues with integer constants being too small for the multiplication
 * by using explicit typing. */
static guint64
usec (guint64 sec)
{
  return sec * G_USEC_PER_SEC;
}

/* Test that the #GType definitions for various types work. */
static void
test_session_limits_types (void)
{
  g_type_ensure (mct_session_limits_get_type ());
  g_type_ensure (mct_session_limits_builder_get_type ());
}

/* Test that ref() and unref() work on an #MctSessionLimits. */
static void
test_session_limits_refs (void)
{
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) limits = NULL;

  /* Use an empty #MctSessionLimits. */
  limits = mct_session_limits_builder_end (&builder);

  g_assert_nonnull (limits);

  /* Call check_time_remaining() to check that the limits object hasn’t been
   * finalised. */
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
  mct_session_limits_ref (limits);
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
  mct_session_limits_unref (limits);
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));

  /* Final ref is dropped by g_autoptr(). */
}

/* Check error handling when passing an invalid time for @now_usecs to
 * mct_session_limits_check_time_remaining(). */
static void
test_session_limits_check_time_remaining_invalid_time (void)
{
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) limits = NULL;
  guint64 time_remaining_secs;
  gboolean time_limit_enabled;

  /* Use an empty #MctSessionLimits. */
  limits = mct_session_limits_builder_end (&builder);

  /* Pass an invalid time to mct_session_limits_check_time_remaining(). */
  g_assert_false (mct_session_limits_check_time_remaining (limits, G_MAXUINT64, &time_remaining_secs, &time_limit_enabled));
  g_assert_cmpuint (time_remaining_secs, ==, 0);
  g_assert_true (time_limit_enabled);
}

/* Basic test of mct_session_limits_serialize() on session limits. */
static void
test_session_limits_serialize (void)
{
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) limits = NULL;
  g_autoptr(GVariant) serialized = NULL;

  /* Use an empty #MctSessionLimits. */
  limits = mct_session_limits_builder_end (&builder);

  /* We can’t assert anything about the serialisation format, since it’s opaque. */
  serialized = mct_session_limits_serialize (limits);
  g_assert_nonnull (serialized);
}

/* Basic test of mct_session_limits_deserialize() on various current and historic
 * serialised app filter variants. */
static void
test_session_limits_deserialize (void)
{
  /* These are all opaque. Older versions should be kept around to test
   * backwards compatibility. */
  const gchar *valid_session_limits[] =
    {
      "@a{sv} {}",
      "{ 'LimitType': <@u 0> }",
      "{ 'LimitType': <@u 1>, 'DailySchedule': <(@u 0, @u 100)> }",
      "{ 'DailySchedule': <(@u 0, @u 100)> }",
    };

  for (gsize i = 0; i < G_N_ELEMENTS (valid_session_limits); i++)
    {
      g_autoptr(GVariant) serialized = NULL;
      g_autoptr(MctSessionLimits) limits = NULL;
      g_autoptr(GError) local_error = NULL;

      g_test_message ("%" G_GSIZE_FORMAT ": %s", i, valid_session_limits[i]);

      serialized = g_variant_parse (NULL, valid_session_limits[i], NULL, NULL, NULL);
      g_assert (serialized != NULL);

      limits = mct_session_limits_deserialize (serialized, 1, &local_error);
      g_assert_no_error (local_error);
      g_assert_nonnull (limits);
    }
}

/* Test of mct_session_limits_deserialize() on various invalid variants. */
static void
test_session_limits_deserialize_invalid (void)
{
  const gchar *invalid_session_limits[] =
    {
      "false",
      "()",
      "{ 'LimitType': <@u 100> }",
      "{ 'DailySchedule': <(@u 100, @u 0)> }",
      "{ 'DailySchedule': <(@u 0, @u 4294967295)> }",
    };

  for (gsize i = 0; i < G_N_ELEMENTS (invalid_session_limits); i++)
    {
      g_autoptr(GVariant) serialized = NULL;
      g_autoptr(MctSessionLimits) limits = NULL;
      g_autoptr(GError) local_error = NULL;

      g_test_message ("%" G_GSIZE_FORMAT ": %s", i, invalid_session_limits[i]);

      serialized = g_variant_parse (NULL, invalid_session_limits[i], NULL, NULL, NULL);
      g_assert (serialized != NULL);

      limits = mct_session_limits_deserialize (serialized, 1, &local_error);
      g_assert_error (local_error, MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_INVALID_DATA);
      g_assert_null (limits);
    }
}

/* Fixture for tests which use an #MctSessionLimitsBuilder. The builder can
 * either be heap- or stack-allocated. @builder will always be a valid pointer
 * to it.
 */
typedef struct
{
  MctSessionLimitsBuilder *builder;
  MctSessionLimitsBuilder stack_builder;
} BuilderFixture;

static void
builder_set_up_stack (BuilderFixture *fixture,
                      gconstpointer   test_data)
{
  mct_session_limits_builder_init (&fixture->stack_builder);
  fixture->builder = &fixture->stack_builder;
}

static void
builder_tear_down_stack (BuilderFixture *fixture,
                         gconstpointer   test_data)
{
  mct_session_limits_builder_clear (&fixture->stack_builder);
  fixture->builder = NULL;
}

static void
builder_set_up_stack2 (BuilderFixture *fixture,
                       gconstpointer   test_data)
{
  MctSessionLimitsBuilder local_builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  memcpy (&fixture->stack_builder, &local_builder, sizeof (local_builder));
  fixture->builder = &fixture->stack_builder;
}

static void
builder_tear_down_stack2 (BuilderFixture *fixture,
                          gconstpointer   test_data)
{
  mct_session_limits_builder_clear (&fixture->stack_builder);
  fixture->builder = NULL;
}

static void
builder_set_up_heap (BuilderFixture *fixture,
                     gconstpointer   test_data)
{
  fixture->builder = mct_session_limits_builder_new ();
}

static void
builder_tear_down_heap (BuilderFixture *fixture,
                        gconstpointer   test_data)
{
  g_clear_pointer (&fixture->builder, mct_session_limits_builder_free);
}

/* Test building a non-empty #MctSessionLimits using an
 * #MctSessionLimitsBuilder. */
static void
test_session_limits_builder_non_empty (BuilderFixture *fixture,
                                       gconstpointer   test_data)
{
  g_autoptr(MctSessionLimits) limits = NULL;
  g_autofree const gchar **sections = NULL;

  mct_session_limits_builder_set_daily_schedule (fixture->builder, 100, 8 * 60 * 60);

  limits = mct_session_limits_builder_end (fixture->builder);

  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (99), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (100), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60 - 1), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60), NULL, NULL));
}

/* Test building an empty #MctSessionLimits using an #MctSessionLimitsBuilder. */
static void
test_session_limits_builder_empty (BuilderFixture *fixture,
                                   gconstpointer   test_data)
{
  g_autoptr(MctSessionLimits) limits = NULL;
  g_autofree const gchar **sections = NULL;

  limits = mct_session_limits_builder_end (fixture->builder);

  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (99), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (100), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60 - 1), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60), NULL, NULL));
}

/* Check that copying a cleared #MctSessionLimitsBuilder works, and the copy can
 * then be initialised and used to build a limits object. */
static void
test_session_limits_builder_copy_empty (void)
{
  g_autoptr(MctSessionLimitsBuilder) builder = mct_session_limits_builder_new ();
  g_autoptr(MctSessionLimitsBuilder) builder_copy = NULL;
  g_autoptr(MctSessionLimits) limits = NULL;

  mct_session_limits_builder_clear (builder);
  builder_copy = mct_session_limits_builder_copy (builder);

  mct_session_limits_builder_init (builder_copy);
  mct_session_limits_builder_set_daily_schedule (builder_copy, 100, 8 * 60 * 60);
  limits = mct_session_limits_builder_end (builder_copy);

  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (99), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (100), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60 - 1), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60), NULL, NULL));
}

/* Check that copying a filled #MctSessionLimitsBuilder works, and the copy can
 * be used to build a limits object. */
static void
test_session_limits_builder_copy_full (void)
{
  g_autoptr(MctSessionLimitsBuilder) builder = mct_session_limits_builder_new ();
  g_autoptr(MctSessionLimitsBuilder) builder_copy = NULL;
  g_autoptr(MctSessionLimits) limits = NULL;

  mct_session_limits_builder_set_daily_schedule (builder, 100, 8 * 60 * 60);
  builder_copy = mct_session_limits_builder_copy (builder);
  limits = mct_session_limits_builder_end (builder_copy);

  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (99), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (100), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60 - 1), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (8 * 60 * 60), NULL, NULL));
}

/* Check that overriding an already-set limit in a #MctSessionLimitsBuilder
 * removes all trace of it. In this test, override with a ‘none’ limit. */
static void
test_session_limits_builder_override_none (void)
{
  g_autoptr(MctSessionLimitsBuilder) builder = mct_session_limits_builder_new ();
  g_autoptr(MctSessionLimits) limits = NULL;

  /* Set up some schedule. */
  mct_session_limits_builder_set_daily_schedule (builder, 100, 8 * 60 * 60);

  /* Override it. */
  mct_session_limits_builder_set_none (builder);
  limits = mct_session_limits_builder_end (builder);

  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (0), NULL, NULL));
}

/* Check that overriding an already-set limit in a #MctSessionLimitsBuilder
 * removes all trace of it. In this test, override with a ‘daily schedule’
 * limit. */
static void
test_session_limits_builder_override_daily_schedule (void)
{
  g_autoptr(MctSessionLimitsBuilder) builder = mct_session_limits_builder_new ();
  g_autoptr(MctSessionLimits) limits = NULL;

  /* Set up some schedule. */
  mct_session_limits_builder_set_daily_schedule (builder, 100, 8 * 60 * 60);

  /* Override it. */
  mct_session_limits_builder_set_daily_schedule (builder, 200, 7 * 60 * 60);
  limits = mct_session_limits_builder_end (builder);

  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (150), NULL, NULL));
  g_assert_true (mct_session_limits_check_time_remaining (limits, usec (4 * 60 * 60), NULL, NULL));
  g_assert_false (mct_session_limits_check_time_remaining (limits, usec (7 * 60 * 60 + 30 * 60), NULL, NULL));
}

/* Fixture for tests which interact with the accountsservice over D-Bus. The
 * D-Bus service is mocked up using @queue, which allows us to reply to D-Bus
 * calls from the code under test from within the test process.
 *
 * It exports one user object (for UID 500) and the manager object. The method
 * return values from UID 500 are up to the test in question, so it could be an
 * administrator, or non-administrator, have a restrictive or permissive app
 * limits, etc.
 */
typedef struct
{
  GtDBusQueue *queue;  /* (owned) */
  uid_t valid_uid;
  uid_t missing_uid;
  MctManager *manager;  /* (owned) */
} BusFixture;

static void
bus_set_up (BusFixture    *fixture,
            gconstpointer  test_data)
{
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *object_path = NULL;

  fixture->valid_uid = 500;  /* arbitrarily chosen */
  fixture->missing_uid = 501;  /* must be different from valid_uid and not exported */
  fixture->queue = gt_dbus_queue_new ();

  gt_dbus_queue_connect (fixture->queue, &local_error);
  g_assert_no_error (local_error);

  gt_dbus_queue_own_name (fixture->queue, "org.freedesktop.Accounts");

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", fixture->valid_uid);
  gt_dbus_queue_export_object (fixture->queue,
                               object_path,
                               (GDBusInterfaceInfo *) &com_endlessm_parental_controls_session_limits_interface,
                               &local_error);
  g_assert_no_error (local_error);

  gt_dbus_queue_export_object (fixture->queue,
                               "/org/freedesktop/Accounts",
                               (GDBusInterfaceInfo *) &org_freedesktop_accounts_interface,
                               &local_error);
  g_assert_no_error (local_error);

  fixture->manager = mct_manager_new (gt_dbus_queue_get_client_connection (fixture->queue));
}

static void
bus_tear_down (BusFixture    *fixture,
               gconstpointer  test_data)
{
  g_clear_object (&fixture->manager);
  gt_dbus_queue_disconnect (fixture->queue, TRUE);
  g_clear_pointer (&fixture->queue, gt_dbus_queue_free);
}

/* Helper #GAsyncReadyCallback which returns the #GAsyncResult in its @user_data. */
static void
async_result_cb (GObject      *obj,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GAsyncResult **result_out = (GAsyncResult **) user_data;

  g_assert_null (*result_out);
  *result_out = g_object_ref (result);
}

/* Generic mock accountsservice implementation which returns the properties
 * given in #GetSessionLimitsData.properties if queried for a UID matching
 * #GetSessionLimitsData.expected_uid. Intended to be used for writing
 * ‘successful’ mct_manager_get_session_limits() tests returning a variety of
 * values. */
typedef struct
{
  uid_t expected_uid;
  const gchar *properties;
} GetSessionLimitsData;

/* This is run in a worker thread. */
static void
get_session_limits_server_cb (GtDBusQueue *queue,
                              gpointer     user_data)
{
  const GetSessionLimitsData *data = user_data;
  g_autoptr(GDBusMethodInvocation) invocation1 = NULL;
  g_autoptr(GDBusMethodInvocation) invocation2 = NULL;
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) properties_variant = NULL;

  /* Handle the FindUserById() call. */
  gint64 user_id;
  invocation1 =
      gt_dbus_queue_assert_pop_message (queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, data->expected_uid);

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", (uid_t) user_id);
  g_dbus_method_invocation_return_value (invocation1, g_variant_new ("(o)", object_path));

  /* Handle the Properties.GetAll() call and return some arbitrary, valid values
   * for the given user. */
  const gchar *property_interface;
  invocation2 =
      gt_dbus_queue_assert_pop_message (queue,
                                        object_path,
                                        "org.freedesktop.DBus.Properties",
                                        "GetAll", "(&s)", &property_interface);
  g_assert_cmpstr (property_interface, ==, "com.endlessm.ParentalControls.SessionLimits");

  properties_variant = g_variant_ref_sink (g_variant_new_parsed (data->properties));
  g_dbus_method_invocation_return_value (invocation2,
                                         g_variant_new_tuple (&properties_variant, 1));
}

/* Test that getting an #MctSessionLimits from the mock D-Bus service works. The
 * @test_data is a boolean value indicating whether to do the call
 * synchronously (%FALSE) or asynchronously (%TRUE).
 *
 * The mock D-Bus replies are generated in get_session_limits_server_cb(), which
 * is used for both synchronous and asynchronous calls. */
static void
test_session_limits_bus_get (BusFixture    *fixture,
                             gconstpointer  test_data)
{
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GError) local_error = NULL;
  guint64 time_remaining_secs;
  gboolean time_limit_enabled;
  gboolean test_async = GPOINTER_TO_UINT (test_data);
  const GetSessionLimitsData get_session_limits_data =
    {
      .expected_uid = fixture->valid_uid,
      .properties = "{"
        "'LimitType': <@u 1>,"
        "'DailySchedule': <(@u 100, @u 8000)>"
      "}"
    };

  gt_dbus_queue_set_server_func (fixture->queue, get_session_limits_server_cb,
                                 (gpointer) &get_session_limits_data);

  if (test_async)
    {
      g_autoptr(GAsyncResult) result = NULL;

      mct_manager_get_session_limits_async (fixture->manager,
                                            fixture->valid_uid,
                                            MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                            async_result_cb, &result);

      while (result == NULL)
        g_main_context_iteration (NULL, TRUE);
      session_limits = mct_manager_get_session_limits_finish (fixture->manager, result, &local_error);
    }
  else
    {
      session_limits = mct_manager_get_session_limits (fixture->manager,
                                                       fixture->valid_uid,
                                                       MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                                       &local_error);
    }

  g_assert_no_error (local_error);
  g_assert_nonnull (session_limits);

  /* Check the session limits properties. */
  g_assert_cmpuint (mct_session_limits_get_user_id (session_limits), ==, fixture->valid_uid);
  g_assert_true (mct_session_limits_is_enabled (session_limits));
  g_assert_false (mct_session_limits_check_time_remaining (session_limits, usec (0),
                                                           &time_remaining_secs, &time_limit_enabled));
  g_assert_true (time_limit_enabled);
  g_assert_true (mct_session_limits_check_time_remaining (session_limits, usec (2000),
                                                          &time_remaining_secs, &time_limit_enabled));
  g_assert_cmpuint (time_remaining_secs, ==, 8000 - 2000);
  g_assert_true (time_limit_enabled);
}

/* Test that getting an #MctSessionLimits from the mock D-Bus service works. The
 * @test_data is a boolean value indicating whether to do the call
 * synchronously (%FALSE) or asynchronously (%TRUE).
 *
 * The mock D-Bus replies are generated in get_session_limits_server_cb(), which
 * is used for both synchronous and asynchronous calls. */
static void
test_session_limits_bus_get_none (BusFixture    *fixture,
                                  gconstpointer  test_data)
{
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GError) local_error = NULL;
  guint64 time_remaining_secs;
  gboolean time_limit_enabled;
  gboolean test_async = GPOINTER_TO_UINT (test_data);
  const GetSessionLimitsData get_session_limits_data =
    {
      .expected_uid = fixture->valid_uid,
      .properties = "{"
        "'LimitType': <@u 0>,"
        "'DailySchedule': <(@u 0, @u 86400)>"
      "}"
    };

  gt_dbus_queue_set_server_func (fixture->queue, get_session_limits_server_cb,
                                 (gpointer) &get_session_limits_data);

  if (test_async)
    {
      g_autoptr(GAsyncResult) result = NULL;

      mct_manager_get_session_limits_async (fixture->manager,
                                            fixture->valid_uid,
                                            MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                            async_result_cb, &result);

      while (result == NULL)
        g_main_context_iteration (NULL, TRUE);
      session_limits = mct_manager_get_session_limits_finish (fixture->manager, result, &local_error);
    }
  else
    {
      session_limits = mct_manager_get_session_limits (fixture->manager,
                                                       fixture->valid_uid,
                                                       MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                                       &local_error);
    }

  g_assert_no_error (local_error);
  g_assert_nonnull (session_limits);

  /* Check the session limits properties. */
  g_assert_cmpuint (mct_session_limits_get_user_id (session_limits), ==, fixture->valid_uid);
  g_assert_false (mct_session_limits_is_enabled (session_limits));
  g_assert_true (mct_session_limits_check_time_remaining (session_limits, usec (0),
                                                          &time_remaining_secs, &time_limit_enabled));
  g_assert_false (time_limit_enabled);
  g_assert_true (mct_session_limits_check_time_remaining (session_limits, usec (2000),
                                                          &time_remaining_secs, &time_limit_enabled));
  g_assert_false (time_limit_enabled);
}

/* Test that mct_manager_get_session_limits() returns an appropriate error if the
 * mock D-Bus service reports that the given user cannot be found.
 *
 * The mock D-Bus replies are generated inline. */
static void
test_session_limits_bus_get_error_invalid_user (BusFixture    *fixture,
                                                gconstpointer  test_data)
{
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GDBusMethodInvocation) invocation = NULL;
  g_autofree gchar *error_message = NULL;
  g_autoptr(MctSessionLimits) session_limits = NULL;

  mct_manager_get_session_limits_async (fixture->manager,
                                        fixture->missing_uid,
                                        MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                        async_result_cb, &result);

  /* Handle the FindUserById() call and claim the user doesn’t exist. */
  gint64 user_id;
  invocation =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, fixture->missing_uid);

  error_message = g_strdup_printf ("Failed to look up user with uid %u.", fixture->missing_uid);
  g_dbus_method_invocation_return_dbus_error (invocation,
                                              "org.freedesktop.Accounts.Error.Failed",
                                              error_message);

  /* Get the get_session_limits() result. */
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  session_limits = mct_manager_get_session_limits_finish (fixture->manager, result,
                                                  &local_error);

  g_assert_error (local_error,
                  MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_INVALID_USER);
  g_assert_null (session_limits);
}

/* Test that mct_manager_get_session_limits() returns an appropriate error if the
 * mock D-Bus service reports that the properties of the given user can’t be
 * accessed due to permissions.
 *
 * The mock D-Bus replies are generated inline. */
static void
test_session_limits_bus_get_error_permission_denied (BusFixture    *fixture,
                                                     gconstpointer  test_data)
{
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GDBusMethodInvocation) invocation1 = NULL;
  g_autoptr(GDBusMethodInvocation) invocation2 = NULL;
  g_autofree gchar *object_path = NULL;
  g_autoptr(MctSessionLimits) session_limits = NULL;

  mct_manager_get_session_limits_async (fixture->manager,
                                        fixture->valid_uid,
                                        MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                        async_result_cb, &result);

  /* Handle the FindUserById() call. */
  gint64 user_id;
  invocation1 =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, fixture->valid_uid);

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", (uid_t) user_id);
  g_dbus_method_invocation_return_value (invocation1, g_variant_new ("(o)", object_path));

  /* Handle the Properties.GetAll() call and return a permission denied error. */
  const gchar *property_interface;
  invocation2 =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        object_path,
                                        "org.freedesktop.DBus.Properties",
                                        "GetAll", "(&s)", &property_interface);
  g_assert_cmpstr (property_interface, ==, "com.endlessm.ParentalControls.SessionLimits");

  g_dbus_method_invocation_return_dbus_error (invocation2,
                                              "org.freedesktop.Accounts.Error.PermissionDenied",
                                              "Not authorized");

  /* Get the get_session_limits() result. */
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  session_limits = mct_manager_get_session_limits_finish (fixture->manager, result,
                                                          &local_error);

  g_assert_error (local_error,
                  MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_PERMISSION_DENIED);
  g_assert_null (session_limits);
}

/* Test that mct_manager_get_session_limits() returns an appropriate error if
 * the mock D-Bus service replies with no session limits properties (implying
 * that it hasn’t sent the property values because of permissions).
 *
 * The mock D-Bus replies are generated inline. */
static void
test_session_limits_bus_get_error_permission_denied_missing (BusFixture    *fixture,
                                                             gconstpointer  test_data)
{
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GDBusMethodInvocation) invocation1 = NULL;
  g_autoptr(GDBusMethodInvocation) invocation2 = NULL;
  g_autofree gchar *object_path = NULL;
  g_autoptr(MctSessionLimits) session_limits = NULL;

  mct_manager_get_session_limits_async (fixture->manager,
                                        fixture->valid_uid,
                                        MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                        async_result_cb, &result);

  /* Handle the FindUserById() call. */
  gint64 user_id;
  invocation1 =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, fixture->valid_uid);

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", (uid_t) user_id);
  g_dbus_method_invocation_return_value (invocation1, g_variant_new ("(o)", object_path));

  /* Handle the Properties.GetAll() call and return an empty array due to not
   * having permission to access the properties. The code actually keys off the
   * presence of the LimitType property, since that was the first one to be
   * added. */
  const gchar *property_interface;
  invocation2 =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        object_path,
                                        "org.freedesktop.DBus.Properties",
                                        "GetAll", "(&s)", &property_interface);
  g_assert_cmpstr (property_interface, ==, "com.endlessm.ParentalControls.SessionLimits");

  g_dbus_method_invocation_return_value (invocation2, g_variant_new ("(a{sv})", NULL));

  /* Get the get_session_limits() result. */
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  session_limits = mct_manager_get_session_limits_finish (fixture->manager, result,
                                                          &local_error);

  g_assert_error (local_error,
                  MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_PERMISSION_DENIED);
  g_assert_null (session_limits);
}

/* Test that mct_manager_get_session_limits() returns an error if the mock D-Bus
 * service reports an unrecognised error.
 *
 * The mock D-Bus replies are generated inline. */
static void
test_session_limits_bus_get_error_unknown (BusFixture    *fixture,
                                           gconstpointer  test_data)
{
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GDBusMethodInvocation) invocation = NULL;
  g_autoptr(MctSessionLimits) session_limits = NULL;

  mct_manager_get_session_limits_async (fixture->manager,
                                        fixture->valid_uid,
                                        MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                        async_result_cb, &result);

  /* Handle the FindUserById() call and return a bogus error. */
  gint64 user_id;
  invocation =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, fixture->valid_uid);

  g_dbus_method_invocation_return_dbus_error (invocation,
                                              "org.freedesktop.Accounts.Error.NewAndInterestingError",
                                              "This is a fake error message "
                                              "which libmalcontent "
                                              "will never have seen before, "
                                              "but must still handle correctly");

  /* Get the get_session_limits() result. */
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  session_limits = mct_manager_get_session_limits_finish (fixture->manager, result,
                                                          &local_error);

  /* We don’t actually care what error is actually used here. */
  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR);
  g_assert_null (session_limits);
}

/* Test that mct_manager_get_session_limits() returns an error if the mock D-Bus
 * service reports an unknown interface, which means that parental controls are
 * not installed properly.
 *
 * The mock D-Bus replies are generated inline. */
static void
test_session_limits_bus_get_error_disabled (BusFixture    *fixture,
                                            gconstpointer  test_data)
{
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GDBusMethodInvocation) invocation1 = NULL;
  g_autoptr(GDBusMethodInvocation) invocation2 = NULL;
  g_autofree gchar *object_path = NULL;
  g_autoptr(MctSessionLimits) session_limits = NULL;

  mct_manager_get_session_limits_async (fixture->manager,
                                        fixture->valid_uid,
                                        MCT_MANAGER_GET_VALUE_FLAGS_NONE, NULL,
                                        async_result_cb, &result);

  /* Handle the FindUserById() call. */
  gint64 user_id;
  invocation1 =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, fixture->valid_uid);

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", (uid_t) user_id);
  g_dbus_method_invocation_return_value (invocation1, g_variant_new ("(o)", object_path));

  /* Handle the Properties.GetAll() call and return an InvalidArgs error. */
  const gchar *property_interface;
  invocation2 =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        object_path,
                                        "org.freedesktop.DBus.Properties",
                                        "GetAll", "(&s)", &property_interface);
  g_assert_cmpstr (property_interface, ==, "com.endlessm.ParentalControls.SessionLimits");

  g_dbus_method_invocation_return_dbus_error (invocation2,
                                              "org.freedesktop.DBus.Error.InvalidArgs",
                                              "No such interface "
                                              "“com.endlessm.ParentalControls.SessionLimits”");

  /* Get the get_session_limits() result. */
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  session_limits = mct_manager_get_session_limits_finish (fixture->manager, result,
                                                          &local_error);

  g_assert_error (local_error,
                  MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_DISABLED);
  g_assert_null (session_limits);
}

/* Generic mock accountsservice implementation which handles properties being
 * set on a mock User object, and compares their values to the given
 * `expected_*` ones.
 *
 * If @error_index is non-negative, it gives the index of a Set() call to return
 * the given @dbus_error_name and @dbus_error_message from, rather than
 * accepting the property value from the caller. If @error_index is negative,
 * all Set() calls will be accepted. */
typedef struct
{
  uid_t expected_uid;

  const gchar * const *expected_properties;

  /* All GVariants in text format: */
  const gchar *expected_limit_type_value;  /* (nullable) */
  const gchar *expected_daily_schedule_value;  /* (nullable) */

  gint error_index;  /* -1 to return no error */
  const gchar *dbus_error_name;  /* NULL to return no error */
  const gchar *dbus_error_message;  /* NULL to return no error */
} SetSessionLimitsData;

static const gchar *
set_session_limits_data_get_expected_property_value (const SetSessionLimitsData *data,
                                                     const gchar                *property_name)
{
  if (g_str_equal (property_name, "LimitType"))
    return data->expected_limit_type_value;
  else if (g_str_equal (property_name, "DailySchedule"))
    return data->expected_daily_schedule_value;
  else
    g_assert_not_reached ();
}

/* This is run in a worker thread. */
static void
set_session_limits_server_cb (GtDBusQueue *queue,
                              gpointer     user_data)
{
  const SetSessionLimitsData *data = user_data;
  g_autoptr(GDBusMethodInvocation) find_invocation = NULL;
  g_autofree gchar *object_path = NULL;

  g_assert ((data->error_index == -1) == (data->dbus_error_name == NULL));
  g_assert ((data->dbus_error_name == NULL) == (data->dbus_error_message == NULL));

  /* Handle the FindUserById() call. */
  gint64 user_id;
  find_invocation =
      gt_dbus_queue_assert_pop_message (queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, data->expected_uid);

  object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", (uid_t) user_id);
  g_dbus_method_invocation_return_value (find_invocation, g_variant_new ("(o)", object_path));

  /* Handle the Properties.Set() calls. */
  gsize i;

  for (i = 0; data->expected_properties[i] != NULL; i++)
    {
      const gchar *property_interface;
      const gchar *property_name;
      g_autoptr(GVariant) property_value = NULL;
      g_autoptr(GDBusMethodInvocation) property_invocation = NULL;
      g_autoptr(GVariant) expected_property_value = NULL;

      property_invocation =
          gt_dbus_queue_assert_pop_message (queue,
                                            object_path,
                                            "org.freedesktop.DBus.Properties",
                                            "Set", "(&s&sv)", &property_interface,
                                            &property_name, &property_value);
      g_assert_cmpstr (property_interface, ==, "com.endlessm.ParentalControls.SessionLimits");
      g_assert_cmpstr (property_name, ==, data->expected_properties[i]);

      if (data->error_index >= 0 && (gsize) data->error_index == i)
        {
          g_dbus_method_invocation_return_dbus_error (property_invocation,
                                                      data->dbus_error_name,
                                                      data->dbus_error_message);
          break;
        }
      else
        {
          expected_property_value = g_variant_new_parsed (set_session_limits_data_get_expected_property_value (data, property_name));
          g_assert_cmpvariant (property_value, expected_property_value);

          g_dbus_method_invocation_return_value (property_invocation, NULL);
        }
    }
}

/* Test that setting an #MctSessionLimits on the mock D-Bus service works. The
 * @test_data is a boolean value indicating whether to do the call
 * synchronously (%FALSE) or asynchronously (%TRUE).
 *
 * The mock D-Bus replies are generated in set_session_limits_server_cb(), which
 * is used for both synchronous and asynchronous calls. */
static void
test_session_limits_bus_set (BusFixture    *fixture,
                             gconstpointer  test_data)
{
  gboolean success;
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GError) local_error = NULL;
  gboolean test_async = GPOINTER_TO_UINT (test_data);
  const gchar *expected_properties[] =
    {
      "DailySchedule",
      "LimitType",
      NULL
    };
  const SetSessionLimitsData set_session_limits_data =
    {
      .expected_uid = fixture->valid_uid,
      .expected_properties = expected_properties,
      .expected_limit_type_value = "@u 1",
      .expected_daily_schedule_value = "(@u 100, @u 4000)",
      .error_index = -1,
    };

  /* Build a session limits object. */
  mct_session_limits_builder_set_daily_schedule (&builder, 100, 4000);

  session_limits = mct_session_limits_builder_end (&builder);

  /* Set the mock service function and set the limits. */
  gt_dbus_queue_set_server_func (fixture->queue, set_session_limits_server_cb,
                                 (gpointer) &set_session_limits_data);

  if (test_async)
    {
      g_autoptr(GAsyncResult) result = NULL;

      mct_manager_set_session_limits_async (fixture->manager,
                                            fixture->valid_uid, session_limits,
                                            MCT_MANAGER_SET_VALUE_FLAGS_NONE, NULL,
                                            async_result_cb, &result);

      while (result == NULL)
        g_main_context_iteration (NULL, TRUE);
      success = mct_manager_set_session_limits_finish (fixture->manager, result,
                                                       &local_error);
    }
  else
    {
      success = mct_manager_set_session_limits (fixture->manager,
                                                fixture->valid_uid, session_limits,
                                                MCT_MANAGER_SET_VALUE_FLAGS_NONE, NULL,
                                                &local_error);
    }

  g_assert_no_error (local_error);
  g_assert_true (success);
}

/* Test that mct_manager_set_session_limits() returns an appropriate error if
 * the mock D-Bus service reports that the given user cannot be found.
 *
 * The mock D-Bus replies are generated inline. */
static void
test_session_limits_bus_set_error_invalid_user (BusFixture    *fixture,
                                                gconstpointer  test_data)
{
  gboolean success;
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GAsyncResult) result = NULL;
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GDBusMethodInvocation) invocation = NULL;
  g_autofree gchar *error_message = NULL;

  /* Use the default session limits. */
  session_limits = mct_session_limits_builder_end (&builder);

  mct_manager_set_session_limits_async (fixture->manager,
                                        fixture->missing_uid, session_limits,
                                        MCT_MANAGER_SET_VALUE_FLAGS_NONE, NULL,
                                        async_result_cb, &result);

  /* Handle the FindUserById() call and claim the user doesn’t exist. */
  gint64 user_id;
  invocation =
      gt_dbus_queue_assert_pop_message (fixture->queue,
                                        "/org/freedesktop/Accounts",
                                        "org.freedesktop.Accounts",
                                        "FindUserById", "(x)", &user_id);
  g_assert_cmpint (user_id, ==, fixture->missing_uid);

  error_message = g_strdup_printf ("Failed to look up user with uid %u.", fixture->missing_uid);
  g_dbus_method_invocation_return_dbus_error (invocation,
                                              "org.freedesktop.Accounts.Error.Failed",
                                              error_message);

  /* Get the set_session_limits() result. */
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);
  success = mct_manager_set_session_limits_finish (fixture->manager, result,
                                                   &local_error);

  g_assert_error (local_error,
                  MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_INVALID_USER);
  g_assert_false (success);
}

/* Test that mct_manager_set_session_limits() returns an appropriate error if the
 * mock D-Bus service replies with a permission denied error when setting
 * properties.
 *
 * The mock D-Bus replies are generated in set_session_limits_server_cb(). */
static void
test_session_limits_bus_set_error_permission_denied (BusFixture    *fixture,
                                                     gconstpointer  test_data)
{
  gboolean success;
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GError) local_error = NULL;
  const gchar *expected_properties[] =
    {
      "LimitType",
      NULL
    };
  const SetSessionLimitsData set_session_limits_data =
    {
      .expected_uid = fixture->valid_uid,
      .expected_properties = expected_properties,
      .error_index = 0,
      .dbus_error_name = "org.freedesktop.Accounts.Error.PermissionDenied",
      .dbus_error_message = "Not authorized",
    };

  /* Use the default session limits. */
  session_limits = mct_session_limits_builder_end (&builder);

  gt_dbus_queue_set_server_func (fixture->queue, set_session_limits_server_cb,
                                 (gpointer) &set_session_limits_data);

  success = mct_manager_set_session_limits (fixture->manager,
                                            fixture->valid_uid, session_limits,
                                            MCT_MANAGER_SET_VALUE_FLAGS_NONE, NULL,
                                            &local_error);

  g_assert_error (local_error,
                  MCT_MANAGER_ERROR, MCT_MANAGER_ERROR_PERMISSION_DENIED);
  g_assert_false (success);
}

/* Test that mct_manager_set_session_limits() returns an error if the mock D-Bus
 * service reports an unrecognised error.
 *
 * The mock D-Bus replies are generated in set_session_limits_server_cb(). */
static void
test_session_limits_bus_set_error_unknown (BusFixture    *fixture,
                                           gconstpointer  test_data)
{
  gboolean success;
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GError) local_error = NULL;
  const gchar *expected_properties[] =
    {
      "LimitType",
      NULL
    };
  const SetSessionLimitsData set_session_limits_data =
    {
      .expected_uid = fixture->valid_uid,
      .expected_properties = expected_properties,
      .error_index = 0,
      .dbus_error_name = "org.freedesktop.Accounts.Error.NewAndInterestingError",
      .dbus_error_message = "This is a fake error message which "
                            "libmalcontent will never have seen "
                            "before, but must still handle correctly",
    };

  /* Use the default session limits. */
  session_limits = mct_session_limits_builder_end (&builder);

  gt_dbus_queue_set_server_func (fixture->queue, set_session_limits_server_cb,
                                 (gpointer) &set_session_limits_data);

  success = mct_manager_set_session_limits (fixture->manager,
                                            fixture->valid_uid, session_limits,
                                            MCT_MANAGER_SET_VALUE_FLAGS_NONE, NULL,
                                            &local_error);

  g_assert_error (local_error, G_IO_ERROR, G_IO_ERROR_DBUS_ERROR);
  g_assert_false (success);
}

/* Test that mct_manager_set_session_limits() returns an error if the mock D-Bus
 * service reports an InvalidArgs error with a given one of its Set() calls.
 *
 * @test_data contains a property index encoded with GINT_TO_POINTER(),
 * indicating which Set() call to return the error on, since the calls are made
 * in series.
 *
 * The mock D-Bus replies are generated in set_session_limits_server_cb(). */
static void
test_session_limits_bus_set_error_invalid_property (BusFixture    *fixture,
                                                    gconstpointer  test_data)
{
  gboolean success;
  g_auto(MctSessionLimitsBuilder) builder = MCT_SESSION_LIMITS_BUILDER_INIT ();
  g_autoptr(MctSessionLimits) session_limits = NULL;
  g_autoptr(GError) local_error = NULL;
  const gchar *expected_properties[] =
    {
      "DailySchedule",
      "LimitType",
      NULL
    };
  const SetSessionLimitsData set_session_limits_data =
    {
      .expected_uid = fixture->valid_uid,
      .expected_properties = expected_properties,
      .expected_limit_type_value = "@u 1",
      .expected_daily_schedule_value = "(@u 100, @u 3000)",
      .error_index = GPOINTER_TO_INT (test_data),
      .dbus_error_name = "org.freedesktop.DBus.Error.InvalidArgs",
      .dbus_error_message = "Mumble mumble something wrong with the limits value",
    };

  /* Build a session limits object. */
  mct_session_limits_builder_set_daily_schedule (&builder, 100, 3000);

  session_limits = mct_session_limits_builder_end (&builder);

  gt_dbus_queue_set_server_func (fixture->queue, set_session_limits_server_cb,
                                 (gpointer) &set_session_limits_data);

  success = mct_manager_set_session_limits (fixture->manager,
                                            fixture->valid_uid, session_limits,
                                            MCT_MANAGER_SET_VALUE_FLAGS_NONE, NULL,
                                            &local_error);

  g_assert_error (local_error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS);
  g_assert_false (success);
}

int
main (int    argc,
      char **argv)
{
  setlocale (LC_ALL, "");
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/session-limits/types", test_session_limits_types);
  g_test_add_func ("/session-limits/refs", test_session_limits_refs);
  g_test_add_func ("/session-limits/check-time-remaining/invalid-time",
                   test_session_limits_check_time_remaining_invalid_time);

  g_test_add_func ("/session-limits/serialize", test_session_limits_serialize);
  g_test_add_func ("/session-limits/deserialize", test_session_limits_deserialize);
  g_test_add_func ("/session-limits/deserialize/invalid", test_session_limits_deserialize_invalid);

  g_test_add ("/session-limits/builder/stack/non-empty", BuilderFixture, NULL,
              builder_set_up_stack, test_session_limits_builder_non_empty,
              builder_tear_down_stack);
  g_test_add ("/session-limits/builder/stack/empty", BuilderFixture, NULL,
              builder_set_up_stack, test_session_limits_builder_empty,
              builder_tear_down_stack);
  g_test_add ("/session-limits/builder/stack2/non-empty", BuilderFixture, NULL,
              builder_set_up_stack2, test_session_limits_builder_non_empty,
              builder_tear_down_stack2);
  g_test_add ("/session-limits/builder/stack2/empty", BuilderFixture, NULL,
              builder_set_up_stack2, test_session_limits_builder_empty,
              builder_tear_down_stack2);
  g_test_add ("/session-limits/builder/heap/non-empty", BuilderFixture, NULL,
              builder_set_up_heap, test_session_limits_builder_non_empty,
              builder_tear_down_heap);
  g_test_add ("/session-limits/builder/heap/empty", BuilderFixture, NULL,
              builder_set_up_heap, test_session_limits_builder_empty,
              builder_tear_down_heap);
  g_test_add_func ("/session-limits/builder/copy/empty",
                   test_session_limits_builder_copy_empty);
  g_test_add_func ("/session-limits/builder/copy/full",
                   test_session_limits_builder_copy_full);
  g_test_add_func ("/session-limits/builder/override/none",
                   test_session_limits_builder_override_none);
  g_test_add_func ("/session-limits/builder/override/daily-schedule",
                   test_session_limits_builder_override_daily_schedule);

  g_test_add ("/session-limits/bus/get/async", BusFixture, GUINT_TO_POINTER (TRUE),
              bus_set_up, test_session_limits_bus_get, bus_tear_down);
  g_test_add ("/session-limits/bus/get/sync", BusFixture, GUINT_TO_POINTER (FALSE),
              bus_set_up, test_session_limits_bus_get, bus_tear_down);
  g_test_add ("/session-limits/bus/get/none", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_get_none, bus_tear_down);

  g_test_add ("/session-limits/bus/get/error/invalid-user", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_get_error_invalid_user, bus_tear_down);
  g_test_add ("/session-limits/bus/get/error/permission-denied", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_get_error_permission_denied, bus_tear_down);
  g_test_add ("/session-limits/bus/get/error/permission-denied-missing", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_get_error_permission_denied_missing, bus_tear_down);
  g_test_add ("/session-limits/bus/get/error/unknown", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_get_error_unknown, bus_tear_down);
  g_test_add ("/session-limits/bus/get/error/disabled", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_get_error_disabled, bus_tear_down);

  g_test_add ("/session-limits/bus/set/async", BusFixture, GUINT_TO_POINTER (TRUE),
              bus_set_up, test_session_limits_bus_set, bus_tear_down);
  g_test_add ("/session-limits/bus/set/sync", BusFixture, GUINT_TO_POINTER (FALSE),
              bus_set_up, test_session_limits_bus_set, bus_tear_down);

  g_test_add ("/session-limits/bus/set/error/invalid-user", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_set_error_invalid_user, bus_tear_down);
  g_test_add ("/session-limits/bus/set/error/permission-denied", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_set_error_permission_denied, bus_tear_down);
  g_test_add ("/session-limits/bus/set/error/unknown", BusFixture, NULL,
              bus_set_up, test_session_limits_bus_set_error_unknown, bus_tear_down);
  g_test_add ("/session-limits/bus/set/error/invalid-property/daily-schedule",
              BusFixture, GINT_TO_POINTER (0), bus_set_up,
              test_session_limits_bus_set_error_invalid_property, bus_tear_down);
  g_test_add ("/session-limits/bus/set/error/invalid-property/limit-type",
              BusFixture, GINT_TO_POINTER (1), bus_set_up,
              test_session_limits_bus_set_error_invalid_property, bus_tear_down);

  return g_test_run ();
}
