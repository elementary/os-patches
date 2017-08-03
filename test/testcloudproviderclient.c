#include <glib.h>
#include <cloudproviderproxy.h>
#include <cloudprovidermanager.h>
#include <cloudproviders.h>

static void
print_gmenu_model (GMenuModel  *model)
{
  gint i, n_items;
  GMenuModel *submodel = NULL;
  gchar *label;

  n_items = g_menu_model_get_n_items (model);

  for (i = 0; i < n_items; i++)
    {
      label = NULL;
      if (g_menu_model_get_item_attribute (model, i, G_MENU_ATTRIBUTE_LABEL, "s", &label))
        {
          g_print ("Menu item - %s\n", label);
          if (label != NULL)
            g_free (label);
        }

      submodel = g_menu_model_get_item_link (model, i, G_MENU_LINK_SECTION);
      if (!submodel)
       submodel = g_menu_model_get_item_link (model, i, G_MENU_LINK_SUBMENU);

      if (!submodel)
          continue;
      g_print ("---------\n");
      print_gmenu_model (submodel);
      g_print ("---------\n");
      g_clear_object (&submodel);
  }
}

static void
on_manager_changed (CloudProviders *manager)
{
  GList *providers;
  GList *l;
  gint provider_status;
  gchar *status_string;
  GIcon *icon;
  gchar *icon_representation;
  GMenuModel *menu;

  providers = cloud_providers_get_providers (manager);
  if(providers == NULL)
    return;
  for (l = providers; l != NULL; l = l->next)
    {
      if(!cloud_provider_proxy_is_available(CLOUD_PROVIDER_PROXY(l->data))) {
         continue;
      }
      g_print ("Providers data\n");
      g_print ("##############\n");
      provider_status = cloud_provider_proxy_get_status (CLOUD_PROVIDER_PROXY (l->data));
      switch (provider_status)
        {
        case CLOUD_PROVIDER_STATUS_INVALID:
          status_string = "invalid";
          break;

        case CLOUD_PROVIDER_STATUS_IDLE:
          status_string = "idle";
          break;

        case CLOUD_PROVIDER_STATUS_SYNCING:
          status_string = "syncing";
          break;

        case CLOUD_PROVIDER_STATUS_ERROR:
          status_string = "error";
          break;

        default:
          g_assert_not_reached ();
        }

      icon = cloud_provider_proxy_get_icon (l->data);
      icon_representation = g_icon_to_string (icon);

      g_print ("Name - %s, Status - %s (%s), Path - %s, Icon - %s\n",
               cloud_provider_proxy_get_name (CLOUD_PROVIDER_PROXY (l->data)),
               status_string,
               cloud_provider_proxy_get_status_details (CLOUD_PROVIDER_PROXY (l->data)),
               cloud_provider_proxy_get_path (CLOUD_PROVIDER_PROXY (l->data)),
               icon_representation);

      g_free (icon_representation);

      menu = cloud_provider_proxy_get_menu_model (l->data);
      g_print ("\nMenu\n");
      print_gmenu_model (menu);
    }
  g_print ("\n");
}

gint
main (gint   argc,
      gchar *argv[])
{
  CloudProviders *manager;
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  manager = cloud_providers_dup_singleton ();
  g_signal_connect (manager, "changed", G_CALLBACK (on_manager_changed), manager);
  cloud_providers_update (manager);

  g_print("Waiting for cloud providers\n\n");

  g_main_loop_run(loop);
  g_free(loop);

  return 0;
}
