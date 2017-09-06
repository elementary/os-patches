#include <glib.h>
#include <cloudprovidermanager.h>

static GMainLoop *loop;
static CloudProviderManager *manager;
static guint dbus_owner_id;

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (name != NULL && name[0] != '\0');

  g_debug ("Connected to the session bus");
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_return_if_fail (connection == NULL || G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (name != NULL && name[0] != '\0');

  g_info ("Lost (or failed to acquire) the name %s on the session message bus", name);
  g_main_loop_quit (loop);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (name != NULL && name[0] != '\0');

  g_debug ("Acquired the name %s on the session message bus", name);

  manager = cloud_provider_manager_new (connection);
  cloud_provider_manager_export (manager);
}

gint
main (gint   argc, gchar *argv[])
{
  loop = g_main_loop_new(NULL, FALSE);

  dbus_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  CLOUD_PROVIDER_MANAGER_DBUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
                                  on_bus_acquired,
                                  on_name_acquired,
                                  on_name_lost,
                                  NULL,
                                  NULL);
  g_main_loop_run(loop);
  g_object_unref(manager);
  g_free(loop);
  return 0;
}
