/*
 * Copyright 2013 Canonical Ltd.
 *
 * Authors:
 *   Charles Kerr <charles.kerr@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License version 3, as published 
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranties of 
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR 
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along 
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
 #include "config.h"
#endif

#include <gtk/gtk.h>

#include "idoactionhelper.h"
#include "idobasicmenuitem.h"

enum
{
  PROP_0,
  PROP_ICON,
  PROP_TEXT,
  PROP_SECONDARY_TEXT,
  PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

struct _IdoBasicMenuItemPrivate
{
  GIcon * icon;
  char * text;
  char * secondary_text;

  GtkWidget * image;
  GtkWidget * label;
  GtkWidget * secondary_label;
};

typedef IdoBasicMenuItemPrivate priv_t;

G_DEFINE_TYPE (IdoBasicMenuItem, ido_basic_menu_item, GTK_TYPE_MENU_ITEM);

/***
****  GObject Virtual Functions
***/

static void
my_get_property (GObject     * o,
                 guint         property_id,
                 GValue      * value,
                 GParamSpec  * pspec)
{
  IdoBasicMenuItem * self = IDO_BASIC_MENU_ITEM (o);
  priv_t * p = self->priv;

  switch (property_id)
    {
      case PROP_ICON:
        g_value_set_object (value, p->icon);
        break;

      case PROP_TEXT:
        g_value_set_string (value, p->text);
        break;

      case PROP_SECONDARY_TEXT:
        g_value_set_string (value, p->secondary_text);
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
  IdoBasicMenuItem * self = IDO_BASIC_MENU_ITEM (o);

  switch (property_id)
    {
      case PROP_ICON:
        ido_basic_menu_item_set_icon (self, g_value_get_object (value));
        break;

      case PROP_TEXT:
        ido_basic_menu_item_set_text (self, g_value_get_string (value));
        break;

      case PROP_SECONDARY_TEXT:
        ido_basic_menu_item_set_secondary_text (self, g_value_get_string (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (o, property_id, pspec);
        break;
    }
}

static void
my_dispose (GObject * object)
{
  IdoBasicMenuItem * self = IDO_BASIC_MENU_ITEM (object);
  priv_t * p = self->priv;

  g_clear_object (&p->icon);

  G_OBJECT_CLASS (ido_basic_menu_item_parent_class)->dispose (object);
}

static void
my_finalize (GObject * object)
{
  IdoBasicMenuItem * self = IDO_BASIC_MENU_ITEM (object);
  priv_t * p = self->priv;

  g_free (p->text);
  g_free (p->secondary_text);

  G_OBJECT_CLASS (ido_basic_menu_item_parent_class)->finalize (object);
}

static void
ido_basic_menu_item_update_image (IdoBasicMenuItem *self)
{
  IdoBasicMenuItemPrivate * p = self->priv;

  gtk_image_clear (GTK_IMAGE (p->image));

  if (p->icon == NULL)
    {
      gtk_widget_set_visible (p->image, FALSE);
    }
  else
    {
      GtkIconInfo *info;
      const gchar *filename;

      info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (), p->icon, 16, 0);
      filename = gtk_icon_info_get_filename (info);

      if (filename)
        {
          GdkPixbuf *pixbuf;

          pixbuf = gdk_pixbuf_new_from_file_at_scale (filename, -1, 16, TRUE, NULL);
          gtk_image_set_from_pixbuf (GTK_IMAGE (p->image), pixbuf);

          g_object_unref (pixbuf);
        }

      gtk_widget_set_visible (p->image, filename != NULL);

      g_object_unref (info);
    }
}

static void
ido_basic_menu_item_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (ido_basic_menu_item_parent_class)->style_updated (widget);

  ido_basic_menu_item_update_image (IDO_BASIC_MENU_ITEM (widget));
  gtk_widget_queue_draw (widget);
}

/***
****  Instantiation
***/

static void
ido_basic_menu_item_class_init (IdoBasicMenuItemClass *klass)
{
  GParamFlags prop_flags;
  GObjectClass * gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (IdoBasicMenuItemPrivate));

  gobject_class->get_property = my_get_property;
  gobject_class->set_property = my_set_property;
  gobject_class->dispose = my_dispose;
  gobject_class->finalize = my_finalize;

  widget_class->style_updated = ido_basic_menu_item_style_updated;

  prop_flags = G_PARAM_CONSTRUCT
             | G_PARAM_READWRITE
             | G_PARAM_STATIC_STRINGS;

  properties[PROP_ICON] = g_param_spec_object ("icon",
                                               "Icon",
                                               "The menuitem's GIcon",
                                               G_TYPE_OBJECT,
                                               prop_flags);

  properties[PROP_TEXT] = g_param_spec_string ("text",
                                               "Text",
                                               "The menuitem's text",
                                               "",
                                               prop_flags);

  properties[PROP_SECONDARY_TEXT] = g_param_spec_string ("secondary-text",
                                                         "Secondary Text",
                                                         "The menuitem's secondary text",
                                                         "",
                                                         prop_flags);

  g_object_class_install_properties (gobject_class, PROP_LAST, properties);
}

static void
ido_basic_menu_item_init (IdoBasicMenuItem *self)
{
  priv_t * p;
  GtkWidget * w;
  GtkGrid * grid;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            IDO_TYPE_BASIC_MENU_ITEM,
                                            IdoBasicMenuItemPrivate);

  p = self->priv;

  p->image = gtk_image_new ();
  gtk_misc_set_alignment(GTK_MISC(p->image), 0.0, 0.0);
  p->label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC(p->label), 0.0, 0.5);
  p->secondary_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC(p->secondary_label), 1.0, 0.5);

  w = gtk_grid_new ();
  grid = GTK_GRID (w);
  gtk_grid_attach (grid, p->image, 0, 0, 1, 1);
  gtk_grid_attach (grid, p->label, 1, 0, 1, 1);
  gtk_grid_attach (grid, p->secondary_label, 2, 0, 1, 1);
  g_object_set (p->image,
                "halign", GTK_ALIGN_START,
                "hexpand", FALSE,
                "valign", GTK_ALIGN_CENTER,
                "margin-right", 6,
                NULL);
  g_object_set (p->label,
                "halign", GTK_ALIGN_START,
                "hexpand", TRUE,
                "margin-right", 6,
                "valign", GTK_ALIGN_CENTER,
                NULL);
  g_object_set (p->secondary_label,
                "halign", GTK_ALIGN_END,
                "hexpand", FALSE,
                "valign", GTK_ALIGN_CENTER,
                NULL);

  gtk_widget_show (w);
  gtk_container_add (GTK_CONTAINER (self), w);
}


/***
****  Public API
***/

/* create a new IdoBasicMenuItem */
GtkWidget *
ido_basic_menu_item_new (void)
{
  return GTK_WIDGET (g_object_new (IDO_TYPE_BASIC_MENU_ITEM, NULL));
}

void
ido_basic_menu_item_set_icon (IdoBasicMenuItem * self, GIcon * icon)
{
  IdoBasicMenuItemPrivate * p = self->priv;

  if (p->icon != icon)
    {
      if (p->icon)
        g_object_unref (p->icon);

      p->icon = icon ? g_object_ref (icon) : NULL;
      ido_basic_menu_item_update_image (self);
    }
}

void
ido_basic_menu_item_set_icon_from_file (IdoBasicMenuItem * self, const char * filename)
{
  GFile * file = filename ? g_file_new_for_path (filename) : NULL;
  GIcon * icon = file ? g_file_icon_new (file) : NULL;

  ido_basic_menu_item_set_icon (self, icon);

  g_clear_object (&icon);
  g_clear_object (&file);
}

void
ido_basic_menu_item_set_text (IdoBasicMenuItem * self, const char * text)
{
  IdoBasicMenuItemPrivate * p = self->priv;

  if (g_strcmp0 (p->text, text))
    {
      g_free (p->text);
      p->text = g_strdup (text);

      g_object_set (G_OBJECT(p->label),
                    "label", p->text,
                    "visible", (gboolean)(p->text && *p->text),
                    NULL);
    }
}

void
ido_basic_menu_item_set_secondary_text (IdoBasicMenuItem * self, const char * secondary_text)
{
  IdoBasicMenuItemPrivate * p = self->priv;

  if (g_strcmp0 (p->secondary_text, secondary_text))
    {
      g_free (p->secondary_text);
      p->secondary_text = g_strdup (secondary_text);

      g_object_set (G_OBJECT(p->secondary_label),
                    "label", p->secondary_text,
                    "visible", (gboolean)(p->secondary_text && *p->secondary_text),
                    NULL);
    }
}

static void
ido_basic_menu_item_activate (GtkMenuItem *item,
                              gpointer     user_data)
{
  IdoActionHelper *helper = user_data;

  ido_action_helper_activate (helper);
}

GtkMenuItem *
ido_basic_menu_item_new_from_model (GMenuItem    * menu_item,
                                    GActionGroup * actions)
{
  GtkWidget *item;
  gchar *label;
  gchar *action;
  GVariant *serialized_icon;

  item = ido_basic_menu_item_new ();

  if (g_menu_item_get_attribute (menu_item, "label", "s", &label))
    {
      ido_basic_menu_item_set_text (IDO_BASIC_MENU_ITEM (item), label);
      g_free (label);
    }

  serialized_icon = g_menu_item_get_attribute_value (menu_item, "icon", NULL);
  if (serialized_icon)
    {
      GIcon *icon;

      icon = g_icon_deserialize (serialized_icon);
      ido_basic_menu_item_set_icon (IDO_BASIC_MENU_ITEM (item), icon);

      g_object_unref (icon);
      g_variant_unref (serialized_icon);
    }

  if (g_menu_item_get_attribute (menu_item, "action", "s", &action))
    {
      IdoActionHelper *helper;
      GVariant *target;

      target = g_menu_item_get_attribute_value (menu_item, "target", NULL);

      helper = ido_action_helper_new (item, actions, action, target);
      g_signal_connect_object (item, "activate",
                               G_CALLBACK (ido_basic_menu_item_activate), helper,
                               0);
      g_signal_connect_swapped (item, "destroy", G_CALLBACK (g_object_unref), helper);

      if (target)
        g_variant_unref (target);
      g_free (action);
    }

  return GTK_MENU_ITEM (item);
}
