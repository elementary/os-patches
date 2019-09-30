/*
An indicator to show information that is in messaging applications
that the user is using.

Copyright 2012 Canonical Ltd.

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

#include <glib.h>
#include <gio/gio.h>
#include <gtest/gtest.h>

extern "C" {
#include "gactionmuxer.h"
}

static gboolean
strv_contains (gchar **str_array,
	       const gchar *str)
{
	gchar **it;

	for (it = str_array; *it; it++) {
		if (!g_strcmp0 (*it, str))
			return TRUE;
	}

	return FALSE;
}

TEST(GActionMuxerTest, Sanity) {
	GActionMuxer *muxer;

#if G_ENCODE_VERSION(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION) <= GLIB_VERSION_2_34
	g_type_init ();
#endif

	g_test_expect_message ("Indicator-Messages", G_LOG_LEVEL_CRITICAL, "*G_IS_ACTION_MUXER*");
	g_action_muxer_insert (NULL, NULL, NULL);
	g_test_assert_expected_messages ();

	g_test_expect_message ("Indicator-Messages", G_LOG_LEVEL_CRITICAL, "*G_IS_ACTION_MUXER*");
	g_action_muxer_remove (NULL, NULL);
	g_test_assert_expected_messages ();

	muxer = g_action_muxer_new ();

	g_action_muxer_insert (muxer, NULL, NULL);
	g_action_muxer_remove (muxer, NULL);

	g_test_expect_message ("Indicator-Messages", G_LOG_LEVEL_CRITICAL, "*NULL*");
	EXPECT_FALSE (g_action_group_has_action (G_ACTION_GROUP (muxer), NULL));
	g_test_assert_expected_messages ();

	g_test_expect_message ("Indicator-Messages", G_LOG_LEVEL_CRITICAL, "*NULL*");
	EXPECT_FALSE (g_action_group_get_action_enabled (G_ACTION_GROUP (muxer), NULL));
	g_test_assert_expected_messages ();

	g_test_expect_message ("Indicator-Messages", G_LOG_LEVEL_CRITICAL, "*NULL*");
	EXPECT_FALSE (g_action_group_query_action (G_ACTION_GROUP (muxer), NULL, NULL, NULL, NULL, NULL, NULL));
	g_test_assert_expected_messages ();

	g_test_expect_message ("GLib-GIO", G_LOG_LEVEL_CRITICAL, "*NULL*");
	g_action_group_activate_action (G_ACTION_GROUP (muxer), NULL, NULL);
	g_test_assert_expected_messages ();

	g_object_unref (muxer);
}

TEST(GActionMuxerTest, Empty) {
	GActionMuxer *muxer;
	gchar **actions;

#if G_ENCODE_VERSION(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION) <= GLIB_VERSION_2_34
	g_type_init ();
#endif

	muxer = g_action_muxer_new ();

	actions = g_action_group_list_actions (G_ACTION_GROUP (muxer));
	EXPECT_EQ (0, g_strv_length (actions));

	g_strfreev (actions);
	g_object_unref (muxer);
}

TEST(GActionMuxerTest, AddAndRemove) {
	const GActionEntry entries1[] = { { "one" }, { "two" }, { "three" } };
	const GActionEntry entries2[] = { { "gb" }, { "es" }, { "fr" } };
	const GActionEntry entries3[] = { { "foo" }, { "bar" } };
	GSimpleActionGroup *group1;
	GSimpleActionGroup *group2;
	GSimpleActionGroup *group3;
	GActionMuxer *muxer;
	gchar **actions;

#if G_ENCODE_VERSION(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION) <= GLIB_VERSION_2_34
	g_type_init ();
#endif

	group1 = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group1),
					 entries1,
					 G_N_ELEMENTS (entries1),
					 NULL);

	group2 = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group2),
					 entries2,
					 G_N_ELEMENTS (entries2),
					 NULL);

	group3 = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP (group3),
					 entries3,
					 G_N_ELEMENTS (entries3),
					 NULL);

	muxer = g_action_muxer_new ();
	g_action_muxer_insert (muxer, "first", G_ACTION_GROUP (group1));
	g_action_muxer_insert (muxer, "second", G_ACTION_GROUP (group2));
	g_action_muxer_insert (muxer, NULL, G_ACTION_GROUP (group3));

	actions = g_action_group_list_actions (G_ACTION_GROUP (muxer));
	EXPECT_TRUE (g_action_group_has_action (G_ACTION_GROUP (muxer), "first.one"));
	EXPECT_FALSE (g_action_group_has_action (G_ACTION_GROUP (muxer), "one"));
	EXPECT_EQ (8, g_strv_length (actions));
	EXPECT_TRUE (strv_contains (actions, "first.one"));
	EXPECT_TRUE (strv_contains (actions, "first.two"));
	EXPECT_TRUE (strv_contains (actions, "first.three"));
	EXPECT_TRUE (strv_contains (actions, "second.gb"));
	EXPECT_TRUE (strv_contains (actions, "second.es"));
	EXPECT_TRUE (strv_contains (actions, "second.fr"));
	EXPECT_TRUE (strv_contains (actions, "foo"));
	EXPECT_TRUE (strv_contains (actions, "bar"));
	g_strfreev (actions);

	g_action_muxer_remove (muxer, NULL);
	EXPECT_FALSE (g_action_group_has_action (G_ACTION_GROUP (muxer), "foo"));
	EXPECT_TRUE (g_action_group_has_action (G_ACTION_GROUP (muxer), "first.one"));
	actions = g_action_group_list_actions (G_ACTION_GROUP (muxer));
	EXPECT_EQ (6, g_strv_length (actions));
	EXPECT_FALSE (strv_contains (actions, "foo"));
	EXPECT_TRUE (strv_contains (actions, "first.one"));
	g_strfreev (actions);

	g_action_muxer_remove (muxer, "first");
	EXPECT_FALSE (g_action_group_has_action (G_ACTION_GROUP (muxer), "first.two"));
	EXPECT_TRUE (g_action_group_has_action (G_ACTION_GROUP (muxer), "second.es"));
	actions = g_action_group_list_actions (G_ACTION_GROUP (muxer));
	EXPECT_EQ (3, g_strv_length (actions));
	EXPECT_FALSE (strv_contains (actions, "first.two"));
	EXPECT_TRUE (strv_contains (actions, "second.es"));
	g_strfreev (actions);

	g_action_muxer_insert (muxer, "second", G_ACTION_GROUP (group2));
	actions = g_action_group_list_actions (G_ACTION_GROUP (muxer));
	EXPECT_EQ (3, g_strv_length (actions));
	g_strfreev (actions);

	g_action_muxer_insert (muxer, NULL, G_ACTION_GROUP (group3));
	actions = g_action_group_list_actions (G_ACTION_GROUP (muxer));
	EXPECT_EQ (5, g_strv_length (actions));
	g_strfreev (actions);

	g_object_unref (muxer);
	g_object_unref (group1);
	g_object_unref (group2);
	g_object_unref (group3);
}

static gboolean
g_variant_equal0 (gconstpointer one,
		  gconstpointer two)
{
	if (one == NULL)
		return two == NULL;
	else
		return g_variant_equal (one, two);
}

TEST(GActionMuxerTest, ActionAttributes) {
	GSimpleActionGroup *group;
	GSimpleAction *action;
	GActionMuxer *muxer;
	gboolean enabled[2];
	const GVariantType *param_type[2];
	const GVariantType *state_type[2];
	GVariant *state_hint[2];
	GVariant *state[2];

#if G_ENCODE_VERSION(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION) <= GLIB_VERSION_2_34
	g_type_init ();
#endif

	group = g_simple_action_group_new ();
	action = g_simple_action_new ("one", G_VARIANT_TYPE_STRING);
	g_action_map_add_action (G_ACTION_MAP(group), G_ACTION (action));

	muxer = g_action_muxer_new ();
	g_action_muxer_insert (muxer, "first", G_ACTION_GROUP (group));

	/* test two of the convenience functions */
	EXPECT_TRUE (g_action_group_get_action_enabled (G_ACTION_GROUP (muxer), "first.one"));
	g_simple_action_set_enabled (action, FALSE);
	EXPECT_FALSE (g_action_group_get_action_enabled (G_ACTION_GROUP (muxer), "first.one"));

	EXPECT_STREQ ((gchar *) g_action_group_get_action_parameter_type (G_ACTION_GROUP (muxer), "first.one"),
		      (gchar *) G_VARIANT_TYPE_STRING);

	/* query_action */
	g_action_group_query_action (G_ACTION_GROUP (group), "one",
				     &enabled[0], &param_type[0], &state_type[0], &state_hint[0], &state[0]);
	g_action_group_query_action (G_ACTION_GROUP (muxer), "first.one",
				     &enabled[1], &param_type[1], &state_type[1], &state_hint[1], &state[1]);
	EXPECT_EQ (enabled[0], enabled[1]);
	EXPECT_STREQ ((gchar *) param_type[0], (gchar *) param_type[1]);
	EXPECT_STREQ ((gchar *) state_type[0], (gchar *) state_type[1]);
	EXPECT_TRUE (g_variant_equal0 ((gconstpointer) state_hint[0], (gconstpointer) state_hint[1]));
	EXPECT_TRUE (g_variant_equal0 ((gconstpointer) state[0], (gconstpointer) state[1]));

	g_object_unref (action);
	g_object_unref (group);
	g_object_unref (muxer);
}

typedef struct {
	gboolean signal_ran;
	const gchar *name;
} TestSignalClosure;

static void
action_added (GActionGroup *group,
	      gchar *action_name,
	      gpointer user_data)
{
	TestSignalClosure *c = (TestSignalClosure *)user_data;
	EXPECT_STREQ (c->name, action_name);
	c->signal_ran = TRUE;
}

static void
action_enabled_changed (GActionGroup *group,
			gchar *action_name,
			gboolean enabled,
			gpointer user_data)
{
	TestSignalClosure *c = (TestSignalClosure *)user_data;
	EXPECT_EQ (enabled, FALSE);
	c->signal_ran = TRUE;
}

static void
action_state_changed (GActionGroup *group,
		      gchar *action_name,
		      GVariant *value,
		      gpointer user_data)
{
	TestSignalClosure *c = (TestSignalClosure *)user_data;
	EXPECT_STREQ (g_variant_get_string (value, NULL), "off");
	c->signal_ran = TRUE;
}

static void
action_removed (GActionGroup *group,
		gchar *action_name,
		gpointer user_data)
{
	TestSignalClosure *c = (TestSignalClosure *)user_data;
	EXPECT_STREQ (c->name, action_name);
	c->signal_ran = TRUE;
}

TEST(GActionMuxerTest, Signals) {
	GSimpleActionGroup *group;
	GSimpleAction *action;
	GActionMuxer *muxer;
	TestSignalClosure closure;

	group = g_simple_action_group_new ();

	action = g_simple_action_new ("one", G_VARIANT_TYPE_STRING);
	g_action_map_add_action (G_ACTION_MAP(group), G_ACTION (action));
	g_object_unref (action);

	muxer = g_action_muxer_new ();

	g_signal_connect (muxer, "action-added",
			  G_CALLBACK (action_added), (gpointer) &closure);
	g_signal_connect (muxer, "action-enabled-changed",
			  G_CALLBACK (action_enabled_changed), (gpointer) &closure);
	g_signal_connect (muxer, "action-state-changed",
			  G_CALLBACK (action_state_changed), (gpointer) &closure);
	g_signal_connect (muxer, "action-removed",
			  G_CALLBACK (action_removed), (gpointer) &closure);

	/* add the group with "one" action and check whether the signal is emitted */
	closure.signal_ran = FALSE;
	closure.name = "first.one";
	g_action_muxer_insert (muxer, "first", G_ACTION_GROUP (group));
	EXPECT_TRUE (closure.signal_ran);

	/* add a second action after the group was added to the muxer */
	closure.signal_ran = FALSE;
	closure.name = "first.two";
	action = g_simple_action_new_stateful ("two", G_VARIANT_TYPE_STRING,
					       g_variant_new_string ("on"));
	g_action_map_add_action (G_ACTION_MAP(group), G_ACTION (action));
	EXPECT_TRUE (closure.signal_ran);

	/* disable the action */
	closure.signal_ran = FALSE;
	g_simple_action_set_enabled (action, FALSE);
	EXPECT_TRUE (closure.signal_ran);

	/* change its state */
	closure.signal_ran = FALSE;
	g_simple_action_set_state (action, g_variant_new_string ("off"));
	EXPECT_TRUE (closure.signal_ran);
	g_object_unref (action);

	/* remove the first action */
	closure.signal_ran = FALSE;
	closure.name = "first.one";
	g_action_map_remove_action (G_ACTION_MAP(group), "one");
	EXPECT_TRUE (closure.signal_ran);

	/* remove the whole group, should be notified about "first.two" */
	closure.signal_ran = FALSE;
	closure.name = "first.two";
	g_action_muxer_remove (muxer, "first");
	EXPECT_TRUE (closure.signal_ran);

	g_object_unref (group);
	g_object_unref (muxer);
}

static void
action_activated (GSimpleAction *simple,
		  GVariant      *parameter,
		  gpointer       user_data)
{
	gboolean *signal_ran = (gboolean *)user_data;

	EXPECT_STREQ (g_variant_get_string (parameter, NULL), "value");
	*signal_ran = TRUE;
}

static void
action_change_state (GSimpleAction *simple,
		     GVariant      *value,
		     gpointer       user_data)
{
	gboolean *signal_ran = (gboolean *)user_data;

	EXPECT_STREQ (g_variant_get_string (value, NULL), "off");
	*signal_ran = TRUE;
}

TEST(GActionMuxerTest, ActivateAction) {
	GSimpleActionGroup *group;
	GSimpleAction *action;
	GActionMuxer *muxer;
	gboolean signal_ran;

	group = g_simple_action_group_new ();

	action = g_simple_action_new ("one", G_VARIANT_TYPE_STRING);
	g_action_map_add_action (G_ACTION_MAP(group), G_ACTION (action));
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_activated), (gpointer) &signal_ran);
	g_object_unref (action);

	action = g_simple_action_new_stateful ("two", NULL,
					       g_variant_new_string ("on"));
	g_action_map_add_action (G_ACTION_MAP(group), G_ACTION (action));
	g_signal_connect (action, "change-state",
			  G_CALLBACK (action_change_state), (gpointer) &signal_ran);
	g_object_unref (action);

	muxer = g_action_muxer_new ();
	g_action_muxer_insert (muxer, "first", G_ACTION_GROUP (group));

	signal_ran = FALSE;
	g_action_group_activate_action (G_ACTION_GROUP (muxer), "first.one",
					g_variant_new_string  ("value"));
	EXPECT_TRUE (signal_ran);

	signal_ran = FALSE;
	g_action_group_change_action_state (G_ACTION_GROUP (muxer), "first.two",
					    g_variant_new_string  ("off"));
	EXPECT_TRUE (signal_ran);

	g_object_unref (group);
	g_object_unref (muxer);
}
