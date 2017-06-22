#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <cloudprovider.h>
#include <cloudprovidermanager.h>

#define TIMEOUT 2000

typedef struct _TestCloudProviderClass TestCloudProviderClass;
typedef struct _TestCloudProvider TestCloudProvider;

struct _TestCloudProviderClass
{
  GObjectClass parent_class;
};

struct _TestCloudProvider
{
  GObject parent_instance;

  gchar *name;
  gint status;
  GIcon *icon;
  gchar *path;
  CloudProviderManager1 *manager_proxy;
  guint timeout_handler;
};


static GType test_cloud_provider_get_type (void);
G_DEFINE_TYPE (TestCloudProvider, test_cloud_provider, G_TYPE_OBJECT);

static void
test_cloud_provider_finalize (GObject *object)
{
  TestCloudProvider *self = (TestCloudProvider*)object;

  g_free (self->name);
  g_free (self->path);
  g_clear_object (&self->icon);
  g_clear_object (&self->manager_proxy);

  G_OBJECT_CLASS (test_cloud_provider_parent_class)->finalize (object);
}

static void
test_cloud_provider_init (TestCloudProvider *self)
{
  GFile *icon_file;
  gchar *current_dir;
  gchar *uri;

  current_dir = g_get_current_dir ();

  self->name = "MyCloud";
  self->path = g_strdup (current_dir);
  self->status = CLOUD_PROVIDER_STATUS_INVALID;
  uri = g_build_filename (current_dir, "apple-red.png", NULL);
  icon_file = g_file_new_for_uri (uri);
  self->icon = g_file_icon_new (icon_file);

  g_object_unref (icon_file);
  g_free (uri);
  g_free (current_dir);
}

static void
test_cloud_provider_class_init (TestCloudProviderClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = test_cloud_provider_finalize;
}

static void
test_cloud_provider_set_status (TestCloudProvider *self,
                           gint           status)
{
  /* Inform the manager that the provider changed */
  self->status = status;
  cloud_provider_manager1_call_cloud_provider_changed(self->manager_proxy,
							  NULL,
							  NULL,
							  NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

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

struct menu {
  GMenu *mainMenu;
  GMenuItem *website;
  GMenuItem *photos;
  GMenuItem *notes;
  GMenuItem *allowSync;
  GMenuItem *buy;
};

static GMenuModel *
get_model (void)
{
  GMenu *section;
  struct menu *m;
  GMenuItem *item;
  GMenu *submenu;

  m = g_new0(struct menu, 1);
  m->mainMenu = g_menu_new();

  section = g_menu_new();
  m->website = g_menu_item_new("MyCloud website", "website");
  g_menu_append_item(section, m->website);
  m->photos = g_menu_item_new("MyCloud photos", "photos");
  g_menu_append_item(section, m->photos);
  m->notes = g_menu_item_new("MyCloud notes", "notes");
  g_menu_append_item(section, m->notes);
  g_menu_append_section(m->mainMenu, NULL, G_MENU_MODEL(section));

  section = g_menu_new();
  m->allowSync = g_menu_item_new("Allow Synchronization", "allow-sync");
  g_menu_append_item(section, m->allowSync);

  submenu = g_menu_new();
  item = g_menu_item_new("5GB", "5");
  g_menu_append_item(submenu, item);
  item = g_menu_item_new("10GB", "10");
  g_menu_append_item(submenu, item);
  item = g_menu_item_new("50GB", "50");
  g_menu_append_item(submenu, item);
  item = g_menu_item_new_submenu("Buy storage", G_MENU_MODEL(submenu));
  g_menu_append_item(section, item);
  g_menu_append_section(m->mainMenu, NULL, G_MENU_MODEL(section));

  return G_MENU_MODEL(m->mainMenu);
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
  TestCloudProvider *cloud_provider = user_data;

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
                                                       "/org/freedesktop/CloudProviderServerExample",
                                                       cloud_provider1_interface_info(),
                                                       &interface_vtable,
                                                       cloud_provider,
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */

  g_assert (registration_id > 0);
  /* Export a menu for our own application */
  export_menu (connection, "/org/freedesktop/CloudProviderServerExample");
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
  TestCloudProvider *cloud_provider = (TestCloudProvider *)user_data;
  GRand *rand;
  gint new_status;

  g_print("Send change_provider message to bus\n");

  rand = g_rand_new ();
  new_status = g_rand_int_range (rand,
                                 CLOUD_PROVIDER_STATUS_IDLE,
                                 CLOUD_PROVIDER_STATUS_ERROR + 1);

  test_cloud_provider_set_status (cloud_provider, new_status);

  return TRUE;
}

static void
on_manager_proxy_created (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  TestCloudProvider *cloud_provider = user_data;
  GError *error = NULL;

  cloud_provider->manager_proxy = cloud_provider_manager1_proxy_new_for_bus_finish (res, &error);
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

  cloud_provider = g_object_new (test_cloud_provider_get_type (), NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.freedesktop.CloudProviderServerExample",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             cloud_provider,
                             NULL);

  /* Create CloudProviderManager proxy for exporting cloud provider changes */
  cloud_provider_manager1_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            "org.freedesktop.CloudProviderManager",
                            "/org/freedesktop/CloudProviderManager",
                            NULL,
                            on_manager_proxy_created,
                            cloud_provider);


  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);

  g_object_unref (cloud_provider);

  return 0;
}

