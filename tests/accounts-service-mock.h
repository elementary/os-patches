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

#include <libdbustest/dbus-test.h>

class AccountsServiceMock
{
		DbusTestDbusMock * mock = nullptr;
		DbusTestDbusMockObject * soundobj = nullptr;
		DbusTestDbusMockObject * userobj = nullptr;

	public:
		AccountsServiceMock () {
			mock = dbus_test_dbus_mock_new("org.freedesktop.Accounts");

			DbusTestDbusMockObject * baseobj = dbus_test_dbus_mock_get_object(mock, "/org/freedesktop/Accounts", "org.freedesktop.Accounts", NULL);

			dbus_test_dbus_mock_object_add_method(mock, baseobj,
				"CacheUser", G_VARIANT_TYPE_STRING, G_VARIANT_TYPE_OBJECT_PATH,
				"ret = dbus.ObjectPath('/user')\n", NULL);
			dbus_test_dbus_mock_object_add_method(mock, baseobj,
				"FindUserById", G_VARIANT_TYPE_INT64, G_VARIANT_TYPE_OBJECT_PATH,
				"ret = dbus.ObjectPath('/user')\n", NULL);
			dbus_test_dbus_mock_object_add_method(mock, baseobj,
				"FindUserByName", G_VARIANT_TYPE_STRING, G_VARIANT_TYPE_OBJECT_PATH,
				"ret = dbus.ObjectPath('/user')\n", NULL);
			dbus_test_dbus_mock_object_add_method(mock, baseobj,
				"ListCachedUsers", NULL, G_VARIANT_TYPE_OBJECT_PATH_ARRAY,
				"ret = [ dbus.ObjectPath('/user') ]\n", NULL);
			dbus_test_dbus_mock_object_add_method(mock, baseobj,
				"UncacheUser", G_VARIANT_TYPE_STRING, NULL,
				"", NULL);

			userobj = dbus_test_dbus_mock_get_object(mock, "/user", "org.freedesktop.Accounts.User", NULL);
			dbus_test_dbus_mock_object_add_property(mock, userobj,
				"UserName", G_VARIANT_TYPE_STRING,
				g_variant_new_string(g_get_user_name()), NULL);

			soundobj = dbus_test_dbus_mock_get_object(mock, "/user", "com.canonical.indicator.sound.AccountsService", NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"Timestamp", G_VARIANT_TYPE_UINT64,
				g_variant_new_uint64(0), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"PlayerName", G_VARIANT_TYPE_STRING,
				g_variant_new_string(""), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"PlayerIcon", G_VARIANT_TYPE_VARIANT,
				g_variant_new_variant(g_variant_new_string("")), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"Running", G_VARIANT_TYPE_BOOLEAN,
				g_variant_new_boolean(FALSE), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"State", G_VARIANT_TYPE_STRING,
				g_variant_new_string(""), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"Title", G_VARIANT_TYPE_STRING,
				g_variant_new_string(""), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"Artist", G_VARIANT_TYPE_STRING,
				g_variant_new_string(""), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"Album", G_VARIANT_TYPE_STRING,
				g_variant_new_string(""), NULL);
			dbus_test_dbus_mock_object_add_property(mock, soundobj,
				"ArtUrl", G_VARIANT_TYPE_STRING,
				g_variant_new_string(""), NULL);
		}

		~AccountsServiceMock () {
			g_debug("Destroying the Accounts Service Mock");
			g_clear_object(&mock);
		}

		operator DbusTestTask* () {
			return DBUS_TEST_TASK(mock);
		}

		operator DbusTestDbusMock* () {
			return mock;
		}

		DbusTestDbusMockObject * get_sound () {
			return soundobj;
		}
};
