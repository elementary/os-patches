/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2020 Endless Mobile, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#include <act/act.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "carousel.h"
#include "user-image.h"
#include "user-selector.h"


static void reload_users (MctUserSelector *self,
                          ActUser         *selected_user);
static void notify_is_loaded_cb (GObject    *obj,
                                 GParamSpec *pspec,
                                 gpointer    user_data);
static void user_added_cb (ActUserManager *user_manager,
                           ActUser        *user,
                           gpointer        user_data);
static void user_changed_or_removed_cb (ActUserManager *user_manager,
                                        ActUser        *user,
                                        gpointer        user_data);
static void carousel_item_activated (MctCarousel     *carousel,
                                     MctCarouselItem *item,
                                     gpointer         user_data);


/**
 * MctUserSelector:
 *
 * The user selector is a widget which lists available user accounts and allows
 * the user to select one.
 *
 * Since: 0.5.0
 */
struct _MctUserSelector
{
  GtkBox parent_instance;

  MctCarousel *carousel;

  ActUserManager *user_manager;  /* (owned) */
  ActUser *user;  /* (owned) */
  gboolean show_administrators;
};

G_DEFINE_TYPE (MctUserSelector, mct_user_selector, GTK_TYPE_BOX)

typedef enum
{
  PROP_USER = 1,
  PROP_USER_MANAGER,
  PROP_SHOW_ADMINISTRATORS,
} MctUserSelectorProperty;

static GParamSpec *properties[PROP_SHOW_ADMINISTRATORS + 1];

static void
mct_user_selector_constructed (GObject *obj)
{
  MctUserSelector *self = MCT_USER_SELECTOR (obj);

  g_assert (self->user_manager != NULL);

  g_signal_connect (self->user_manager, "user-changed",
                    G_CALLBACK (user_changed_or_removed_cb), self);
  g_signal_connect (self->user_manager, "user-is-logged-in-changed",
                    G_CALLBACK (user_changed_or_removed_cb), self);
  g_signal_connect (self->user_manager, "user-added",
                    G_CALLBACK (user_added_cb), self);
  g_signal_connect (self->user_manager, "user-removed",
                    G_CALLBACK (user_changed_or_removed_cb), self);
  g_signal_connect (self->user_manager, "notify::is-loaded",
                    G_CALLBACK (notify_is_loaded_cb), self);

  /* Start loading the user accounts. */
  notify_is_loaded_cb (G_OBJECT (self->user_manager), NULL, self);

  G_OBJECT_CLASS (mct_user_selector_parent_class)->constructed (obj);
}

static void
mct_user_selector_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  MctUserSelector *self = MCT_USER_SELECTOR (object);

  switch ((MctUserSelectorProperty) prop_id)
    {
    case PROP_USER:
      g_value_set_object (value, self->user);
      break;

    case PROP_USER_MANAGER:
      g_value_set_object (value, self->user_manager);
      break;

    case PROP_SHOW_ADMINISTRATORS:
      g_value_set_boolean (value, self->show_administrators);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mct_user_selector_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  MctUserSelector *self = MCT_USER_SELECTOR (object);

  switch ((MctUserSelectorProperty) prop_id)
    {
    case PROP_USER:
      /* Currently read only */
      g_assert_not_reached ();
      break;

    case PROP_USER_MANAGER:
      g_assert (self->user_manager == NULL);
      self->user_manager = g_value_dup_object (value);
      break;

    case PROP_SHOW_ADMINISTRATORS:
      self->show_administrators = g_value_get_boolean (value);
      reload_users (self, NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
mct_user_selector_dispose (GObject *object)
{
  MctUserSelector *self = (MctUserSelector *)object;

  g_clear_object (&self->user);

  if (self->user_manager != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->user_manager, notify_is_loaded_cb, self);
      g_signal_handlers_disconnect_by_func (self->user_manager, user_changed_or_removed_cb, self);
      g_signal_handlers_disconnect_by_func (self->user_manager, user_added_cb, self);

      g_clear_object (&self->user_manager);
    }

  G_OBJECT_CLASS (mct_user_selector_parent_class)->dispose (object);
}

static void
mct_user_selector_class_init (MctUserSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = mct_user_selector_constructed;
  object_class->get_property = mct_user_selector_get_property;
  object_class->set_property = mct_user_selector_set_property;
  object_class->dispose = mct_user_selector_dispose;

  /**
   * MctUserSelector:user: (nullable)
   *
   * The currently selected user account, or %NULL if no user is selected.
   * Currently read only but may become writable in future.
   *
   * Since: 0.5.0
   */
  properties[PROP_USER] =
      g_param_spec_object ("user",
                           "User",
                           "The currently selected user account, or %NULL if no user is selected.",
                           ACT_TYPE_USER,
                           G_PARAM_READABLE |
                           G_PARAM_STATIC_STRINGS |
                           G_PARAM_EXPLICIT_NOTIFY);

  /**
   * MctUserSelector:user-manager: (not nullable)
   *
   * The user manager providing the data for the widget.
   *
   * Since: 0.5.0
   */
  properties[PROP_USER_MANAGER] =
      g_param_spec_object ("user-manager",
                           "User Manager",
                           "The user manager providing the data for the widget.",
                           ACT_TYPE_USER_MANAGER,
                           G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_STATIC_STRINGS |
                           G_PARAM_EXPLICIT_NOTIFY);

  /**
   * MctUserSelector:show-administrators:
   *
   * Whether to show administrators in the list, or hide them.
   *
   * Since: 0.5.0
   */
  properties[PROP_SHOW_ADMINISTRATORS] =
      g_param_spec_boolean ("show-administrators",
                            "Show Administrators?",
                            "Whether to show administrators in the list, or hide them.",
                            TRUE,
                            G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/freedesktop/MalcontentControl/ui/user-selector.ui");

  gtk_widget_class_bind_template_child (widget_class, MctUserSelector, carousel);

  gtk_widget_class_bind_template_callback (widget_class, carousel_item_activated);
}

static void
mct_user_selector_init (MctUserSelector *self)
{
  self->show_administrators = TRUE;

  /* Ensure the types used in the UI are registered. */
  g_type_ensure (MCT_TYPE_CAROUSEL);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
notify_is_loaded_cb (GObject    *obj,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  MctUserSelector *self = MCT_USER_SELECTOR (user_data);
  gboolean is_loaded;

  /* The implementation of #ActUserManager guarantees that once is-loaded is
   * true, it is never reset to false. */
  g_object_get (self->user_manager, "is-loaded", &is_loaded, NULL);
  if (is_loaded)
    reload_users (self, NULL);
}

static const gchar *
get_real_or_user_name (ActUser *user)
{
  const gchar *name;

  name = act_user_get_real_name (user);
  if (name == NULL)
    name = act_user_get_user_name (user);

  return name;
}

static void
carousel_item_activated (MctCarousel     *carousel,
                         MctCarouselItem *item,
                         gpointer         user_data)
{
  MctUserSelector *self = MCT_USER_SELECTOR (user_data);
  uid_t uid;
  ActUser *user = NULL;

  g_clear_object (&self->user);

  uid = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "uid"));
  user = act_user_manager_get_user_by_id (self->user_manager, uid);

  if (g_set_object (&self->user, user))
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USER]);
}

static gint
sort_users (gconstpointer a,
            gconstpointer b)
{
  ActUser *ua, *ub;
  gint result;

  ua = ACT_USER ((gpointer) a);
  ub = ACT_USER ((gpointer) b);

  /* Make sure the current user is shown first */
  if (act_user_get_uid (ua) == getuid ())
    {
      result = G_MININT32;
    }
  else if (act_user_get_uid (ub) == getuid ())
    {
      result = G_MAXINT32;
    }
  else
    {
      g_autofree gchar *name1 = NULL, *name2 = NULL;

      name1 = g_utf8_collate_key (get_real_or_user_name (ua), -1);
      name2 = g_utf8_collate_key (get_real_or_user_name (ub), -1);

      result = strcmp (name1, name2);
    }

  return result;
}

static gint
user_compare (gconstpointer i,
              gconstpointer u)
{
  MctCarouselItem *item;
  ActUser *user;
  gint uid_a, uid_b;
  gint result;

  item = (MctCarouselItem *) i;
  user = ACT_USER ((gpointer) u);

  uid_a = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "uid"));
  uid_b = act_user_get_uid (user);

  result = uid_a - uid_b;

  return result;
}

static void
reload_users (MctUserSelector *self,
              ActUser         *selected_user)
{
  ActUser *user;
  g_autoptr(GSList) list = NULL;
  GSList *l;
  MctCarouselItem *item = NULL;
  GtkSettings *settings;
  gboolean animations;

  settings = gtk_widget_get_settings (GTK_WIDGET (self->carousel));

  g_object_get (settings, "gtk-enable-animations", &animations, NULL);
  g_object_set (settings, "gtk-enable-animations", FALSE, NULL);

  mct_carousel_purge_items (self->carousel);

  list = act_user_manager_list_users (self->user_manager);
  g_debug ("Got %u users", g_slist_length (list));

  list = g_slist_sort (list, (GCompareFunc) sort_users);
  for (l = list; l; l = l->next)
    {
      user = l->data;

      if (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR &&
          !self->show_administrators)
        {
          g_debug ("Ignoring administrator %s", get_real_or_user_name (user));
          continue;
        }

      g_debug ("Adding user %s", get_real_or_user_name (user));
      user_added_cb (self->user_manager, user, self);
    }

  if (selected_user)
    item = mct_carousel_find_item (self->carousel, selected_user, user_compare);
  mct_carousel_select_item (self->carousel, item);

  g_object_set (settings, "gtk-enable-animations", animations, NULL);

  gtk_revealer_set_reveal_child (GTK_REVEALER (self->carousel), TRUE);
}

static GtkWidget *
create_carousel_entry (MctUserSelector *self,
                       ActUser         *user)
{
  GtkWidget *box, *widget;
  gchar *label;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  widget = mct_user_image_new ();
  mct_user_image_set_user (MCT_USER_IMAGE (widget), user);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

  label = g_strdup_printf ("<b>%s</b>",
                           get_real_or_user_name (user));
  widget = gtk_label_new (label);
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
  gtk_widget_set_margin_top (widget, 5);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
  g_free (label);

  if (act_user_get_uid (user) == getuid ())
    label = g_strdup_printf ("<small>%s</small>", _("Your account"));
  else
    label = g_strdup (" ");

  widget = gtk_label_new (label);
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  g_free (label);

  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget),
                               "dim-label");

  return box;
}

static void
user_added_cb (ActUserManager *user_manager,
               ActUser        *user,
               gpointer        user_data)
{
  MctUserSelector *self = MCT_USER_SELECTOR (user_data);
  GtkWidget *item, *widget;

  if (act_user_is_system_account (user) ||
      (act_user_get_account_type (user) == ACT_USER_ACCOUNT_TYPE_ADMINISTRATOR &&
       !self->show_administrators))
    return;

  g_debug ("User added: %u %s", (guint) act_user_get_uid (user), get_real_or_user_name (user));

  widget = create_carousel_entry (self, user);
  item = mct_carousel_item_new ();
  gtk_container_add (GTK_CONTAINER (item), widget);

  g_object_set_data (G_OBJECT (item), "uid", GINT_TO_POINTER (act_user_get_uid (user)));
  gtk_container_add (GTK_CONTAINER (self->carousel), item);
}

static void
user_changed_or_removed_cb (ActUserManager *user_manager,
                            ActUser        *user,
                            gpointer        user_data)
{
  MctUserSelector *self = MCT_USER_SELECTOR (user_data);

  reload_users (self, self->user);
}

/**
 * mct_user_selector_new:
 * @user_manager: (transfer none): an #ActUserManager to provide the user data
 *
 * Create a new #MctUserSelector widget.
 *
 * Returns: (transfer full): a new user selector
 * Since: 0.5.0
 */
MctUserSelector *
mct_user_selector_new (ActUserManager *user_manager)
{
  g_return_val_if_fail (ACT_IS_USER_MANAGER (user_manager), NULL);

  return g_object_new (MCT_TYPE_USER_SELECTOR,
                       "user-manager", user_manager,
                       NULL);
}

/**
 * mct_user_selector_get_user:
 * @self: an #MctUserSelector
 *
 * Get the currently selected user, or %NULL if no user is selected.
 *
 * Returns: (transfer none) (nullable): the currently selected user
 * Since: 0.5.0
 */
ActUser *
mct_user_selector_get_user (MctUserSelector *self)
{
  g_return_val_if_fail (MCT_IS_USER_SELECTOR (self), NULL);

  return self->user;
}

/**
 * mct_user_selector_select_user_by_username:
 * @self: an #MctUserSelector
 * @username: username of the user to select
 *
 * Selects the given @username in the widget. This might fail if @username isn’t
 * a valid user, or if they aren’t listed in the selector due to being an
 * administrator (see #MctUserSelector:show-administrators).
 *
 * Returns: %TRUE if the user was successfully selected, %FALSE otherwise
 * Since: 0.10.0
 */
gboolean
mct_user_selector_select_user_by_username (MctUserSelector *self,
                                           const gchar     *username)
{
  MctCarouselItem *item = NULL;
  ActUser *user = NULL;

  g_return_val_if_fail (MCT_IS_USER_SELECTOR (self), FALSE);
  g_return_val_if_fail (username != NULL && *username != '\0', FALSE);

  user = act_user_manager_get_user (self->user_manager, username);
  if (user == NULL)
    return FALSE;

  item = mct_carousel_find_item (self->carousel, user, user_compare);
  if (item == NULL)
    return FALSE;

  mct_carousel_select_item (self->carousel, item);

  return TRUE;
}
