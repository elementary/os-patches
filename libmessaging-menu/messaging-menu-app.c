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

#include "messaging-menu-app.h"
#include "indicator-messages-service.h"
#include "indicator-messages-application.h"

#include <gio/gdesktopappinfo.h>
#include <string.h>

/**
 * SECTION:messaging-menu
 * @title: MessagingMenuApp
 * @short_description: An application section in the messaging menu
 * @include: messaging-menu.h
 *
 * A #MessagingMenuApp represents an application section in the
 * Messaging Menu.  An application section is tied to an installed
 * application through a desktop file id, which must be passed to
 * messaging_menu_app_new().
 *
 * To register the application with the Messaging Menu, call
 * messaging_menu_app_register().  This signifies that the application
 * should be present in the menu and be marked as "running".
 *
 * The first menu item in an application section represents the
 * application itself, using the name and icon found in the associated
 * desktop file.  Activating this item starts the application.
 *
 * Following the application item, the Messaging Menu inserts all
 * shortcut actions found in the desktop file.  Actions whose
 * <code>NotShowIn</code> keyword contains "Messaging Menu" or whose
 * <code>OnlyShowIn</code> keyword does not contain "Messaging Menu"
 * will not appear (the <ulink
 * url="http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-1.1.html#extra-actions">
 * desktop file specification</ulink> contains a detailed explanation of
 * shortcut actions.)  An application cannot add, remove, or change
 * these shortcut items while it is running.
 *
 * Next, an application section contains menu items for message sources.
 * What exactly constitutes a message source depends on the type of
 * application:  an email client's message sources are folders
 * containing new messages, while those of a chat program are persons
 * that have contacted the user.
 *
 * A message source is represented in the menu by a label and optionally
 * also an icon.  It can be associated with either a count, a time, or
 * an arbitrary string, which will appear on the right side of the menu
 * item.
 *
 * When the user activates a source, the source is immediately removed
 * from the menu and the "activate-source" signal is emitted.
 *
 * Applications should always expose all the message sources available.
 * However, the Messaging Menu might limit the amount of sources it
 * displays to the user.
 *
 * The Messaging Menu offers users a way to set their chat status
 * (available, away, busy, invisible, or offline) for multiple
 * applications at once.  Applications that appear in the Messaging Menu
 * can integrate with this by setting the
 * "X-MessagingMenu-UsesChatSection" key in their desktop file to True.
 * Use messaging_menu_app_set_status() to signify that the application's
 * chat status has changed.  When the user changes status through the
 * Messaging Menu, the ::status-changed signal will be emitted.
 *
 * If the application stops running without calling
 * messaging_menu_app_unregister(), it will be marked as "not running".
 * Its application and shortcut items stay in the menu, but all message
 * sources are removed.  If messaging_menu_app_unregister() is called,
 * the application section is removed completely.
 *
 * More information about the design and recommended usage of the
 * Messaging Menu is available at <ulink
 * url="https://wiki.ubuntu.com/MessagingMenu">https://wiki.ubuntu.com/MessagingMenu</ulink>.
 */

/**
 * MessagingMenuApp:
 *
 * #MessagingMenuApp is an opaque structure.
 */
struct _MessagingMenuApp
{
  GObject parent_instance;

  GDesktopAppInfo *appinfo;
  int registered;  /* -1 for unknown */
  MessagingMenuStatus status;
  gboolean status_set;
  GDBusConnection *bus;

  GHashTable *messages;
  GList *sources;
  IndicatorMessagesApplication *app_interface;

  IndicatorMessagesService *messages_service;
  guint watch_id;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (MessagingMenuApp, messaging_menu_app, G_TYPE_OBJECT);

enum {
  PROP_0,
  PROP_DESKTOP_ID,
  N_PROPERTIES
};

enum {
  ACTIVATE_SOURCE,
  STATUS_CHANGED,
  N_SIGNALS
};

static GParamSpec *properties[N_PROPERTIES];
static guint signals[N_SIGNALS];

static const gchar *status_ids[] = { "available", "away", "busy", "invisible", "offline" };

typedef struct
{
  gchar *id;
  GIcon *icon;
  gchar *label;

  guint32 count;
  gint64 time;
  gchar *string;
  gboolean draws_attention;
} Source;

static void global_status_changed (IndicatorMessagesService *service,
                                   const gchar *status_str,
                                   gpointer user_data);

/* in messaging-menu-message.c */
GVariant * _messaging_menu_message_to_variant (MessagingMenuMessage *msg);

static void
source_free (gpointer data)
{
  Source *source = data;

  if (source)
    {
      g_free (source->id);
      g_clear_object (&source->icon);
      g_free (source->label);
      g_free (source->string);
      g_slice_free (Source, source);
    }
}

static GVariant *
source_to_variant (Source *source)
{
  GVariant *v;
  GVariant *serialized_icon;
  GVariantBuilder builder;

  serialized_icon = source->icon ? g_icon_serialize (source->icon) : NULL;
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));
  if (serialized_icon)
    {
      g_variant_builder_add (&builder, "v", serialized_icon);
      g_variant_unref (serialized_icon);
    }

  v = g_variant_new ("(ssavuxsb)", source->id,
                                   source->label,
                                   &builder,
                                   source->count,
                                   source->time,
                                   source->string ? source->string : "",
                                   source->draws_attention);

  return v;
}

static gchar *
messaging_menu_app_get_dbus_object_path (MessagingMenuApp *app)
{
  gchar *path;

  if (!app->appinfo)
    return NULL;

  path = g_strconcat ("/com/canonical/indicator/messages/",
                      g_app_info_get_id (G_APP_INFO (app->appinfo)),
                      NULL);

  g_strcanon (path, "/ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", '_');

  return path;
}

static void
messaging_menu_app_got_bus (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  MessagingMenuApp *app = user_data;
  GError *error = NULL;
  gchar *object_path;

  app->bus = g_bus_get_finish (res, &error);
  if (app->bus == NULL)
    {
      g_warning ("unable to connect to session bus: %s", error->message);
      g_error_free (error);
      return;
    }

  object_path = messaging_menu_app_get_dbus_object_path (app);

  if (object_path &&
      !g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (app->app_interface),
                                         app->bus, object_path, &error))
    {
      g_warning ("unable to export application interface: %s", error->message);
      g_clear_error (&error);
    }

  g_free (object_path);
}

static void
messaging_menu_app_set_desktop_id (MessagingMenuApp *app,
                                   const gchar      *desktop_id)
{
  g_return_if_fail (desktop_id != NULL);

  /* no need to clean up, it's construct only */
  app->appinfo = g_desktop_app_info_new (desktop_id);
  if (app->appinfo == NULL)
    {
      g_warning ("could not find the desktop file for '%s'",
                 desktop_id);
    }

  g_bus_get (G_BUS_TYPE_SESSION,
             app->cancellable,
             messaging_menu_app_got_bus,
             app);
}

static void
messaging_menu_app_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  MessagingMenuApp *app = MESSAGING_MENU_APP (object);

  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      messaging_menu_app_set_desktop_id (app, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
messaging_menu_app_finalize (GObject *object)
{
  G_OBJECT_CLASS (messaging_menu_app_parent_class)->finalize (object);
}

static void
messaging_menu_app_dispose (GObject *object)
{
  MessagingMenuApp *app = MESSAGING_MENU_APP (object);

  if (app->watch_id > 0)
    {
      g_bus_unwatch_name (app->watch_id);
      app->watch_id = 0;
    }

  if (app->cancellable)
    {
      g_cancellable_cancel (app->cancellable);
      g_object_unref (app->cancellable);
      app->cancellable = NULL;
    }

  if (app->messages_service)
    {
      indicator_messages_service_call_application_stopped_running (app->messages_service,
                                                                   g_app_info_get_id (G_APP_INFO (app->appinfo)),
                                                                   NULL, NULL, NULL);

      g_signal_handlers_disconnect_by_func (app->messages_service,
                                            global_status_changed,
                                            app);
      g_clear_object (&app->messages_service);
    }

  g_clear_pointer (&app->messages, g_hash_table_unref);

  g_list_free_full (app->sources, source_free);
  app->sources = NULL;

  g_clear_object (&app->app_interface);
  g_clear_object (&app->appinfo);
  g_clear_object (&app->bus);

  G_OBJECT_CLASS (messaging_menu_app_parent_class)->dispose (object);
}

static void
messaging_menu_app_class_init (MessagingMenuAppClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->set_property = messaging_menu_app_set_property;
  object_class->finalize = messaging_menu_app_finalize;
  object_class->dispose = messaging_menu_app_dispose;

  /**
   * MessagingMenuApp:desktop-id:
   *
   * The desktop id of the application associated with this application
   * section.  Must be given when the #MessagingMenuApp is created.
   */
  properties[PROP_DESKTOP_ID] = g_param_spec_string ("desktop-id",
                                                     "Desktop Id",
                                                     "The desktop id of the associated application",
                                                     NULL,
                                                     G_PARAM_WRITABLE |
                                                     G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * MessagingMenuApp::activate-source:
   * @mmapp: the #MessagingMenuApp
   * @source_id: the source id that was activated
   *
   * Emitted when the user has activated the message source with id
   * @source_id.  The source is immediately removed from the menu,
   * handlers of this signal do not need to call
   * messaging_menu_app_remove_source().
   */
  signals[ACTIVATE_SOURCE] = g_signal_new ("activate-source",
                                           MESSAGING_MENU_TYPE_APP,
                                           G_SIGNAL_RUN_FIRST |
                                           G_SIGNAL_DETAILED,
                                           0,
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__STRING,
                                           G_TYPE_NONE, 1, G_TYPE_STRING);

  /**
   * MessagingMenuApp::status-changed:
   * @mmapp: the #MessagingMenuApp
   * @status: a #MessagingMenuStatus
   *
   * Emitted when the chat status is changed through the messaging menu.
   *
   * Applications which are registered to use the chat status should
   * change their status to @status upon receiving this signal.  Call
   * messaging_menu_app_set_status() to acknowledge that the application
   * changed its status.
   */
  signals[STATUS_CHANGED] = g_signal_new ("status-changed",
                                          MESSAGING_MENU_TYPE_APP,
                                          G_SIGNAL_RUN_FIRST,
                                          0,
                                          NULL, NULL,
                                          g_cclosure_marshal_VOID__INT,
                                          G_TYPE_NONE, 1, G_TYPE_INT);
}

static void
created_messages_service (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  MessagingMenuApp *app = user_data;
  GError *error = NULL;

  app->messages_service = indicator_messages_service_proxy_new_finish (result, &error);
  if (!app->messages_service)
    {
      g_warning ("unable to connect to the mesaging menu service: %s", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (app->messages_service, "status-changed",
                    G_CALLBACK (global_status_changed), app);

  /* sync current status */
  if (app->registered == TRUE)
    messaging_menu_app_register (app);
  else if (app->registered == FALSE)
    messaging_menu_app_unregister (app);
  if (app->status_set)
    messaging_menu_app_set_status (app, app->status);
}

static void
indicator_messages_appeared (GDBusConnection *bus,
                             const gchar     *name,
                             const gchar     *name_owner,
                             gpointer         user_data)
{
  MessagingMenuApp *app = user_data;

  indicator_messages_service_proxy_new (bus,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        "com.canonical.indicator.messages",
                                        "/com/canonical/indicator/messages/service",
                                        app->cancellable,
                                        created_messages_service,
                                        app);
}

static void
indicator_messages_vanished (GDBusConnection *bus,
                             const gchar     *name,
                             gpointer         user_data)
{
  MessagingMenuApp *app = user_data;

  if (app->messages_service)
    {
      g_signal_handlers_disconnect_by_func (app->messages_service,
                                            global_status_changed,
                                            app);
      g_clear_object (&app->messages_service);
    }
}

static gboolean
messaging_menu_app_list_sources (IndicatorMessagesApplication *app_interface,
                                 GDBusMethodInvocation        *invocation,
                                 gpointer                      user_data)
{
  MessagingMenuApp *app = user_data;
  GVariantBuilder builder;
  GList *it;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ssavuxsb)"));

  for (it = app->sources; it; it = it->next)
    g_variant_builder_add_value (&builder, source_to_variant (it->data));

  indicator_messages_application_complete_list_sources (app_interface,
                                                        invocation,
                                                        g_variant_builder_end (&builder));

  return TRUE;
}

static gint
compare_source_id (gconstpointer a,
                   gconstpointer b)
{
  const Source *source = a;
  const gchar *id = b;

  return strcmp (source->id, id);
}

static gboolean
messaging_menu_app_remove_source_internal (MessagingMenuApp *app,
                                           const gchar      *source_id)
{
  GList *node;

  node = g_list_find_custom (app->sources, source_id, compare_source_id);
  if (node)
    {
      source_free (node->data);
      app->sources = g_list_delete_link (app->sources, node);
      return TRUE;
    }

  return FALSE;
}

static gboolean
messaging_menu_app_remove_message_internal (MessagingMenuApp *app,
                                            const gchar      *message_id)
{
  return g_hash_table_remove (app->messages, message_id);
}

static gboolean
messaging_menu_app_activate_source (IndicatorMessagesApplication *app_interface,
                                    GDBusMethodInvocation        *invocation,
                                    const gchar                  *source_id,
                                    gpointer                      user_data)
{
  MessagingMenuApp *app = user_data;
  GQuark q = g_quark_from_string (source_id);

  /* Activate implies removing the source, no need for SourceRemoved */
  if (messaging_menu_app_remove_source_internal (app, source_id))
    g_signal_emit (app, signals[ACTIVATE_SOURCE], q, source_id);

  indicator_messages_application_complete_activate_source (app_interface, invocation);

  return TRUE;
}

static gboolean
messaging_menu_app_list_messages (IndicatorMessagesApplication *app_interface,
                                  GDBusMethodInvocation        *invocation,
                                  gpointer                      user_data)
{
  MessagingMenuApp *app = user_data;
  GVariantBuilder builder;
  GHashTableIter iter;
  MessagingMenuMessage *message;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(savsssxaa{sv}b)"));

  g_hash_table_iter_init (&iter, app->messages);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &message))
    g_variant_builder_add_value (&builder, _messaging_menu_message_to_variant (message));

  indicator_messages_application_complete_list_messages (app_interface,
                                                         invocation,
                                                         g_variant_builder_end (&builder));

  return TRUE;
}

static gboolean
messaging_menu_app_activate_message (IndicatorMessagesApplication *app_interface,
                                     GDBusMethodInvocation        *invocation,
                                     const gchar                  *message_id,
                                     const gchar                  *action_id,
                                     GVariant                     *params,
                                     gpointer                      user_data)
{
  MessagingMenuApp *app = user_data;
  MessagingMenuMessage *msg;

  msg = g_hash_table_lookup (app->messages, message_id);
  if (msg)
    {
      if (*action_id)
        {
          gchar *signal;

          signal = g_strconcat ("activate::", action_id, NULL);

          if (g_variant_n_children (params))
            {
              GVariant *param;

              g_variant_get_child (params, 0, "v", &param);
              g_signal_emit_by_name (msg, signal, action_id, param);

              g_variant_unref (param);
            }
          else
            g_signal_emit_by_name (msg, signal, action_id, NULL);

          g_free (signal);
        }
      else
        g_signal_emit_by_name (msg, "activate", NULL, NULL);


      /* Activate implies removing the message, no need for MessageRemoved  */
      messaging_menu_app_remove_message_internal (app, message_id);
    }

  indicator_messages_application_complete_activate_message (app_interface, invocation);

  return TRUE;
}

static gboolean
messaging_menu_app_dismiss (IndicatorMessagesApplication *app_interface,
                            GDBusMethodInvocation        *invocation,
                            const gchar * const          *sources,
                            const gchar * const          *messages,
                            gpointer                      user_data)
{
  MessagingMenuApp *app = user_data;
  const gchar * const *it;

  for (it = sources; *it; it++)
    messaging_menu_app_remove_source_internal (app, *it);

  for (it = messages; *it; it++)
    messaging_menu_app_remove_message_internal (app, *it);

  return TRUE;
}

static void
messaging_menu_app_init (MessagingMenuApp *app)
{
  app->registered = -1;
  app->status_set = FALSE;
  app->bus = NULL;

  app->cancellable = g_cancellable_new ();

  app->app_interface = indicator_messages_application_skeleton_new ();
  g_signal_connect (app->app_interface, "handle-list-sources",
                    G_CALLBACK (messaging_menu_app_list_sources), app);
  g_signal_connect (app->app_interface, "handle-activate-source",
                    G_CALLBACK (messaging_menu_app_activate_source), app);
  g_signal_connect (app->app_interface, "handle-list-messages",
                    G_CALLBACK (messaging_menu_app_list_messages), app);
  g_signal_connect (app->app_interface, "handle-activate-message",
                    G_CALLBACK (messaging_menu_app_activate_message), app);
  g_signal_connect (app->app_interface, "handle-dismiss",
                    G_CALLBACK (messaging_menu_app_dismiss), app);

  app->messages = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  app->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                    "com.canonical.indicator.messages",
                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                    indicator_messages_appeared,
                                    indicator_messages_vanished,
                                    app,
                                    NULL);
}

/**
 * messaging_menu_new:
 * @desktop_id: a desktop file id. See g_desktop_app_info_new()
 *
 * Creates a new #MessagingMenuApp for the application associated with
 * @desktop_id.
 *
 * The application will not show up (nor be marked as "running") in the
 * Messaging Menu before messaging_menu_app_register() has been called.
 *
 * Returns: (transfer full): a new #MessagingMenuApp
 */
MessagingMenuApp *
messaging_menu_app_new (const gchar *desktop_id)
{
  return g_object_new (MESSAGING_MENU_TYPE_APP,
                       "desktop-id", desktop_id,
                       NULL);
}

/**
 * messaging_menu_app_register:
 * @app: a #MessagingMenuApp
 *
 * Registers @app with the Messaging Menu.
 *
 * If the application doesn't already have a section in the Messaging
 * Menu, one will be created for it.  The application will also be
 * marked as "running".
 *
 * The application will be marked as "not running" as soon as @app is
 * destroyed.  The application launcher as well as shortcut actions will
 * remain in the menu.  To completely remove the application section
 * from the Messaging Menu, call messaging_menu_app_unregister().
 */
void
messaging_menu_app_register (MessagingMenuApp *app)
{
  gchar *object_path;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));

  app->registered = TRUE;

  /* state will be synced right after connecting to the service */
  if (!app->messages_service)
    return;

  object_path = messaging_menu_app_get_dbus_object_path (app);
  if (!object_path)
    return;

  indicator_messages_service_call_register_application (app->messages_service,
                                                        g_app_info_get_id (G_APP_INFO (app->appinfo)),
                                                        object_path,
                                                        app->cancellable,
                                                        NULL, NULL);

  g_free (object_path);
}

/**
 * messaging_menu_app_unregister:
 * @app: a #MessagingMenuApp
 *
 * Completely removes the @app from the Messaging Menu.  If the
 * application's launcher and shortcut actions should remain in the
 * menu, destroying @app with g_object_unref() suffices.
 *
 * Note: @app will remain valid and usable after this call.
 */
void
messaging_menu_app_unregister (MessagingMenuApp *app)
{
  g_return_if_fail (MESSAGING_MENU_IS_APP (app));

  app->registered = FALSE;

  /* state will be synced right after connecting to the service */
  if (!app->messages_service)
    return;

  if (!app->appinfo)
    return;

  indicator_messages_service_call_unregister_application (app->messages_service,
                                                          g_app_info_get_id (G_APP_INFO (app->appinfo)),
                                                          app->cancellable,
                                                          NULL, NULL);
}

/**
 * messaging_menu_app_set_status:
 * @app: a #MessagingMenuApp
 * @status: a #MessagingMenuStatus
 *
 * Notify the Messaging Menu that the chat status of @app has changed to
 * @status.
 *
 * Connect to the ::status-changed signal to receive notification about
 * the user changing their global chat status through the Messaging
 * Menu.
 *
 * This function does nothing for applications whose desktop file does
 * not include X-MessagingMenu-UsesChatSection.
 */
void
messaging_menu_app_set_status (MessagingMenuApp    *app,
                               MessagingMenuStatus  status)
{
  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (status >= MESSAGING_MENU_STATUS_AVAILABLE &&
                    status <= MESSAGING_MENU_STATUS_OFFLINE);

  app->status = status;
  app->status_set = TRUE;

  /* state will be synced right after connecting to the service */
  if (!app->messages_service)
    return;

  if (!app->appinfo)
    return;

  indicator_messages_service_call_set_status (app->messages_service,
                                              g_app_info_get_id (G_APP_INFO (app->appinfo)),
                                              status_ids [status],
                                              app->cancellable,
                                              NULL, NULL);
}

static int
status_from_string (const gchar *s)
{
  int i;

  if (!s)
    return -1;

  for (i = 0; i <= MESSAGING_MENU_STATUS_OFFLINE; i++)
    {
      if (g_str_equal (s, status_ids[i]))
        return i;
    }

  return -1;
}

static void
global_status_changed (IndicatorMessagesService *service,
                       const gchar *status_str,
                       gpointer user_data)
{
  MessagingMenuApp *app = user_data;
  int status;

  status = status_from_string (status_str);
  g_return_if_fail (status >= 0);

  g_signal_emit (app, signals[STATUS_CHANGED], 0, status);
}

static Source *
messaging_menu_app_lookup_source (MessagingMenuApp *app,
                                  const gchar      *id)
{
  GList *node;

  node = g_list_find_custom (app->sources, id, compare_source_id);

  return node ? node->data : NULL;
}

static Source *
messaging_menu_app_get_source (MessagingMenuApp *app,
                               const gchar      *id)
{
  Source *source;

  source = messaging_menu_app_lookup_source (app, id);
  if (!source)
    g_warning ("a source with id '%s' doesn't exist", id);

  return source;
}

static void
messaging_menu_app_notify_source_changed (MessagingMenuApp *app,
                                          Source           *source)
{
  indicator_messages_application_emit_source_changed (app->app_interface,
                                                      source_to_variant (source));
}

static void
messaging_menu_app_insert_source_internal (MessagingMenuApp *app,
                                           gint              position,
                                           const gchar      *id,
                                           GIcon            *icon,
                                           const gchar      *label,
                                           guint             count,
                                           gint64            time,
                                           const gchar      *string)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (id != NULL);
  g_return_if_fail (label != NULL);

  if (messaging_menu_app_lookup_source (app, id))
    {
      g_warning ("a source with id '%s' already exists", id);
      return;
    }

  source = g_slice_new0 (Source);
  source->id = g_strdup (id);
  source->label = g_strdup (label);
  if (icon)
    source->icon = g_object_ref (icon);
  source->count = count;
  source->time = time;
  source->string = g_strdup (string);
  app->sources = g_list_insert (app->sources, source, position);

  indicator_messages_application_emit_source_added (app->app_interface,
                                                    position,
                                                    source_to_variant (source));
}

/**
 * messaging_menu_app_insert_source:
 * @app: a #MessagingMenuApp
 * @position: the position at which to insert the source
 * @id: a unique identifier for the source to be added
 * @icon: the icon associated with the source
 * @label: a user-visible string best describing the source
 *
 * Inserts a new message source into the section representing @app.  Equivalent
 * to calling messaging_menu_app_insert_source_with_time() with the current
 * time.
 *
 * It is an error to insert a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_insert_source (MessagingMenuApp *app,
                                  gint              position,
                                  const gchar      *id,
                                  GIcon            *icon,
                                  const gchar      *label)
{
  messaging_menu_app_insert_source_with_time (app, position, id, icon, label,
                                              g_get_real_time ());
}

/**
 * messaging_menu_app_append_source:
 * @app: a #MessagingMenuApp
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 *
 * Appends a new message source to the end of the section representing @app.
 * Equivalent to calling messaging_menu_app_append_source_with_time() with the
 * current time.
 *
 * It is an error to add a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_append_source (MessagingMenuApp *app,
                                  const gchar      *id,
                                  GIcon            *icon,
                                  const gchar      *label)
{
  messaging_menu_app_insert_source (app, -1, id, icon, label);
}

/**
 * messaging_menu_app_insert_source_with_count:
 * @app: a #MessagingMenuApp
 * @position: the position at which to insert the source
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 * @count: the count for the source
 *
 * Inserts a new message source into the section representing @app and
 * initializes it with @count.
 *
 * To update the count, use messaging_menu_app_set_source_count().
 *
 * It is an error to insert a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_insert_source_with_count (MessagingMenuApp *app,
                                             gint              position,
                                             const gchar      *id,
                                             GIcon            *icon,
                                             const gchar      *label,
                                             guint             count)
{
  messaging_menu_app_insert_source_internal (app, position, id, icon, label, count, 0, "");
}

/**
 * messaging_menu_app_append_source_with_count:
 * @app: a #MessagingMenuApp
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 * @count: the count for the source
 *
 * Appends a new message source to the end of the section representing @app and
 * initializes it with @count.
 *
 * To update the count, use messaging_menu_app_set_source_count().
 *
 * It is an error to add a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void messaging_menu_app_append_source_with_count (MessagingMenuApp *app,
                                                  const gchar      *id,
                                                  GIcon            *icon,
                                                  const gchar      *label,
                                                  guint             count)
{
  messaging_menu_app_insert_source_with_count (app, -1, id, icon, label, count);
}

/**
 * messaging_menu_app_insert_source_with_time:
 * @app: a #MessagingMenuApp
 * @position: the position at which to insert the source
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 * @time: the time when the source was created, in microseconds
 *
 * Inserts a new message source into the section representing @app and
 * initializes it with @time.  Use messaging_menu_app_insert_source() to
 * insert a source with the current time.
 *
 * To change the time, use messaging_menu_app_set_source_time().
 *
 * It is an error to insert a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_insert_source_with_time (MessagingMenuApp *app,
                                            gint              position,
                                            const gchar      *id,
                                            GIcon            *icon,
                                            const gchar      *label,
                                            gint64            time)
{
  messaging_menu_app_insert_source_internal (app, position, id, icon, label, 0, time, "");
}

/**
 * messaging_menu_app_append_source_with_time:
 * @app: a #MessagingMenuApp
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 * @time: the time when the source was created, in microseconds
 *
 * Appends a new message source to the end of the section representing
 * @app and initializes it with @time.  Use
 * messaging_menu_app_append_source() to append a source with the
 * current time.
 *
 * To change the time, use messaging_menu_app_set_source_time().
 *
 * It is an error to insert a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_append_source_with_time (MessagingMenuApp *app,
                                            const gchar      *id,
                                            GIcon            *icon,
                                            const gchar      *label,
                                            gint64            time)
{
  messaging_menu_app_insert_source_with_time (app, -1, id, icon, label, time);
}

/**
 * messaging_menu_app_insert_source_with_string:
 * @app: a #MessagingMenuApp
 * @position: the position at which to insert the source
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 * @str: a string associated with the source
 *
 * Inserts a new message source into the section representing @app and
 * initializes it with @str.
 *
 * To update the string, use messaging_menu_app_set_source_string().
 *
 * It is an error to insert a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_insert_source_with_string (MessagingMenuApp *app,
                                              gint              position,
                                              const gchar      *id,
                                              GIcon            *icon,
                                              const gchar      *label,
                                              const gchar      *str)
{
  messaging_menu_app_insert_source_internal (app, position, id, icon, label, 0, 0, str);
}

/**
 * messaging_menu_app_append_source_with_string:
 * @app: a #MessagingMenuApp
 * @id: a unique identifier for the source to be added
 * @icon: (allow-none): the icon associated with the source
 * @label: a user-visible string best describing the source
 * @str: a string associated with the source
 *
 * Appends a new message source to the end of the section representing @app and
 * initializes it with @str.
 *
 * To update the string, use messaging_menu_app_set_source_string().
 *
 * It is an error to insert a source with an @id which already exists.  Use
 * messaging_menu_app_has_source() to find out whether there is such a source.
 */
void
messaging_menu_app_append_source_with_string (MessagingMenuApp *app,
                                              const gchar      *id,
                                              GIcon            *icon,
                                              const gchar      *label,
                                              const gchar      *str)
{
  messaging_menu_app_insert_source_with_string (app, -1, id, icon, label, str);
}

/**
 * messaging_menu_app_remove_source:
 * @app: a #MessagingMenuApp
 * @source_id: the id of the source to remove
 *
 * Removes the source corresponding to @source_id from the menu.
 */
void
messaging_menu_app_remove_source (MessagingMenuApp *app,
                                  const gchar      *source_id)
{
  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  if (messaging_menu_app_remove_source_internal (app, source_id))
    indicator_messages_application_emit_source_removed (app->app_interface, source_id);
}

/**
 * messaging_menu_app_has_source:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 *
 * Returns: TRUE if there is a source associated with @source_id
 */
gboolean
messaging_menu_app_has_source (MessagingMenuApp *app,
                               const gchar      *source_id)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_APP (app), FALSE);
  g_return_val_if_fail (source_id != NULL, FALSE);

  return messaging_menu_app_lookup_source (app, source_id) != NULL;
}

/**
 * messaging_menu_app_set_source_label:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 * @label: the new label for the source
 *
 * Changes the label of @source_id to @label.
 */
void
messaging_menu_app_set_source_label (MessagingMenuApp *app,
                                     const gchar      *source_id,
                                     const gchar      *label)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);
  g_return_if_fail (label != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      g_free (source->label);
      source->label = g_strdup (label);
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_set_source_icon:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 * @icon: (allow-none): the new icon for the source
 *
 * Changes the icon of @source_id to @icon.
 */
void
messaging_menu_app_set_source_icon (MessagingMenuApp *app,
                                    const gchar      *source_id,
                                    GIcon            *icon)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      g_clear_object (&source->icon);
      if (icon)
        source->icon = g_object_ref (icon);
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_set_source_count:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 * @count: the new count for the source
 *
 * Updates the count of @source_id to @count.
 */
void messaging_menu_app_set_source_count (MessagingMenuApp *app,
                                          const gchar      *source_id,
                                          guint             count)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      source->count = count;
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_set_source_time:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 * @time: the new time for the source, in microseconds
 *
 * Updates the time of @source_id to @time.
 */
void
messaging_menu_app_set_source_time (MessagingMenuApp *app,
                                    const gchar      *source_id,
                                    gint64            time)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      source->time = time;
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_set_source_string:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 * @str: the new string for the source
 *
 * Updates the string displayed next to @source_id to @str.
 */
void
messaging_menu_app_set_source_string (MessagingMenuApp *app,
                                      const gchar      *source_id,
                                      const gchar      *str)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      g_free (source->string);
      source->string = g_strdup (str);
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_draw_attention:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 *
 * Indicates that @source_id has important unread messages.  Currently, this
 * means that the messaging menu's envelope icon will turn blue.
 *
 * Use messaging_menu_app_remove_attention() to stop indicating that the source
 * needs attention.
 */
void
messaging_menu_app_draw_attention (MessagingMenuApp *app,
                                   const gchar      *source_id)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      source->draws_attention = TRUE;
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_remove_attention:
 * @app: a #MessagingMenuApp
 * @source_id: a source id
 *
 * Stop indicating that @source_id needs attention.
 *
 * This function does not need to be called when the source is removed
 * with messaging_menu_app_remove_source() or the user has activated the
 * source.
 *
 * Use messaging_menu_app_draw_attention() to make @source_id draw attention
 * again.
 */
void
messaging_menu_app_remove_attention (MessagingMenuApp *app,
                                     const gchar      *source_id)
{
  Source *source;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (source_id != NULL);

  source = messaging_menu_app_get_source (app, source_id);
  if (source)
    {
      source->draws_attention = FALSE;
      messaging_menu_app_notify_source_changed (app, source);
    }
}

/**
 * messaging_menu_app_append_message:
 * @app: a #MessagingMenuApp
 * @msg: the #MessagingMenuMessage to append
 * @source_id: (allow-none): the source id to which @msg is added, or NULL
 * @notify: whether a notification bubble should be shown for this
 *          message
 *
 * Appends @msg to the source with id @source_id of @app.  The messaging
 * menu might not display this message immediately if other messages are
 * queued before this one.
 *
 * If @source_id has a count associated with it, that count will be
 * increased by one.
 *
 * If @source_id is %NULL, @msg won't be associated with a source.
 */
void
messaging_menu_app_append_message (MessagingMenuApp     *app,
                                   MessagingMenuMessage *msg,
                                   const gchar          *source_id,
                                   gboolean              notify)
{
  const gchar *id;

  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (MESSAGING_MENU_IS_MESSAGE (msg));

  id = messaging_menu_message_get_id (msg);

  if (g_hash_table_lookup (app->messages, id))
    {
      g_warning ("a message with id '%s' already exists", id);
      return;
    }

  g_hash_table_insert (app->messages, g_strdup (id), g_object_ref (msg));
  indicator_messages_application_emit_message_added (app->app_interface,
                                                     _messaging_menu_message_to_variant (msg));

  if (source_id)
    {
      Source *source;

      source = messaging_menu_app_get_source (app, source_id);
      if (source && source->count >= 0)
        {
          source->count++;
          messaging_menu_app_notify_source_changed (app, source);
        }
    }
}

/**
 * messaging_menu_app_get_message:
 * @app: a #MessagingMenuApp
 * @id: id of the message to retrieve
 *
 * Retrieves the message with @id, that was added with
 * messaging_menu_app_append_message().
 *
 * Returns: (transfer none) (allow-none): the #MessagingMenuApp with
 * @id, or %NULL
 */
MessagingMenuMessage *
messaging_menu_app_get_message (MessagingMenuApp *app,
                                const gchar      *id)
{
  g_return_val_if_fail (MESSAGING_MENU_IS_APP (app), NULL);
  g_return_val_if_fail (id != NULL, NULL);

  return g_hash_table_lookup (app->messages, id);
}

/**
 * messaging_menu_app_remove_message:
 * @app: a #MessagingMenuApp
 * @msg: the #MessagingMenuMessage to remove
 *
 * Removes @msg from @app.
 *
 * If @source_id has a count associated with it, that count will be
 * decreased by one.
 */
void
messaging_menu_app_remove_message (MessagingMenuApp     *app,
                                   MessagingMenuMessage *msg)
{
  /* take a ref of @msg here to make sure the pointer returned by
   * _get_id() is valid for the duration of remove_message_by_id. */
  g_object_ref (msg);
  messaging_menu_app_remove_message_by_id (app, messaging_menu_message_get_id (msg));
  g_object_unref (msg);
}

/**
 * messaging_menu_app_remove_message_by_id:
 * @app: a #MessagingMenuApp
 * @id: the unique id of @msg
 *
 * Removes the message with the id @id from @app.
 *
 * If @source_id has a count associated with it, that count will be
 * decreased by one.
 */
void
messaging_menu_app_remove_message_by_id (MessagingMenuApp     *app,
                                         const gchar          *id)
{
  g_return_if_fail (MESSAGING_MENU_IS_APP (app));
  g_return_if_fail (id != NULL);

  if (messaging_menu_app_remove_message_internal (app, id))
    indicator_messages_application_emit_message_removed (app->app_interface, id);
}
