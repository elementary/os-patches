#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gtkcloudprovider.h>

#define TIMEOUT 2000

typedef struct _CloudProviderClass CloudProviderClass;
typedef struct _CloudProvider CloudProvider;

struct _CloudProviderClass
{
  GObjectClass parent_class;
};

struct _CloudProvider
{
  GObject parent_instance;

  gchar *name;
  gint status;
  GIcon *icon;
  gchar *path;
  GDBusProxy *manager_proxy;
  guint timeout_handler;
};


static GType cloud_provider_get_type (void);
G_DEFINE_TYPE (CloudProvider, cloud_provider, G_TYPE_OBJECT);

static void
cloud_provider_finalize (GObject *object)
{
  CloudProvider *self = (CloudProvider*)object;

  g_free (self->name);
  g_free (self->path);
  g_clear_object (&self->icon);
  g_clear_object (&self->manager_proxy);

  G_OBJECT_CLASS (cloud_provider_parent_class)->finalize (object);
}

static void
cloud_provider_init (CloudProvider *self)
{
  GFile *icon_file;
  gchar *current_dir;
  gchar *uri;

  current_dir = g_get_current_dir ();

  self->name = "MyCloud";
  self->path = g_strdup (current_dir);
  self->status = GTK_CLOUD_PROVIDER_STATUS_INVALID;
  uri = g_build_filename (current_dir, "apple-red.png", NULL);
  icon_file = g_file_new_for_uri (uri);
  self->icon = g_file_icon_new (icon_file);

  g_object_unref (icon_file);
  g_free (uri);
  g_free (current_dir);
}

static void
cloud_provider_class_init (CloudProviderClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = cloud_provider_finalize;
}

static void
cloud_provider_set_status (CloudProvider *self,
                           gint           status)
{
  /* Inform the manager that the provider changed */
  self->status = status;
  g_dbus_proxy_call (self->manager_proxy,
                     "CloudProviderChanged",
                     g_variant_new ("()"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar provider_xml[] =
  "<node>"
  "  <interface name='org.gtk.CloudProvider'>"
  "    <method name='GetName'>"
  "      <arg type='s' name='name' direction='out'/>"
  "    </method>"
  "    <method name='GetStatus'>"
  "      <arg type='i' name='status' direction='out'/>"
  "    </method>"
  "    <method name='GetIcon'>"
  "      <arg type='v' name='icon' direction='out'/>"
  "    </method>"
  "    <method name='GetPath'>"
  "      <arg type='s' name='path' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static const gchar manager_xml[] =
  "<node>"
  "  <interface name='org.gtk.CloudProviderManager'>"
  "    <method name='CloudProviderChanged'>"
  "    </method>"
  "  </interface>"
  "</node>";

static const gchar menu_markup[] =
  "<interface>\n"
  "<menu id='menu'>\n"
  "  <section>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>MyCloud website</attribute>\n"
  "      <attribute name='action'>actions.website</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>MyCloud Photos</attribute>\n"
  "      <attribute name='action'>actions.photos</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>MyCloud Notes</attribute>\n"
  "      <attribute name='action'>actions.notes</attribute>\n"
  "    </item>\n"
  "  </section>\n"
  "  <section>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Allow Synchronization</attribute>\n"
  "      <attribute name='action'>actions.allow-sync</attribute>\n"
  "    </item>\n"
  "    <submenu>\n"
  "      <attribute name='label' translatable='yes'>Buy Storage</attribute>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>5GB for 200CZK</attribute>\n"
  "        <attribute name='action'>actions.buy</attribute>\n"
  "        <attribute name='target'>5</attribute>\n"
  "      </item>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>10GB for 500CZK</attribute>\n"
  "        <attribute name='action'>actions.buy</attribute>\n"
  "        <attribute name='target'>10</attribute>\n"
  "      </item>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>30GB for 600CZK</attribute>\n"
  "        <attribute name='action'>actions.buy</attribute>\n"
  "        <attribute name='target'>30</attribute>\n"
  "      </item>\n"
  "    </submenu>\n"
  "  </section>\n"
  "</menu>\n"
  "</interface>\n";

static void
activate_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  g_print ("Action %s activated\n", g_action_get_name (G_ACTION (action)));
}

static void
activate_toggle (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GVariant *old_state, *new_state;

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_boolean (!g_variant_get_boolean (old_state));

  g_print ("Toggle action %s activated, state changes from %d to %d\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_boolean (old_state),
           g_variant_get_boolean (new_state));

  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static void
activate_radio (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GVariant *old_state, *new_state;

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_string (g_variant_get_string (parameter, NULL));

  g_print ("Radio action %s activated, state changes from %s to %s\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_string (old_state, NULL),
           g_variant_get_string (new_state, NULL));

  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static GActionEntry actions[] = {
  { "website",  activate_action, NULL, NULL, NULL },
  { "photos",  activate_action, NULL, NULL, NULL },
  { "notes",   activate_action, NULL, NULL, NULL },
  { "allow-sync",  activate_toggle, NULL, "true", NULL },
  { "buy",  activate_radio,  "s",  NULL, NULL },
};

static GMenuModel *
get_model (void)
{
  GError *error = NULL;
  GtkBuilder *builder;
  GMenuModel *menu;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, menu_markup, -1, &error);
  g_assert_no_error (error);

  menu = g_object_ref (gtk_builder_get_object (builder, "menu"));
  g_object_unref (builder);

  return menu;
}

static GActionGroup *
get_action_group (void)
{
  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (group),
                                   actions,
                                   G_N_ELEMENTS (actions), NULL);

  return G_ACTION_GROUP (group);
}

static void
export_menu (GDBusConnection *bus,
             gchar *object_path)
{
  GMenuModel *model;
  GActionGroup *action_group;
  GError *error = NULL;

  model = get_model ();
  action_group = get_action_group ();

  g_print ("Exporting menus on the bus...\n");
  if (!g_dbus_connection_export_menu_model (bus, object_path, model, &error))
    {
      g_warning ("Menu export failed: %s", error->message);
      exit (1);
    }
  g_print ("Exporting actions on the bus...\n");
  if (!g_dbus_connection_export_action_group (bus, object_path, action_group, &error))
    {
      g_warning ("Action export failed: %s", error->message);
      exit (1);
    }
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  CloudProvider *cloud_provider = user_data;

  g_debug ("Handling dbus call in server\n");
  if (g_strcmp0 (method_name, "GetName") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", cloud_provider->name));
    }
  else if (g_strcmp0 (method_name, "GetStatus") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(i)", cloud_provider->status));
    }
  else if (g_strcmp0 (method_name, "GetIcon") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(v)", g_icon_serialize (cloud_provider->icon)));
    }
  else if (g_strcmp0 (method_name, "GetPath") == 0)
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(s)", cloud_provider->path));
    }
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
};

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  CloudProvider *cloud_provider = user_data;
  guint registration_id;

  g_debug ("Registering cloud provider server 'MyCloud'\n");
  registration_id = g_dbus_connection_register_object (connection,
                                                       "/org/gtk/CloudProviderServerExample",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       cloud_provider,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);
  /* Export a menu for our own application */
  export_menu (connection, "/org/gtk/CloudProviderServerExample");
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

static gboolean
change_provider (gpointer user_data)
{
  CloudProvider *cloud_provider = (CloudProvider *)user_data;
  GRand *rand;
  gint new_status;

  g_print("Send change_provider message to bus\n");

  rand = g_rand_new ();
  new_status = g_rand_int_range (rand,
                                 GTK_CLOUD_PROVIDER_STATUS_IDLE,
                                 GTK_CLOUD_PROVIDER_STATUS_ERROR + 1);

  cloud_provider_set_status (cloud_provider, new_status);

  return TRUE;
}

static void
on_manager_proxy_created (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CloudProvider *cloud_provider = user_data;
  GError *error = NULL;

  cloud_provider->manager_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    g_warning ("Error creating proxy for cloud provider manager %s", error->message);
  else
    g_print ("Manager proxy created for 'MyCloud'\n");

  cloud_provider->timeout_handler = g_timeout_add (TIMEOUT,
                                                   (GSourceFunc) change_provider,
                                                   cloud_provider);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  CloudProvider *cloud_provider;
  guint owner_id;
  GDBusNodeInfo *proxy_info;
  GDBusInterfaceInfo *interface_info;
  GError *error = NULL;

  /* Export the interface we listen to, so clients can request properties of
   * the cloud provider such as name, status or icon */
  introspection_data = g_dbus_node_info_new_for_xml (provider_xml, NULL);
  g_assert (introspection_data != NULL);

  cloud_provider = g_object_new (cloud_provider_get_type (), NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.CloudProviderServerExample",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             cloud_provider,
                             NULL);

  /* Create CloudProviderManager proxy for exporting cloud provider changes */
  proxy_info = g_dbus_node_info_new_for_xml (manager_xml, &error);
  interface_info = g_dbus_node_info_lookup_interface (proxy_info, "org.gtk.CloudProviderManager");
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            interface_info,
                            "org.gtk.CloudProviderManager",
                            "/org/gtk/CloudProviderManager",
                            "org.gtk.CloudProviderManager",
                            NULL,
                            on_manager_proxy_created,
                            cloud_provider);


  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_dbus_node_info_unref (introspection_data);

  g_object_unref (cloud_provider);

  return 0;
}

