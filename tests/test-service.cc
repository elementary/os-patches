/*
Copyright 2012 Canonical Ltd.

Authors:
    Charles Kerr <charles.kerr@canonical.com>

This program is free software: you can redistribute it and/or modify it 
under the terms of the GNU General Public License version 3, as published 
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranties of 
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along 
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "gtest-dbus-fixture.h"
#include "service.h"
#include "backend-mock.h"
#include "backend-mock-users.h"
#include "backend-mock-guest.h"
#include "backend-mock-actions.h"

/***
****
***/

#if 0
namespace
{
  void
  dump_menu_model (GMenuModel * model, int depth)
  {
    GString * indent = g_string_new_len ("                                                  ", (depth*4));
    const int n = g_menu_model_get_n_items (model);
    g_message ("%s depth[%d] menu model[%p] has %d items", indent->str, depth, (void*)model, n);

    for (int i=0; i<n; ++i)
      {
        const char * name;
        GMenuModel * link_value;
        GVariant * attribute_value;

        GMenuAttributeIter * attribute_iter = g_menu_model_iterate_item_attributes (model, i);
        while (g_menu_attribute_iter_get_next (attribute_iter, &name, &attribute_value))
          {
            char * str = g_variant_print (attribute_value, TRUE);
            g_message ("%s depth[%d] menu model[%p] item[%d] attribute key[%s] value[%s]", indent->str, depth, (void*)model, i, name, str);
            g_free (str);
            g_variant_unref (attribute_value);
          }
        g_clear_object (&attribute_iter);

        GMenuLinkIter * link_iter = g_menu_model_iterate_item_links (model, i);
        while (g_menu_link_iter_get_next (link_iter, &name, &link_value))
          {
            g_message ("%s depth[%d] menu model[%p] item[%d] attribute key[%s] model[%p]", indent->str, depth, (void*)model, i, name, (void*)link_value);
            dump_menu_model (link_value, depth+1);
            g_object_unref (link_value);
          }
        g_clear_object (&link_iter);
      }
    g_string_free (indent, TRUE);
  }
}
#endif


/* cppcheck-suppress noConstructor */
class ServiceTest: public GTestDBusFixture
{
    typedef GTestDBusFixture super;

    enum { TIME_LIMIT_SEC = 10 };

  private:

    static void on_name_appeared (GDBusConnection * connection   G_GNUC_UNUSED,
                                  const gchar     * name         G_GNUC_UNUSED,
                                  const gchar     * name_owner   G_GNUC_UNUSED,
                                  gpointer          gself)
    {
      g_main_loop_quit (static_cast<ServiceTest*>(gself)->loop);
    }

    GSList * menu_references;

    gboolean any_item_changed;

    static void on_items_changed (GMenuModel  * model      G_GNUC_UNUSED,
                                  gint          position   G_GNUC_UNUSED,
                                  gint          removed    G_GNUC_UNUSED,
                                  gint          added      G_GNUC_UNUSED,
                                  gpointer      any_item_changed)
    {
      *((gboolean*)any_item_changed) = true;
    }

  protected:

    void activate_subtree (GMenuModel * model)
    {
      // query the GDBusMenuModel for information to activate it
      int n = g_menu_model_get_n_items (model);
      if (!n)
        {
          // give the model a moment to populate its info
          wait_msec (100);
          n = g_menu_model_get_n_items (model);
        }

      // keep a ref so that it stays activated
      menu_references = g_slist_prepend (menu_references, g_object_ref(model));

      g_signal_connect (model, "items-changed", G_CALLBACK(on_items_changed), &any_item_changed);

      // recurse
      for (int i=0; i<n; ++i)
        {
          GMenuModel * link;
          GMenuLinkIter * iter = g_menu_model_iterate_item_links (model, i);
          while (g_menu_link_iter_get_next (iter, NULL, &link))
            {
              activate_subtree (link);
              g_object_unref (link);
            }
          g_clear_object (&iter);
        }
    }

    void sync_menu (void)
    {
      g_slist_free_full (menu_references, (GDestroyNotify)g_object_unref);
      menu_references = NULL;
      activate_subtree (G_MENU_MODEL (menu_model));
    }

    GDBusMenuModel * menu_model;
    GDBusActionGroup * action_group;
    IndicatorSessionService * service;
    GTimer * timer;
    GSettings * indicator_settings;

    virtual void SetUp ()
    {
      super :: SetUp ();

      menu_references = NULL;
      any_item_changed = FALSE;

      timer = g_timer_new ();
      mock_settings = g_settings_new ("com.canonical.indicator.session.backendmock");
      mock_actions = indicator_session_actions_mock_new ();
      mock_users = indicator_session_users_mock_new ();
      mock_guest = indicator_session_guest_mock_new ();
      indicator_settings = g_settings_new ("com.canonical.indicator.session");

      // Start an IndicatorSessionService and wait for it to appear on the bus.
      // This way our calls to g_dbus_*_get() in the next paragraph won't activate
      // a second copy of the service...
      service = indicator_session_service_new ();

      // wait for the service to show up on the bus
      const guint watch_id = g_bus_watch_name_on_connection (conn,
                                                             "com.canonical.indicator.session",
                                                             G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                             on_name_appeared, // quits the loop
                                                             NULL, this, NULL);
      const guint timer_id = g_timeout_add_seconds (TIME_LIMIT_SEC, (GSourceFunc)g_main_loop_quit, loop);
      g_main_loop_run (loop);
      g_source_remove (timer_id);
      g_bus_unwatch_name (watch_id);
      ASSERT_FALSE (times_up());

      // get the actions & menus that the service exported.
      action_group = g_dbus_action_group_get (conn,
                                              "com.canonical.indicator.session",
                                              "/com/canonical/indicator/session");
      menu_model = g_dbus_menu_model_get (conn,
                                          "com.canonical.indicator.session",
                                          "/com/canonical/indicator/session/desktop");
      // the actions are added asynchronously, so wait for the actions
      if (!g_action_group_has_action (G_ACTION_GROUP(action_group), "about"))
        wait_for_signal (action_group, "action-added");
      // activate all the groups in the menu so it'll be ready when we need it
      g_signal_connect (menu_model, "items-changed", G_CALLBACK(on_items_changed), &any_item_changed);
      sync_menu ();
    }

    virtual void TearDown ()
    {
      g_clear_pointer (&timer, g_timer_destroy);

      g_slist_free_full (menu_references, (GDestroyNotify)g_object_unref);
      menu_references = NULL;
      g_clear_object (&menu_model);

      g_clear_object (&action_group);
      g_clear_object (&mock_settings);
      g_clear_object (&indicator_settings);
      g_clear_object (&service);
      g_clear_object (&mock_actions);
      g_clear_object (&mock_users);
      g_clear_object (&mock_guest);
      wait_msec (100);

      super :: TearDown ();
    }

  protected:

    bool times_up () const
    {
      return g_timer_elapsed (timer, NULL) >= TIME_LIMIT_SEC;
    }

    void wait_for_has_action (const char * name)
    {
      while (!g_action_group_has_action (G_ACTION_GROUP(action_group), name) && !times_up())
        wait_msec (50);

      ASSERT_FALSE (times_up());
      ASSERT_TRUE (g_action_group_has_action (G_ACTION_GROUP(action_group), name));
    }

    void wait_for_menu_resync (void)
    {
      any_item_changed = false;
      while (!times_up() && !any_item_changed)
        wait_msec (50);
      g_warn_if_fail (any_item_changed);
      sync_menu ();
    }

  protected:

    void check_last_command_is (const char * expected)
    {
      char * str = g_settings_get_string (mock_settings, "last-command");
      ASSERT_STREQ (expected, str);
      g_free (str);
    }

    void test_simple_action (const char * action_name)
    {
      wait_for_has_action (action_name);

      g_action_group_activate_action (G_ACTION_GROUP (action_group), action_name, NULL);
      wait_for_signal (mock_settings, "changed::last-command");
      check_last_command_is (action_name);
    }

  protected:

    bool find_menu_item_for_action (const char * action_key, GMenuModel ** setme, int * item_index)
    {
      bool success = false;

      for (GSList * l=menu_references; !success && (l!=NULL); l=l->next)
        {
          GMenuModel * mm = G_MENU_MODEL (l->data);
          const int n = g_menu_model_get_n_items (mm);

          for (int i=0; !success && i<n; ++i)
            {
              char * action = NULL;
              if (!g_menu_model_get_item_attribute (mm, i, G_MENU_ATTRIBUTE_ACTION, "s", &action))
                continue;

              if ((success = !g_strcmp0 (action, action_key)))
                {
                  if (setme != NULL)
                    *setme = G_MENU_MODEL (g_object_ref (G_OBJECT(mm)));

                  if (item_index != NULL)
                    *item_index = i;
                }

              g_free (action);
            }
        }

      return success;
    }

    bool action_menuitem_exists (const char * action_name)
    {
      bool found;
      GMenuModel * model = 0;
      int pos = -1;

      if ((found = find_menu_item_for_action (action_name, &model, &pos)))
        g_object_unref (G_OBJECT(model));

      return found;
    }

    bool action_menuitem_label_is_ellipsized (const char * action_name)
    {
      int pos = -1;
      GMenuModel * model = 0;
      bool ellipsized = false;
      char * label = NULL;

      if (find_menu_item_for_action (action_name, &model, &pos))
        {
          g_menu_model_get_item_attribute (model, pos, G_MENU_ATTRIBUTE_LABEL, "s", &label);
          g_object_unref (G_OBJECT(model));
        }

      ellipsized = (label != NULL) && g_str_has_suffix (label, "\342\200\246");
      g_free (label);
      return ellipsized;
    }

    void check_header (const char * expected_label, const char * expected_icon, const char * expected_a11y)
    {
      GVariant * state = g_action_group_get_action_state (G_ACTION_GROUP(action_group), "_header");
      ASSERT_TRUE (state != NULL);
      ASSERT_TRUE (g_variant_is_of_type (state, G_VARIANT_TYPE ("a{sv}")));

      if (expected_label != NULL)
        {
          GVariant * v = g_variant_lookup_value (state, "label", G_VARIANT_TYPE_STRING);
          if (!v) // if no label in the state, expected_label must be an empty string
            ASSERT_FALSE (*expected_label);
          else
            ASSERT_STREQ (expected_label, g_variant_get_string (v, NULL));
        }

      if (expected_a11y != NULL)
        {
          GVariant * v = g_variant_lookup_value (state, "accessible-desc", G_VARIANT_TYPE_STRING);
          ASSERT_TRUE (v != NULL);
          ASSERT_STREQ (expected_a11y, g_variant_get_string (v, NULL));
          g_variant_unref (v);
        }

      if (expected_icon != NULL)
        {
          GVariant * v = g_variant_lookup_value (state, "icon", NULL);
          GIcon * expected = g_themed_icon_new_with_default_fallbacks (expected_icon);
          GIcon * actual = g_icon_deserialize (v);
          ASSERT_TRUE (g_icon_equal (expected, actual));
          g_object_unref (actual);
          g_object_unref (expected);
          g_variant_unref (v);
        }

      // the session menu is always visible...
      gboolean visible = false;
      g_variant_lookup (state, "visible", "b", &visible);
      ASSERT_TRUE (visible);

      g_variant_unref (state);
    }

    void check_label (const char * expected_label, GMenuModel * model, int pos)
    {
      char * label = NULL;
      ASSERT_TRUE (g_menu_model_get_item_attribute (model, pos, G_MENU_ATTRIBUTE_LABEL, "s", &label));
      ASSERT_STREQ (expected_label, label);
      g_free (label);
    }
};

/***
****
***/

#if 0
TEST_F (ServiceTest, HelloWorld)
{
  ASSERT_TRUE (true);
}
#endif

TEST_F (ServiceTest, About)
{
  test_simple_action ("about");
}

TEST_F (ServiceTest, Help)
{
  test_simple_action ("help");
}

TEST_F (ServiceTest, Hibernate)
{
  test_simple_action ("hibernate");
}

TEST_F (ServiceTest, Settings)
{
  test_simple_action ("settings");
}

TEST_F (ServiceTest, Logout)
{
  test_simple_action ("logout");
}

TEST_F (ServiceTest, PowerOff)
{
  test_simple_action ("power-off");
}

TEST_F (ServiceTest, Reboot)
{
  test_simple_action ("reboot");
}

TEST_F (ServiceTest, SwitchToScreensaver)
{
  test_simple_action ("switch-to-screensaver");
}

TEST_F (ServiceTest, SwitchToGuest)
{
  test_simple_action ("switch-to-guest");
}

TEST_F (ServiceTest, SwitchToGreeter)
{
  test_simple_action ("switch-to-greeter");
}

TEST_F (ServiceTest, Suspend)
{
  test_simple_action ("suspend");
}

#if 0
namespace
{
  gboolean
  find_menu_item_for_action (GMenuModel * top, const char * action_key, GMenuModel ** setme, int * item_index)
  {
    gboolean success = FALSE;
    const int n = g_menu_model_get_n_items (top);

    for (int i=0; !success && i<n; ++i)
      {
        char * action = NULL;
        if (g_menu_model_get_item_attribute (top, i, G_MENU_ATTRIBUTE_ACTION, "s", &action))
          {
            if ((success = !g_strcmp0 (action, action_key)))
              {
                *setme = G_MENU_MODEL (g_object_ref (G_OBJECT(top)));
                *item_index = i;
              }

            g_free (action);
          }

        const char * name = NULL;
        GMenuModel * link_value = NULL;
        GMenuLinkIter * link_iter = g_menu_model_iterate_item_links (top, i);
        while (!success && g_menu_link_iter_get_next (link_iter, &name, &link_value))
          {
            success = find_menu_item_for_action (link_value, action_key, setme, item_index);
            g_object_unref (link_value);
          }
        g_clear_object (&link_iter);
      }

    return success;
  }

  gchar *
  get_menu_label_for_action (GMenuModel * top, const char * action)
  {
    int pos;
    GMenuModel * model;
    gchar * label = NULL;

    if (find_menu_item_for_action (top, action, &model, &pos))
      {
        g_menu_model_get_item_attribute (model, pos, G_MENU_ATTRIBUTE_LABEL, "s", &label);
        g_object_unref (G_OBJECT(model));
      }

    return label;
  }

  gboolean str_is_ellipsized (const char * str)
  {
    g_assert (str != NULL);
    return g_str_has_suffix (str, "\342\200\246");
  }
}
#endif

TEST_F (ServiceTest, ConfirmationDisabledByBackend)
{
  const char * const confirm_supported_key = "can-prompt";
  const char * const confirm_disabled_key = "suppress-logout-restart-shutdown";

  bool confirm_supported = g_settings_get_boolean (mock_settings, confirm_supported_key);
  bool confirm_disabled = g_settings_get_boolean (indicator_settings, confirm_disabled_key);
  bool confirm = confirm_supported && !confirm_disabled;

  // confirm that the ellipsis are correct
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.switch-to-screensaver"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.logout"));
  if (action_menuitem_exists ("indicator.reboot"))
    ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.reboot"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.power-off"));

  // now toggle the can-prompt flag
  confirm_supported = !confirm_supported;
  g_settings_set_boolean (mock_settings, confirm_supported_key, confirm_supported);
  confirm = confirm_supported && !confirm_disabled;

  wait_for_menu_resync ();

  // confirm that the ellipsis are correct
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.switch-to-greeter"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.logout"));
  if (action_menuitem_exists ("indicator.reboot"))
    ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.reboot"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.power-off"));

  // cleanup
  g_settings_reset (mock_settings, confirm_supported_key);
}

TEST_F (ServiceTest, ConfirmationDisabledByUser)
{
  const char * const confirm_supported_key = "can-prompt";
  const char * const confirm_disabled_key = "suppress-logout-restart-shutdown";

  bool confirm_supported = g_settings_get_boolean (mock_settings, confirm_supported_key);
  bool confirm_disabled = g_settings_get_boolean (indicator_settings, confirm_disabled_key);
  bool confirm = confirm_supported && !confirm_disabled;

  // confirm that the ellipsis are correct
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.switch-to-screensaver"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.logout"));
  if (action_menuitem_exists ("indicator.reboot"))
    ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.reboot"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.power-off"));

  // now toggle the can-prompt flag
  confirm_disabled = !confirm_disabled;
  g_settings_set_boolean (indicator_settings, confirm_disabled_key, confirm_disabled);
  confirm = confirm_supported && !confirm_disabled;

  wait_for_menu_resync ();

  // confirm that the ellipsis are correct
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.switch-to-greeter"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.logout"));
  if (action_menuitem_exists ("indicator.reboot"))
    ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.reboot"));
  ASSERT_EQ (confirm, action_menuitem_label_is_ellipsized ("indicator.power-off"));

  // cleanup
  g_settings_reset (indicator_settings, confirm_disabled_key);
}

/**
 * Check that the default menu has items for each of these actions
 */
TEST_F (ServiceTest, DefaultMenuItems)
{
  ASSERT_TRUE (find_menu_item_for_action ("indicator.about", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.help", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.settings", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-guest", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.logout", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.suspend", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.hibernate", NULL, NULL));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.power-off", NULL, NULL));
}

TEST_F (ServiceTest, OnlineAccountError)
{
  bool err;
  int pos = -1;
  GMenuModel * model = 0;
  const char * const error_key = "has-online-account-error";

  // check the initial default header state
  check_header ("", "system-devices-panel", "System");

  // check that the menuitems' existence matches the error flag
  err = g_settings_get_boolean (mock_settings, error_key);
  ASSERT_FALSE (err);
  ASSERT_EQ (err, find_menu_item_for_action ("indicator.online-accounts", &model, &pos));
  g_clear_object (&model);

  // now toggle the error flag
  err = !err;
  g_settings_set_boolean (mock_settings, error_key, err);

  // wait for the _header action and error menuitem to update
  wait_for_menu_resync ();

  // check that the menuitems' existence matches the error flag
  ASSERT_TRUE (g_settings_get_boolean (mock_settings, error_key));
  ASSERT_TRUE (find_menu_item_for_action ("indicator.online-accounts", &model, &pos));
  g_clear_object (&model);

  // check that the service has a corresponding action
  ASSERT_TRUE (g_action_group_has_action (G_ACTION_GROUP(action_group), "online-accounts"));
  ASSERT_TRUE (g_action_group_get_action_enabled (G_ACTION_GROUP(action_group), "online-accounts"));

  // confirm that activating the action is handled by the service
  g_action_group_activate_action (G_ACTION_GROUP(action_group), "online-accounts", NULL);
  wait_for_signal (mock_settings, "changed::last-command");
  check_last_command_is ("online-accounts");

  // check that the header's icon and a11y adjusted to the error state
  check_header ("", "system-devices-panel", "System");

  // cleanup
  g_settings_reset (mock_settings, error_key);
}

namespace
{
  gboolean set_live_session_to_true (gpointer unused G_GNUC_UNUSED)
  {
    const char * const live_session_key = "is-live-session";
    g_settings_set_boolean (mock_settings, live_session_key, true);
    return G_SOURCE_REMOVE;
  }
}

TEST_F (ServiceTest, LiveSession)
{
  gboolean b;
  const char * const live_session_key = "is-live-session";

  // default BackendMock is not a live session
  ASSERT_FALSE (g_settings_get_boolean (mock_settings, live_session_key));
  g_object_get (mock_users, INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION, &b, NULL);
  ASSERT_FALSE (b);

  // confirm that we can see live sessions
  g_idle_add (set_live_session_to_true, NULL);
  wait_for_signal (mock_users, "notify::" INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION);
  ASSERT_TRUE (g_settings_get_boolean (mock_settings, live_session_key));
  wait_msec (50);
  g_object_get (mock_users, INDICATOR_SESSION_USERS_PROP_IS_LIVE_SESSION, &b, NULL);
  ASSERT_TRUE (b);

  // cleanup
  g_settings_reset (mock_settings, live_session_key);
}


TEST_F (ServiceTest, User)
{
  const char * const error_key = "has-online-account-error";
  const char * const show_name_key = "show-real-name-on-panel";

  struct {
    guint uid;
    guint64 login_frequency;
    const gchar * user_name;
    const gchar * real_name;
  } account_info[] = {
    { 101, 134, "whartnell",  "First Doctor"    },
    { 102, 119, "ptroughton", "Second Doctor"   },
    { 103, 128, "jpertwee",   "Third Doctor"    },
    { 104, 172, "tbaker",     "Fourth Doctor"   },
    { 105,  69, "pdavison",   "Fifth Doctor"    },
    { 106,  31, "cbaker",     "Sixth Doctor"    },
    { 107,  42, "smccoy",     "Seventh Doctor"  },
    { 108,   1, "pmcgann",    "Eigth Doctor"    },
    { 109,  13, "ceccleston", "Ninth Doctor"    },
    { 110,  47, "dtennant",   "Tenth Doctor"    },
    { 111,  34, "msmith",     "Eleventh Doctor" },
    { 201,   1, "rhurndall",  "First Doctor"    }
  };

  // Find the switcher menu model.
  // In BackendMock's default setup, it will only have two menuitems: lockswitch & guest
  int pos = 0;
  GMenuModel * switch_menu = 0;
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (2, g_menu_model_get_n_items (switch_menu));
  g_clear_object (&switch_menu);

  // now add some users
  IndicatorSessionUser * users[12];
  for (int i=0; i<12; ++i)
    users[i] = 0;
  for (int i=0; i<5; ++i)
    {
      IndicatorSessionUser * u = g_new0 (IndicatorSessionUser, 1);
      u->is_current_user = false;
      u->is_logged_in = false;
      u->uid = account_info[i].uid;
      u->login_frequency = account_info[i].login_frequency;
      u->user_name = g_strdup (account_info[i].user_name);
      u->real_name = g_strdup (account_info[i].real_name);
      indicator_session_users_mock_add_user (INDICATOR_SESSION_USERS_MOCK(mock_users), u);
      users[i] = u;
    }

  wait_for_menu_resync ();

  // now there should be 7 menuitems: greeter + guest + the five doctors
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (7, g_menu_model_get_n_items (switch_menu));
  // confirm that the doctor names are sorted
  check_label ("Fifth Doctor", switch_menu, 2);
  check_label ("First Doctor", switch_menu, 3);
  check_label ("Fourth Doctor", switch_menu, 4);
  check_label ("Second Doctor", switch_menu, 5);
  check_label ("Third Doctor", switch_menu, 6);
  g_clear_object (&switch_menu);

  // now remove a couple of 'em
  indicator_session_users_mock_remove_user (INDICATOR_SESSION_USERS_MOCK(mock_users), account_info[3].uid);
  indicator_session_users_mock_remove_user (INDICATOR_SESSION_USERS_MOCK(mock_users), account_info[4].uid);

  wait_for_menu_resync ();

  // now there should be 5 menuitems: greeter + guest + the three doctors
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (5, g_menu_model_get_n_items (switch_menu));
  // confirm that the doctor names are sorted
  check_label ("First Doctor", switch_menu, 2);
  check_label ("Second Doctor", switch_menu, 3);
  check_label ("Third Doctor", switch_menu, 4);
  g_clear_object (&switch_menu);

  // now let's have the third one be the current user
  users[2]->is_current_user = true;
  users[2]->is_logged_in = true;
  indicator_session_users_changed (mock_users, users[2]->uid);

  wait_for_menu_resync ();

  // now there should be 5 menuitems: greeter + guest + the three doctors
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (5, g_menu_model_get_n_items (switch_menu));
  g_clear_object (&switch_menu);

  // oh hey, while we've got an active user let's check the header
  ASSERT_FALSE (g_settings_get_boolean (indicator_settings, show_name_key));
  ASSERT_FALSE (g_settings_get_boolean (mock_settings, error_key));
  check_header ("", "system-devices-panel", "System");
  g_settings_set_boolean (indicator_settings, show_name_key, true);
  wait_for_signal (action_group, "action-state-changed");
  check_header ("Third Doctor", "system-devices-panel", "System, Third Doctor");
  g_settings_reset (indicator_settings, show_name_key);

  // try setting the max user count to 2...
  // since troughton has the fewest logins, he should get culled
  g_object_set (service, "max-users", 2, NULL);
  guint max_users;
  g_object_get (service, "max-users", &max_users, NULL);
  ASSERT_EQ (2, max_users);
  wait_for_menu_resync ();
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (4, g_menu_model_get_n_items (switch_menu));
  check_label ("First Doctor", switch_menu, 2);
  check_label ("Third Doctor", switch_menu, 3);
  g_clear_object (&switch_menu);

  // add some more, test sorting and culling.
  // add in all the doctors, but only show 7, and make msmith the current session
  g_object_set (service, "max-users", 7, NULL);
  g_object_get (service, "max-users", &max_users, NULL);
  ASSERT_EQ (7, max_users);
  for (int i=3; i<12; ++i)
    {
      IndicatorSessionUser * u = g_new0 (IndicatorSessionUser, 1);
      u->is_current_user = false;
      u->is_logged_in = false;
      u->uid = 101 + i;
      u->login_frequency = account_info[i].login_frequency;
      u->user_name = g_strdup (account_info[i].user_name);
      u->real_name = g_strdup (account_info[i].real_name);
      indicator_session_users_mock_add_user (INDICATOR_SESSION_USERS_MOCK(mock_users), u);
      users[i] = u;
    }
  users[2]->is_current_user = false;
  indicator_session_users_changed (mock_users, users[2]->uid);
  users[10]->is_current_user = true;
  users[10]->is_logged_in = true;
  indicator_session_users_changed (mock_users, users[10]->uid);
  wait_for_menu_resync ();
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (9, g_menu_model_get_n_items (switch_menu));
  check_label ("Eleventh Doctor", switch_menu, 2);
  check_label ("Fifth Doctor", switch_menu, 3);
  check_label ("First Doctor", switch_menu, 4);
  check_label ("Fourth Doctor", switch_menu, 5);
  check_label ("Second Doctor", switch_menu, 6);
  check_label ("Tenth Doctor", switch_menu, 7);
  check_label ("Third Doctor", switch_menu, 8);
  g_clear_object (&switch_menu);

  /* Hide the user list */
  g_settings_set_boolean (indicator_settings, "user-show-menu", FALSE);
  wait_for_menu_resync ();
  // now there should be 2 menuitems: greeter + guest
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  ASSERT_EQ (0, pos);
  ASSERT_EQ (2, g_menu_model_get_n_items (switch_menu));
  g_clear_object (&switch_menu);
  g_settings_set_boolean (indicator_settings, "user-show-menu", TRUE);

  // now switch to one of the doctors
  g_action_group_activate_action (G_ACTION_GROUP(action_group),
                                  "switch-to-user",
                                  g_variant_new_string("tbaker"));
  wait_for_signal (mock_settings, "changed::last-command");
  check_last_command_is ("switch-to-user::tbaker");
}

TEST_F (ServiceTest, UserLabels)
{
  int pos = 0;
  GMenuModel * switch_menu = 0;

  // Check label uses username when real name is blank
  IndicatorSessionUser * u = g_new0 (IndicatorSessionUser, 1);
  u->uid = 100;
  u->user_name = g_strdup ("blank");
  u->real_name = g_strdup ("");
  indicator_session_users_mock_add_user (INDICATOR_SESSION_USERS_MOCK(mock_users), u);
  wait_for_menu_resync ();
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  check_label ("blank", switch_menu, 2);
  g_clear_object (&switch_menu);
  indicator_session_users_mock_remove_user (INDICATOR_SESSION_USERS_MOCK(mock_users), 100);

  // Check label uses username when real name is all whitespace
  u = g_new0 (IndicatorSessionUser, 1);
  u->uid = 100;
  u->user_name = g_strdup ("whitespace");
  u->real_name = g_strdup (" ");
  indicator_session_users_mock_add_user (INDICATOR_SESSION_USERS_MOCK(mock_users), u);
  wait_for_menu_resync ();
  ASSERT_TRUE (find_menu_item_for_action ("indicator.switch-to-screensaver", &switch_menu, &pos));
  check_label ("whitespace", switch_menu, 2);
  g_clear_object (&switch_menu);
  indicator_session_users_mock_remove_user (INDICATOR_SESSION_USERS_MOCK(mock_users), 100);
}
