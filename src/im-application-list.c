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
 */

#include "im-application-list.h"

#include "indicator-messages-application.h"
#include "gactionmuxer.h"
#include "indicator-desktop-shortcuts.h"

#include <gio/gdesktopappinfo.h>
#include <string.h>

#include "glib/gi18n.h"

typedef GObjectClass ImApplicationListClass;

struct _ImApplicationList
{
  GObject parent;

  GHashTable *applications;
  GActionMuxer *muxer;

  GSimpleActionGroup * globalactions;
  GSimpleAction * statusaction;

  GHashTable *app_status;
};

G_DEFINE_TYPE (ImApplicationList, im_application_list, G_TYPE_OBJECT);
G_DEFINE_QUARK (draws_attention, message_action_draws_attention);

enum
{
  SOURCE_ADDED,
  SOURCE_CHANGED,
  SOURCE_REMOVED,
  MESSAGE_ADDED,
  MESSAGE_REMOVED,
  APP_ADDED,
  APP_STOPPED,
  REMOVE_ALL,
  STATUS_SET,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

typedef struct
{
  ImApplicationList *list;
  GDesktopAppInfo *info;
  gchar *id;
  IndicatorMessagesApplication *proxy;
  GActionMuxer *muxer;
  GSimpleActionGroup *source_actions;
  GSimpleActionGroup *message_actions;
  GActionMuxer *message_sub_actions;
  GCancellable *cancellable;
  gboolean draws_attention;
  IndicatorDesktopShortcuts * shortcuts;
} Application;


/* Prototypes */
static void         status_activated           (GSimpleAction *    action,
                                                GVariant *         param,
                                                gpointer           user_data);

static void
application_free (gpointer data)
{
  Application *app = data;

  if (!app)
    return;

  g_object_unref (app->info);
  g_free (app->id);

  if (app->cancellable)
    {
      g_cancellable_cancel (app->cancellable);
      g_clear_object (&app->cancellable);
    }

  if (app->proxy)
    g_object_unref (app->proxy);

  if (app->muxer)
    {
      g_object_unref (app->muxer);
      g_object_unref (app->source_actions);
      g_object_unref (app->message_actions);
      g_object_unref (app->message_sub_actions);
    }

  g_clear_object (&app->shortcuts);

  g_slice_free (Application, app);
}

/* Check to see if we have actions by getting the full list of
   names and see if there is one.  Not exactly efficient :-/ */
static gboolean
_g_action_group_has_actions (GActionGroup * ag)
{
  gchar ** list = NULL;
  gboolean retval = FALSE;

  list = g_action_group_list_actions(ag);
  retval = (list[0] != NULL);
  g_strfreev(list);

  return retval;
}

/* Check to see if either of our action groups has any actions, if
   so return TRUE so we get chosen! */
static gboolean
application_has_items (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
  Application *app = value;

  return _g_action_group_has_actions(G_ACTION_GROUP(app->source_actions)) ||
    _g_action_group_has_actions(G_ACTION_GROUP(app->message_actions));
}

static gboolean
application_draws_attention (gpointer key,
                             gpointer value,
                             gpointer user_data)
{
  Application *app = value;

  return app->draws_attention;
}

static void
im_application_list_update_root_action (ImApplicationList *list)
{
  const gchar *base_icon_name;
  const gchar *accessible_name;
  gchar *icon_name;
  GIcon * icon;
  GVariant *serialized_icon;
  GVariantBuilder builder;
  GVariant *state;
  guint n_applications;

  /* Figure out what type of icon we should be drawing */
  if (g_hash_table_find (list->applications, application_draws_attention, NULL)) {
    base_icon_name = "indicator-messages-new-%s";
    accessible_name = _("New Messages");
  } else {
    base_icon_name = "indicator-messages-%s";
    accessible_name = _("Messages");
  }

  /* Include the IM state in the icon */
  state = g_action_group_get_action_state(G_ACTION_GROUP(list->globalactions), "status");
  icon_name = g_strdup_printf(base_icon_name, g_variant_get_string(state, NULL));
  g_variant_unref(state);

  /* Build up the dictionary of values for the state */
  g_variant_builder_init(&builder, G_VARIANT_TYPE_DICTIONARY);

  /* icon */
  icon = g_themed_icon_new_with_default_fallbacks(icon_name);
  g_free(icon_name);
  if ((serialized_icon = g_icon_serialize(icon)))
    {
      g_variant_builder_add (&builder, "{sv}", "icon", serialized_icon);
      g_variant_unref (serialized_icon);
    }
  g_object_unref(icon);

  /* title */
  g_variant_builder_add (&builder, "{sv}", "title", g_variant_new_string (_("Incoming")));

  /* accessible description */
  g_variant_builder_open(&builder, G_VARIANT_TYPE_DICT_ENTRY);
  g_variant_builder_add_value(&builder, g_variant_new_string("accessible-desc"));
  g_variant_builder_add_value(&builder, g_variant_new_variant(g_variant_new_string(accessible_name)));
  g_variant_builder_close(&builder);

  /* visibility */
  n_applications = g_hash_table_size (list->applications);
  g_variant_builder_add (&builder, "{sv}", "visible", g_variant_new_boolean (n_applications > 0));

  /* Set the state */
  g_action_group_change_action_state (G_ACTION_GROUP(list->globalactions), "messages", g_variant_builder_end(&builder));

  GAction * remove_action = g_action_map_lookup_action (G_ACTION_MAP (list->globalactions), "remove-all");
  if (g_hash_table_find (list->applications, application_has_items, NULL)) {
    g_debug("Enabling remove-all");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(remove_action), TRUE);
  } else {
    g_debug("Disabling remove-all");
    g_simple_action_set_enabled(G_SIMPLE_ACTION(remove_action), FALSE);
  }
}

/* Check a source action to see if it draws */
static gboolean
app_source_action_check_draw (Application * app, const gchar * action_name)
{
  GVariant *state;
  guint32 count;
  gint64 time;
  const gchar *string;
  gboolean draws_attention;

  state = g_action_group_get_action_state (G_ACTION_GROUP(app->source_actions), action_name);
  if (state == NULL)
    return FALSE;

  g_variant_get (state, "(ux&sb)", &count, &time, &string, &draws_attention);

  /* invisible sources do not draw attention */
  if (count == 0 && time == 0 && (string == NULL || string[0] == '\0'))
    draws_attention = FALSE;

  g_variant_unref(state);

  return draws_attention;
}

/* Check a message action to see if it draws */
static gboolean
app_message_action_check_draw (Application * app, const gchar * action_name)
{
  GAction * action = NULL;
  action = g_action_map_lookup_action (G_ACTION_MAP (app->message_actions), action_name);
  return GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(action), message_action_draws_attention_quark()));
}

/* Regenerate the draw attention flag based on the sources and messages
 * that we have in the action groups.
 *
 * Returns TRUE if app->draws_attention has changed
 */
static gboolean
application_update_draws_attention (Application * app)
{
  gchar **source_actions = NULL;
  gchar **message_actions = NULL;
  gchar **it;
  gboolean was_drawing_attention = app->draws_attention;

  app->draws_attention = FALSE;

  source_actions = g_action_group_list_actions (G_ACTION_GROUP (app->source_actions));
  for (it = source_actions; *it && !app->draws_attention; it++)
    app->draws_attention = app_source_action_check_draw (app, *it);

  message_actions = g_action_group_list_actions (G_ACTION_GROUP (app->message_actions));
  for (it = message_actions; *it && !app->draws_attention; it++)
    app->draws_attention = app_message_action_check_draw (app, *it);

  g_strfreev (source_actions);
  g_strfreev (message_actions);

  return was_drawing_attention != app->draws_attention;
}

/* Remove a source from an application, signal up and update the status
   of the draws attention flag. */
static void
im_application_list_source_removed (Application *app,
                                    const gchar *id)
{
  g_action_map_remove_action (G_ACTION_MAP(app->source_actions), id);

  g_signal_emit (app->list, signals[SOURCE_REMOVED], 0, app->id, id);

  if (application_update_draws_attention(app))
    im_application_list_update_root_action (app->list);
}

static void
im_application_list_source_activated (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
  Application *app = user_data;
  const gchar *source_id;

  source_id = g_action_get_name (G_ACTION (action));

  if (g_variant_get_boolean (parameter))
    {
      indicator_messages_application_call_activate_source (app->proxy,
                                                           source_id,
                                                           app->cancellable,
                                                           NULL, NULL);
    }
  else
    {
      const gchar *sources[] = { source_id, NULL };
      const gchar *messages[] = { NULL };
      indicator_messages_application_call_dismiss (app->proxy, sources, messages,
                                                   app->cancellable, NULL, NULL);
    }

  im_application_list_source_removed (app, source_id);
}

static void
im_application_list_message_removed (Application *app,
                                     const gchar *id)
{
  g_action_map_remove_action (G_ACTION_MAP(app->message_actions), id);
  g_action_muxer_remove (app->message_sub_actions, id);

  if (application_update_draws_attention(app))
    im_application_list_update_root_action (app->list);

  g_signal_emit (app->list, signals[MESSAGE_REMOVED], 0, app->id, id);
}

static void
im_application_list_message_activated (GSimpleAction *action,
                                       GVariant      *parameter,
                                       gpointer       user_data)
{
  Application *app = user_data;
  const gchar *message_id;

  message_id = g_action_get_name (G_ACTION (action));

  if (g_variant_get_boolean (parameter))
    {
      indicator_messages_application_call_activate_message (app->proxy,
                                                            message_id,
                                                            "",
                                                            g_variant_new_array (G_VARIANT_TYPE_VARIANT, NULL, 0),
                                                            app->cancellable,
                                                            NULL, NULL);
    }
  else
    {
      const gchar *sources[] = { NULL };
      const gchar *messages[] = { message_id, NULL };
      indicator_messages_application_call_dismiss (app->proxy, sources, messages,
                                                   app->cancellable, NULL, NULL);
    }

  im_application_list_message_removed (app, message_id);
}

static void
im_application_list_sub_message_activated (GSimpleAction *action,
                                           GVariant      *parameter,
                                           gpointer       user_data)
{
  Application *app = user_data;
  const gchar *message_id;
  const gchar *action_id;
  GVariantBuilder builder;

  message_id = g_object_get_data (G_OBJECT (action), "message");
  action_id = g_action_get_name (G_ACTION (action));

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));
  if (parameter)
    g_variant_builder_add (&builder, "v", parameter);

  indicator_messages_application_call_activate_message (app->proxy,
                                                        message_id,
                                                        action_id,
                                                        g_variant_builder_end (&builder),
                                                        app->cancellable,
                                                        NULL, NULL);

  im_application_list_message_removed (app, message_id);
}

static void
im_application_list_remove_all (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       user_data)
{
  ImApplicationList *list = user_data;
  GHashTableIter iter;
  Application *app;

  g_signal_emit (list, signals[REMOVE_ALL], 0);

  g_hash_table_iter_init (&iter, list->applications);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &app))
    {
      gchar **source_actions;
      gchar **message_actions;
      gchar **it;

      app->draws_attention = FALSE;

      source_actions = g_action_group_list_actions (G_ACTION_GROUP (app->source_actions));
      for (it = source_actions; *it; it++)
        im_application_list_source_removed (app, *it);

      message_actions = g_action_group_list_actions (G_ACTION_GROUP (app->message_actions));
      for (it = message_actions; *it; it++)
        im_application_list_message_removed (app, *it);

      if (app->proxy != NULL) /* If it is remote, we tell the app we've cleared */
        {
          indicator_messages_application_call_dismiss (app->proxy, 
                                                       (const gchar * const *) source_actions,
                                                       (const gchar * const *) message_actions,
                                                       app->cancellable, NULL, NULL);
        }

      g_strfreev (source_actions);
      g_strfreev (message_actions);
    }

  im_application_list_update_root_action (list);
}

static void
im_application_list_dispose (GObject *object)
{
  ImApplicationList *list = IM_APPLICATION_LIST (object);

  g_clear_object (&list->statusaction);
  g_clear_object (&list->globalactions);
  g_clear_pointer (&list->app_status, g_hash_table_unref);

  g_clear_pointer (&list->applications, g_hash_table_unref);
  g_clear_object (&list->muxer);

  G_OBJECT_CLASS (im_application_list_parent_class)->dispose (object);
}

static void
im_application_list_finalize (GObject *object)
{
  G_OBJECT_CLASS (im_application_list_parent_class)->finalize (object);
}

static void
im_application_list_class_init (ImApplicationListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = im_application_list_dispose;
  object_class->finalize = im_application_list_finalize;

  signals[SOURCE_ADDED] = g_signal_new ("source-added",
                                        IM_TYPE_APPLICATION_LIST,
                                        G_SIGNAL_RUN_FIRST,
                                        0,
                                        NULL, NULL,
                                        g_cclosure_marshal_generic,
                                        G_TYPE_NONE,
                                        5,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_VARIANT,
                                        G_TYPE_BOOLEAN);

  signals[SOURCE_CHANGED] = g_signal_new ("source-changed",
                                          IM_TYPE_APPLICATION_LIST,
                                          G_SIGNAL_RUN_FIRST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_generic,
                                          G_TYPE_NONE,
                                          5,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING,
                                          G_TYPE_VARIANT,
                                          G_TYPE_BOOLEAN);

  signals[SOURCE_REMOVED] = g_signal_new ("source-removed",
                                          IM_TYPE_APPLICATION_LIST,
                                          G_SIGNAL_RUN_FIRST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_generic,
                                          G_TYPE_NONE,
                                          2,
                                          G_TYPE_STRING,
                                          G_TYPE_STRING);

  signals[MESSAGE_ADDED] = g_signal_new ("message-added",
                                         IM_TYPE_APPLICATION_LIST,
                                         G_SIGNAL_RUN_FIRST,
                                         0,
                                         NULL, NULL,
                                         g_cclosure_marshal_generic,
                                         G_TYPE_NONE,
                                         10,
                                         G_TYPE_STRING,
                                         G_TYPE_ICON,
                                         G_TYPE_STRING,
                                         G_TYPE_VARIANT,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_VARIANT,
                                         G_TYPE_INT64,
                                         G_TYPE_BOOLEAN);

  signals[MESSAGE_REMOVED] = g_signal_new ("message-removed",
                                           IM_TYPE_APPLICATION_LIST,
                                           G_SIGNAL_RUN_FIRST,
                                           0,
                                           NULL, NULL,
                                           g_cclosure_marshal_generic,
                                           G_TYPE_NONE,
                                           2,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);

  signals[APP_ADDED] = g_signal_new ("app-added",
                                     IM_TYPE_APPLICATION_LIST,
                                     G_SIGNAL_RUN_FIRST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_generic,
                                     G_TYPE_NONE,
                                     2,
                                     G_TYPE_STRING,
                                     G_TYPE_DESKTOP_APP_INFO);

  signals[APP_STOPPED] = g_signal_new ("app-stopped",
                                       IM_TYPE_APPLICATION_LIST,
                                       G_SIGNAL_RUN_FIRST,
                                       0,
                                       NULL, NULL,
                                       g_cclosure_marshal_VOID__STRING,
                                       G_TYPE_NONE,
                                       1,
                                       G_TYPE_STRING);

  signals[REMOVE_ALL] = g_signal_new ("remove-all",
                                      IM_TYPE_APPLICATION_LIST,
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL, NULL,
                                      g_cclosure_marshal_VOID__VOID,
                                      G_TYPE_NONE,
                                      0);

  signals[STATUS_SET] = g_signal_new ("status-set",
                                      IM_TYPE_APPLICATION_LIST,
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL, NULL,
                                      g_cclosure_marshal_generic,
                                      G_TYPE_NONE,
                                      1,
                                      G_TYPE_STRING);
}

static void
im_application_list_init (ImApplicationList *list)
{
  const GActionEntry action_entries[] = {
    { "remove-all", im_application_list_remove_all }
  };

  list->applications = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, application_free);
  list->app_status = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  list->globalactions = g_simple_action_group_new ();
  {
    GSimpleAction * messages = g_simple_action_new_stateful("messages", NULL, g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0));
    g_action_map_add_action(G_ACTION_MAP(list->globalactions), G_ACTION(messages));
  }
  g_action_map_add_action_entries (G_ACTION_MAP (list->globalactions), action_entries, G_N_ELEMENTS (action_entries), list);

  list->statusaction = g_simple_action_new_stateful("status", G_VARIANT_TYPE_STRING, g_variant_new_string("offline"));
  g_signal_connect(list->statusaction, "activate", G_CALLBACK(status_activated), list);
  g_action_map_add_action(G_ACTION_MAP(list->globalactions), G_ACTION(list->statusaction));

  list->muxer = g_action_muxer_new ();
  g_action_muxer_insert (list->muxer, NULL, G_ACTION_GROUP (list->globalactions));

  im_application_list_update_root_action (list);
}

ImApplicationList *
im_application_list_new (void)
{
  return g_object_new (IM_TYPE_APPLICATION_LIST, NULL);
}

static gchar *
im_application_list_canonical_id (const gchar *id)
{
  gchar *str;
  gchar *p;
  int len;

  len = strlen (id);
  if (g_str_has_suffix (id, ".desktop"))
    len -= 8;

  str = g_strndup (id, len);

  for (p = str; *p; p++)
    {
      if (*p == '.')
        *p = '_';
    }

  return str;
}

static Application *
im_application_list_lookup (ImApplicationList *list,
                            const gchar       *desktop_id)
{
  gchar *id;
  Application *app;

  id = im_application_list_canonical_id (desktop_id);
  app = g_hash_table_lookup (list->applications, id);

  g_free (id);
  return app;
}

void
im_application_list_activate_launch (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
  Application *app = user_data;
  GError *error = NULL;

  if (!g_app_info_launch (G_APP_INFO (app->info), NULL, NULL, &error))
    {
      g_warning ("unable to launch application: %s", error->message);
      g_error_free (error);
    }
}

void
im_application_list_activate_app_action (GSimpleAction *action,
                                         GVariant      *parameter,
                                         gpointer       user_data)
{
  Application *app = user_data;

  indicator_desktop_shortcuts_nick_exec_with_context (app->shortcuts, g_action_get_name (G_ACTION (action)), NULL);
}

gboolean
im_application_list_add (ImApplicationList  *list,
                         const gchar        *desktop_id)
{
  GDesktopAppInfo *info;
  Application *app;
  const gchar *id;
  GSimpleActionGroup *actions;
  GSimpleAction *launch_action;
  IndicatorDesktopShortcuts * shortcuts = NULL;

  g_return_val_if_fail (IM_IS_APPLICATION_LIST (list), FALSE);
  g_return_val_if_fail (desktop_id != NULL, FALSE);

  if (im_application_list_lookup (list, desktop_id))
    return TRUE;

  info = g_desktop_app_info_new (desktop_id);
  if (!info)
    {
      g_warning ("an application with id '%s' is not installed", desktop_id);
      return FALSE;
    }

  id = g_app_info_get_id (G_APP_INFO (info));
  g_return_val_if_fail (id != NULL, FALSE);

  {
    const char * filename = g_desktop_app_info_get_filename(info);
    if (filename != NULL)
      shortcuts = indicator_desktop_shortcuts_new(filename, "Messaging Menu");
  }

  app = g_slice_new0 (Application);
  app->info = info;
  app->id = im_application_list_canonical_id (id);
  app->list = list;
  app->muxer = g_action_muxer_new ();
  app->source_actions = g_simple_action_group_new ();
  app->message_actions = g_simple_action_group_new ();
  app->message_sub_actions = g_action_muxer_new ();
  app->draws_attention = FALSE;
  app->shortcuts = shortcuts;

  actions = g_simple_action_group_new ();

  launch_action = g_simple_action_new_stateful ("launch", NULL, g_variant_new_boolean (FALSE));
  g_signal_connect (launch_action, "activate", G_CALLBACK (im_application_list_activate_launch), app);
  g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (launch_action));

  if (app->shortcuts != NULL) {
    const gchar ** nicks;

    for (nicks = indicator_desktop_shortcuts_get_nicks (app->shortcuts); *nicks; nicks++)
      {
        GSimpleAction *action;

        action = g_simple_action_new (*nicks, NULL);
        g_signal_connect (action, "activate", G_CALLBACK (im_application_list_activate_app_action), app);
        g_action_map_add_action (G_ACTION_MAP (actions), G_ACTION (action));

        g_object_unref (action);
      }
  }

  g_action_muxer_insert (app->muxer, NULL, G_ACTION_GROUP (actions));
  g_action_muxer_insert (app->muxer, "src", G_ACTION_GROUP (app->source_actions));
  g_action_muxer_insert (app->muxer, "msg", G_ACTION_GROUP (app->message_actions));
  g_action_muxer_insert (app->muxer, "msg-actions", G_ACTION_GROUP (app->message_sub_actions));

  g_hash_table_insert (list->applications, (gpointer) app->id, app);
  g_action_muxer_insert (list->muxer, app->id, G_ACTION_GROUP (app->muxer));

  im_application_list_update_root_action (list);

  g_signal_emit (app->list, signals[APP_ADDED], 0, app->id, app->info);

  g_object_unref (launch_action);
  g_object_unref (actions);

  return TRUE;
}

void
im_application_list_remove (ImApplicationList *list,
                            const gchar       *id)
{
  Application *app;

  g_return_if_fail (IM_IS_APPLICATION_LIST (list));

  app = im_application_list_lookup (list, id);
  if (app)
    {
      if (app->proxy || app->cancellable)
        g_signal_emit (app->list, signals[APP_STOPPED], 0, app->id);

      g_hash_table_remove (list->applications, id);
      g_action_muxer_remove (list->muxer, id);

      im_application_list_update_root_action (list);
    }
}

static void
im_application_list_source_added (Application *app,
                                  guint        position,
                                  GVariant    *source)
{
  const gchar *id;
  const gchar *label;
  GVariant *maybe_serialized_icon;
  guint32 count;
  gint64 time;
  const gchar *string;
  gboolean draws_attention;
  gboolean visible;
  GVariant *serialized_icon = NULL;
  GVariant *state;
  GSimpleAction *action;

  g_variant_get (source, "(&s&s@avux&sb)",
                 &id, &label, &maybe_serialized_icon, &count, &time, &string, &draws_attention);

  if (g_variant_n_children (maybe_serialized_icon) == 1)
    g_variant_get_child (maybe_serialized_icon, 0, "v", &serialized_icon);

  visible = count > 0 || time != 0 || (string != NULL && string[0] != '\0');

  state = g_variant_new ("(uxsb)", count, time, string, draws_attention);
  action = g_simple_action_new_stateful (id, G_VARIANT_TYPE_BOOLEAN, state);
  g_signal_connect (action, "activate", G_CALLBACK (im_application_list_source_activated), app);

  g_action_map_add_action (G_ACTION_MAP(app->source_actions), G_ACTION (action));

  g_signal_emit (app->list, signals[SOURCE_ADDED], 0, app->id, id, label, serialized_icon, visible);

  if (visible && draws_attention && app->draws_attention == FALSE)
    {
      app->draws_attention = TRUE;
      im_application_list_update_root_action (app->list);
    }

  g_object_unref (action);
  if (serialized_icon)
    g_variant_unref (serialized_icon);
  g_variant_unref (maybe_serialized_icon);
}

static void
im_application_list_source_changed (Application *app,
                                    GVariant    *source)
{
  const gchar *id;
  const gchar *label;
  GVariant *maybe_serialized_icon;
  guint32 count;
  gint64 time;
  const gchar *string;
  gboolean draws_attention;
  GVariant *serialized_icon = NULL;
  gboolean visible;

  g_variant_get (source, "(&s&s@avux&sb)",
                 &id, &label, &maybe_serialized_icon, &count, &time, &string, &draws_attention);

  if (g_variant_n_children (maybe_serialized_icon) == 1)
    g_variant_get_child (maybe_serialized_icon, 0, "v", &serialized_icon);

  g_action_group_change_action_state (G_ACTION_GROUP (app->source_actions), id,
                                      g_variant_new ("(uxsb)", count, time, string, draws_attention));

  visible = count > 0 || time != 0 || (string != NULL && string[0] != '\0');

  g_signal_emit (app->list, signals[SOURCE_CHANGED], 0, app->id, id, label, serialized_icon, visible);

  if (application_update_draws_attention (app))
    im_application_list_update_root_action (app->list);

  if (serialized_icon)
    g_variant_unref (serialized_icon);
  g_variant_unref (maybe_serialized_icon);
}

static void
im_application_list_sources_listed (GObject      *source_object,
                                    GAsyncResult *result,
                                    gpointer      user_data)
{
  Application *app = user_data;
  GVariant *sources;
  GError *error = NULL;

  if (indicator_messages_application_call_list_sources_finish (app->proxy, &sources, result, &error))
    {
      GVariantIter iter;
      GVariant *source;
      guint i = 0;

      g_variant_iter_init (&iter, sources);
      while ((source = g_variant_iter_next_value (&iter)))
        {
          im_application_list_source_added (app, i++, source);
          g_variant_unref (source);
        }

      g_variant_unref (sources);
    }
  else
    {
      g_warning ("could not fetch the list of sources: %s", error->message);
      g_error_free (error);
    }
}

static GIcon *
get_symbolic_app_icon (GAppInfo *info)
{
  GIcon *icon;
  const gchar * const *names;
  gchar *symbolic_name;
  GIcon *symbolic_icon;

  icon = g_app_info_get_icon (info);
  if (icon == NULL)
    return NULL;

  if (!G_IS_THEMED_ICON (icon))
    return g_object_ref (icon);

  names = g_themed_icon_get_names (G_THEMED_ICON (icon));
  if (!names || !names[0])
    return g_object_ref (icon);

  symbolic_name = g_strconcat (names[0], "-symbolic", NULL);

  symbolic_icon = g_themed_icon_new_from_names ((gchar **) names, -1);
  g_themed_icon_prepend_name (G_THEMED_ICON (symbolic_icon), symbolic_name);

  g_free (symbolic_name);

  return symbolic_icon;
}

static void
im_application_list_message_added (Application *app,
                                   GVariant    *message)
{
  const gchar *id;
  GVariant *maybe_serialized_icon;
  const gchar *title;
  const gchar *subtitle;
  const gchar *body;
  gint64 time;
  GVariantIter *action_iter;
  gboolean draws_attention;
  GVariant *serialized_icon = NULL;
  GSimpleAction *action;
  GIcon *app_icon;
  GVariant *actions = NULL;

  g_variant_get (message, "(&s@av&s&s&sxaa{sv}b)",
                 &id, &maybe_serialized_icon, &title, &subtitle, &body, &time, &action_iter, &draws_attention);

  if (g_variant_n_children (maybe_serialized_icon) == 1)
    g_variant_get_child (maybe_serialized_icon, 0, "v", &serialized_icon);

  action = g_simple_action_new (id, G_VARIANT_TYPE_BOOLEAN);
  g_object_set_qdata(G_OBJECT(action), message_action_draws_attention_quark(), GINT_TO_POINTER(draws_attention));
  g_signal_connect (action, "activate", G_CALLBACK (im_application_list_message_activated), app);
  g_action_map_add_action (G_ACTION_MAP(app->message_actions), G_ACTION (action));

  {
    GVariant *entry;
    GSimpleActionGroup *action_group;
    GVariantBuilder actions_builder;

    g_variant_builder_init (&actions_builder, G_VARIANT_TYPE ("aa{sv}"));
    action_group = g_simple_action_group_new ();

    while ((entry = g_variant_iter_next_value (action_iter)))
      {
        const gchar *name;
        GSimpleAction *action;
        GVariant *label;
        const gchar *type = NULL;
        GVariant *hint;
        GVariantBuilder dict_builder;
        gchar *prefixed_name;

        if (!g_variant_lookup (entry, "name", "&s", &name))
          {
            g_warning ("action dictionary for message '%s' is missing 'name' key", id);
            continue;
          }

        label = g_variant_lookup_value (entry, "label", G_VARIANT_TYPE_STRING);
        g_variant_lookup (entry, "parameter-type", "&g", &type);
        hint = g_variant_lookup_value (entry, "parameter-hint", NULL);

        action = g_simple_action_new (name, type ? G_VARIANT_TYPE (type) : NULL);
        g_object_set_data_full (G_OBJECT (action), "message", g_strdup (id), g_free);
        g_signal_connect (action, "activate", G_CALLBACK (im_application_list_sub_message_activated), app);
        g_action_map_add_action (G_ACTION_MAP(action_group), G_ACTION (action));

        g_variant_builder_init (&dict_builder, G_VARIANT_TYPE ("a{sv}"));

        prefixed_name = g_strjoin (".", app->id, "msg-actions", id, name, NULL);
        g_variant_builder_add (&dict_builder, "{sv}", "name", g_variant_new_string (prefixed_name));

        if (label)
          {
            g_variant_builder_add (&dict_builder, "{sv}", "label", label);
            g_variant_unref (label);
          }

        if (type)
          g_variant_builder_add (&dict_builder, "{sv}", "parameter-type", g_variant_new_string (type));

        if (hint)
          {
            g_variant_builder_add (&dict_builder, "{sv}", "parameter-hint", hint);
            g_variant_unref (hint);
          }

        g_variant_builder_add (&actions_builder, "a{sv}", &dict_builder);

        g_object_unref (action);
        g_variant_unref (entry);
        g_free (prefixed_name);
      }

    g_action_muxer_insert (app->message_sub_actions, id, G_ACTION_GROUP (action_group));
    actions = g_variant_builder_end (&actions_builder);

    g_object_unref (action_group);
  }

  if (draws_attention && !app->draws_attention)
    {
      app->draws_attention = TRUE;
      im_application_list_update_root_action (app->list);
    }

  app_icon = get_symbolic_app_icon (G_APP_INFO (app->info));

  g_signal_emit (app->list, signals[MESSAGE_ADDED], 0,
                 app->id, app_icon, id, serialized_icon, title,
                 subtitle, body, actions, time, draws_attention);

  g_variant_iter_free (action_iter);
  g_object_unref (action);
  if (serialized_icon)
    g_variant_unref (serialized_icon);
  g_variant_unref (maybe_serialized_icon);
  g_object_unref (app_icon);
}

static void
im_application_list_messages_listed (GObject      *source_object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  Application *app = user_data;
  GVariant *messages;
  GError *error = NULL;

  if (indicator_messages_application_call_list_messages_finish (app->proxy, &messages, result, &error))
    {
      GVariantIter iter;
      GVariant *message;

      g_variant_iter_init (&iter, messages);
      while ((message = g_variant_iter_next_value (&iter)))
        {
          im_application_list_message_added (app, message);
          g_variant_unref (message);
        }

      g_variant_unref (messages);
    }
  else
    {
      g_warning ("could not fetch the list of messages: %s", error->message);
      g_error_free (error);
    }
}

static void
im_application_list_unset_remote (Application *app)
{
  gboolean was_running;

  was_running = app->proxy || app->cancellable;

  if (app->cancellable)
    {
      g_cancellable_cancel (app->cancellable);
      g_clear_object (&app->cancellable);
    }
  g_clear_object (&app->proxy);

  /* clear actions by creating a new action group and overriding it in
   * the muxer */
  g_object_unref (app->source_actions);
  g_object_unref (app->message_actions);
  g_object_unref (app->message_sub_actions);
  app->source_actions = g_simple_action_group_new ();
  app->message_actions = g_simple_action_group_new ();
  app->message_sub_actions = g_action_muxer_new ();
  g_action_muxer_insert (app->muxer, "src", G_ACTION_GROUP (app->source_actions));
  g_action_muxer_insert (app->muxer, "msg", G_ACTION_GROUP (app->message_actions));
  g_action_muxer_insert (app->muxer, "msg-actions", G_ACTION_GROUP (app->message_sub_actions));

  app->draws_attention = FALSE;
  im_application_list_update_root_action (app->list);

  g_action_group_change_action_state (G_ACTION_GROUP (app->muxer), "launch", g_variant_new_boolean (FALSE));

  if (was_running)
    g_signal_emit (app->list, signals[APP_STOPPED], 0, app->id);
}

static void
im_application_list_app_vanished (GDBusConnection *connection,
                                  const gchar     *name,
                                  gpointer         user_data)
{
  Application *app = user_data;

  im_application_list_unset_remote (app);
}

static void
im_application_list_proxy_created (GObject      *source_object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  Application *app = user_data;
  GError *error = NULL;

  app->proxy = indicator_messages_application_proxy_new_finish (result, &error);
  if (!app->proxy)
    {
      if (error->code != G_IO_ERROR_CANCELLED)
        g_warning ("could not create application proxy: %s", error->message);
      g_error_free (error);
      return;
    }

  indicator_messages_application_call_list_sources (app->proxy, app->cancellable,
                                                    im_application_list_sources_listed, app);
  indicator_messages_application_call_list_messages (app->proxy, app->cancellable,
                                                     im_application_list_messages_listed, app);

  g_signal_connect_swapped (app->proxy, "source-added", G_CALLBACK (im_application_list_source_added), app);
  g_signal_connect_swapped (app->proxy, "source-changed", G_CALLBACK (im_application_list_source_changed), app);
  g_signal_connect_swapped (app->proxy, "source-removed", G_CALLBACK (im_application_list_source_removed), app);
  g_signal_connect_swapped (app->proxy, "message-added", G_CALLBACK (im_application_list_message_added), app);
  g_signal_connect_swapped (app->proxy, "message-removed", G_CALLBACK (im_application_list_message_removed), app);

  g_action_group_change_action_state (G_ACTION_GROUP (app->muxer), "launch", g_variant_new_boolean (TRUE));

  g_bus_watch_name_on_connection (g_dbus_proxy_get_connection (G_DBUS_PROXY (app->proxy)),
                                  g_dbus_proxy_get_name (G_DBUS_PROXY (app->proxy)),
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  NULL, im_application_list_app_vanished,
                                  app, NULL);
}

void
im_application_list_set_remote (ImApplicationList *list,
                                const gchar       *id,
                                GDBusConnection   *connection,
                                const gchar       *unique_bus_name,
                                const gchar       *object_path)
{
  Application *app;

  g_return_if_fail (IM_IS_APPLICATION_LIST (list));

  app = im_application_list_lookup (list, id);
  if (!app)
    {
      g_warning ("'%s' is not a registered application", id);
      return;
    }

  if (!connection && !unique_bus_name && !object_path)
    {
      im_application_list_unset_remote (app);
      return;
    }

  if (app->cancellable)
    {
      gchar *name_owner = NULL;

      if (app->proxy)
        name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (app->proxy));

      if (g_strcmp0 (name_owner, unique_bus_name) != 0)
        {
          g_warning ("replacing '%s' at %s with %s", id, name_owner, unique_bus_name);
          im_application_list_unset_remote (app);
        }

      g_free (name_owner);
    }

  app->cancellable = g_cancellable_new ();
  indicator_messages_application_proxy_new (connection, G_DBUS_PROXY_FLAGS_NONE,
                                            unique_bus_name, object_path, app->cancellable,
                                            im_application_list_proxy_created, app);
}

GActionGroup *
im_application_list_get_action_group (ImApplicationList *list)
{
  g_return_val_if_fail (IM_IS_APPLICATION_LIST (list), NULL);

  return G_ACTION_GROUP (list->muxer);
}

GList *
im_application_list_get_applications (ImApplicationList *list)
{
  g_return_val_if_fail (IM_IS_APPLICATION_LIST (list), NULL);

  return g_hash_table_get_keys (list->applications);
}

GDesktopAppInfo *
im_application_list_get_application (ImApplicationList *list,
                                     const gchar       *id)
{
  Application *app;

  g_return_val_if_fail (IM_IS_APPLICATION_LIST (list), NULL);

  app = g_hash_table_lookup (list->applications, id);
  return app ? app->info : NULL;
}

static void
status_activated (GSimpleAction * action, GVariant * param, gpointer user_data)
{
  g_return_if_fail (IM_IS_APPLICATION_LIST(user_data));
  ImApplicationList * list = IM_APPLICATION_LIST(user_data);
  const gchar * status = g_variant_get_string(param, NULL);

  g_simple_action_set_state(action, param);

  GList * appshash = g_hash_table_get_keys(list->app_status);
  GList * appsfree = g_list_copy_deep(appshash, (GCopyFunc)g_strdup, NULL);
  GList * app;

  for (app = appsfree; app != NULL; app = g_list_next(app)) {
    g_hash_table_insert(list->app_status, app->data, g_strdup(status));
  }

  g_list_free(appshash);
  g_list_free(appsfree);

  g_signal_emit (list, signals[STATUS_SET], 0, status);

  im_application_list_update_root_action(list);

  return;
}

#define STATUS_ID_OFFLINE  (G_N_ELEMENTS(status_ids) - 1)
static const gchar *status_ids[] = { "available", "away", "busy", "invisible", "offline" };

static guint
status2val (const gchar * string)
{
	if (string == NULL) return STATUS_ID_OFFLINE;

	guint i;
	for (i = 0; i < G_N_ELEMENTS(status_ids); i++) {
		if (g_strcmp0(status_ids[i], string) == 0) {
			break;
		}
	}

	if (i > STATUS_ID_OFFLINE)
		i = STATUS_ID_OFFLINE;

	return i;
}

void
im_application_list_set_status (ImApplicationList * list, const gchar * id, const gchar *status)
{
	g_return_if_fail (IM_IS_APPLICATION_LIST (list));

	g_hash_table_insert(list->app_status, im_application_list_canonical_id(id), g_strdup(status));

	guint final_status = STATUS_ID_OFFLINE;

	GList * statuses = g_hash_table_get_values(list->app_status);
	GList * statusentry;

	for (statusentry = statuses; statusentry != NULL; statusentry = g_list_next(statusentry)) {
		guint statusval = status2val((gchar *)statusentry->data);
		final_status = MIN(final_status, statusval);
	}

	g_list_free(statuses);

	g_simple_action_set_state(list->statusaction, g_variant_new_string(status_ids[final_status]));

	im_application_list_update_root_action(list);

	return;
}

