/*
Copyright 2011 Canonical Ltd.

Authors:
    Conor Curran <conor.curran@canonical.com>
    Mirco Müller <mirco.mueller@canonical.com>
    Charles Kerr <charles.kerr@canonical.com>

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License version 3, as published
by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranties of
MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <gtk/gtk.h>

#include "idousermenuitem.h"
#include "idoactionhelper.h"

#define FALLBACK_ICON_NAME "avatar-default"

enum
{
  PROP_0,
  PROP_LABEL,
  PROP_ICON,
  PROP_IS_LOGGED_IN,
  PROP_IS_CURRENT_USER,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _IdoUserMenuItemPrivate
{
  GtkWidget* user_image;
  GtkWidget* user_name;
  GtkWidget* container;
  GtkWidget* tick_icon;
  gboolean is_logged_in;
  gboolean is_current_user;
  gchar * label;
  GIcon * icon;
};

G_DEFINE_TYPE (IdoUserMenuItem, ido_user_menu_item, GTK_TYPE_MENU_ITEM);

/* Prototypes */
static gboolean ido_user_menu_item_primitive_draw_cb_gtk_3 (GtkWidget * image,
                                                            cairo_t   * cr,
                                                            gpointer    gself);

/***
****  GObject virtual functions
***/

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IdoUserMenuItem * self = IDO_USER_MENU_ITEM (o);

  switch (property_id)
    {
      case PROP_LABEL:
        g_value_set_string (value, self->priv->label);
        break;

      case PROP_ICON:
        g_value_set_object (value, self->priv->icon);
        break;

      case PROP_IS_LOGGED_IN:
        g_value_set_boolean (value, self->priv->is_logged_in);
        break;

      case PROP_IS_CURRENT_USER:
        g_value_set_boolean (value, self->priv->is_current_user);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
        break;
    }
}

static void
my_set_property (GObject       * o,
                 guint           property_id,
                 const GValue  * value,
                 GParamSpec    * pspec)
{
  IdoUserMenuItem * self = IDO_USER_MENU_ITEM (o);

  switch (property_id)
    {
      case PROP_LABEL:
        ido_user_menu_item_set_label (self, g_value_get_string (value));
        break;

      case PROP_ICON:
        ido_user_menu_item_set_icon (self, g_value_get_object (value));
        break;

      case PROP_IS_LOGGED_IN:
        ido_user_menu_item_set_logged_in (self, g_value_get_boolean (value));
        break;

      case PROP_IS_CURRENT_USER:
        self->priv->is_current_user = g_value_get_boolean (value);
        gtk_widget_queue_draw (GTK_WIDGET(self));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
        break;
    }
}

static void
my_dispose (GObject *object)
{
  IdoUserMenuItem * self = IDO_USER_MENU_ITEM (object);

  g_clear_object (&self->priv->icon);

  G_OBJECT_CLASS (ido_user_menu_item_parent_class)->dispose (object);
}

static void
my_finalize (GObject *object)
{
  IdoUserMenuItem * self = IDO_USER_MENU_ITEM (object);

  g_free (self->priv->label);

  G_OBJECT_CLASS (ido_user_menu_item_parent_class)->finalize (object);
}

static void
ido_user_menu_item_class_init (IdoUserMenuItemClass *klass)
{
  GParamFlags prop_flags;
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (IdoUserMenuItemPrivate));

  gobject_class->get_property = my_get_property;
  gobject_class->set_property = my_set_property;
  gobject_class->dispose = my_dispose;
  gobject_class->finalize = my_finalize;

  prop_flags = G_PARAM_CONSTRUCT
             | G_PARAM_READWRITE
             | G_PARAM_STATIC_STRINGS;

  properties[PROP_LABEL] = g_param_spec_string ("label",
                                                "The user's name",
                                                "The name to display",
                                                "J. Random User",
                                                prop_flags);

  properties[PROP_ICON] = g_param_spec_object ("icon",
                                               "Icon",
                                               "The user's GIcon",
                                               G_TYPE_OBJECT,
                                               prop_flags);

  properties[PROP_IS_LOGGED_IN] = g_param_spec_boolean ("is-logged-in",
                                                        "is logged in",
                                                        "is user logged in?",
                                                        FALSE,
                                                        prop_flags);

  properties[PROP_IS_CURRENT_USER] = g_param_spec_boolean ("is-current-user",
                                                           "is current user",
                                                           "is user current?",
                                                           FALSE,
                                                           prop_flags);

  g_object_class_install_properties (gobject_class, PROP_LAST, properties);
}

static void
ido_user_menu_item_init (IdoUserMenuItem *self)
{
  IdoUserMenuItemPrivate * priv;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            IDO_USER_MENU_ITEM_TYPE,
                                            IdoUserMenuItemPrivate);

  priv = self->priv;

  // Create the UI elements.
  priv->user_image = gtk_image_new ();
  gtk_image_set_from_icon_name (GTK_IMAGE (priv->user_image),
                                FALLBACK_ICON_NAME,
                                GTK_ICON_SIZE_MENU);

  priv->user_name = gtk_label_new (NULL);

  priv->container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  priv->tick_icon = gtk_image_new_from_icon_name ("account-logged-in",
                                                   GTK_ICON_SIZE_MENU);
  gtk_misc_set_alignment(GTK_MISC(priv->tick_icon), 1.0, 0.5);

  // Pack it together
  gtk_box_pack_start (GTK_BOX (priv->container),
                      priv->user_image,
                      FALSE,
                      TRUE,
                      0);
  gtk_box_pack_start (GTK_BOX (priv->container),
                      priv->user_name,
                      FALSE,
                      FALSE,
                      3);
  gtk_box_pack_end (GTK_BOX(priv->container),
                    priv->tick_icon,
                    FALSE,
                    FALSE, 5);

  gtk_widget_show_all (priv->container);
  gtk_container_add (GTK_CONTAINER (self), priv->container);
  gtk_widget_show_all (priv->tick_icon);
  gtk_widget_set_no_show_all (priv->tick_icon, TRUE);
  gtk_widget_hide (priv->tick_icon);


  // Fetch the drawing context.
  g_signal_connect_after (GTK_WIDGET(self), "draw",
                          G_CALLBACK(ido_user_menu_item_primitive_draw_cb_gtk_3),
                          GTK_WIDGET(self));
}


/*****************************************************************/

static gboolean
ido_user_menu_item_primitive_draw_cb_gtk_3 (GtkWidget * widget,
                                            cairo_t   * cr,
                                            gpointer    user_data)
{
  IdoUserMenuItemPrivate * priv;

  g_return_val_if_fail(IS_IDO_USER_MENU_ITEM(user_data), FALSE);

  priv = IDO_USER_MENU_ITEM(user_data)->priv;

  /* Draw dot only when user is the current user. */
  if (priv->is_current_user)
    {
      GtkStyleContext * style_context;
      GtkStateFlags state_flags;
      GdkRGBA color;
      gdouble x, y;

      /* get the foreground color */
      style_context = gtk_widget_get_style_context (widget);
      state_flags = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_color (style_context, state_flags, &color);

      GtkAllocation allocation;
      gtk_widget_get_allocation (widget, &allocation);
      x = allocation.x + 13;
      y = allocation.height / 2;

      cairo_arc (cr, x, y, 3.0, 0.0, 2 * G_PI);

      gdk_cairo_set_source_rgba (cr, &color);

      cairo_fill (cr);
    }

  return FALSE;
}

/***
****  Avatar
***/
static gboolean
ido_user_menu_item_set_icon_from_file_icon (IdoUserMenuItem *self,
                                            GFileIcon       *icon)
{
  GFile *file;
  gchar *path;
  gint width;
  gint height;
  GdkPixbuf *pixbuf;

  file = g_file_icon_get_file (G_FILE_ICON (icon));
  path = g_file_get_path (file);

  /* width and height will always be set by this function */
  gtk_icon_size_lookup_for_settings (gtk_widget_get_settings (GTK_WIDGET (self)),
                                     GTK_ICON_SIZE_MENU,
                                     &width, &height);

  pixbuf = gdk_pixbuf_new_from_file_at_scale (path, width, height, TRUE, NULL);
  g_free (path);

  if (pixbuf)
    {
      gtk_image_set_from_pixbuf (GTK_IMAGE (self->priv->user_image), pixbuf);
      g_object_unref (pixbuf);
      return TRUE;
    }

  return FALSE;
}

/***
****  PUBLIC API
***/

void
ido_user_menu_item_set_icon (IdoUserMenuItem * self, GIcon * icon)
{
  IdoUserMenuItemPrivate * p = self->priv;

  if (p->icon == icon)
    return;

  g_clear_object (&p->icon);

  if (icon)
    p->icon = g_object_ref (icon);

  /* Avatars are always loaded from disk. Show the fallback when no icon
   * is set, the icon is not a file icon, or the file could not be
   * found.
   */
  if (icon == NULL ||
      !G_IS_FILE_ICON (icon) ||
      !ido_user_menu_item_set_icon_from_file_icon (self, G_FILE_ICON (icon)))
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (p->user_image),
                                    FALLBACK_ICON_NAME,
                                    GTK_ICON_SIZE_MENU);
    }
}

void
ido_user_menu_item_set_icon_from_file (IdoUserMenuItem * self, const char * filename)
{
  GFile * file = filename ? g_file_new_for_path (filename) : NULL;
  GIcon * icon = file ? g_file_icon_new (file) : NULL;

  ido_user_menu_item_set_icon (self, icon);

  g_clear_object (&icon);
  g_clear_object (&file);
}

void
ido_user_menu_item_set_logged_in (IdoUserMenuItem * self, gboolean is_logged_in)
{
  gtk_widget_set_visible (self->priv->tick_icon, is_logged_in);
}

void
ido_user_menu_item_set_current_user (IdoUserMenuItem * self, gboolean is_current_user)
{
  self->priv->is_current_user = is_current_user;
  gtk_widget_queue_draw (GTK_WIDGET (self));
}

void
ido_user_menu_item_set_label (IdoUserMenuItem * self, const char * label)
{
  gtk_label_set_label (GTK_LABEL(self->priv->user_name), label);
}

GtkWidget*
ido_user_menu_item_new (void)
{
  return GTK_WIDGET (g_object_new (IDO_USER_MENU_ITEM_TYPE, NULL));
}

/***
****
***/

/*
 * This is a helper function for creating user menuitems for both
 * "indicator.user-menu-item" and "indicator.guest-menu-item",
 * since they only differ in how they use their action's state.
 */
static GtkMenuItem *
user_menu_item_new_from_model (GMenuItem    * menuitem,
                               GActionGroup * actions,
                               GCallback      state_changed_callback)
{
  guint i;
  guint n;
  IdoUserMenuItem * ido_user;
  gchar * str;
  gchar * action;
  GVariant * v;
  GParameter parameters[4];

  /* create the ido_user */

  n = 0;

  if (g_menu_item_get_attribute (menuitem, G_MENU_ATTRIBUTE_LABEL, "s", &str))
    {
      GParameter p = { "label", G_VALUE_INIT };
      g_value_init (&p.value, G_TYPE_STRING);
      g_value_take_string (&p.value, str);
      parameters[n++] = p;
    }

  if ((v = g_menu_item_get_attribute_value (menuitem, G_MENU_ATTRIBUTE_ICON, NULL)))
    {
      GParameter p = { "icon", G_VALUE_INIT };
      GIcon * icon = g_icon_deserialize (v);
      g_value_init (&p.value, G_TYPE_OBJECT);
      g_value_take_object (&p.value, icon);
      g_variant_unref (v);
      parameters[n++] = p;
    }

  g_assert (n <= G_N_ELEMENTS (parameters));
  ido_user = g_object_newv (IDO_USER_MENU_ITEM_TYPE, n, parameters);

  for (i=0; i<n; i++)
    g_value_unset (&parameters[i].value);

  /* gie it an ActionHelper */

  if (g_menu_item_get_attribute (menuitem, G_MENU_ATTRIBUTE_ACTION, "s", &action))
    {
      IdoActionHelper *helper;
      GVariant *target;

      target = g_menu_item_get_attribute_value (menuitem, G_MENU_ATTRIBUTE_TARGET, G_VARIANT_TYPE_ANY);

      helper = ido_action_helper_new (GTK_WIDGET (ido_user), actions, action, target);
      g_signal_connect (helper, "action-state-changed",
                        state_changed_callback, NULL);

      g_signal_connect_object (ido_user, "activate",
                               G_CALLBACK (ido_action_helper_activate),
                               helper, G_CONNECT_SWAPPED);
      g_signal_connect_swapped (ido_user, "destroy", G_CALLBACK (g_object_unref), helper);

      if (target)
        g_variant_unref (target);
      g_free (action);
    }

  return GTK_MENU_ITEM (ido_user);
}

/***
****  indicator.user-menu-item handler
***/

/**
 * user_menu_item_state_changed:
 *
 * Updates an IdoUserMenuItem from @state. The state contains a
 * dictionary with keys 'active-user' (for the user that the current
 * session belongs too) and 'logged-in-users' (a list of all currently
 * logged in users).
 */
static void
user_menu_item_state_changed (IdoActionHelper *helper,
                              GVariant        *state,
                              gpointer         user_data)
{
  gboolean is_logged_in = FALSE;
  gboolean is_current_user = FALSE;
  IdoUserMenuItem *item;
  GVariant *target;
  GVariant *v;

  item = IDO_USER_MENU_ITEM (ido_action_helper_get_widget (helper));

  target = ido_action_helper_get_action_target (helper);
  g_return_if_fail (g_variant_is_of_type (target, G_VARIANT_TYPE_STRING));

  if ((v = g_variant_lookup_value (state, "active-user", G_VARIANT_TYPE_STRING)))
    {
      if (g_variant_equal (v, target))
        is_current_user = TRUE;

      g_variant_unref (v);
    }

  if ((v = g_variant_lookup_value (state, "logged-in-users", G_VARIANT_TYPE_STRING_ARRAY)))
    {
      GVariantIter it;
      GVariant *user;

      g_variant_iter_init (&it, v);
      while ((user = g_variant_iter_next_value (&it)))
        {
          if (g_variant_equal (user, target))
            is_logged_in = TRUE;

          g_variant_unref (user);
        }

      g_variant_unref (v);
    }

  ido_user_menu_item_set_logged_in (item, is_logged_in);
  ido_user_menu_item_set_current_user (item, is_current_user);
}

/**
 * ido_user_menu_item_new_from_model:
 *
 * Creates an #IdoUserMenuItem. If @menuitem contains an action, the
 * widget is bound to that action in @actions.
 *
 * Returns: (transfer full): a new #IdoUserMenuItem
 */
GtkMenuItem *
ido_user_menu_item_new_from_model (GMenuItem    *menuitem,
                                   GActionGroup *actions)
{
  return user_menu_item_new_from_model (menuitem,
                                        actions,
                                        G_CALLBACK(user_menu_item_state_changed));
}

/***
****  indicator.guest-menu-item handler
***/

static void
guest_menu_item_state_changed (IdoActionHelper *helper,
                               GVariant        *state,
                               gpointer         user_data)
{
  IdoUserMenuItem * item = IDO_USER_MENU_ITEM (ido_action_helper_get_widget (helper));
  gboolean b;

  if ((g_variant_lookup (state, "is-active", "b", &b)))
    ido_user_menu_item_set_current_user (item, b);

  if ((g_variant_lookup (state, "is-logged-in", "b", &b)))
    ido_user_menu_item_set_logged_in (item, b);
}

/**
 * ido_guest_menu_item_new_from_model:
 *
 * Creates an #IdoUserMenuItem. If @menuitem contains an action, the
 * widget is bound to that action in @actions.
 *
 * Returns: (transfer full): a new #IdoUserMenuItem
 */
GtkMenuItem *
ido_guest_menu_item_new_from_model (GMenuItem    *menuitem,
                                    GActionGroup *actions)
{
  return user_menu_item_new_from_model (menuitem,
                                        actions,
                                        G_CALLBACK(guest_menu_item_state_changed));
}

