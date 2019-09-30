/*
 * Copyright 2012 Canonical Ltd.
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
 *
 * Authors:
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 *     Ryan Lortie <desrt@desrt.ca>
 */

#include "gactionmuxer.h"

#include <string.h>

/*
 * SECTION:gactionmuxer
 * @short_description: Aggregate several action groups
 *
 * #GActionMuxer is a #GActionGroup that is capable of containing other
 * #GActionGroup instances.
 *
 * The typical use is aggregating all of the actions applicable to a
 * particular context into a single action group, with namespacing.
 *
 * Consider the case of two action groups -- one containing actions
 * applicable to an entire application (such as 'quit') and one
 * containing actions applicable to a particular window in the
 * application (such as 'fullscreen').
 *
 * In this case, each of these action groups could be added to a
 * #GActionMuxer with the prefixes "app" and "win", respectively.  This
 * would expose the actions as "app.quit" and "win.fullscreen" on the
 * #GActionGroup interface presented by the #GActionMuxer.
 *
 * Activations and state change requests on the #GActionMuxer are wired
 * through to the underlying action group in the expected way.
 */

typedef GObjectClass GActionMuxerClass;

struct _GActionMuxer
{
  GObject parent;
  GActionGroup *global_actions;
  GHashTable *groups;  /* prefix -> subgroup */
  GHashTable *reverse; /* subgroup -> prefix */
};


static void     g_action_muxer_group_init             (GActionGroupInterface *iface);
static void     g_action_muxer_dispose                (GObject *object);
static void     g_action_muxer_finalize               (GObject *object);
static void     g_action_muxer_disconnect_group       (GActionMuxer *muxer,
                                                       GActionGroup *subgroup);
static gchar ** g_action_muxer_list_actions           (GActionGroup *group);
static void     g_action_muxer_activate_action        (GActionGroup *group,
                                                       const gchar  *action_name,
                                                       GVariant     *parameter);
static void     g_action_muxer_change_action_state    (GActionGroup *group,
                                                       const gchar  *action_name,
                                                       GVariant     *value);
static gboolean g_action_muxer_query_action           (GActionGroup        *group,
                                                       const gchar         *action_name,
                                                       gboolean            *enabled,
                                                       const GVariantType **parameter_type,
                                                       const GVariantType **state_type,
                                                       GVariant           **state_hint,
                                                       GVariant           **state);
static void     g_action_muxer_action_added           (GActionGroup *group,
                                                       gchar        *action_name,
                                                       gpointer      user_data);
static void     g_action_muxer_action_removed         (GActionGroup *group,
                                                       gchar        *action_name,
                                                       gpointer      user_data);
static void     g_action_muxer_action_state_changed   (GActionGroup *group,
                                                       gchar        *action_name,
                                                       GVariant     *value,
                                                       gpointer      user_data);
static void     g_action_muxer_action_enabled_changed (GActionGroup *group,
                                                       gchar        *action_name,
                                                       gboolean      enabled,
                                                       gpointer      user_data);

G_DEFINE_TYPE_WITH_CODE (GActionMuxer, g_action_muxer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_action_muxer_group_init));


static void
g_action_muxer_class_init (GObjectClass *klass)
{
  klass->dispose = g_action_muxer_dispose;
  klass->finalize = g_action_muxer_finalize;
}

static void
g_action_muxer_init (GActionMuxer *muxer)
{
  muxer->global_actions = NULL;
  muxer->groups = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  muxer->reverse = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
g_action_muxer_group_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_action_muxer_list_actions;
  iface->activate_action = g_action_muxer_activate_action;
  iface->change_action_state = g_action_muxer_change_action_state;
  iface->query_action = g_action_muxer_query_action;
}

static void
g_action_muxer_dispose (GObject *object)
{
  GActionMuxer *muxer = G_ACTION_MUXER (object);
  GHashTableIter it;
  GActionGroup *subgroup;

  if (muxer->global_actions)
    {
      g_action_muxer_disconnect_group (muxer, muxer->global_actions);
      g_clear_object (&muxer->global_actions);
    }

  g_hash_table_iter_init (&it, muxer->groups);
  while (g_hash_table_iter_next (&it, NULL, (gpointer *) &subgroup))
    g_action_muxer_disconnect_group (muxer, subgroup);

  g_hash_table_remove_all (muxer->groups);
  g_hash_table_remove_all (muxer->reverse);
}

static void
g_action_muxer_finalize (GObject *object)
{
  GActionMuxer *muxer = G_ACTION_MUXER (object);

  g_hash_table_unref (muxer->groups);
  g_hash_table_unref (muxer->reverse);

  G_OBJECT_CLASS (g_action_muxer_parent_class)->finalize (object);
}

static GActionGroup *
g_action_muxer_lookup_group (GActionMuxer *muxer,
                             const gchar  *full_name,
                             const gchar **action_name)
{
  const gchar *sep;
  GActionGroup *group;

  sep = strchr (full_name, '.');

  if (sep)
    {
      gchar *prefix;
      prefix = g_strndup (full_name, sep - full_name);
      group = g_hash_table_lookup (muxer->groups, prefix);
      g_free (prefix);
      if (action_name)
        *action_name = sep + 1;
    }
  else
    {
      group = muxer->global_actions;
      if (action_name)
        *action_name = full_name;
    }

  return group;
}

static gchar *
g_action_muxer_lookup_full_name (GActionMuxer *muxer,
                                 GActionGroup *subgroup,
                                 const gchar  *action_name)
{
  gpointer prefix;

  if (subgroup == muxer->global_actions)
    return g_strdup (action_name);

  if (g_hash_table_lookup_extended (muxer->reverse, subgroup, NULL, &prefix))
    return g_strdup_printf ("%s.%s", (gchar *) prefix, action_name);

  return NULL;
}

static void
g_action_muxer_disconnect_group (GActionMuxer *muxer,
                                 GActionGroup *subgroup)
{
  gchar **actions;
  gchar **action;

  actions = g_action_group_list_actions (subgroup);
  for (action = actions; *action; action++)
    g_action_muxer_action_removed (subgroup, *action, muxer);
  g_strfreev (actions);

  g_signal_handlers_disconnect_by_func (subgroup, g_action_muxer_action_added, muxer);
  g_signal_handlers_disconnect_by_func (subgroup, g_action_muxer_action_removed, muxer);
  g_signal_handlers_disconnect_by_func (subgroup, g_action_muxer_action_enabled_changed, muxer);
  g_signal_handlers_disconnect_by_func (subgroup, g_action_muxer_action_state_changed, muxer);
}

static gchar **
g_action_muxer_list_actions (GActionGroup *group)
{
  GActionMuxer *muxer = G_ACTION_MUXER (group);
  GHashTableIter it;
  GArray *all_actions;
  gchar *prefix;
  GActionGroup *subgroup;
  gchar **actions;
  gchar **a;

  all_actions = g_array_sized_new (TRUE, FALSE, sizeof (gchar *), 8);

  if (muxer->global_actions)
    {
      actions = g_action_group_list_actions (muxer->global_actions);
      for (a = actions; *a; a++)
        {
          gchar *name = g_strdup (*a);
          g_array_append_val (all_actions, name);
        }
      g_strfreev (actions);
    }

  g_hash_table_iter_init (&it, muxer->groups);
  while (g_hash_table_iter_next (&it, (gpointer *) &prefix, (gpointer *) &subgroup))
    {
      actions = g_action_group_list_actions (subgroup);
      for (a = actions; *a; a++)
        {
          gchar *full_name = g_strdup_printf ("%s.%s", prefix, *a);
          g_array_append_val (all_actions, full_name);
        }
      g_strfreev (actions);
    }

  return (gchar **) g_array_free (all_actions, FALSE);
}

static void
g_action_muxer_activate_action (GActionGroup  *group,
                                const gchar   *action_name,
                                GVariant      *parameter)
{
  GActionMuxer *muxer = G_ACTION_MUXER (group);
  GActionGroup *subgroup;
  const gchar *action;

  g_return_if_fail (action_name != NULL);

  subgroup = g_action_muxer_lookup_group (muxer, action_name, &action);

  if (subgroup)
    g_action_group_activate_action (subgroup, action, parameter);
}

static void
g_action_muxer_change_action_state (GActionGroup  *group,
                                    const gchar   *action_name,
                                    GVariant      *value)
{
  GActionMuxer *muxer = G_ACTION_MUXER (group);
  GActionGroup *subgroup;
  const gchar *action;

  g_return_if_fail (action_name != NULL);

  subgroup = g_action_muxer_lookup_group (muxer, action_name, &action);

  if (subgroup)
    g_action_group_change_action_state (subgroup, action, value);
}

static gboolean
g_action_muxer_query_action (GActionGroup        *group,
                             const gchar         *action_name,
                             gboolean            *enabled,
                             const GVariantType **parameter_type,
                             const GVariantType **state_type,
                             GVariant           **state_hint,
                             GVariant           **state)
{
  GActionMuxer *muxer = G_ACTION_MUXER (group);
  GActionGroup *subgroup;
  const gchar *action;

  g_return_val_if_fail (action_name != NULL, FALSE);

  subgroup = g_action_muxer_lookup_group (muxer, action_name, &action);

  if (!subgroup)
    return FALSE;

  return g_action_group_query_action (subgroup, action, enabled, parameter_type,
                                      state_type, state_hint, state);
}

static void
g_action_muxer_action_added (GActionGroup *group,
                             gchar        *action_name,
                             gpointer      user_data)
{
  GActionMuxer *muxer = user_data;
  gchar *full_name;

  full_name = g_action_muxer_lookup_full_name (muxer, group, action_name);

  if (full_name)
    {
      g_action_group_action_added (G_ACTION_GROUP (muxer), full_name);
      g_free (full_name);
    }
}

static void
g_action_muxer_action_removed (GActionGroup *group,
                               gchar        *action_name,
                               gpointer      user_data)
{
  GActionMuxer *muxer = user_data;
  gchar *full_name;

  full_name = g_action_muxer_lookup_full_name (muxer, group, action_name);

  if (full_name)
    {
      g_action_group_action_removed (G_ACTION_GROUP (muxer), full_name);
      g_free (full_name);
    }
}

static void
g_action_muxer_action_state_changed (GActionGroup *group,
                                     gchar        *action_name,
                                     GVariant     *value,
                                     gpointer      user_data)
{
  GActionMuxer *muxer = user_data;
  gchar *full_name;

  full_name = g_action_muxer_lookup_full_name (muxer, group, action_name);

  if (full_name)
    {
      g_action_group_action_state_changed (G_ACTION_GROUP (muxer), full_name, value);
      g_free (full_name);
    }
}

static void
g_action_muxer_action_enabled_changed (GActionGroup *group,
                                       gchar        *action_name,
                                       gboolean      enabled,
                                       gpointer      user_data)
{
  GActionMuxer *muxer = user_data;
  gchar *full_name;

  full_name = g_action_muxer_lookup_full_name (muxer, group, action_name);

  if (full_name)
    {
      g_action_group_action_enabled_changed (G_ACTION_GROUP (muxer), full_name, enabled);
      g_free (full_name);
    }
}

/*
 * g_action_muxer_new:
 *
 * Creates a new #GActionMuxer.
 */
GActionMuxer *
g_action_muxer_new (void)
{
  return g_object_new (G_TYPE_ACTION_MUXER, NULL);
}

/*
 * g_action_muxer_insert:
 * @muxer: a #GActionMuxer
 * @prefix: (allow-none): the prefix string for the action group, or NULL
 * @group: (allow-none): a #GActionGroup, or NULL
 *
 * Adds the actions in @group to the list of actions provided by @muxer.
 * @prefix is prefixed to each action name, such that for each action
 * <varname>x</varname> in @group, there is an equivalent action
 * @prefix<literal>.</literal><varname>x</varname> in @muxer.
 *
 * For example, if @prefix is "<literal>app</literal>" and @group contains an
 * action called "<literal>quit</literal>", then @muxer will now contain an
 * action called "<literal>app.quit</literal>".
 *
 * If @prefix is <literal>NULL</literal>, the actions in @group will be added
 * to @muxer without prefix.
 *
 * If @group is <literal>NULL</literal>, this function has the same effect as
 * calling g_action_muxer_remove() with @prefix.
 *
 * There may only be one group per prefix (including the
 * <literal>NULL</literal>-prefix).  If a group has been added with @prefix in
 * a previous call to this function, it will be removed.
 *
 * @prefix must not contain a dot ('.').
 */
void
g_action_muxer_insert (GActionMuxer *muxer,
                       const gchar  *prefix,
                       GActionGroup *group)
{
  gchar *prefix_copy;
  gchar **actions;
  gchar **action;

  g_return_if_fail (G_IS_ACTION_MUXER (muxer));
  g_return_if_fail (group == NULL || G_IS_ACTION_GROUP (group));

  g_action_muxer_remove (muxer, prefix);

  if (group == NULL)
    return;

  if (prefix)
    {
      prefix_copy = g_strdup (prefix);
      g_hash_table_insert (muxer->groups, prefix_copy, g_object_ref (group));
      g_hash_table_insert (muxer->reverse, group, prefix_copy);
    }
  else
    muxer->global_actions = g_object_ref (group);

  actions = g_action_group_list_actions (group);
  for (action = actions; *action; action++)
    g_action_muxer_action_added (group, *action, muxer);
  g_strfreev (actions);

  g_signal_connect (group, "action-added", G_CALLBACK (g_action_muxer_action_added), muxer);
  g_signal_connect (group, "action-removed", G_CALLBACK (g_action_muxer_action_removed), muxer);
  g_signal_connect (group, "action-enabled-changed", G_CALLBACK (g_action_muxer_action_enabled_changed), muxer);
  g_signal_connect (group, "action-state-changed", G_CALLBACK (g_action_muxer_action_state_changed), muxer);
}

/*
 * g_action_muxer_remove:
 * @muxer: a #GActionMuxer
 * @prefix: (allow-none): the prefix of the action group to remove, or NULL
 *
 * Removes a #GActionGroup from the #GActionMuxer.
 */
void
g_action_muxer_remove (GActionMuxer *muxer,
                       const gchar  *prefix)
{
  GActionGroup *subgroup;

  g_return_if_fail (G_IS_ACTION_MUXER (muxer));

  subgroup = prefix ? g_hash_table_lookup (muxer->groups, prefix) : muxer->global_actions;
  if (!subgroup)
    return;

  g_action_muxer_disconnect_group (muxer, subgroup);

  if (prefix)
    {
      g_hash_table_remove (muxer->groups, prefix);
      g_hash_table_remove (muxer->reverse, subgroup);
    }
  else
    g_clear_object (&muxer->global_actions);
}

GActionGroup *
g_action_muxer_get_group (GActionMuxer *muxer,
                          const gchar  *prefix)
{
  g_return_val_if_fail (G_IS_ACTION_MUXER (muxer), NULL);

  return prefix ? g_hash_table_lookup (muxer->groups, prefix) : muxer->global_actions;
}
