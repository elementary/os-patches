/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gtest-mock-dbus-fixture.h"

#include "backend.h"
#include "backend-dbus/backend-dbus.h"

/***
****
***/

class Guest: public GTestMockDBusFixture
{
  private:

    typedef GTestMockDBusFixture super;

  protected:

    GCancellable * cancellable;
    IndicatorSessionGuest * guest;

    virtual void SetUp ()
    {
      super :: SetUp ();

      // get the guest-dbus
      cancellable = g_cancellable_new ();
      guest = 0;
      backend_get (cancellable, NULL, NULL, &guest);
      wait_msec (100);

      // test the default state
      ASSERT_TRUE (guest != 0);
      ASSERT_FALSE (indicator_session_guest_is_allowed (guest));
      ASSERT_FALSE (indicator_session_guest_is_logged_in (guest));
      ASSERT_FALSE (indicator_session_guest_is_active (guest));
    }

    virtual void TearDown ()
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&cancellable);
      g_clear_object (&guest); 

      super :: TearDown ();
    }

  protected:

    void add_mock_guest (MockUser               *& guest_user,
                         int                     & guest_session_tag)
    {
      guest_user = new MockUser (loop, conn, "guest-jjbEVV", "Guest", 10, 100);
      accounts->add_user (guest_user);
      guest_user->set_system_account (true);
      guest_session_tag = login1_manager->add_session (login1_seat, guest_user);
    }
};

/**
 * Confirms that the Fixture's SetUp() and TearDown() work
 */
TEST_F (Guest, HelloWorld)
{
  ASSERT_TRUE (true);
}

/**
 * Toggle in the DM whether or not guests are allowed.
 * Confirm that "guest" reflects the changes.
 */
TEST_F (Guest, Allowed)
{
  dm_seat->set_guest_allowed (true);
  wait_for_signal (guest, "notify::guest-is-allowed");
  ASSERT_TRUE (indicator_session_guest_is_allowed (guest));
  ASSERT_FALSE (indicator_session_guest_is_logged_in (guest));
  ASSERT_FALSE (indicator_session_guest_is_active (guest));

  dm_seat->set_guest_allowed (false);
  wait_for_signal (guest, "notify::guest-is-allowed");
  ASSERT_FALSE (indicator_session_guest_is_allowed (guest));
  ASSERT_FALSE (indicator_session_guest_is_logged_in (guest));
  ASSERT_FALSE (indicator_session_guest_is_active (guest));
}

/**
 * Have a guest user log in & out.
 * Confirm that "guest" reflects the changes.
 */
TEST_F (Guest, Login)
{
  gboolean b;

  dm_seat->set_guest_allowed (true);

  // Log a Guest in
  // And confirm that guest's is_login changes to true
  MockUser * guest_user;
  int session_tag;
  add_mock_guest (guest_user, session_tag);
  wait_for_signal (guest, "notify::guest-is-logged-in");
  ASSERT_TRUE (indicator_session_guest_is_allowed (guest));
  ASSERT_TRUE (indicator_session_guest_is_logged_in (guest));
  g_object_get (guest, INDICATOR_SESSION_GUEST_PROPERTY_LOGGED_IN, &b,NULL);
  ASSERT_TRUE (b);
  ASSERT_FALSE (indicator_session_guest_is_active (guest));

  // Log the Guest User out
  // and confirm that guest's is_login changes to false
  login1_manager->remove_session (login1_seat, session_tag);
  accounts->remove_user (guest_user);
  delete guest_user;
  wait_for_signal (guest, "notify::guest-is-logged-in");
  ASSERT_TRUE (indicator_session_guest_is_allowed (guest));
  ASSERT_FALSE (indicator_session_guest_is_logged_in (guest));
  g_object_get (guest, INDICATOR_SESSION_GUEST_PROPERTY_LOGGED_IN, &b,NULL);
  ASSERT_FALSE (b);
  ASSERT_FALSE (indicator_session_guest_is_active (guest));
}

/**
 * Activate a Guest session, then activate a different session.
 * Confirm that "guest" reflects the changes.
 */
TEST_F (Guest, Active)
{
  gboolean b;
  const int user_session_tag = login1_seat->active_session();

  dm_seat->set_guest_allowed (true);
  MockUser * guest_user;
  int guest_session_tag;
  add_mock_guest (guest_user, guest_session_tag);

  // Activate the guest session
  // and confirm that guest's is_active changes to true
  login1_seat->activate_session (guest_session_tag);
  wait_for_signal (guest, "notify::guest-is-active-session");
  ASSERT_TRUE (indicator_session_guest_is_allowed (guest));
  ASSERT_TRUE (indicator_session_guest_is_logged_in (guest));
  ASSERT_TRUE (indicator_session_guest_is_active (guest));
  g_object_get (guest, INDICATOR_SESSION_GUEST_PROPERTY_ACTIVE, &b,NULL);
  ASSERT_TRUE (b);

  // Activate a non-guest session
  // and confirm that guest's is_active changes to false
  login1_seat->activate_session (user_session_tag);
  wait_for_signal (guest, "notify::guest-is-active-session");
  ASSERT_TRUE (indicator_session_guest_is_allowed (guest));
  ASSERT_TRUE (indicator_session_guest_is_logged_in (guest));
  ASSERT_FALSE (indicator_session_guest_is_active (guest));
  g_object_get (guest, INDICATOR_SESSION_GUEST_PROPERTY_ACTIVE, &b,NULL);
  ASSERT_FALSE (b);
}

/**
 * Activate a guest session using the "guest" API.
 * Confirm that the guest session gets activated on the bus.
 */
TEST_F (Guest, Activate)
{
  dm_seat->set_guest_allowed (true);
  wait_for_signal (guest, "notify::guest-is-allowed");

  MockUser * guest_user;
  int guest_session_tag;
  add_mock_guest (guest_user, guest_session_tag);

  indicator_session_guest_switch_to_guest (guest);

  wait_for_signal (login1_seat->skeleton(), "notify::active-session");
  ASSERT_EQ (guest_session_tag, login1_seat->active_session());
  wait_msec (50);
}
