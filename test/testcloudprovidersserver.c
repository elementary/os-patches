#include <glib.h>
#include <stdlib.h>
#include <gio/gio.h>
#include <cloudprovidersproviderexporter.h>
#include <cloudprovidersaccountexporter.h>
/* for CLoudProviderStatus enum */
#include <cloudprovidersaccount.h>


#define TIMEOUT 800
#define COUNT_PLACEHOLDER_ACCOUNTS 3
#define TEST_CLOUD_PROVIDERS_BUS_NAME "org.freedesktop.CloudProviders.ServerExample"
#define TEST_CLOUD_PROVIDERS_OBJECT_PATH "/org/freedesktop/CloudProviders/ServerExample"

#define CLOUD_PROVIDERS_TYPE_TEST_SERVER (cloud_providers_test_server_get_type())
G_DECLARE_FINAL_TYPE (CloudProvidersTestServer, cloud_providers_test_server, CLOUD_PROVIDERS, TEST_SERVER, GObject);

struct _CloudProvidersTestServerClass
{
  GObjectClass parent_class;
};

struct _CloudProvidersTestServer
{
  GObject parent_instance;

  GHashTable *accounts;
  gchar *name;
  GIcon *icon;
  gchar *path;
  guint timeout_handler;
  GDBusConnection *connection;
  CloudProvidersProviderExporter *exporter;
};

G_DEFINE_TYPE (CloudProvidersTestServer, cloud_providers_test_server, G_TYPE_OBJECT);

static CloudProvidersTestServer*
cloud_providers_test_server_new (void)
{
  CloudProvidersTestServer *self;

  self = g_object_new (CLOUD_PROVIDERS_TYPE_TEST_SERVER, NULL);

  return self;
}

static void
test_cloud_provider_finalize (GObject *object)
{
  CloudProvidersTestServer *self = CLOUD_PROVIDERS_TEST_SERVER (object);

  g_hash_table_unref (self->accounts);
  g_free (self->name);
  g_free (self->path);
  g_clear_object (&self->icon);

  G_OBJECT_CLASS (cloud_providers_test_server_parent_class)->finalize (object);
}

static void
cloud_providers_test_server_init (CloudProvidersTestServer *self)
{
  GFile *icon_file;
  gchar *current_dir;
  gchar *uri;

  current_dir = g_get_current_dir ();

  self->accounts = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  self->name = "MyCloud";
  self->path = g_strdup (current_dir);
  uri = g_build_filename (current_dir, "icon.svg", NULL);
  icon_file = g_file_new_for_uri (uri);
  self->icon = g_file_icon_new (icon_file);

  g_object_unref (icon_file);
  g_free (uri);
  g_free (current_dir);
}

static void
cloud_providers_test_server_class_init (CloudProvidersTestServerClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = test_cloud_provider_finalize;
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
  CloudProvidersTestServer *self = (CloudProvidersTestServer *)user_data;
  CloudProvidersAccountExporter *account;
  GRand *rand;
  gint new_status;
  gint account_id;

  rand = g_rand_new ();
  account_id = g_rand_int_range (rand, 0, COUNT_PLACEHOLDER_ACCOUNTS);
  new_status = g_rand_int_range (rand,
                                 CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE,
                                 CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR + 1);

  g_print ("Change status of %03d to %d\n", account_id, new_status);
  account = g_hash_table_lookup (self->accounts, GINT_TO_POINTER (account_id));
  cloud_providers_account_exporter_set_status (account, new_status);

  return TRUE;
}

static gchar *
get_status_details (CloudProvidersAccountStatus status)
{
    gchar *description = "";
    switch (status) {
      case CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE:
        description = "Details: Sync idle";
        break;
      case CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING:
        description = "Details: Syncing";
        break;
      case CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR:
        description = "Details: Error";
        break;
      case CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID:
        description = "Details: Sync status details not set";
        break;
    }
    return description;
}

static gboolean
add_accounts (CloudProvidersTestServer *self)
{
  guint n;

  // export multiple accounts as DBus objects to the bus
  for (n = 0; n < COUNT_PLACEHOLDER_ACCOUNTS; n++)
    {
      g_autoptr (CloudProvidersAccountExporter) account = NULL;
      g_autofree gchar *account_object_name = NULL;
      g_autofree gchar *account_name = NULL;

      account_object_name = g_strdup_printf ("MyAccount%d", n);
      account_name = g_strdup_printf ("MyAccount %d", n);
      g_debug ("Adding account %s", account_name);
      account = cloud_providers_account_exporter_new (self->exporter,
                                                      account_object_name);

      cloud_providers_provider_exporter_add_account (self->exporter, account);

      cloud_providers_account_exporter_set_name (account, account_name);
      cloud_providers_account_exporter_set_icon (account, self->icon);
      cloud_providers_account_exporter_set_path (account, self->path);
      cloud_providers_account_exporter_set_status (account,
                                                   CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID);
      cloud_providers_account_exporter_set_status_details (account,
                                                           get_status_details (CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID));
      cloud_providers_account_exporter_set_menu_model (account, get_model ());
      cloud_providers_account_exporter_set_action_group (account, get_action_group ());
      g_hash_table_insert (self->accounts, GINT_TO_POINTER (n), account);
    }

    return G_SOURCE_REMOVE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
    CloudProvidersTestServer *self = CLOUD_PROVIDERS_TEST_SERVER (user_data);

    g_debug ("Bus adquired: %s\n", name);

    g_debug ("Registering cloud provider server 'MyCloud'\n");

    self->connection = connection;
    self->exporter = cloud_providers_provider_exporter_new(self->connection,
                                                           TEST_CLOUD_PROVIDERS_BUS_NAME,
                                                           TEST_CLOUD_PROVIDERS_OBJECT_PATH);
    cloud_providers_provider_exporter_set_name (self->exporter, "My cloud");
    add_accounts (self);

}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  CloudProvidersTestServer *self = (CloudProvidersTestServer *)user_data;
  self->timeout_handler = g_timeout_add (TIMEOUT,
                                         (GSourceFunc) change_random_cloud_provider_state,
                                         self);
  g_debug ("Server test name adquired");
  change_random_cloud_provider_state (self);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
    g_critical ("Name lost: %s\n", name);
}

int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  CloudProvidersTestServer *test_cloud_provider;
  guint owner_id;

  test_cloud_provider = cloud_providers_test_server_new ();

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             TEST_CLOUD_PROVIDERS_BUS_NAME,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             test_cloud_provider,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

    g_debug("going oooooout/n");
  g_bus_unown_name (owner_id);
  g_object_unref (test_cloud_provider);

  return 0;
}

