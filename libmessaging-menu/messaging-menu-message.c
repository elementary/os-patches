/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as
 * published by the Free Software Foundation.
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

#include "messaging-menu-message.h"

typedef GObjectClass MessagingMenuMessageClass;

struct _MessagingMenuMessage
{
  GObject parent;

  gchar *id;
  GIcon *icon;
  gchar *title;
  gchar *subtitle;
  gchar *body;
  gint64 time;
  gboolean draws_attention;

  GSList *actions;
};

G_DEFINE_TYPE (MessagingMenuMessage, messaging_menu_message, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_ID,
  PROP_ICON,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_BODY,
  PROP_TIME,
  PROP_DRAWS_ATTENTION,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

typedef struct
{
  gchar *id;
  gchar *label;
  GVariantType *parameter_type;
  GVariant *parameter_hint;
} Action;

static void
action_free (gpointer data)
{
  Action *action = data;

  g_free (action->id);
  g_free (action->label);

  if (action->parameter_type)
    g_variant_type_free (action->parameter_type);

  if (action->parameter_hint)
    g_variant_unref (action->parameter_hint);

  g_slice_free (Action, action);
}

static void
messaging_menu_message_dispose (GObject *object)
{
  MessagingMenuMessage *msg = MESSAGING_MENU_MESSAGE (object);

  g_clear_object (&msg->icon);

  G_OBJECT_CLASS (messaging_menu_message_parent_class)->dispose (object);
}

static void
messaging_menu_message_finalize (GObject *object)
{
  MessagingMenuMessage *msg = MESSAGING_MENU_MESSAGE (object);

  g_free (msg->id);
  g_free (msg->title);
  g_free (msg->subtitle);
  g_free (msg->body);

  g_slist_free_full (msg->actions, action_free);
  msg->actions = NULL;

  G_OBJECT_CLASS (messaging_menu_message_parent_class)->finalize (object);
}

static void
messaging_menu_message_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  MessagingMenuMessage *msg = MESSAGING_MENU_MESSAGE (object);

  switch (property_id)
    {
    case PROP_ID:
      g_value_set_string (value, msg->id);
      break;

    case PROP_ICON:
      g_value_set_object (value, msg->icon);
      break;

    case PROP_TITLE:
      g_value_set_string (value, msg->title);
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, msg->subtitle);
      break;

    case PROP_BODY:
      g_value_set_string (value, msg->body);

    case PROP_TIME:
      g_value_set_int64 (value, msg->time);
      break;

    case PROP_DRAWS_ATTENTION:
      g_value_set_boolean (value, msg->draws_attention);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
messaging_menu_message_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  MessagingMenuMessage *msg = MESSAGING_MENU_MESSAGE (object);

  switch (property_id)
    {
    case PROP_ID:
      msg->id = g_value_dup_string (value);
      break;

    case PROP_ICON:
      msg->icon = g_value_dup_object (value);
      break;

    case PROP_TITLE:
      msg->title = g_value_dup_string (value);
      break;

    case PROP_SUBTITLE:
      msg->subtitle = g_value_dup_string (value);
      break;

    case PROP_BODY:
      msg->body = g_value_dup_string (value);

    case PROP_TIME:
      msg->time = g_value_get_int64 (value);
      break;

    case PROP_DRAWS_ATTENTION:
      messaging_menu_message_set_draws_attention (msg, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
messaging_menu_message_class_init (MessagingMenuMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = messaging_menu_message_dispose;
  object_class->finalize = messaging_menu_message_finalize;
  object_class->get_property = messaging_menu_message_get_property;
  object_class->set_property = messaging_menu_message_set_property;

  properties[PROP_ID] = g_param_spec_string ("id", "Id",
                                             "Unique id of the message",
                                             NULL,
                                             G_PARAM_CONSTRUCT_ONLY |
                                             G_PARAM_READWRITE |
                                             G_PARAM_STATIC_STRINGS);

  properties[PROP_ICON] = g_param_spec_object ("icon", "Icon",
                                               "Icon of the message",
                                               G_TYPE_ICON,
                                               G_PARAM_CONSTRUCT_ONLY |
                                               G_PARAM_READWRITE |
                                               G_PARAM_STATIC_STRINGS);

  properties[PROP_TITLE] = g_param_spec_string ("title", "Title",
                                                "Title of the message",
                                                NULL,
                                                G_PARAM_CONSTRUCT_ONLY |
                                                G_PARAM_READWRITE |
                                                G_PARAM_STATIC_STRINGS);

  properties[PROP_SUBTITLE] = g_param_spec_string ("subtitle", "Subtitle",
                                                   "Subtitle of the message",
                                                   NULL,
                                                   G_PARAM_CONSTRUCT_ONLY |
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_STATIC_STRINGS);

  properties[PROP_BODY] = g_param_spec_string ("body", "Body",
                                               "First lines of the body of the message",
                                               NULL,
                                               G_PARAM_CONSTRUCT_ONLY |
                                               G_PARAM_READWRITE |
                                               G_PARAM_STATIC_STRINGS);

  properties[PROP_TIME] = g_param_spec_int64 ("time", "Time",
                                              "Time the message was sent, in microseconds", 0, G_MAXINT64, 0,
                                               G_PARAM_CONSTRUCT_ONLY |
                                               G_PARAM_READWRITE |
                                               G_PARAM_STATIC_STRINGS);

  properties[PROP_DRAWS_ATTENTION] = g_param_spec_boolean ("draws-attention", "Draws attention",
                                                           "Whether the message should draw attention",
                                                           TRUE,
                                                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (klass, NUM_PROPERTIES, properties);

  /**
   * MessagingMenuMessage::activate:
   * @msg: the #MessagingMenuMessage
   * @action: (allow-none): the id of activated action, or %NULL
   * @parameter: (allow-none): activation parameter, or %NULL
   *
   * Emitted when the user has activated the message.  The message is
   * immediately removed from the application's menu, handlers of this
   * signal do not need to call messaging_menu_app_remove_message().
   */
  g_signal_new ("activate",
                MESSAGING_MENU_TYPE_MESSAGE,
                G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                0,
                NULL, NULL,
                g_cclosure_marshal_generic,
                G_TYPE_NONE, 2,
                G_TYPE_STRING,
                G_TYPE_VARIANT);
}

static void
messaging_menu_message_init (MessagingMenuMessage *self)
{
  self->draws_attention = TRUE;
}

/**
 * messaging_menu_message_new:
 * @id: unique id of the message
 * @icon: (transfer full) (allow-none): a #GIcon representing the message
 * @title: the title of the message
 * @subtitle: (allow-none): the subtitle of the message
 * @body: (allow-none): the message body
 * @time: the time the message was received
 *
 * Creates a new #MessagingMenuMessage.
 *
 * Returns: (transfer full): a new #MessagingMenuMessage
 */
MessagingMenuMessage *
messaging_menu_message_new (const gchar *id,
                            GIcon       *icon,
                            const gchar *title,
                            const gchar *subtitle,
                            const gchar *body,
                            gint64       time)
{
  g_return_val_if_fail (id != NULL, NULL);
  g_return_val_if_fail (title != NULL, NULL);

  return g_object_new (MESSAGING_MENU_TYPE_MESSAGE,
                       "id", id,
                       "icon", icon,
                       "title", title,
                       "subtitle", subtitle,
                       "body", body,
                       "time", time,
                       NULL);
}

/**
 * messaging_menu_message_get_id:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: the unique id of @msg
 */
const gchar *
messaging_menu_message_get_id (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), NULL);

  return msg->id;
}

/**
 * messaging_menu_message_get_icon:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: (transfer none): the icon of @msg
 */
GIcon *
messaging_menu_message_get_icon (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), NULL);

  return msg->icon;
}

/**
 * messaging_menu_message_get_title:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: the title of @msg
 */
const gchar *
messaging_menu_message_get_title (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), NULL);

  return msg->title;
}

/**
 * messaging_menu_message_get_subtitle:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: the subtitle of @msg
 */
const gchar *
messaging_menu_message_get_subtitle (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), NULL);

  return msg->subtitle;
}

/**
 * messaging_menu_message_get_body:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: the body of @msg
 */
const gchar *
messaging_menu_message_get_body (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), NULL);

  return msg->body;
}

/**
 * messaging_menu_message_get_time:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: the time at which @msg was received
 */
gint64
messaging_menu_message_get_time (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), 0);

  return msg->time;
}

/**
 * messaging_menu_message_get_draws_attention:
 * @msg: a #MessagingMenuMessage
 *
 * Returns: whether @msg is drawing attention
 */
gboolean
messaging_menu_message_get_draws_attention  (MessagingMenuMessage *msg)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), FALSE);

  return msg->draws_attention;
}

/**
 * messaging_menu_message_set_draws_attention:
 * @msg: a #MessagingMenuMessage
 * @draws_attention: whether @msg should draw attention
 *
 * Sets whether @msg is drawing attention.
 */
void
messaging_menu_message_set_draws_attention  (MessagingMenuMessage *msg,
                                             gboolean              draws_attention)
{
  g_return_if_fail (MESSAGING_MENU_IS_MESSAGE (msg));

  msg->draws_attention = draws_attention;
  g_object_notify_by_pspec (G_OBJECT (msg), properties[PROP_DRAWS_ATTENTION]);
}

/**
 * messaging_menu_message_add_action:
 * @msg: a #MessagingMenuMessage
 * @id: unique id of the action
 * @label: (allow-none): label of the action
 * @parameter_type: (allow-none): a #GVariantType
 * @parameter_hint: (allow-none): a #GVariant suggesting a valid range
 * for parameters
 *
 * Adds an action with @id and @label to @message.  Actions are an
 * alternative way for users to activate a message.  Note that messages
 * can still be activated without an action.
 *
 * If @parameter_type is non-%NULL, the action is able to receive user
 * input in addition to simply activating the action.  Currently, only
 * string parameters are supported.
 *
 * A list of predefined parameters can be supplied as a #GVariant array
 * of @parameter_type in @parameter_hint.  If @parameter_hint is
 * floating, it will be consumed.
 *
 * It is recommended to add at most two actions to a message.
 */
void
messaging_menu_message_add_action (MessagingMenuMessage *msg,
                                   const gchar          *id,
                                   const gchar          *label,
                                   const GVariantType   *parameter_type,
                                   GVariant             *parameter_hint)
{
  Action *action;

  g_return_if_fail (MESSAGING_MENU_IS_MESSAGE (msg));
  g_return_if_fail (id != NULL);

  action = g_slice_new (Action);
  action->id = g_strdup (id);
  action->label = g_strdup (label);
  action->parameter_type = parameter_type ? g_variant_type_copy (parameter_type) : NULL;
  action->parameter_hint = parameter_hint ? g_variant_ref_sink (parameter_hint) : NULL;

  msg->actions = g_slist_append (msg->actions, action);
}

static GVariant *
action_to_variant (Action *action)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "name", g_variant_new_string (action->id));

  if (action->label)
    g_variant_builder_add (&builder, "{sv}", "label", g_variant_new_string (action->label));

  if (action->parameter_type)
    {
      gchar *type = g_variant_type_dup_string (action->parameter_type);
      g_variant_builder_add (&builder, "{sv}", "parameter-type", g_variant_new_signature (type));
      g_free (type);
    }

  if (action->parameter_hint)
    g_variant_builder_add (&builder, "{sv}", "parameter-hint", action->parameter_hint);

  return g_variant_builder_end (&builder);
}

/*<internal>
 * _messaging_menu_message_to_variant:
 * @msg: a #MessagingMenuMessage
 *
 * Serializes @msg to a #GVariant of the form (savsssxaa{sv}b):
 *
 *   id
 *   icon (fake-maybe)
 *   title
 *   subtitle
 *   body
 *   time
 *   array of action dictionaries
 *   draws_attention
 *
 * Returns: a new floating #GVariant instance
 */
GVariant *
_messaging_menu_message_to_variant (MessagingMenuMessage *msg)
{
  GVariantBuilder builder;
  GSList *it;
  GVariant *serialized_icon;
  GVariantBuilder icon_builder;

  g_return_val_if_fail (MESSAGING_MENU_IS_MESSAGE (msg), NULL);

  serialized_icon = msg->icon ? g_icon_serialize (msg->icon) : NULL;
  g_variant_builder_init (&icon_builder, G_VARIANT_TYPE ("av"));
  if (serialized_icon)
    {
      g_variant_builder_add (&icon_builder, "v", serialized_icon);
      g_variant_unref (serialized_icon);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(savsssxaa{sv}b)"));
  g_variant_builder_add (&builder, "s", msg->id);
  g_variant_builder_add (&builder, "av", &icon_builder);
  g_variant_builder_add (&builder, "s", msg->title ? msg->title : "");
  g_variant_builder_add (&builder, "s", msg->subtitle ? msg->subtitle : "");
  g_variant_builder_add (&builder, "s", msg->body ? msg->body : "");
  g_variant_builder_add (&builder, "x", msg->time);

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));
  for (it = msg->actions; it; it = it->next)
    g_variant_builder_add_value (&builder, action_to_variant (it->data));
  g_variant_builder_close (&builder);

  g_variant_builder_add (&builder, "b", msg->draws_attention);

  return g_variant_builder_end (&builder);
}
