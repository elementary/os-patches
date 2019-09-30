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

#define SUPPRESS_KEY "suppress-logout-restart-shutdown"

/***
****
***/

class Actions: public GTestMockDBusFixture
{
  private:

    typedef GTestMockDBusFixture super;

  protected:

    GCancellable * cancellable;
    IndicatorSessionActions * actions;
    GSettings * indicator_settings;

    virtual void SetUp ()
    {
      super :: SetUp ();

      // init 'actions'
      indicator_settings = g_settings_new ("com.canonical.indicator.session");
      cancellable = g_cancellable_new ();
      actions = 0;
      backend_get (cancellable, &actions, NULL, NULL);
      g_assert (actions != 0);
      wait_msec (300);
    }

    virtual void TearDown ()
    {
      g_cancellable_cancel (cancellable);
      g_clear_object (&indicator_settings);
      g_clear_object (&cancellable);
      g_clear_object (&actions);

      super :: TearDown ();
    }
};

/***
****
***/

TEST_F (Actions, HelloWorld)
{
  ASSERT_TRUE (true);
}

namespace
{
  static gboolean toggle_can_switch (gpointer settings)
  {
    const char * key = "disable-user-switching";
    gboolean b = g_settings_get_boolean (G_SETTINGS(settings), key);
    g_settings_set_boolean (G_SETTINGS(settings), key, !b);
    return G_SOURCE_REMOVE;
  }
}

TEST_F (Actions, CanSwitch)
{
  const char * schema_id = "org.gnome.desktop.lockdown";
  const char * settings_key = "disable-user-switching";
  GSettings * s = g_settings_new (schema_id);

  for (int i=0; i<3; ++i)
    {
      bool b;
      gboolean b2;

      b = login1_seat->can_activate_sessions() && !g_settings_get_boolean (s, settings_key);
      ASSERT_EQ (b, indicator_session_actions_can_switch (actions));
      g_object_get (actions, INDICATOR_SESSION_ACTIONS_PROP_CAN_SWITCH, &b2, NULL);
      ASSERT_EQ (b, b2);

      g_idle_add (toggle_can_switch, s);
      wait_for_signal (actions, "notify::" INDICATOR_SESSION_ACTIONS_PROP_CAN_SWITCH);
    }

  g_object_unref (s);
}

namespace
{
  static gboolean toggle_can_lock (gpointer settings)
  {
    const char * key = "disable-lock-screen";
    gboolean b = g_settings_get_boolean (G_SETTINGS(settings), key);
    g_settings_set_boolean (G_SETTINGS(settings), key, !b);
    return G_SOURCE_REMOVE;
  }
}

TEST_F (Actions, CanLock)
{
  const char * schema_id = "org.gnome.desktop.lockdown";
  const char * settings_key = "disable-lock-screen";
  GSettings * s = g_settings_new (schema_id);

  for (int i=0; i<3; ++i)
    {
      bool b;
      gboolean b2;

      b = g_settings_get_boolean (s, settings_key);
      ASSERT_EQ (b, !indicator_session_actions_can_lock (actions));
      g_object_get (actions, INDICATOR_SESSION_ACTIONS_PROP_CAN_LOCK, &b2, NULL);
      ASSERT_EQ (b, !b2);

      g_idle_add (toggle_can_lock, s);
      wait_for_signal (actions, "notify::" INDICATOR_SESSION_ACTIONS_PROP_CAN_LOCK);
    }

  g_object_unref (s);
}

namespace
{
  static gboolean toggle_can_logout (gpointer settings)
  {
    const char * key = "disable-log-out";
    gboolean b = g_settings_get_boolean (G_SETTINGS(settings), key);
    g_settings_set_boolean (G_SETTINGS(settings), key, !b);
    return G_SOURCE_REMOVE;
  }
}

TEST_F (Actions, CanLogout)
{
  const char * schema_id = "org.gnome.desktop.lockdown";
  const char * settings_key = "disable-log-out";
  GSettings * s = g_settings_new (schema_id);

  for (int i=0; i<3; ++i)
    {
      bool b;
      gboolean b2;

      b = g_settings_get_boolean (s, settings_key);
      ASSERT_EQ (b, !indicator_session_actions_can_logout (actions));
      g_object_get (actions, INDICATOR_SESSION_ACTIONS_PROP_CAN_LOGOUT, &b2, NULL);
      ASSERT_EQ (b, !b2);

      g_idle_add (toggle_can_logout, s);
      wait_for_signal (actions, "notify::" INDICATOR_SESSION_ACTIONS_PROP_CAN_LOGOUT);
    }

  g_object_unref (s);
}

TEST_F (Actions, CanSuspend)
{
  const std::string can_suspend = login1_manager->can_suspend ();
  gboolean b = indicator_session_actions_can_suspend (actions);
  ASSERT_EQ (b, can_suspend=="yes" || can_suspend=="challenge");
}

TEST_F (Actions, CanHibernate)
{
  const std::string can_hibernate = login1_manager->can_hibernate ();
  gboolean b = indicator_session_actions_can_hibernate (actions);
  ASSERT_EQ (b, can_hibernate=="yes" || can_hibernate=="challenge");
}

TEST_F (Actions, Reboot)
{
  ASSERT_TRUE (login1_manager->last_action().empty());
  ASSERT_FALSE (g_settings_get_boolean (indicator_settings, SUPPRESS_KEY));

  // confirm that user is prompted
  // and that no action is taken when the user cancels the dialog
  indicator_session_actions_reboot (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open());
  end_session_dialog->cancel();
  wait_msec (50);
  ASSERT_TRUE (login1_manager->last_action().empty());

  // confirm that user is prompted
  // and that no action is taken when the user cancels the dialog
  indicator_session_actions_reboot (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open ());
  end_session_dialog->confirm_reboot ();
  wait_msec (100);
  ASSERT_EQ (login1_manager->last_action(), "reboot");

  // confirm that we try to reboot w/o prompting
  // if prompting is disabled
  login1_manager->clear_last_action ();
  ASSERT_EQ ("", login1_manager->last_action());
  g_settings_set_boolean (indicator_settings, SUPPRESS_KEY, TRUE);
  wait_msec (50);
  ASSERT_TRUE (login1_manager->last_action().empty());
  wait_msec (50);
  indicator_session_actions_reboot (actions);
  wait_msec (50);
  ASSERT_EQ ("reboot", login1_manager->last_action());

  g_settings_reset (indicator_settings, SUPPRESS_KEY);
}

TEST_F (Actions, PowerOff)
{
  ASSERT_TRUE (login1_manager->last_action().empty());

  // confirm that user is prompted
  // and that no action is taken when the user cancels the dialog
  indicator_session_actions_power_off (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open());
  end_session_dialog->cancel();
  wait_msec (50);
  ASSERT_TRUE (login1_manager->last_action().empty());

  // confirm that user is prompted
  // and that no action is taken when the user cancels the dialog
  indicator_session_actions_power_off (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open ());
  end_session_dialog->confirm_shutdown ();
  wait_msec (100);
  ASSERT_EQ (login1_manager->last_action(), "power-off");

  // confirm that we try to shutdown w/o prompting
  // if the EndSessionDialog isn't available
  // if prompting is disabled
  login1_manager->clear_last_action ();
  ASSERT_EQ ("", login1_manager->last_action());
  g_settings_set_boolean (indicator_settings, SUPPRESS_KEY, TRUE);
  wait_msec (50);
  indicator_session_actions_power_off (actions);
  wait_msec (50);
  ASSERT_EQ (login1_manager->last_action(), "power-off");

  g_settings_reset (indicator_settings, SUPPRESS_KEY);
}

TEST_F (Actions, LogoutUnity)
{
  MockUnitySession unity_session(loop, conn);
  ASSERT_EQ (MockUnitySession::None, unity_session.last_action());
  wait_msec();

  // confirm that user is prompted
  // and that no action is taken when the user cancels the dialog
  indicator_session_actions_logout (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open());
  end_session_dialog->cancel();
  wait_msec (50);
  ASSERT_EQ (MockUnitySession::None, unity_session.last_action());

  // confirm that user is prompted
  // and that logout is called when user confirms the logout dialog
  indicator_session_actions_logout (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open ());
  end_session_dialog->confirm_logout ();
  wait_msec (100);
  ASSERT_EQ (MockUnitySession::RequestLogout, unity_session.last_action());

  // confirm that we try to call SessionManager::LogoutQuet
  // when prompts are disabled
  login1_manager->clear_last_action ();
  unity_session.clear_last_action ();
  ASSERT_EQ ("", login1_manager->last_action());
  ASSERT_EQ (MockUnitySession::None, unity_session.last_action ());
  g_settings_set_boolean (indicator_settings, SUPPRESS_KEY, TRUE);
  wait_msec (50);
  indicator_session_actions_logout (actions);
  wait_msec (50);
  ASSERT_EQ (MockUnitySession::RequestLogout, unity_session.last_action ());
  g_settings_reset (indicator_settings, SUPPRESS_KEY);
}

TEST_F (Actions, LogoutGnome)
{
  MockSessionManager session_manager (loop, conn);
  ASSERT_EQ (MockSessionManager::None, session_manager.last_action ());
  wait_msec(50);

  // confirm that user is prompted
  // and that no action is taken when the user cancels the dialog
  indicator_session_actions_logout (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open());
  end_session_dialog->cancel();
  wait_msec (50);
  ASSERT_EQ (MockSessionManager::None, session_manager.last_action ());

  // confirm that user is prompted
  // and that logout is called when user confirms in the dialog
  indicator_session_actions_logout (actions);
  wait_msec (50);
  ASSERT_TRUE (end_session_dialog->is_open ());
  end_session_dialog->confirm_logout ();
  wait_msec (100);
  ASSERT_EQ (MockSessionManager::LogoutQuiet, session_manager.last_action ());
 
  // confirm that we try to call SessionManager::LogoutQuet
  // when prompts are disabled
  login1_manager->clear_last_action ();
  ASSERT_EQ ("", login1_manager->last_action());
  g_settings_set_boolean (indicator_settings, SUPPRESS_KEY, TRUE);
  wait_msec (50);
  indicator_session_actions_logout (actions);
   wait_msec (50);
  ASSERT_EQ (MockSessionManager::LogoutQuiet, session_manager.last_action ());

  g_settings_reset (indicator_settings, SUPPRESS_KEY);
}

TEST_F (Actions, Suspend)
{
  ASSERT_TRUE (login1_manager->last_action().empty());
  indicator_session_actions_suspend (actions);
  wait_msec (50);
  ASSERT_EQ (login1_manager->last_action(), "suspend");
}

TEST_F (Actions, Hibernate)
{
  ASSERT_TRUE (login1_manager->last_action().empty());
  indicator_session_actions_hibernate (actions);
  wait_msec (50);
  ASSERT_EQ (login1_manager->last_action(), "hibernate");
}

TEST_F (Actions, SwitchToScreensaver)
{
  MockUnitySession unity_session(loop, conn);

  ASSERT_EQ (MockUnitySession::None, unity_session.last_action());
  indicator_session_actions_switch_to_screensaver (actions);
  wait_msec (50);
  ASSERT_EQ (MockUnitySession::Lock, unity_session.last_action());
}

TEST_F (Actions, SwitchToGreeter)
{
  MockUnitySession unity_session(loop, conn);

  ASSERT_NE (MockDisplayManagerSeat::GREETER, dm_seat->last_action());
  ASSERT_EQ (MockUnitySession::None, unity_session.last_action());
  indicator_session_actions_switch_to_greeter (actions);
  wait_msec (50);
  ASSERT_EQ (MockUnitySession::PromptLock, unity_session.last_action());
  ASSERT_EQ (MockDisplayManagerSeat::GREETER, dm_seat->last_action());
}

TEST_F (Actions, SwitchToGuest)
{
  MockUnitySession unity_session(loop, conn);

  // allow guests
  dm_seat->set_guest_allowed (true);

  // set up a guest
  MockUser * guest_user = new MockUser (loop, conn, "guest-zzbEVV", "Guest", 10);
  guest_user->set_system_account (true);
  accounts->add_user (guest_user);
  int guest_session_tag = login1_manager->add_session (login1_seat, guest_user);

  // try to switch to guest
  indicator_session_actions_switch_to_guest (actions);
  wait_for_signal (login1_seat->skeleton(), "notify::active-session");
  ASSERT_EQ (guest_session_tag, login1_seat->active_session());
  wait_msec (50);
  ASSERT_EQ (MockUnitySession::PromptLock, unity_session.last_action());
}

TEST_F (Actions, SwitchToUsername)
{
  MockUnitySession unity_session(loop, conn);
  const char * const dr1_username = "whartnell";
  const char * const dr2_username = "ptroughton";
  MockUser * dr1_user;
  MockUser * dr2_user;
  int dr1_session;
  int dr2_session;

  dr1_user = accounts->find_by_username (dr1_username);
  dr1_session = login1_manager->add_session (login1_seat, dr1_user);

  dr2_user = accounts->find_by_username (dr2_username);
  dr2_session = login1_manager->add_session (login1_seat, dr2_user);

  indicator_session_actions_switch_to_username (actions, dr1_username);
  wait_for_signal (login1_seat->skeleton(), "notify::active-session");
  ASSERT_EQ (dr1_session, login1_seat->active_session());
  wait_msec (50);
  ASSERT_EQ (MockUnitySession::PromptLock, unity_session.last_action());

  indicator_session_actions_switch_to_username (actions, dr2_username);
  wait_for_signal (login1_seat->skeleton(), "notify::active-session");
  ASSERT_EQ (dr2_session, login1_seat->active_session());
  wait_msec (50);

  indicator_session_actions_switch_to_username (actions, dr1_username);
  wait_for_signal (login1_seat->skeleton(), "notify::active-session");
  ASSERT_EQ (dr1_session, login1_seat->active_session());
  wait_msec (50);
}

TEST_F (Actions, HasOnlineAccountError)
{
  bool b;
  gboolean gb;

  b = webcredentials->has_error ();
  ASSERT_EQ (b, indicator_session_actions_has_online_account_error (actions));
  g_object_get (actions, INDICATOR_SESSION_ACTIONS_PROP_HAS_ONLINE_ACCOUNT_ERROR, &gb, NULL);
  ASSERT_EQ (b, gb);

  b = !b;
  webcredentials->set_error (b);
  wait_msec (50);
  ASSERT_EQ (b, indicator_session_actions_has_online_account_error (actions));
  g_object_get (actions, INDICATOR_SESSION_ACTIONS_PROP_HAS_ONLINE_ACCOUNT_ERROR, &gb, NULL);
  ASSERT_EQ (b, gb);

  b = !b;
  webcredentials->set_error (b);
  wait_msec (50);
  ASSERT_EQ (b, indicator_session_actions_has_online_account_error (actions));
  g_object_get (actions, INDICATOR_SESSION_ACTIONS_PROP_HAS_ONLINE_ACCOUNT_ERROR, &gb, NULL);
  ASSERT_EQ (b, gb);
}

namespace
{
  static gboolean toggle_suppress (gpointer settings)
  {
    const char * key = SUPPRESS_KEY;
    gboolean b = g_settings_get_boolean (G_SETTINGS(settings), key);
    g_settings_set_boolean (G_SETTINGS(settings), key, !b);
    return G_SOURCE_REMOVE;
  }
}

TEST_F (Actions, SuppressPrompts)
{
  for (int i=0; i<3; ++i)
    {
      bool b;
      gboolean b2;

      b = indicator_session_actions_can_prompt (actions);
      b2 = !g_settings_get_boolean (indicator_settings, SUPPRESS_KEY);
      ASSERT_EQ (b, b2);
      
      g_idle_add (toggle_suppress, indicator_settings);
      wait_for_signal (actions, "notify::" INDICATOR_SESSION_ACTIONS_PROP_CAN_PROMPT);
    }

  g_settings_reset (indicator_settings, SUPPRESS_KEY);
}
