#include <glib.h>
#include <cloudprovidermanager.h>

gint
main (gint   argc, gchar *argv[])
{
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);
  CloudProviderManager *manager = cloud_provider_manager_dup_singleton ();
  g_main_loop_run(loop);
  g_free(manager);
  g_free(loop);
  return 0;
}
