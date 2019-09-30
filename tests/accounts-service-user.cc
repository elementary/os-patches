/*
 * Copyright © 2014 Canonical Ltd.
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

#include <gtest/gtest.h>
#include <gio/gio.h>
#include <libdbustest/dbus-test.h>
#include <act/act.h>

#include "accounts-service-mock.h"

extern "C" {
#include "indicator-sound-service.h"
#include "vala-mocks.h"
}

class AccountsServiceUserTest : public ::testing::Test
{

	protected:
		DbusTestService * service = NULL;
		DbusTestDbusMock * mock = NULL;

		GDBusConnection * session = NULL;
		GDBusConnection * system = NULL;
		GDBusProxy * proxy = NULL;

		virtual void SetUp() {
			service = dbus_test_service_new(NULL);

			AccountsServiceMock service_mock;

			dbus_test_service_add_task(service, (DbusTestTask*)service_mock);
			dbus_test_service_start_tasks(service);

			g_setenv("DBUS_SYSTEM_BUS_ADDRESS", g_getenv("DBUS_SESSION_BUS_ADDRESS"), TRUE);

			session = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
			ASSERT_NE(nullptr, session);
			g_dbus_connection_set_exit_on_close(session, FALSE);
			g_object_add_weak_pointer(G_OBJECT(session), (gpointer *)&session);

			system = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
			ASSERT_NE(nullptr, system);
			g_dbus_connection_set_exit_on_close(system, FALSE);
			g_object_add_weak_pointer(G_OBJECT(system), (gpointer *)&system);

			proxy = g_dbus_proxy_new_sync(session,
				G_DBUS_PROXY_FLAGS_NONE,
				NULL,
				"org.freedesktop.Accounts",
				"/user",
				"org.freedesktop.DBus.Properties",
				NULL, NULL);
			ASSERT_NE(nullptr, proxy);
		}

		virtual void TearDown() {
			g_clear_object(&proxy);
			g_clear_object(&mock);
			g_clear_object(&service);

			g_object_unref(session);
			g_object_unref(system);

			#if 0
			/* Accounts Service keeps a bunch of references around so we
			   have to split the tests and can't check this :-( */
			unsigned int cleartry = 0;
			while ((session != NULL || system != NULL) && cleartry < 100) {
				loop(100);
				cleartry++;
			}

			ASSERT_EQ(nullptr, session);
			ASSERT_EQ(nullptr, system);
			#endif
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

		static int unref_idle (gpointer user_data) {
			g_variant_unref(static_cast<GVariant *>(user_data));
			return G_SOURCE_REMOVE;
		}

		const gchar * get_property_string (const gchar * name) {
			GVariant * propval = g_dbus_proxy_call_sync(proxy,
				"Get",
				g_variant_new("(ss)", "com.canonical.indicator.sound.AccountsService", name),
				G_DBUS_CALL_FLAGS_NONE,
				-1, NULL, NULL
				);

			if (propval == nullptr) {
				return nullptr;
			}

			/* NOTE: This is a bit of a hack, basically if main gets
			   called the returned string becomes invalid.  But it
			   makes the test much easier to read :-/ */
			g_idle_add(unref_idle, propval);

			const gchar * ret = NULL;
			GVariant * child = g_variant_get_child_value(propval, 0);
			GVariant * vstr = g_variant_get_variant(child);
			ret = g_variant_get_string(vstr, NULL);
			g_variant_unref(vstr);
			g_variant_unref(child);

			return ret;
		}
};

TEST_F(AccountsServiceUserTest, BasicObject) {
	AccountsServiceUser * srv = accounts_service_user_new();
	loop(50);
	g_object_unref(srv);
}

TEST_F(AccountsServiceUserTest, SetMediaPlayer) {
	MediaPlayerTrack * track = media_player_track_new("Artist", "Title", "Album", "http://art.url");

	MediaPlayerMock * media = MEDIA_PLAYER_MOCK(
		g_object_new(TYPE_MEDIA_PLAYER_MOCK,
			"mock-id", "player-id",
			"mock-name", "Test Player",
			"mock-state", "Playing",
			"mock-is-running", TRUE,
			"mock-can-raise", FALSE,
			"mock-current-track", track,
			NULL)
	);
	g_clear_object(&track);

	AccountsServiceUser * srv = accounts_service_user_new();

	accounts_service_user_set_player(srv, MEDIA_PLAYER(media));

	loop(500);

	/* Verify the values are on the other side of the bus */
	EXPECT_STREQ("Test Player", get_property_string("PlayerName"));
	EXPECT_STREQ("Playing", get_property_string("State"));
	EXPECT_STREQ("Title", get_property_string("Title"));
	EXPECT_STREQ("Artist", get_property_string("Artist"));
	EXPECT_STREQ("Album", get_property_string("Album"));
	EXPECT_STREQ("http://art.url", get_property_string("ArtUrl"));

	/* Check changing the track info */
	track = media_player_track_new("Artist-ish", "Title-like", "Psuedo Album", "http://fake.art.url");
	media_player_mock_set_mock_current_track(media, track);
	g_clear_object(&track);
	accounts_service_user_set_player(srv, MEDIA_PLAYER(media));

	loop(500);

	EXPECT_STREQ("Test Player", get_property_string("PlayerName"));
	EXPECT_STREQ("Playing", get_property_string("State"));
	EXPECT_STREQ("Title-like", get_property_string("Title"));
	EXPECT_STREQ("Artist-ish", get_property_string("Artist"));
	EXPECT_STREQ("Psuedo Album", get_property_string("Album"));
	EXPECT_STREQ("http://fake.art.url", get_property_string("ArtUrl"));

	/* Check to ensure the state can be updated */
	media_player_set_state(MEDIA_PLAYER(media), "Paused");
	accounts_service_user_set_player(srv, MEDIA_PLAYER(media));

	loop(500);

	EXPECT_STREQ("Paused", get_property_string("State"));

	g_object_unref(media);
	g_object_unref(srv);
}
