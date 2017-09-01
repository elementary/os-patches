#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <cloudprovider.h>
#include <cloudprovideraccountexporter.h>
/* for CLoudProviderStatus enum */
#include <cloudproviderproxy.h>


#define TIMEOUT 800
#define COUNT_PLACEHOLDER_ACCOUNTS 3
#define TEST_CLOUD_PROVIDER_BUS_NAME "org.freedesktop.CloudProviderServerExample"
#define TEST_CLOUD_PROVIDER_OBJECT_PATH "/org/freedesktop/CloudProviderServerExample"

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
  guint timeout_handler;
  GDBusConnection *connection;
  CloudProvider *cloud_provider;
  GDBusObjectManagerServer *manager;
};


static GType test_cloud_provider_get_type (void);
G_DEFINE_TYPE (TestCloudProvider, test_cloud_provider, G_TYPE_OBJECT);

TestCloudProvider*
test_cloud_provider_new (const gchar *name)
{
  TestCloudProvider *self;

  self = g_object_new (test_cloud_provider_get_type(), NULL);
  self->name = g_strdup(name);

  return self;
}

static void
test_cloud_provider_finalize (GObject *object)
{
  TestCloudProvider *self = (TestCloudProvider*)object;

  g_free (self->name);
  g_free (self->path);
  g_clear_object (&self->icon);

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
  uri = g_build_filename (current_dir, "icon.png", NULL);
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
  self->status = status;
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
  m->website = g_menu_item_new("MyCloud website", "cloudprovider.website");
  g_menu_append_item(section, m->website);
  m->photos = g_menu_item_new("MyCloud photos", "cloudprovider.photos");
  g_menu_append_item(section, m->photos);
  m->notes = g_menu_item_new("MyCloud notes", "cloudprovider.notes");
  g_menu_append_item(section, m->notes);
  g_menu_append_section(m->mainMenu, NULL, G_MENU_MODEL(section));

  section = g_menu_new();
  m->allowSync = g_menu_item_new("Allow Synchronization", "cloudprovider.allow-sync");
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

static gboolean
change_random_cloud_provider_state (gpointer user_data)
{
  TestCloudProvider *test_cloud_provider = (TestCloudProvider *)user_data;
  GRand *rand;
  gint new_status;
  gint account_id;
  gchar *account_object_name;

  rand = g_rand_new ();
  account_id = g_rand_int_range (rand, 0, COUNT_PLACEHOLDER_ACCOUNTS);
  new_status = g_rand_int_range (rand,
                                 CLOUD_PROVIDER_STATUS_IDLE,
                                 CLOUD_PROVIDER_STATUS_ERROR + 1);

  account_object_name = g_strdup_printf ("MyCloud%d", account_id);
  g_print ("Change status of %03d to %d\n", account_id, new_status);
  test_cloud_provider_set_status (test_cloud_provider, new_status);
  cloud_provider_emit_changed (test_cloud_provider->cloud_provider, account_object_name);
  return TRUE;
}


static gchar *
on_get_name (CloudProviderAccountExporter *account,
             gpointer              user_data)
{
  gchar *name = (gchar*)user_data;
  return name;
}

static GIcon *
on_get_icon (CloudProviderAccountExporter *account,
             gpointer              user_data)
{
    TestCloudProvider *self = user_data;
    return self->icon;
}

static gchar *
on_get_path (CloudProviderAccountExporter *account,
             gpointer              user_data)
{
    TestCloudProvider *self = user_data;
    return self->path;
}

static guint
on_get_status (CloudProviderAccountExporter *account,
               gpointer              user_data)
{
    TestCloudProvider *self = user_data;
    return self->status;
}

static gchar *
on_get_status_details (CloudProviderAccountExporter *account,
                       gpointer              user_data)
{
    gchar *description = "";
    TestCloudProvider *self = user_data;
    switch (self->status) {
      case CLOUD_PROVIDER_STATUS_IDLE:
        description = "Sync idle";
        break;
      case CLOUD_PROVIDER_STATUS_SYNCING:
        description = "Syncing";
        break;
      case CLOUD_PROVIDER_STATUS_ERROR:
        description = "Error";
        break;
    }
    return description;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  TestCloudProvider *self = user_data;
  guint n;

  self->connection = connection;
  self->cloud_provider = cloud_provider_new(self->connection,
                                            TEST_CLOUD_PROVIDER_BUS_NAME,
                                            TEST_CLOUD_PROVIDER_OBJECT_PATH);

  g_debug ("Registering cloud provider server 'MyCloud'\n");

  // export multiple accounts as DBus objects to the bus
  for (n = 0; n < COUNT_PLACEHOLDER_ACCOUNTS; n++)
    {
      gchar *account_object_name = g_strdup_printf ("MyCloud%d", n);
      gchar *account_name = g_strdup_printf ("MyCloud %d", n);

      CloudProviderAccountExporter *cloud_provider_account_exporter = cloud_provider_account_exporter_new(account_object_name);
      g_signal_connect(cloud_provider_account_exporter, "handle_get_name", G_CALLBACK (on_get_name), account_name);
      g_signal_connect(cloud_provider_account_exporter, "handle_get_icon", G_CALLBACK (on_get_icon), self);
      g_signal_connect(cloud_provider_account_exporter, "handle_get_path", G_CALLBACK (on_get_path), self);
      g_signal_connect(cloud_provider_account_exporter, "handle_get_status", G_CALLBACK (on_get_status), self);
      g_signal_connect(cloud_provider_account_exporter, "handle_get_status_details", G_CALLBACK (on_get_status_details), self);

      cloud_provider_add_account(self->cloud_provider, cloud_provider_account_exporter);
      cloud_provider_export_menu (self->cloud_provider, account_object_name, get_model ());
      cloud_provider_export_action_group (self->cloud_provider, account_object_name, get_action_group ());

      g_free(account_object_name);
    }

  cloud_provider_export_objects (self->cloud_provider);

  return;
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  TestCloudProvider *self = (TestCloudProvider *)user_data;
  self->timeout_handler = g_timeout_add (TIMEOUT,
                                         (GSourceFunc) change_random_cloud_provider_state,
                                         self);
  change_random_cloud_provider_state (self);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  exit (1);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  TestCloudProvider *test_cloud_provider;
  guint owner_id;

  test_cloud_provider = g_object_new (test_cloud_provider_get_type (), NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             TEST_CLOUD_PROVIDER_BUS_NAME,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             test_cloud_provider,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);
  g_object_unref (test_cloud_provider);

  return 0;
}

