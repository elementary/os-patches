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

#include "accounts-service-mock.h"

extern "C" {
#include "indicator-sound-service.h"
}

class MediaPlayerUserTest : public ::testing::Test
{

	protected:
		DbusTestService * service = NULL;
		AccountsServiceMock service_mock;

		GDBusConnection * session = NULL;
		GDBusConnection * system = NULL;
		GDBusProxy * proxy = NULL;

		virtual void SetUp() {
			service = dbus_test_service_new(NULL);


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

		void set_property (const gchar * name, GVariant * value) {
			dbus_test_dbus_mock_object_update_property((DbusTestDbusMock *)service_mock, service_mock.get_sound(), name, value, NULL);
		}
};

TEST_F(MediaPlayerUserTest, BasicObject) {
	MediaPlayerUser * player = media_player_user_new("user");
	ASSERT_NE(nullptr, player);

	/* Protected, but no useful data */
	EXPECT_FALSE(media_player_get_is_running(MEDIA_PLAYER(player)));
	EXPECT_TRUE(media_player_get_can_raise(MEDIA_PLAYER(player)));
	EXPECT_STREQ("user", media_player_get_id(MEDIA_PLAYER(player)));
	EXPECT_STREQ("", media_player_get_name(MEDIA_PLAYER(player)));
	EXPECT_STREQ("", media_player_get_state(MEDIA_PLAYER(player)));
	EXPECT_EQ(nullptr, media_player_get_icon(MEDIA_PLAYER(player)));
	EXPECT_EQ(nullptr, media_player_get_current_track(MEDIA_PLAYER(player)));

	/* Get the proxy -- but no good data */
	loop(100);

	/* Ensure even with the proxy we don't have anything */
	EXPECT_FALSE(media_player_get_is_running(MEDIA_PLAYER(player)));
	EXPECT_TRUE(media_player_get_can_raise(MEDIA_PLAYER(player)));
	EXPECT_STREQ("user", media_player_get_id(MEDIA_PLAYER(player)));
	EXPECT_STREQ("", media_player_get_name(MEDIA_PLAYER(player)));
	EXPECT_STREQ("", media_player_get_state(MEDIA_PLAYER(player)));
	EXPECT_EQ(nullptr, media_player_get_icon(MEDIA_PLAYER(player)));
	EXPECT_EQ(nullptr, media_player_get_current_track(MEDIA_PLAYER(player)));

	g_clear_object(&player);
}

TEST_F(MediaPlayerUserTest, DataSet) {
	/* Put data into Acts */
	set_property("Timestamp", g_variant_new_uint64(g_get_monotonic_time()));
	set_property("PlayerName", g_variant_new_string("The Player Formerly Known as Prince"));
	GIcon * in_icon = g_themed_icon_new_with_default_fallbacks("foo-bar-fallback");
	set_property("PlayerIcon", g_variant_new_variant(g_icon_serialize(in_icon)));
	set_property("State", g_variant_new_string("Chillin'"));
	set_property("Title", g_variant_new_string("Dictator"));
	set_property("Artist", g_variant_new_string("Bansky"));
	set_property("Album", g_variant_new_string("Vinyl is dead"));
	set_property("ArtUrl", g_variant_new_string("http://art.url"));

	/* Build our media player */
	MediaPlayerUser * player = media_player_user_new("user");
	ASSERT_NE(nullptr, player);

	/* Get the proxy -- and it's precious precious data -- oh, my, precious! */
	loop(100);

	/* Ensure even with the proxy we don't have anything */
	EXPECT_TRUE(media_player_get_is_running(MEDIA_PLAYER(player)));
	EXPECT_TRUE(media_player_get_can_raise(MEDIA_PLAYER(player)));
	EXPECT_STREQ("user", media_player_get_id(MEDIA_PLAYER(player)));
	EXPECT_STREQ("The Player Formerly Known as Prince", media_player_get_name(MEDIA_PLAYER(player)));
	EXPECT_STREQ("Chillin'", media_player_get_state(MEDIA_PLAYER(player)));

	GIcon * out_icon = media_player_get_icon(MEDIA_PLAYER(player));
	EXPECT_NE(nullptr, out_icon);
	EXPECT_TRUE(g_icon_equal(in_icon, out_icon));
	// NOTE: No reference in 'out_icon' returned

	MediaPlayerTrack * track = media_player_get_current_track(MEDIA_PLAYER(player));
	EXPECT_NE(nullptr, track);
	EXPECT_STREQ("Dictator", media_player_track_get_title(track));
	EXPECT_STREQ("Bansky", media_player_track_get_artist(track));
	EXPECT_STREQ("Vinyl is dead", media_player_track_get_album(track));
	EXPECT_STREQ("http://art.url", media_player_track_get_art_url(track));
	// NOTE: No reference in 'track' returned

	g_clear_object(&in_icon);
	g_clear_object(&player);
}

TEST_F(MediaPlayerUserTest, TimeoutTest) {
	/* Put data into Acts -- but 15 minutes ago */
	set_property("Timestamp", g_variant_new_uint64(g_get_monotonic_time() - 15 * 60 * 1000 * 1000));
	set_property("PlayerName", g_variant_new_string("The Player Formerly Known as Prince"));
	GIcon * in_icon = g_themed_icon_new_with_default_fallbacks("foo-bar-fallback");
	set_property("PlayerIcon", g_variant_new_variant(g_icon_serialize(in_icon)));
	set_property("State", g_variant_new_string("Chillin'"));
	set_property("Title", g_variant_new_string("Dictator"));
	set_property("Artist", g_variant_new_string("Bansky"));
	set_property("Album", g_variant_new_string("Vinyl is dead"));
	set_property("ArtUrl", g_variant_new_string("http://art.url"));

	/* Build our media player */
	MediaPlayerUser * player = media_player_user_new("user");
	ASSERT_NE(nullptr, player);

	/* Get the proxy -- and the old data, so old, like forever */
	loop(100);

	/* Ensure that we show up as not running */
	EXPECT_FALSE(media_player_get_is_running(MEDIA_PLAYER(player)));

	/* Update to make running */
	set_property("Timestamp", g_variant_new_uint64(g_get_monotonic_time()));
	loop(100);

	EXPECT_TRUE(media_player_get_is_running(MEDIA_PLAYER(player)));

	g_clear_object(&in_icon);
	g_clear_object(&player);
}
