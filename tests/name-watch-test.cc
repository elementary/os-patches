/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Ted Gould <ted@canonical.com>
 */

#include <gio/gio.h>
#include <gtest/gtest.h>

extern "C" {
#include "bus-watch-namespace.h"
}

class NameWatchTest : public ::testing::Test
{
	private:
		GTestDBus * testbus = NULL;

	protected:
		virtual void SetUp() {
			testbus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(testbus);
		}

		virtual void TearDown() {
			g_test_dbus_down(testbus);
			g_clear_object(&testbus);
		}

		static gboolean timeout_cb (gpointer user_data) {
			GMainLoop * loop = static_cast<GMainLoop *>(user_data);
			g_main_loop_quit(loop);
			return G_SOURCE_REMOVE;
		}

		void loop (unsigned int ms) {
			GMainLoop * loop = g_main_loop_new(NULL, FALSE);
			g_timeout_add(ms, timeout_cb, loop);
			g_main_loop_run(loop);
			g_main_loop_unref(loop);
		}
};

typedef struct {
	guint appeared;
	guint vanished;
} callback_count_t;

static void
appeared_simple_cb (GDBusConnection * bus, const gchar * name, const gchar * owner, gpointer user_data)
{
	callback_count_t * callback_count = static_cast<callback_count_t *>(user_data);
	callback_count->appeared++;
}

static void
vanished_simple_cb (GDBusConnection * bus, const gchar * name, gpointer user_data)
{
	callback_count_t * callback_count = static_cast<callback_count_t *>(user_data);
	callback_count->vanished++;
}


TEST_F(NameWatchTest, BaseWatch)
{
	callback_count_t callback_count = {0};

	guint ns_watch = bus_watch_namespace(G_BUS_TYPE_SESSION,
	                                     "com.foo",
	                                     appeared_simple_cb,
	                                     vanished_simple_cb,
	                                     &callback_count,
	                                     NULL);

	guint name1 = g_bus_own_name(G_BUS_TYPE_SESSION,
	                             "com.foo.bar",
	                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                             NULL, NULL, NULL, NULL, NULL);
	guint name2 = g_bus_own_name(G_BUS_TYPE_SESSION,
	                             "com.foo.bar_too",
	                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                             NULL, NULL, NULL, NULL, NULL);

	loop(100);

	ASSERT_EQ(callback_count.appeared, 2);

	g_bus_unown_name(name1);
	g_bus_unown_name(name2);

	loop(100);

	ASSERT_EQ(callback_count.vanished, 2);

	bus_unwatch_namespace(ns_watch);
}

TEST_F(NameWatchTest, NonMatches)
{
	callback_count_t callback_count = {0};

	guint ns_watch = bus_watch_namespace(G_BUS_TYPE_SESSION,
	                                     "com.foo",
	                                     appeared_simple_cb,
	                                     vanished_simple_cb,
	                                     &callback_count,
	                                     NULL);

	guint name1 = g_bus_own_name(G_BUS_TYPE_SESSION,
	                             "com.foobar.bar",
	                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                             NULL, NULL, NULL, NULL, NULL);
	guint name2 = g_bus_own_name(G_BUS_TYPE_SESSION,
	                             "com.bar.com.foo",
	                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                             NULL, NULL, NULL, NULL, NULL);

	loop(100);

	ASSERT_EQ(callback_count.appeared, 0);

	g_bus_unown_name(name1);
	g_bus_unown_name(name2);

	loop(100);

	ASSERT_EQ(callback_count.vanished, 0);

	bus_unwatch_namespace(ns_watch);
}

TEST_F(NameWatchTest, StartupNames)
{
	guint name1 = g_bus_own_name(G_BUS_TYPE_SESSION,
	                             "com.foo.bar",
	                             G_BUS_NAME_OWNER_FLAGS_NONE,
	                             NULL, NULL, NULL, NULL, NULL);

	loop(100);

	callback_count_t callback_count = {0};

	guint ns_watch = bus_watch_namespace(G_BUS_TYPE_SESSION,
	                                     "com.foo",
	                                     appeared_simple_cb,
	                                     vanished_simple_cb,
	                                     &callback_count,
	                                     NULL);

	loop(100);

	ASSERT_EQ(callback_count.appeared, 1);

	g_bus_unown_name(name1);

	loop(100);

	ASSERT_EQ(callback_count.vanished, 1);

	bus_unwatch_namespace(ns_watch);
}

