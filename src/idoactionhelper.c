/*
* Copyright 2013 Canonical Ltd.
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
*/

#include "idoactionhelper.h"

typedef GObjectClass IdoActionHelperClass;

struct _IdoActionHelper
{
  GObject parent;

  GtkWidget *widget;
  GActionGroup *actions;
  gchar *action_name;
  GVariant *action_target;
  guint idle_source_id;
};

G_DEFINE_TYPE (IdoActionHelper, ido_action_helper, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_WIDGET,
  PROP_ACTION_GROUP,
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  NUM_PROPERTIES
};

enum
{
  ACTION_STATE_CHANGED,
  NUM_SIGNALS
};

static GParamSpec *properties[NUM_PROPERTIES];
static guint signals[NUM_SIGNALS];

static void
ido_action_helper_action_added (GActionGroup *actions,
                                const gchar  *action_name,
                                gpointer      user_data)
{
  IdoActionHelper *helper = user_data;
  gboolean enabled;
  GVariant *state;

  if (!g_str_equal (action_name, helper->action_name))
    return;

  if (g_action_group_query_action (actions, action_name,
                                   &enabled, NULL, NULL, NULL, &state))
    {
      gtk_widget_set_sensitive (helper->widget, enabled);

      if (state)
        {
          g_signal_emit (helper, signals[ACTION_STATE_CHANGED], 0, state);
          g_variant_unref (state);
        }
    }
  else
    {
      gtk_widget_set_sensitive (helper->widget, FALSE);
    }
}

static void
ido_action_helper_action_removed (GActionGroup *action_group,
                                  gchar        *action_name,
                                  gpointer      user_data)
{
  IdoActionHelper *helper = user_data;

  if (g_str_equal (action_name, helper->action_name))
    gtk_widget_set_sensitive (helper->widget, FALSE);
}

static void
ido_action_helper_action_enabled_changed (GActionGroup *action_group,
                                          gchar        *action_name,
                                          gboolean      enabled,
                                          gpointer      user_data)
{
  IdoActionHelper *helper = user_data;

  if (g_str_equal (action_name, helper->action_name))
    gtk_widget_set_sensitive (helper->widget, enabled);
}

static void
ido_action_helper_action_state_changed (GActionGroup *action_group,
                                        gchar        *action_name,
                                        GVariant     *value,
                                        gpointer      user_data)
{
  IdoActionHelper *helper = user_data;

  if (g_str_equal (action_name, helper->action_name))
    g_signal_emit (helper, signals[ACTION_STATE_CHANGED], 0, value);
}

static gboolean
call_action_added (gpointer user_data)
{
  IdoActionHelper *helper = user_data;

  ido_action_helper_action_added (helper->actions, helper->action_name, helper);

  helper->idle_source_id = 0;
  return G_SOURCE_REMOVE;
}

static void
ido_action_helper_constructed (GObject *object)
{
  IdoActionHelper *helper = IDO_ACTION_HELPER (object);

  g_signal_connect (helper->actions, "action-added",
                    G_CALLBACK (ido_action_helper_action_added), helper);
  g_signal_connect (helper->actions, "action-removed",
                    G_CALLBACK (ido_action_helper_action_removed), helper);
  g_signal_connect (helper->actions, "action-enabled-changed",
                    G_CALLBACK (ido_action_helper_action_enabled_changed), helper);
  g_signal_connect (helper->actions, "action-state-changed",
                    G_CALLBACK (ido_action_helper_action_state_changed), helper);

  if (g_action_group_has_action (helper->actions, helper->action_name))
    {
      /* call action_added in an idle, so that we don't fire the
       * state-changed signal during construction (nobody could have
       * connected by then).
       */
      helper->idle_source_id = g_idle_add (call_action_added, helper);
    }

  G_OBJECT_CLASS (ido_action_helper_parent_class)->constructed (object);
}

static void
ido_action_helper_get_property (GObject    *object,
                                guint       id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  IdoActionHelper *helper = IDO_ACTION_HELPER (object);

  switch (id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, helper->widget);
      break;

    case PROP_ACTION_GROUP:
      g_value_set_object (value, helper->actions);
      break;

    case PROP_ACTION_NAME:
      g_value_set_string (value, helper->action_name);
      break;

    case PROP_ACTION_TARGET:
      g_value_set_variant (value, helper->action_target);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
ido_action_helper_set_property (GObject      *object,
                                guint         id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  IdoActionHelper *helper = IDO_ACTION_HELPER (object);

  switch (id)
    {
    case PROP_WIDGET: /* construct-only */
      helper->widget = g_value_dup_object (value);
      break;

    case PROP_ACTION_GROUP: /* construct-only */
      helper->actions = g_value_dup_object (value);
      break;

    case PROP_ACTION_NAME: /* construct-only */
      helper->action_name = g_value_dup_string (value);
      break;

    case PROP_ACTION_TARGET: /* construct-only */
      helper->action_target = g_value_dup_variant (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
ido_action_helper_finalize (GObject *object)
{
  IdoActionHelper *helper = IDO_ACTION_HELPER (object);

  if (helper->idle_source_id)
    g_source_remove (helper->idle_source_id);

  g_object_unref (helper->widget);

  g_signal_handlers_disconnect_by_data (helper->actions, helper);
  g_object_unref (helper->actions);

  g_free (helper->action_name);

  if (helper->action_target)
    g_variant_unref (helper->action_target);

  G_OBJECT_CLASS (ido_action_helper_parent_class)->finalize (object);
}

static void
ido_action_helper_class_init (IdoActionHelperClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = ido_action_helper_constructed;
  object_class->get_property = ido_action_helper_get_property;
  object_class->set_property = ido_action_helper_set_property;
  object_class->finalize = ido_action_helper_finalize;

  /**
   * IdoActionHelper::action-state-changed:
   * @helper: the #IdoActionHelper watching the action
   * @state: the new state of the action
   *
   * Emitted when the widget must be updated from the action's state,
   * which happens every time the action appears in the group and when
   * the action changes its state.
   */
  signals[ACTION_STATE_CHANGED] = g_signal_new ("action-state-changed",
                                                IDO_TYPE_ACTION_HELPER,
                                                G_SIGNAL_RUN_FIRST,
                                                0, NULL, NULL,
                                                g_cclosure_marshal_VOID__VARIANT,
                                                G_TYPE_NONE, 1, G_TYPE_VARIANT);

  /**
   * IdoActionHelper:widget:
   *
   * The widget that is associated with this action helper. The action
   * helper updates the widget's "sensitive" property to reflect whether
   * the action #IdoActionHelper:action-name exists in
   * #IdoActionHelper:action-group.
   */
  properties[PROP_WIDGET] = g_param_spec_object ("widget", "", "",
                                                 GTK_TYPE_WIDGET,
                                                 G_PARAM_CONSTRUCT_ONLY |
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_STRINGS);

  /**
   * IdoActionHelper:action-group:
   *
   * The action group that eventually contains the action that
   * #IdoActionHelper:widget should be bound to.
   */
  properties[PROP_ACTION_GROUP] = g_param_spec_object ("action-group", "", "",
                                                       G_TYPE_ACTION_GROUP,
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS);

  /**
   * IdoActionHelper:action-name:
   *
   * The name of the action in #IdoActionHelper:action-group that
   * should be bound to #IdoActionHelper:widget
   */
  properties[PROP_ACTION_NAME] = g_param_spec_string ("action-name", "", "", NULL,
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS);

  /**
   * IdoActionHelper:action-target:
   *
   * The target of #IdoActionHelper:widget. ido_action_helper_activate()
   * passes the target as parameter when activating the action.
   *
   * The handler of #IdoActionHelper:action-state-changed is responsible
   * for comparing this target with the action's state and updating the
   * #IdoActionHelper:widget appropriately.
   */
  properties[PROP_ACTION_TARGET] = g_param_spec_variant ("action-target", "", "",
                                                         G_VARIANT_TYPE_ANY, NULL,
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
ido_action_helper_init (IdoActionHelper *helper)
{
}

/**
 * ido_action_helper_new:
 * @widget: a #GtkWidget
 * @action_group: a #GActionGroup
 * @action_name: the name of an action in @action_group
 * @target: the target of the action
 *
 * Creates a new #IdoActionHelper. This helper ties @widget to an action
 * (and a target), and performs some common tasks:
 *
 * @widget will be set to insensitive whenever @action_group does not
 * contain an action with the name @action_name, or the action with that
 * name is disabled.
 *
 * Also, the helper emits the "action-state-changed" signal whenever the
 * widget must be updated from the action's state. This includes once
 * when the action was added, and every time the action changes its
 * state.
 *
 * Returns: (transfer full): a new #IdoActionHelper
 */
IdoActionHelper *
ido_action_helper_new (GtkWidget    *widget,
                       GActionGroup *action_group,
                       const gchar  *action_name,
                       GVariant     *target)
{
  g_return_val_if_fail (widget != NULL, NULL);
  g_return_val_if_fail (action_group != NULL, NULL);
  g_return_val_if_fail (action_name != NULL, NULL);

  return g_object_new (IDO_TYPE_ACTION_HELPER,
                       "widget", widget,
                       "action-group", action_group,
                       "action-name", action_name,
                       "action-target", target,
                       NULL);
}

/**
 * ido_action_helper_get_widget:
 * @helper: an #IdoActionHelper
 *
 * Returns: (transfer none): the #GtkWidget associated with @helper
 */
GtkWidget *
ido_action_helper_get_widget (IdoActionHelper *helper)
{
  g_return_val_if_fail (IDO_IS_ACTION_HELPER (helper), NULL);

  return helper->widget;
}

/**
 * ido_action_helper_get_action_target:
 * @helper: an #IdoActionHelper
 *
 * Returns: (transfer none): the action target that was set in
 * ido_action_helper_new() as a #GVariant
 */
GVariant *
ido_action_helper_get_action_target (IdoActionHelper *helper)
{
  g_return_val_if_fail (IDO_IS_ACTION_HELPER (helper), NULL);

  return helper->action_target;
}

/**
 * ido_action_helper_activate:
 * @helper: an #IdoActionHelper
 *
 * Activates the action that is associated with this helper.
 */
void
ido_action_helper_activate (IdoActionHelper *helper)
{
  g_return_if_fail (IDO_IS_ACTION_HELPER (helper));

  if (helper->actions && helper->action_name)
    g_action_group_activate_action (helper->actions, helper->action_name, helper->action_target);
}

/**
 * ido_action_helper_activate_with_parameter:
 * @helper: an #IdoActionHelper
 * @parameter: a #GVariant containing the parameter
 *
 * Activates the action that is associated with this helper passing
 * @parameter instead the "target" associated with the menu item this
 * helper is bound to.
 */
void
ido_action_helper_activate_with_parameter (IdoActionHelper *helper,
                                           GVariant        *parameter)
{
  g_return_if_fail (IDO_IS_ACTION_HELPER (helper));
  g_return_if_fail (parameter != NULL);

  g_variant_ref_sink (parameter);

  if (helper->actions && helper->action_name)
    g_action_group_activate_action (helper->actions, helper->action_name, parameter);

  g_variant_unref (parameter);
}

/**
 * ido_action_helper_change_action_state:
 * @helper: an #IdoActionHelper
 * @state: the proposed new state of the action
 *
 * Requests changing the state of the action that is associated with
 * @helper to @state.
 *
 * If @state is floating, it is consumed.
 */
void
ido_action_helper_change_action_state (IdoActionHelper *helper,
                                       GVariant        *state)
{
  g_return_if_fail (IDO_IS_ACTION_HELPER (helper));
  g_return_if_fail (state != NULL);

  g_variant_ref_sink (state);

  if (helper->actions && helper->action_name)
    g_action_group_change_action_state (helper->actions, helper->action_name, state);

  g_variant_unref (state);
}
