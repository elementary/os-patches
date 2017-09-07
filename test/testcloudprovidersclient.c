#include <glib.h>
#include <cloudprovidersaccount.h>
#include <cloudproviderscollector.h>
#include <cloudprovidersprovider.h>

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

static gchar*
get_status_string (CloudProvidersAccountStatus status)
{
  gchar *status_string;

  switch (status)
    {
    case CLOUD_PROVIDERS_ACCOUNT_STATUS_INVALID:
      status_string = "invalid";
      break;

    case CLOUD_PROVIDERS_ACCOUNT_STATUS_IDLE:
      status_string = "idle";
      break;

    case CLOUD_PROVIDERS_ACCOUNT_STATUS_SYNCING:
      status_string = "syncing";
      break;

    case CLOUD_PROVIDERS_ACCOUNT_STATUS_ERROR:
      status_string = "error";
      break;

    default:
      g_assert_not_reached ();
    }

  return g_strdup (status_string);
}

static void
on_account_status_changed (CloudProvidersAccount *account)
{
    g_autofree gchar* status_string = NULL;
    GIcon *icon;
    g_autofree gchar *icon_representation = NULL;
    GMenuModel *menu;

    icon = cloud_providers_account_get_icon (account);
    icon_representation = g_icon_to_string (icon);
    status_string = get_status_string (cloud_providers_account_get_status (account));
    g_print ("Account: Name - %s, Status - %s (%s), Path - %s, Icon - %s\n",
             cloud_providers_account_get_name (account),
             status_string,
             cloud_providers_account_get_status_details (account),
             cloud_providers_account_get_path (account),
             icon_representation);
    menu = cloud_providers_account_get_menu_model (account);
    g_print ("\nMenu\n");
    print_gmenu_model (menu);
}

static void
on_provider_accounts_changed (CloudProvidersProvider *provider)
{
    GList *l;
    gint status;
    gchar *status_string;
    GIcon *icon;
    gchar *icon_representation;
    GList *accounts;
    GMenuModel *menu;

    accounts = cloud_providers_provider_get_accounts (provider);
    for (l = accounts; l != NULL; l = l->next)
    {
        CloudProvidersAccount *account;

        account = CLOUD_PROVIDERS_ACCOUNT (l->data);
        g_signal_connect (account,
                          "notify::status",
                          G_CALLBACK (on_account_status_changed),
                          NULL);
        status = cloud_providers_account_get_status (account);
        status_string = get_status_string (status);
        icon = cloud_providers_account_get_icon (account);
        icon_representation = icon != NULL ? g_icon_to_string (icon) : "no icon";

        g_print ("Account: Name - %s, Status - %s (%s), Path - %s, Icon - %s\n",
                 cloud_providers_account_get_name (account),
                 status_string,
                 cloud_providers_account_get_status_details (account),
                 cloud_providers_account_get_path (account),
                 icon_representation);

        g_free (icon_representation);

        menu = cloud_providers_account_get_menu_model (account);
        g_print ("\nMenu\n");
        print_gmenu_model (menu);
    }
}

static void
on_provider_name_changed (CloudProvidersProvider *provider)
{
    g_print ("Provider changed: %s\n", cloud_providers_provider_get_name (provider));
}

static void
on_collector_changed (CloudProvidersCollector *collector)
{
    GList *providers;
    GList *accounts;
    GList *l;
    GList *l2;
    gint status;
    gchar *status_string;
    GIcon *icon;
    gchar *icon_representation;
    GMenuModel *menu;

    providers = cloud_providers_collector_get_providers (collector);
    if (providers == NULL)
    {
        return;
    }

  for (l = providers; l != NULL; l = l->next)
  {
      CloudProvidersProvider *provider;

      provider = CLOUD_PROVIDERS_PROVIDER (l->data);
      g_print ("Provider data for %s\n", cloud_providers_provider_get_name (provider));
      g_print ("--------------------------\n");
      accounts = cloud_providers_provider_get_accounts (provider);
      for (l2 = accounts; l2 != NULL; l2 = l2->next)
      {
          CloudProvidersAccount *account;

          account = CLOUD_PROVIDERS_ACCOUNT (l2->data);
          g_signal_connect_swapped (account,
                                    "notify::status",
                                    G_CALLBACK (on_account_status_changed),
                                    NULL);
          status = cloud_providers_account_get_status (account);
          status_string = get_status_string (status);
          icon = cloud_providers_account_get_icon (account);
          icon_representation = g_icon_to_string (icon);

          g_print ("Account: Name - %s, Status - %s (%s), Path - %s, Icon - %s\n",
                   cloud_providers_account_get_name (account),
                   status_string,
                   cloud_providers_account_get_status_details (account),
                   cloud_providers_account_get_path (account),
                   icon_representation);

          g_free (icon_representation);

          menu = cloud_providers_account_get_menu_model (account);
          g_print ("\nMenu\n");
          print_gmenu_model (menu);
      }

      g_signal_connect_swapped (provider, "accounts-changed",
                                G_CALLBACK (on_provider_accounts_changed), provider);
      g_signal_connect_swapped (provider, "notify::name",
                                G_CALLBACK (on_provider_name_changed), provider);
  }
  g_print ("\n");
}

gint
main (gint   argc,
      gchar *argv[])
{
  CloudProvidersCollector *collector;
  GMainLoop *loop = g_main_loop_new(NULL, FALSE);

  collector = cloud_providers_collector_dup_singleton ();
  g_signal_connect_swapped (collector, "providers-changed",
                            G_CALLBACK (on_collector_changed), collector);
  on_collector_changed (collector);

  g_print("Waiting for cloud providers\n\n");

  g_main_loop_run(loop);
  g_free(loop);

  return 0;
}
