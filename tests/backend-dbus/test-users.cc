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

class Users: public GTestMockDBusFixture
{
  private:

    typedef GTestMockDBusFixture super;

  protected:

    GCancellable * cancellable;
    IndicatorSessionUsers * users;

    virtual void SetUp ()
    {
      super :: SetUp ();

      init_event_keys (0);

      // init 'users'
      cancellable = g_cancellable_new ();
      users = 0;
      backend_get (cancellable, NULL, &users, NULL);
      g_assert (users != 0);

      // wait for the users added by GTestMockDBusFixture::SetUp() to show up
      wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_ADDED, 12);
      init_event_keys (0);
    }

    virtual void TearDown ()
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&cancellable);
      g_clear_object (&users);

      super :: TearDown ();
    }

  protected:

    void compare_user (const MockUser * mu, const IndicatorSessionUser * isu, const std::string& user_state)
    {
      ASSERT_EQ (user_state, login1_seat->user_state (mu->uid()));
      ASSERT_EQ (mu->uid(), isu->uid);
      ASSERT_EQ (mu->login_frequency(), isu->login_frequency);
      ASSERT_STREQ (mu->username(), isu->user_name);
      ASSERT_STREQ (mu->realname(), isu->real_name);
      ASSERT_EQ (mu->uid(), isu->uid);
      ASSERT_EQ (user_state!="offline", isu->is_logged_in);
      ASSERT_EQ (user_state=="active", isu->is_current_user);
    }

    void compare_user (const MockUser * mu, guint uid, const std::string& user_state)
    {
      IndicatorSessionUser * isu;
      isu = indicator_session_users_get_user (users, uid);
      compare_user (mu, isu, user_state);
      indicator_session_user_free (isu);
    }

    void compare_user (guint uid, const std::string& user_state)
    {
      IndicatorSessionUser * isu = indicator_session_users_get_user (users, uid);
      MockUser * mu = accounts->find_by_uid (uid);
      compare_user (mu, isu, user_state);
      indicator_session_user_free (isu);
    }

  private:

    void init_event_keys (size_t n)
    {
      expected_event_count = n;
      event_keys.clear();
    }

    static gboolean
    wait_for_signals__timeout (gpointer name)
    {
      g_error ("%s: timed out waiting for signal '%s'", G_STRLOC, (char*)name);
      return G_SOURCE_REMOVE;
    }

    static void
    wait_for_signals__event (IndicatorSessionUser * u G_GNUC_UNUSED,
                             guint                  uid,
                             gpointer               gself)
    {
      Users * self = static_cast<Users*>(gself);

      self->event_keys.push_back (uid);

      if (self->event_keys.size() == self->expected_event_count)
        g_main_loop_quit (self->loop);
    }

  protected:

    std::vector<guint> event_keys;
    size_t expected_event_count;

    void wait_for_signals (gpointer o, const gchar * name, size_t n)
    {
      const int timeout_seconds = 5; // arbitrary

      init_event_keys (n);

      guint handler_id = g_signal_connect (o, name,
                                           G_CALLBACK(wait_for_signals__event),
                                           this);
      gulong timeout_id = g_timeout_add_seconds (timeout_seconds,
                                                 wait_for_signals__timeout,
                                                 (gpointer)name);
      g_main_loop_run (loop);
      g_source_remove (timeout_id);
      g_signal_handler_disconnect (o, handler_id);
    }
};

/***
****
***/

/**
 * Confirm that the fixture's SetUp() and TearDown() work
 */
TEST_F (Users, HelloWorld)
{
  ASSERT_TRUE (true);
}

/**
 * Confirm that 'users' can get the cached users from our Mock Accounts
 */
TEST_F (Users, InitialUsers)
{
  GList * l;
  GList * uids = indicator_session_users_get_uids (users);

  ASSERT_EQ (12, g_list_length (uids));

  for (l=uids; l!=NULL; l=l->next)
    {
      const guint uid = GPOINTER_TO_UINT (l->data);
      compare_user (uid, login1_seat->user_state (uid));
    }

  g_list_free (uids);
}

/**
 * Confirm that 'users' can tell when a new user is added
 */
TEST_F (Users, UserAdded)
{
  MockUser * mu;

  mu = new MockUser (loop, conn, "pcushing", "Peter Cushing", 2);
  accounts->add_user (mu);
  ASSERT_EQ (0, event_keys.size());
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_ADDED, 1);
  ASSERT_EQ (1, event_keys.size());
  compare_user (mu, event_keys[0], "offline");
}

/**
 * Confirm that 'users' can tell when a user is removed
 */
TEST_F (Users, UserRemoved)
{
  MockUser * mu = accounts->find_by_username ("pdavison");

  /* confirm that users knows about pdavison */
  IndicatorSessionUser * isu = indicator_session_users_get_user (users, mu->uid());
  ASSERT_TRUE (isu != NULL);
  compare_user (mu, isu, "offline");
  g_clear_pointer (&isu, indicator_session_user_free);

  /* on the bus, remove pdavison. */
  accounts->remove_user (mu);

  /* now, users should emit a 'user removed' signal... */
  ASSERT_EQ (0, event_keys.size());
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_REMOVED, 1);
  ASSERT_EQ (1, event_keys.size());

  /* confirm that users won't give us pdavison's info */
  isu = indicator_session_users_get_user (users, mu->uid());
  ASSERT_TRUE (isu == NULL);

  /* confirm that users won't give us pdavison's uid */
  GList * uids = indicator_session_users_get_uids (users);
  ASSERT_TRUE (g_list_find (uids, GUINT_TO_POINTER(mu->uid())) == NULL);
  g_list_free (uids);

  delete mu;
}

/**
 * Confirm that 'users' notices when a user's real name changes
 */
TEST_F (Users, RealnameChanged)
{
  MockUser * mu;

  mu = accounts->find_by_username ("pdavison");
  const char * const realname = "Peter M. G. Moffett";
  mu->set_realname (realname);
  ASSERT_NE (mu->realname(), realname);
  ASSERT_STREQ (mu->realname(), realname);
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 1);
  ASSERT_EQ (1, event_keys.size());
  compare_user (mu, event_keys[0], "offline");
}

/**
 * Confirm that 'users' notices when users log in and out
 */
TEST_F (Users, LogInLogOut)
{
  // The fist doctor logs in.
  // Confirm that 'users' notices.
  MockUser * mu = accounts->find_by_username ("whartnell");
  ASSERT_EQ (login1_seat->user_state (mu->uid()), "offline");
  const int session_tag = login1_manager->add_session (login1_seat, mu);
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 1);
  ASSERT_EQ (1, event_keys.size());
  compare_user (mu, event_keys[0], "online");

  // The first doctor logs out.
  // Confirm that 'users' notices.
  login1_manager->remove_session (login1_seat, session_tag);
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 1);
  ASSERT_EQ (1, event_keys.size());
  compare_user (mu, event_keys[0], "offline");
}

/**
 * Confirm that 'users' notices when the active session changes
 */
TEST_F (Users, ActivateSession)
{
  // confirm preconditions: msmith is active, msmith is offline
  MockUser * const whartnell = accounts->find_by_username ("whartnell");
  ASSERT_EQ (login1_seat->user_state (whartnell->uid()), "offline");
  MockUser * const msmith = accounts->find_by_username ("msmith");
  ASSERT_EQ (login1_seat->user_state (msmith->uid()), "active");

  // whartnell logs in... confirm that 'users' notices
  login1_manager->add_session (login1_seat, whartnell);
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 1);
  ASSERT_EQ (1, event_keys.size());
  compare_user (whartnell, event_keys[0], "online");

  // activate whartnell's session... confirm that 'users' sees:
  //  1. msmith changes from 'active' to 'online'
  //  2. whartnell changes from 'online' to 'active'
  login1_seat->switch_to_user (whartnell->username());
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 2);
  ASSERT_EQ (2, event_keys.size());
  compare_user (msmith, event_keys[0], "online");
  compare_user (whartnell, event_keys[1], "active");

  // reverse the test
  login1_seat->switch_to_user (msmith->username());
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 2);
  ASSERT_EQ (2, event_keys.size());
  compare_user (whartnell, event_keys[0], "online");
  compare_user (msmith, event_keys[1], "active");
}

/**
 * Confirm that we can change the active session via users' API.
 * This is nearly the same as ActivateSession but uses users' API
 */
TEST_F (Users, ActivateUser)
{
  // confirm preconditions: msmith is active, msmith is offline
  MockUser * const whartnell = accounts->find_by_username ("whartnell");
  ASSERT_EQ (login1_seat->user_state (whartnell->uid()), "offline");
  MockUser * const msmith = accounts->find_by_username ("msmith");
  ASSERT_EQ (login1_seat->user_state (msmith->uid()), "active");

  // whartnell logs in... confirm that 'users' notices
  login1_manager->add_session (login1_seat, whartnell);
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 1);
  ASSERT_EQ (1, event_keys.size());
  compare_user (whartnell, event_keys[0], "online");

  // activate whartnell's session... confirm that 'users' sees:
  //  1. msmith changes from 'active' to 'online'
  //  2. whartnell changes from 'online' to 'active'
  indicator_session_users_activate_user (users, whartnell->uid());
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 2);
  ASSERT_EQ (2, event_keys.size());
  compare_user (msmith, event_keys[0], "online");
  compare_user (whartnell, event_keys[1], "active");

  // reverse the test
  indicator_session_users_activate_user (users, msmith->uid());
  wait_for_signals (users, INDICATOR_SESSION_USERS_SIGNAL_USER_CHANGED, 2);
  ASSERT_EQ (2, event_keys.size());
  compare_user (whartnell, event_keys[0], "online");
  compare_user (msmith, event_keys[1], "active");
}

/**
 * Confirm that adding a Guest doesn't show up in the users list
 */
TEST_F (Users, UnwantedGuest)
{
  GList * uids;

  uids = indicator_session_users_get_uids (users);
  const size_t n = g_list_length (uids);
  g_list_free (uids);

  MockUser * mu = new MockUser (loop, conn, "guest-jjbEVV", "Guest", 1);
  mu->set_system_account (true);
  accounts->add_user (mu);
  wait_msec (50);

  uids = indicator_session_users_get_uids (users);
  ASSERT_EQ (n, g_list_length (uids));
  g_list_free (uids);
}


/**
 * Confirm that we can detect live sessions
 */
TEST_F (Users, LiveSession)
{
  gboolean b;

  // not initially a live session
  ASSERT_FALSE (indicator_session_users_is_live_session (users));
  g_object_get (users, INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION, &b, NULL);
  ASSERT_FALSE (b);

  // now add the criteria for a live session
  MockUser * live_user = new MockUser (loop, conn, "ubuntu", "Ubuntu", 1, 999);
  live_user->set_system_account (true);
  accounts->add_user (live_user);
  const int session_tag = login1_manager->add_session (login1_seat, live_user);
  wait_msec (100);
  login1_seat->activate_session (session_tag);
  wait_for_signal (users, "notify::" INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION);

  // confirm the backend thinks it's a live session
  ASSERT_TRUE (indicator_session_users_is_live_session (users));
  g_object_get (users, INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION, &b, NULL);
  ASSERT_TRUE (b);
}
