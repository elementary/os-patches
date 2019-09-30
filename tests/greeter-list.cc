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

extern "C" {
#include "indicator-sound-service.h"
#include "vala-mocks.h"
}

class GreeterListTest : public ::testing::Test
{
	protected:
		GTestDBus * bus = nullptr;

		virtual void SetUp() {
			bus = g_test_dbus_new(G_TEST_DBUS_NONE);
			g_test_dbus_up(bus);
		}

		virtual void TearDown() {
			g_test_dbus_down(bus);
			g_clear_object(&bus);
		}

};

TEST_F(GreeterListTest, BasicObject) {
	MediaPlayerListGreeter * list = media_player_list_greeter_new();

	ASSERT_NE(nullptr, list);

	g_clear_object(&list);
	return;
}

TEST_F(GreeterListTest, BasicIterator) {
	MediaPlayerListGreeter * list = media_player_list_greeter_new();
	ASSERT_NE(nullptr, list);

	MediaPlayerListGreeterIterator * iter = media_player_list_greeter_iterator_new(list);
	ASSERT_NE(nullptr, iter);

	MediaPlayer * player = media_player_list_iterator_next_value (MEDIA_PLAYER_LIST_ITERATOR(iter));
	ASSERT_EQ(nullptr, player);

	g_clear_object(&iter);
	g_clear_object(&list);
	return;
}

