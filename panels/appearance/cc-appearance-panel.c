/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

#include "cc-appearance-panel.h"
#include "bg-wallpapers-source.h"
#include "bg-pictures-source.h"
#include "bg-colors-source.h"

#ifdef HAVE_LIBSOCIALWEB
#include "bg-flickr-source.h"
#endif

#include "cc-appearance-item.h"
#include "cc-appearance-xml.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_URI_KEY "picture-uri"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

enum {
  COL_SOURCE_NAME,
  COL_SOURCE_TYPE,
  COL_SOURCE,
  NUM_COLS
};

G_DEFINE_DYNAMIC_TYPE (CcAppearancePanel, cc_appearance_panel, CC_TYPE_PANEL)

#define APPEARANCE_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_APPEARANCE_PANEL, CcAppearancePanelPrivate))

struct _CcAppearancePanelPrivate
{
  GtkBuilder *builder;

  BgWallpapersSource *wallpapers_source;
  BgPicturesSource *pictures_source;
  BgColorsSource *colors_source;

#ifdef HAVE_LIBSOCIALWEB
  BgFlickrSource *flickr_source;
#endif

  GSettings *settings;
  GSettings *interface_settings;
  GSettings *wm_theme_settings;
  GSettings *unity_settings;
  GSettings *compizcore_settings;
  GSettings *unity_own_settings;
  GSettings *unity_launcher_settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcAppearanceItem *current_background;
  gint current_source;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;

  GdkPixbuf *display_base;
  GdkPixbuf *display_overlay;
};

enum
{
  SOURCE_WALLPAPERS,
  SOURCE_PICTURES,
  SOURCE_COLORS,
#ifdef HAVE_LIBSOCIALWEB
  SOURCE_FLICKR
#endif
};

#define UNITY_GSETTINGS_SCHEMA "org.compiz.unityshell"
#define UNITY_PROFILE_PATH "/org/compiz/profiles/unity/plugins/"
#define UNITY_GSETTINGS_PATH UNITY_PROFILE_PATH"unityshell/"
#define UNITY_ICONSIZE_KEY "icon-size"
#define UNITY_LAUNCHERSENSITIVITY_KEY "edge-responsiveness"
#define UNITY_LAUNCHERHIDE_KEY "launcher-hide-mode"
#define UNITY_LAUNCHERREVEAL_KEY "reveal-trigger"
#define CANONICAL_DESKTOP_INTERFACE "com.canonical.desktop.interface"

#define COMPIZCORE_GSETTINGS_SCHEMA "org.compiz.core"
#define COMPIZCORE_GSETTINGS_PATH UNITY_PROFILE_PATH"core/"
#define COMPIZCORE_HSIZE_KEY "hsize"
#define COMPIZCORE_VSIZE_KEY "vsize"

#define UNITY_OWN_GSETTINGS_SCHEMA "com.canonical.Unity"
#define UNITY_LAUNCHER_GSETTINGS_SCHEMA "com.canonical.Unity.Launcher"
#define UNITY_FAVORITES_KEY "favorites"
#define UNITY_INTEGRATED_MENUS_KEY "integrated-menus"
#define SHOW_DESKTOP_UNITY_FAVORITE_STR "unity://desktop-icon"

#define MIN_ICONSIZE 16.0
#define MAX_ICONSIZE 64.0
#define DEFAULT_ICONSIZE 48.0

#define MIN_LAUNCHER_SENSIVITY 0.2
#define MAX_LAUNCHER_SENSIVITY 8.0

#define WID(y) (GtkWidget *) gtk_builder_get_object (priv->builder, y)

static void
cc_appearance_panel_get_property (GObject    *object,
                                  guint       property_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_appearance_panel_set_property (GObject      *object,
                                  guint         property_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_appearance_panel_dispose (GObject *object)
{
  CcAppearancePanelPrivate *priv = CC_APPEARANCE_PANEL (object)->priv;

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;

      /* destroying the builder object will also destroy the spinner */
      priv->spinner = NULL;
    }

  if (priv->wallpapers_source)
    {
      g_object_unref (priv->wallpapers_source);
      priv->wallpapers_source = NULL;
    }

  if (priv->pictures_source)
    {
      g_object_unref (priv->pictures_source);
      priv->pictures_source = NULL;
    }

  if (priv->colors_source)
    {
      g_object_unref (priv->colors_source);
      priv->colors_source = NULL;
    }
#ifdef HAVE_LIBSOCIALWEB
  if (priv->flickr_source)
    {
      g_object_unref (priv->flickr_source);
      priv->flickr_source = NULL;
    }
#endif

  if (priv->settings)
    {
      g_object_unref (priv->settings);
      priv->settings = NULL;
    }

  if (priv->interface_settings)
    {
      g_object_unref (priv->interface_settings);
      priv->interface_settings = NULL;
    }

  if (priv->wm_theme_settings)
    {
      g_object_unref (priv->wm_theme_settings);
      priv->wm_theme_settings = NULL;
    }

  if (priv->unity_settings)
    {
      g_object_unref (priv->unity_settings);
      priv->unity_settings = NULL;
    }

  if (priv->compizcore_settings)
    {
      g_object_unref (priv->compizcore_settings);
      priv->compizcore_settings = NULL;
    }

  if (priv->unity_launcher_settings)
    {
      g_object_unref (priv->unity_launcher_settings);
      priv->unity_launcher_settings = NULL;
    }

  if (priv->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (priv->copy_cancellable);

      g_object_unref (priv->copy_cancellable);
      priv->copy_cancellable = NULL;
    }

  if (priv->thumb_factory)
    {
      g_object_unref (priv->thumb_factory);
      priv->thumb_factory = NULL;
    }

  if (priv->display_base)
    {
      g_object_unref (priv->display_base);
      priv->display_base = NULL;
    }

  if (priv->display_overlay)
    {
      g_object_unref (priv->display_overlay);
      priv->display_overlay = NULL;
    }

  G_OBJECT_CLASS (cc_appearance_panel_parent_class)->dispose (object);
}

static void
cc_appearance_panel_finalize (GObject *object)
{
  CcAppearancePanelPrivate *priv = CC_APPEARANCE_PANEL (object)->priv;

  if (priv->current_background)
    {
      g_object_unref (priv->current_background);
      priv->current_background = NULL;
    }

  G_OBJECT_CLASS (cc_appearance_panel_parent_class)->finalize (object);
}

static void
cc_appearance_panel_class_init (CcAppearancePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcAppearancePanelPrivate));

  object_class->get_property = cc_appearance_panel_get_property;
  object_class->set_property = cc_appearance_panel_set_property;
  object_class->dispose = cc_appearance_panel_dispose;
  object_class->finalize = cc_appearance_panel_finalize;
}

static void
cc_appearance_panel_class_finalize (CcAppearancePanelClass *klass)
{
}

static void
source_update_edit_box (CcAppearancePanelPrivate *priv,
			gboolean                  initial)
{
  CcAppearanceItemFlags flags;

  flags = cc_appearance_item_get_flags (priv->current_background);

  if ((flags & CC_APPEARANCE_ITEM_HAS_SCOLOR &&
       priv->current_source != SOURCE_COLORS) ||
      cc_appearance_item_get_shading (priv->current_background) == G_DESKTOP_BACKGROUND_SHADING_SOLID)
    gtk_widget_hide (WID ("style-scolor"));
  else
    gtk_widget_show (WID ("style-scolor"));

  if (flags & CC_APPEARANCE_ITEM_HAS_PCOLOR &&
      priv->current_source != SOURCE_COLORS)
    gtk_widget_hide (WID ("style-pcolor"));
  else
    gtk_widget_show (WID ("style-pcolor"));

  if (gtk_widget_get_visible (WID ("style-pcolor")) &&
      gtk_widget_get_visible (WID ("style-scolor")))
    gtk_widget_show (WID ("swap-color-button"));
  else
    gtk_widget_hide (WID ("swap-color-button"));

  if (flags & CC_APPEARANCE_ITEM_HAS_PLACEMENT ||
      cc_appearance_item_get_uri (priv->current_background) == NULL)
    gtk_widget_hide (WID ("style-combobox"));
  else
    gtk_widget_show (WID ("style-combobox"));

  /* FIXME What to do if the background has a gradient shading
   * and provides the colours? */
}

static void
source_changed_cb (GtkComboBox              *combo,
                   CcAppearancePanelPrivate *priv)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkIconView *view;
  guint type;
  BgSource *source;

  gtk_combo_box_get_active_iter (combo, &iter);
  model = gtk_combo_box_get_model (combo);
  gtk_tree_model_get (model, &iter,
                      COL_SOURCE_TYPE, &type,
                      COL_SOURCE, &source, -1);

  view = (GtkIconView *) gtk_builder_get_object (priv->builder,
                                                 "backgrounds-iconview");

  gtk_icon_view_set_model (view,
                           GTK_TREE_MODEL (bg_source_get_liststore (source)));
}

static void
select_style (GtkComboBox *box,
	      GDesktopBackgroundStyle new_style)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean cont;

  model = gtk_combo_box_get_model (box);
  cont = gtk_tree_model_get_iter_first (model, &iter);
  while (cont != FALSE)
    {
      GDesktopBackgroundStyle style;

      gtk_tree_model_get (model, &iter,
			  1, &style,
			  -1);

      if (style == new_style)
        {
          gtk_combo_box_set_active_iter (box, &iter);
          break;
	}
      cont = gtk_tree_model_iter_next (model, &iter);
    }

  if (cont == FALSE)
    gtk_combo_box_set_active (box, -1);
}

static void
update_preview (CcAppearancePanelPrivate *priv,
                CcAppearanceItem         *item)
{
  gchar *markup;
  gboolean changes_with_time;

  if (item && priv->current_background)
    {
      g_object_unref (priv->current_background);
      priv->current_background = cc_appearance_item_copy (item);
      cc_appearance_item_load (priv->current_background, NULL);
    }

  source_update_edit_box (priv, FALSE);

  changes_with_time = FALSE;

  if (priv->current_background)
    {
      GdkRGBA pcolor, scolor;
      const char* bgsize = NULL;

      markup = g_strdup_printf ("<i>%s</i>", cc_appearance_item_get_name (priv->current_background));
      gtk_label_set_markup (GTK_LABEL (WID ("background-label")), markup);
      g_free (markup);

      bgsize = cc_appearance_item_get_size (priv->current_background);
      if (bgsize && *bgsize != '\0')
       {
          markup = g_strdup_printf ("(%s)", bgsize);
          gtk_label_set_text (GTK_LABEL (WID ("size_label")), markup);
          g_free (markup);
       }
      else
          gtk_label_set_text (GTK_LABEL (WID ("size_label")), "");

      gdk_rgba_parse (&pcolor, cc_appearance_item_get_pcolor (priv->current_background));
      gdk_rgba_parse (&scolor, cc_appearance_item_get_scolor (priv->current_background));

      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (WID ("style-pcolor")), &pcolor);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (WID ("style-scolor")), &scolor);

      select_style (GTK_COMBO_BOX (WID ("style-combobox")),
                    cc_appearance_item_get_placement (priv->current_background));

      changes_with_time = cc_appearance_item_changes_with_time (priv->current_background);
    }

  gtk_widget_set_visible (WID ("slide_image"), changes_with_time);
  gtk_widget_set_visible (WID ("slide-label"), changes_with_time);

  gtk_widget_queue_draw (WID ("preview-area"));
}

static char *
get_save_path (void)
{
  return g_build_filename (g_get_user_config_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   "last-edited.xml",
			   NULL);
}

static gboolean
create_save_dir (void)
{
  char *path;

  path = g_build_filename (g_get_user_config_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   NULL);
  if (g_mkdir_with_parents (path, 0755) < 0)
    {
      g_warning ("Failed to create directory '%s'", path);
      g_free (path);
      return FALSE;
    }
  g_free (path);
  return TRUE;
}

static void
copy_finished_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      pointer)
{
  GError *err = NULL;
  CcAppearancePanel *panel = (CcAppearancePanel *) pointer;
  CcAppearancePanelPrivate *priv = panel->priv;
  CcAppearanceItem *item;

  if (!g_file_copy_finish (G_FILE (source_object), result, &err))
    {
      if (err->code != G_IO_ERROR_CANCELLED)
        g_warning ("Failed to copy image to cache location: %s", err->message);

      g_error_free (err);
    }
  item = g_object_get_data (source_object, "item");

  /* the panel may have been destroyed before the callback is run, so be sure
   * to check the widgets are not NULL */

  if (priv->spinner)
    {
      gtk_widget_destroy (GTK_WIDGET (priv->spinner));
      priv->spinner = NULL;
    }

  if (priv->current_background)
    cc_appearance_item_load (priv->current_background, NULL);

  if (priv->builder)
    {
      char *filename;

      update_preview (priv, item);

      /* Save the source XML if there is one */
      filename = get_save_path ();
      if (create_save_dir ())
        cc_appearance_xml_save (priv->current_background, filename);
    }

  /* remove the reference taken when the copy was set up */
  g_object_unref (panel);
}

static void
update_remove_button (CcAppearancePanel *panel,
		      CcAppearanceItem  *item)
{
  CcAppearancePanelPrivate *priv;
  const char *uri;
  char *cache_path;
  GFile *bg, *cache, *parent;
  gboolean sensitive = FALSE;

  priv = panel->priv;

  if (priv->current_source != SOURCE_PICTURES)
    goto bail;

  uri = cc_appearance_item_get_uri (item);
  if (uri == NULL)
    goto bail;

  bg = g_file_new_for_uri (uri);
  parent = g_file_get_parent (bg);
  if (parent == NULL)
    {
      g_object_unref (bg);
      goto bail;
    }
  cache_path = bg_pictures_source_get_cache_path ();
  cache = g_file_new_for_path (cache_path);
  g_free (cache_path);

  if (g_file_equal (parent, cache))
    sensitive = TRUE;

  g_object_unref (parent);
  g_object_unref (cache);

bail:
  gtk_widget_set_sensitive (WID ("remove_button"), sensitive);

}

static CcAppearanceItem *
get_selected_item (CcAppearancePanel *panel)
{
  CcAppearancePanelPrivate *priv = panel->priv;
  GtkIconView *icon_view;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GList *list;
  CcAppearanceItem *item;

  icon_view = GTK_ICON_VIEW (WID ("backgrounds-iconview"));
  item = NULL;
  list = gtk_icon_view_get_selected_items (icon_view);

  if (!list)
    return NULL;

  model = gtk_icon_view_get_model (icon_view);

  if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data) == FALSE)
    goto bail;

  gtk_tree_model_get (model, &iter, 1, &item, -1);

bail:
  g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (list);

  return item;
}

static void
backgrounds_changed_cb (GtkIconView       *icon_view,
                        CcAppearancePanel *panel)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  CcAppearanceItem *item;
  CcAppearancePanelPrivate *priv = panel->priv;
  char *pcolor, *scolor;
  gboolean draw_preview = TRUE;
  const char *uri;
  CcAppearanceItemFlags flags;
  char *filename;

  item = get_selected_item (panel);

  if (item == NULL)
    return;

  /* Update current source */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sources-combobox")));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("sources-combobox")),
                                 &iter);
  gtk_tree_model_get (model, &iter,
		      COL_SOURCE_TYPE, &priv->current_source, -1);

  uri = cc_appearance_item_get_uri (item);
  flags = cc_appearance_item_get_flags (item);

  if ((flags & CC_APPEARANCE_ITEM_HAS_URI) && uri == NULL)
    {
      g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, G_DESKTOP_BACKGROUND_STYLE_NONE);
      g_settings_set_string (priv->settings, WP_URI_KEY, "");
    }
  else if (cc_appearance_item_get_source_url (item) != NULL &&
	   cc_appearance_item_get_needs_download (item))
    {
      GFile *source, *dest;
      gchar *cache_path, *basename, *dest_path, *display_name, *dest_uri;
      GdkPixbuf *pixbuf;

      cache_path = bg_pictures_source_get_cache_path ();
      if (g_mkdir_with_parents (cache_path, 0755) < 0)
        {
          g_warning ("Failed to create directory '%s'", cache_path);
          g_free (cache_path);
          return;
	}
      g_free (cache_path);

      dest_path = bg_pictures_source_get_unique_path (cc_appearance_item_get_source_url (item));
      dest = g_file_new_for_path (dest_path);
      g_free (dest_path);
      source = g_file_new_for_uri (cc_appearance_item_get_source_url (item));
      basename = g_file_get_basename (source);
      display_name = g_filename_display_name (basename);
      dest_path = g_file_get_path (dest);
      g_free (basename);

      /* create a blank image to use until the source image is ready */
      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
      gdk_pixbuf_fill (pixbuf, 0x00000000);
      gdk_pixbuf_save (pixbuf, dest_path, "png", NULL, NULL);
      g_object_unref (pixbuf);
      g_free (dest_path);

      if (priv->copy_cancellable)
        {
          g_cancellable_cancel (priv->copy_cancellable);
          g_cancellable_reset (priv->copy_cancellable);
        }

      if (priv->spinner)
        {
          gtk_widget_destroy (GTK_WIDGET (priv->spinner));
          priv->spinner = NULL;
        }

      /* create a spinner while the file downloads */
      priv->spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (priv->spinner));
      gtk_box_pack_start (GTK_BOX (WID ("bottom-hbox")), priv->spinner, FALSE,
                          FALSE, 6);
      gtk_widget_show (priv->spinner);

      /* reference the panel in case it is removed before the copy is
       * finished */
      g_object_ref (panel);
      g_object_set_data_full (G_OBJECT (source), "item", g_object_ref (item), g_object_unref);
      g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, priv->copy_cancellable,
                         NULL, NULL,
                         copy_finished_cb, panel);
      g_object_unref (source);
      dest_uri = g_file_get_uri (dest);
      g_object_unref (dest);

      g_settings_set_string (priv->settings, WP_URI_KEY, dest_uri);
      g_object_set (G_OBJECT (item),
		    "uri", dest_uri,
		    "needs-download", FALSE,
		    "name", display_name,
		    NULL);
      g_free (display_name);
      g_free (dest_uri);

      /* delay the updated drawing of the preview until the copy finishes */
      draw_preview = FALSE;
    }
  else
    {
      g_settings_set_string (priv->settings, WP_URI_KEY, uri);
    }

  /* Also set the placement if we have a URI and the previous value was none */
  if (flags & CC_APPEARANCE_ITEM_HAS_PLACEMENT)
    {
      g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, cc_appearance_item_get_placement (item));
    }
  else if (uri != NULL)
    {
      GDesktopBackgroundStyle style;
      style = g_settings_get_enum (priv->settings, WP_OPTIONS_KEY);
      if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
        g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, cc_appearance_item_get_placement (item));
    }

  if (flags & CC_APPEARANCE_ITEM_HAS_SHADING)
    g_settings_set_enum (priv->settings, WP_SHADING_KEY, cc_appearance_item_get_shading (item));

  /* When changing to a background with colours set,
   * don't overwrite what's in GSettings, but read
   * from it instead.
   * We have a hack for the colors source though */
  if (flags & CC_APPEARANCE_ITEM_HAS_PCOLOR &&
      priv->current_source != SOURCE_COLORS)
    {
      g_settings_set_string (priv->settings, WP_PCOLOR_KEY, cc_appearance_item_get_pcolor (item));
    }
  else
    {
      pcolor = g_settings_get_string (priv->settings, WP_PCOLOR_KEY);
      g_object_set (G_OBJECT (item), "primary-color", pcolor, NULL);
    }

  if (flags & CC_APPEARANCE_ITEM_HAS_SCOLOR &&
      priv->current_source != SOURCE_COLORS)
    {
      g_settings_set_string (priv->settings, WP_SCOLOR_KEY, cc_appearance_item_get_scolor (item));
    }
  else
    {
      scolor = g_settings_get_string (priv->settings, WP_SCOLOR_KEY);
      g_object_set (G_OBJECT (item), "secondary-color", scolor, NULL);
    }

  /* Apply all changes */
  g_settings_apply (priv->settings);

  update_remove_button (panel, item);

  /* update the preview information */
  if (draw_preview != FALSE)
    {
      update_preview (priv, item);

      /* Save the source XML if there is one */
      filename = get_save_path ();
      if (create_save_dir ())
        cc_appearance_xml_save (priv->current_background, filename);
    }
}

static gboolean
preview_draw_cb (GtkWidget         *widget,
                 cairo_t           *cr,
                 CcAppearancePanel *panel)
{
  GtkAllocation allocation;
  CcAppearancePanelPrivate *priv = panel->priv;
  GdkPixbuf *pixbuf = NULL;
  const gint preview_width = 416;
  const gint preview_height = 248;
  const gint preview_x = 45;
  const gint preview_y = 84;
  GdkPixbuf *preview, *temp;
  gint size;

  gtk_widget_get_allocation (widget, &allocation);

  if (priv->current_background)
    {
      GIcon *icon;
      icon = cc_appearance_item_get_frame_thumbnail (priv->current_background,
                                                     priv->thumb_factory,
                                                     preview_width,
                                                     preview_height,
                                                     -2, TRUE);
      pixbuf = GDK_PIXBUF (icon);
    }

  if (!priv->display_base)
    return FALSE;


  preview = gdk_pixbuf_copy (priv->display_base);

  if (pixbuf)
    {
      gdk_pixbuf_composite (pixbuf, preview,
                            preview_x, preview_y,
                            preview_width, preview_height,
                            preview_x, preview_y, 1, 1,
                            GDK_INTERP_BILINEAR, 255);

      g_object_unref (pixbuf);
    }


  if (priv->display_overlay)
    {
      gdk_pixbuf_composite (priv->display_overlay, preview,
                            0, 0, 512, 512,
                            0, 0, 1, 1,
                            GDK_INTERP_BILINEAR, 255);
    }


  if (allocation.width < allocation.height)
    size = allocation.width;
  else
    size = allocation.height;

  temp = gdk_pixbuf_scale_simple (preview, size, size, GDK_INTERP_BILINEAR);

  gdk_cairo_set_source_pixbuf (cr,
                               temp,
                               allocation.width / 2 - (size / 2),
                               allocation.height / 2 - (size / 2));
  cairo_paint (cr);

  g_object_unref (temp);
  g_object_unref (preview);

  return TRUE;
}

static void
style_changed_cb (GtkComboBox       *box,
                  CcAppearancePanel *panel)
{
  CcAppearancePanelPrivate *priv = panel->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GDesktopBackgroundStyle value;

  if (!gtk_combo_box_get_active_iter (box, &iter))
    {
      return;
    }

  model = gtk_combo_box_get_model (box);

  gtk_tree_model_get (model, &iter, 1, &value, -1);

  g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, value);

  if (priv->current_background)
    g_object_set (G_OBJECT (priv->current_background), "placement", value, NULL);

  g_settings_apply (priv->settings);

  update_preview (priv, NULL);
}

/* Convert RGBA to the old GdkColor string format for backwards compatibility */
static gchar *
rgba_to_string (GdkRGBA *color)
{
    return g_strdup_printf ("#%04x%04x%04x",
                            (int)(color->red * 65535. + 0.5),
                            (int)(color->green * 65535. + 0.5),
                            (int)(color->blue * 65535. + 0.5));
}

static void
color_changed_cb (GtkColorButton    *button,
                  CcAppearancePanel *panel)
{
  CcAppearancePanelPrivate *priv = panel->priv;
  GdkRGBA color;
  gchar *value;
  gboolean is_pcolor = FALSE;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);
  if (WID ("style-pcolor") == GTK_WIDGET (button))
    is_pcolor = TRUE;

  value = rgba_to_string (&color);

  if (priv->current_background)
    {
      g_object_set (G_OBJECT (priv->current_background),
		    is_pcolor ? "primary-color" : "secondary-color", value, NULL);
    }

  g_settings_set_string (priv->settings,
			 is_pcolor ? WP_PCOLOR_KEY : WP_SCOLOR_KEY, value);

  g_settings_apply (priv->settings);

  g_free (value);

  update_preview (priv, NULL);
}

static void
swap_colors_clicked (GtkButton         *button,
                     CcAppearancePanel *panel)
{
  CcAppearancePanelPrivate *priv = panel->priv;
  GdkRGBA pcolor, scolor;
  char *new_pcolor, *new_scolor;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (WID ("style-pcolor")), &pcolor);
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (WID ("style-scolor")), &scolor);

  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (WID ("style-scolor")), &pcolor);
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (WID ("style-pcolor")), &scolor);

  new_pcolor = rgba_to_string (&scolor);
  new_scolor = rgba_to_string (&pcolor);

  g_object_set (priv->current_background,
                "primary-color", new_pcolor,
                "secondary-color", new_scolor,
                NULL);

  g_settings_set_string (priv->settings, WP_PCOLOR_KEY, new_pcolor);
  g_settings_set_string (priv->settings, WP_SCOLOR_KEY, new_scolor);

  g_free (new_pcolor);
  g_free (new_scolor);

  g_settings_apply (priv->settings);

  update_preview (priv, NULL);
}

static void
row_inserted (GtkTreeModel      *tree_model,
	      GtkTreePath       *path,
	      GtkTreeIter       *iter,
	      CcAppearancePanel *panel)
{
  GtkListStore *store;
  CcAppearancePanelPrivate *priv;

  priv = panel->priv;

  store = bg_source_get_liststore (BG_SOURCE (panel->priv->pictures_source));
  g_signal_handlers_disconnect_by_func (G_OBJECT (store), G_CALLBACK (row_inserted), panel);

  /* Change source */
  gtk_combo_box_set_active (GTK_COMBO_BOX (WID ("sources-combobox")), SOURCE_PICTURES);

  /* And select the newly added item */
  gtk_icon_view_select_path (GTK_ICON_VIEW (WID ("backgrounds-iconview")), path);
}

static void
add_custom_wallpaper (CcAppearancePanel *panel,
		      const char        *uri)
{
  GtkListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (panel->priv->pictures_source));
  g_signal_connect (G_OBJECT (store), "row-inserted",
		    G_CALLBACK (row_inserted), panel);

  if (bg_pictures_source_add (panel->priv->pictures_source, uri) == FALSE) {
    g_signal_handlers_disconnect_by_func (G_OBJECT (store), G_CALLBACK (row_inserted), panel);
    return;
  }

  /* Wait for the item to get added */
}

static void
file_chooser_response (GtkDialog         *chooser,
                       gint               response,
                       CcAppearancePanel *panel)
{
  GSList *selected, *l;

  if (response != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (GTK_WIDGET (chooser));
      return;
    }

  selected = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
  gtk_widget_destroy (GTK_WIDGET (chooser));

  for (l = selected; l != NULL; l = l->next)
    {
      char *uri = l->data;
      add_custom_wallpaper (panel, uri);
      g_free (uri);
    }
  g_slist_free (selected);
}

static void
update_chooser_preview (GtkFileChooser    *chooser,
			CcAppearancePanel *panel)
{
  GnomeDesktopThumbnailFactory *thumb_factory;
  char *uri;

  thumb_factory = panel->priv->thumb_factory;

  uri = gtk_file_chooser_get_preview_uri (chooser);

  if (uri)
    {
      GdkPixbuf *pixbuf = NULL;
      const gchar *mime_type = NULL;
      GFile *file;
      GFileInfo *file_info;
      GtkWidget *preview;

      preview = gtk_file_chooser_get_preview_widget (chooser);

      file = g_file_new_for_uri (uri);
      file_info = g_file_query_info (file,
				     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				     G_FILE_QUERY_INFO_NONE,
				     NULL, NULL);
      g_object_unref (file);

      if (file_info != NULL) {
	      mime_type = g_file_info_get_content_type (file_info);
	      g_object_unref (file_info);
      }

      if (mime_type)
        {
        pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumb_factory,
								     uri,
								     mime_type);
	}

      gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
					 GTK_RESPONSE_ACCEPT,
					 (pixbuf != NULL));

      if (pixbuf != NULL)
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	  g_object_unref (pixbuf);
	}
      else
        {
          gtk_image_set_from_icon_name (GTK_IMAGE (preview),
				    "dialog-question",
				    GTK_ICON_SIZE_DIALOG);
	}

      if (bg_pictures_source_is_known (panel->priv->pictures_source, uri))
        gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT, FALSE);
      else
        gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT, TRUE);

      g_free (uri);
    }

  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static void
add_button_clicked (GtkButton         *button,
		    CcAppearancePanel *panel)
{
  GtkWidget *chooser;
  const gchar *folder;
  GtkWidget *preview;
  GtkFileFilter *filter;
  CcAppearancePanelPrivate *priv;
  const char * const * content_types;
  guint i;

  priv = panel->priv;

  filter = gtk_file_filter_new ();
  content_types = bg_pictures_get_support_content_types ();
  for (i = 0; content_types[i] != NULL; i++)
    gtk_file_filter_add_mime_type (filter, content_types[i]);

  chooser = gtk_file_chooser_dialog_new (_("Browse for more pictures"),
					 GTK_WINDOW (gtk_widget_get_toplevel (WID ("appearance-panel"))),
					 GTK_FILE_CHOOSER_ACTION_OPEN,
					 _("_Cancel"), GTK_RESPONSE_CANCEL,
					 _("_Open"), GTK_RESPONSE_ACCEPT,
					 NULL);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);

  preview = gtk_image_new ();
  gtk_widget_set_size_request (preview, 128, -1);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview);
  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (chooser), FALSE);
  gtk_widget_show (preview);
  g_signal_connect (chooser, "update-preview",
		    G_CALLBACK (update_chooser_preview), panel);

  folder = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					 folder);

  g_signal_connect (chooser, "response",
		    G_CALLBACK (file_chooser_response), panel);

  gtk_window_present (GTK_WINDOW (chooser));
}

static void
remove_button_clicked (GtkButton         *button,
		       CcAppearancePanel *panel)
{
  CcAppearanceItem *item;
  GtkListStore *store;
  GtkTreePath *path;
  CcAppearancePanelPrivate *priv;

  priv = panel->priv;

  item = get_selected_item (panel);
  if (item == NULL)
    g_assert_not_reached ();

  bg_pictures_source_remove (panel->priv->pictures_source, item);
  g_object_unref (item);

  /* Are there any items left in the pictures tree store? */
  store = bg_source_get_liststore (BG_SOURCE (panel->priv->pictures_source));
  if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 0)
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID ("sources-combobox")), SOURCE_WALLPAPERS);

  path = gtk_tree_path_new_from_string ("0");
  gtk_icon_view_select_path (GTK_ICON_VIEW (WID ("backgrounds-iconview")), path);
  gtk_tree_path_free (path);
}

static void
load_current_bg (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv;
  CcAppearanceItem *saved, *configured;
  gchar *uri, *pcolor, *scolor;

  priv = self->priv;

  /* Load the saved configuration */
  uri = get_save_path ();
  saved = cc_appearance_xml_get_item (uri);
  g_free (uri);

  /* initalise the current background information from settings */
  uri = g_settings_get_string (priv->settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    {
      g_free (uri);
      uri = NULL;
    }
  else
    {
      GFile *file;

      file = g_file_new_for_commandline_arg (uri);
      g_object_unref (file);
    }
  configured = cc_appearance_item_new (uri);
  g_free (uri);

  pcolor = g_settings_get_string (priv->settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (priv->settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
		"name", _("Current background"),
		"placement", g_settings_get_enum (priv->settings, WP_OPTIONS_KEY),
		"shading", g_settings_get_enum (priv->settings, WP_SHADING_KEY),
		"primary-color", pcolor,
		"secondary-color", scolor,
		NULL);
  g_free (pcolor);
  g_free (scolor);

  if (saved != NULL && cc_appearance_item_compare (saved, configured))
    {
      CcAppearanceItemFlags flags;
      flags = cc_appearance_item_get_flags (saved);
      /* Special case for colours */
      if (cc_appearance_item_get_placement (saved) == G_DESKTOP_BACKGROUND_STYLE_NONE)
        flags &=~ (CC_APPEARANCE_ITEM_HAS_PCOLOR | CC_APPEARANCE_ITEM_HAS_SCOLOR);
      g_object_set (G_OBJECT (configured),
		    "name", cc_appearance_item_get_name (saved),
		    "flags", flags,
		    "source-url", cc_appearance_item_get_source_url (saved),
		    "source-xml", cc_appearance_item_get_source_xml (saved),
		    NULL);
    }
  if (saved != NULL)
    g_object_unref (saved);

  priv->current_background = configured;
  cc_appearance_item_load (priv->current_background, NULL);
}

static void
scrolled_realize_cb (GtkWidget         *scrolled,
                     CcAppearancePanel *self)
{
  /* FIXME, hack for https://bugzilla.gnome.org/show_bug.cgi?id=645649 */
  GdkScreen *screen;
  GdkRectangle rect;
  int monitor;

  screen = gtk_widget_get_screen (scrolled);
  monitor = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (scrolled));
  gdk_screen_get_monitor_geometry (screen, monitor, &rect);
  if (rect.height <= 768)
    g_object_set (G_OBJECT (scrolled), "height-request", 280, NULL);
}

static void
cc_appearance_panel_drag_uris (GtkWidget *widget,
			       GdkDragContext *context, gint x, gint y,
			       GtkSelectionData *data, guint info, guint time,
			       CcAppearancePanel *panel)
{
  gint i;
  char *uri;
  gchar **uris;

  uris = gtk_selection_data_get_uris (data);
  if (!uris)
    return;

  gtk_drag_finish (context, TRUE, FALSE, time);

  for (i = 0; uris[i] != NULL; i++) {
    uri = uris[i];
    if (!bg_pictures_source_is_known (panel->priv->pictures_source, uri)) {
      add_custom_wallpaper (panel, uri);
    }
  }

  g_strfreev(uris);
}

static gchar *themes_id[] = { "Adwaita", "Ambiance", "Radiance", "HighContrast" };
static gchar *themes_name[] = { "Adwaita", "Ambiance", "Radiance", "High Contrast" };

static gboolean
get_theme_data (const gchar *theme_name,
		gchar **gtk_theme,
		gchar **icon_theme,
		gchar **window_theme,
		gchar **cursor_theme)
{
  gchar *path;
  GKeyFile *theme_file;
  GError *error = NULL;
  gboolean result = FALSE;

  *gtk_theme = *icon_theme = *window_theme = *cursor_theme = NULL;

  theme_file = g_key_file_new ();
  path = g_build_filename ("/usr/share/themes", theme_name, "index.theme", NULL);
  if (g_key_file_load_from_file (theme_file, path, G_KEY_FILE_NONE, &error))
    {
      *gtk_theme = g_key_file_get_string (theme_file, "X-GNOME-Metatheme", "GtkTheme", NULL);
      *icon_theme = g_key_file_get_string (theme_file, "X-GNOME-Metatheme", "IconTheme", NULL);
      *window_theme = g_key_file_get_string (theme_file, "X-GNOME-Metatheme", "MetacityTheme", NULL);
      *cursor_theme = g_key_file_get_string (theme_file, "X-GNOME-Metatheme", "CursorTheme", NULL);

      result = TRUE;
    }
  else
    {
      g_warning ("Could not load %s: %s", path, error->message);
      g_error_free (error);
    }

  g_key_file_free (theme_file);
  g_free (path);

  return result;
}

static void
theme_selection_changed (GtkComboBox *combo, CcAppearancePanel *self)
{
  gint active;
  gchar *gtk_theme, *icon_theme, *window_theme, *cursor_theme;

  active = gtk_combo_box_get_active (combo);
  g_return_if_fail (active >= 0 && active < G_N_ELEMENTS (themes_id));

  if (!get_theme_data (gtk_combo_box_get_active_id (combo),
                       &gtk_theme, &icon_theme, &window_theme, &cursor_theme))
    return;

  g_settings_delay (self->priv->interface_settings);

  g_settings_set_string (self->priv->interface_settings, "gtk-theme", gtk_theme);
  g_settings_set_string (self->priv->interface_settings, "icon-theme", icon_theme);
  g_settings_set_string (self->priv->interface_settings, "cursor-theme", cursor_theme);
  g_settings_set_string (self->priv->wm_theme_settings, "theme", window_theme);

  g_settings_apply (self->priv->interface_settings);

  g_free (gtk_theme);
  g_free (icon_theme);
  g_free (window_theme);
  g_free (cursor_theme);
}

static void
setup_theme_selector (CcAppearancePanel *self)
{
  gchar *current_gtk_theme;
  gchar *default_gtk_theme;
  gint i, current_theme_index = 0;
  GtkWidget *widget;
  GtkWidget *liststore;
  GSettingsSchemaSource *source;
  CcAppearancePanelPrivate *priv = self->priv;
  GSettings *defaults_settings = g_settings_new ("org.gnome.desktop.interface");

  priv->interface_settings = g_settings_new ("org.gnome.desktop.interface");

  source = g_settings_schema_source_get_default ();

  priv->wm_theme_settings = g_settings_new ("org.gnome.desktop.wm.preferences");
  current_gtk_theme = g_settings_get_string (priv->interface_settings, "gtk-theme");

  /* gettint the default for the theme */
  g_settings_delay (defaults_settings);
  g_settings_reset (defaults_settings, "gtk-theme");
  default_gtk_theme = g_settings_get_string (defaults_settings, "gtk-theme");
  g_object_unref (defaults_settings);

  widget = WID ("theme-selector");
  liststore = WID ("theme-list-store");

  for (i = 0; i < G_N_ELEMENTS (themes_id); i++, current_theme_index++)
    {
      gchar *gtk_theme, *icon_theme, *window_theme, *cursor_theme, *new_theme_name;
      GtkTreeIter iter;

      if (!get_theme_data (themes_id[i], &gtk_theme, &icon_theme, &window_theme, &cursor_theme))
        {
          current_theme_index--;
          continue;
        }

      if (g_strcmp0 (gtk_theme, default_gtk_theme) == 0)
        new_theme_name = g_strdup_printf ("%s <small><i>(%s)</i></small>", themes_name[i], _("default"));
      else
        new_theme_name = g_strdup (themes_name[i]);

      gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
      gtk_list_store_set (GTK_LIST_STORE (liststore), &iter, 0, themes_id[i], 1, new_theme_name, -1);

      if (g_strcmp0 (gtk_theme, current_gtk_theme) == 0)
	  /* This is the current theme, so select item in the combo box */
         gtk_combo_box_set_active (GTK_COMBO_BOX (widget), current_theme_index);

      g_free (gtk_theme);
      g_free (new_theme_name);
      g_free (icon_theme);
      g_free (window_theme);
      g_free (cursor_theme);
    }
    g_free (current_gtk_theme);
    g_free (default_gtk_theme);

    g_signal_connect (G_OBJECT (widget), "changed",
		      G_CALLBACK (theme_selection_changed), self);
}

static void
iconsize_widget_refresh (GtkAdjustment *iconsize_adj, GSettings *unity_settings)
{
  gint value = g_settings_get_int (unity_settings, UNITY_ICONSIZE_KEY);
  gtk_adjustment_set_value (iconsize_adj, (gdouble)value / 2);
}

static void
ext_iconsize_changed_callback (GSettings* settings,
                               guint key,
                               gpointer user_data)
{
  iconsize_widget_refresh (GTK_ADJUSTMENT (user_data), settings);
}

static gchar *
on_iconsize_format_value (GtkScale *scale, gdouble value)
{
  return g_strdup_printf ("%d", (int) value * 2);
}

static void
on_iconsize_changed (GtkAdjustment *adj, GSettings *unity_settings)
{
  g_settings_set_int (unity_settings, UNITY_ICONSIZE_KEY, (gint)gtk_adjustment_get_value (adj) * 2);
}

static void
refresh_was_modified_by_external_tool (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv = self->priv;
  gboolean modified_ext_tool = FALSE;

  // reveal side
  modified_ext_tool = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("unity_reveal_spot_otheroption")));

  // autohide mode
  if (!modified_ext_tool && (!gtk_widget_get_sensitive (WID ("unity_launcher_autohide"))))
    modified_ext_tool = TRUE;

  gtk_widget_set_visible (WID ("unity-label-external-tool"), modified_ext_tool);
}

static void
hidelauncher_set_sensitivity_reveal (CcAppearancePanel *self, gboolean autohide)
{
  CcAppearancePanelPrivate *priv = self->priv;
  gtk_widget_set_sensitive (WID ("unity_reveal_label"), autohide);
  gtk_widget_set_sensitive (WID ("unity_reveal_spot_topleft"), autohide);
  gtk_widget_set_sensitive (WID ("unity_reveal_spot_left"), autohide);
  gtk_widget_set_sensitive (WID ("unity-launcher-sensitivity"), autohide);
  gtk_widget_set_sensitive (WID ("unity-launcher-sensitivity-label"), autohide);
  gtk_widget_set_sensitive (WID ("unity-launcher-sensitivity-low-label"), autohide);
  gtk_widget_set_sensitive (WID ("unity-launcher-sensitivity-high-label"), autohide);
  gtk_widget_set_sensitive (WID ("unity-launcher-sensitivity-high-label"), autohide);
}

static void
hidelauncher_widget_refresh (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv = self->priv;
  gint value = g_settings_get_int (priv->unity_settings, UNITY_LAUNCHERHIDE_KEY);
  gboolean autohide = (value != 0);

  // handle not supported value
  if (value != 0 && value != 1)
    {
      gtk_widget_set_sensitive (WID ("unity_launcher_autohide"), FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (WID ("unity_launcher_autohide"), TRUE);
      gtk_switch_set_active (GTK_SWITCH (WID ("unity_launcher_autohide")), autohide);
    }

  hidelauncher_set_sensitivity_reveal (self, autohide);
  refresh_was_modified_by_external_tool (self);
}

static void
ext_hidelauncher_changed_callback (GSettings* settings,
                                   guint key,
                                   gpointer user_data)
{
  hidelauncher_widget_refresh (CC_APPEARANCE_PANEL (user_data));
}

static void
on_hidelauncher_changed (GtkSwitch *switcher, gboolean enabled, gpointer user_data)
{
  gint value = 0;
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);

  gint unity_value = g_settings_get_int (self->priv->unity_settings, UNITY_LAUNCHERHIDE_KEY);
  gboolean unity_autohide_enabled;

  unity_autohide_enabled = (unity_value != 0);
  if (gtk_switch_get_active (switcher))
    {
      /* change value to "active" if activation isn't due to gsettings switching to any value */
      if (unity_autohide_enabled)
        return;
      value = 1;
    }

  /* 3d */
  g_settings_set_int (self->priv->unity_settings, UNITY_LAUNCHERHIDE_KEY, value);
  hidelauncher_set_sensitivity_reveal (self, (value != -1));
}

static void
reveallauncher_widget_refresh (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv = self->priv;
  gint value = g_settings_get_int (priv->unity_settings, UNITY_LAUNCHERREVEAL_KEY);

  if (value == 1)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("unity_reveal_spot_topleft")), TRUE);
  else if (value == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("unity_reveal_spot_left")), TRUE);
  else
    /* this is a hidden spot when another option is selected (through ccsm) */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("unity_reveal_spot_otheroption")), TRUE);

  refresh_was_modified_by_external_tool (self);
}

static void
ext_reveallauncher_changed_callback (GSettings* settings,
                                     guint key,
                                     gpointer user_data)
{
  reveallauncher_widget_refresh (CC_APPEARANCE_PANEL (user_data));
}

static void
on_reveallauncher_changed (GtkToggleButton *button, gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;
  gint reveal_spot = 0;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("unity_reveal_spot_topleft"))))
    reveal_spot = 1;
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("unity_reveal_spot_left"))))
    reveal_spot = 0;

  g_settings_set_int (priv->unity_settings, UNITY_LAUNCHERREVEAL_KEY, reveal_spot);
  reveallauncher_widget_refresh (self);
}

static void
launcher_sensitivity_widget_refresh (GtkAdjustment *launcher_sensitivity_adj, GSettings *unity_settings)
{
  gdouble value = g_settings_get_double (unity_settings, UNITY_LAUNCHERSENSITIVITY_KEY);
  gtk_adjustment_set_value (launcher_sensitivity_adj, (gdouble)value);
}

static void
ext_launchersensitivity_changed_callback (GSettings* settings,
                                          guint key,
                                          gpointer user_data)
{
  launcher_sensitivity_widget_refresh (GTK_ADJUSTMENT (user_data), settings);
}

static void
on_launchersensitivity_changed (GtkAdjustment *adj, gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;
  gdouble value = gtk_adjustment_get_value (adj);

  g_settings_set_double (priv->unity_settings, UNITY_LAUNCHERSENSITIVITY_KEY, value);
}

gboolean
enable_workspaces_widget_refresh (gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;
  GtkToggleButton *button = GTK_TOGGLE_BUTTON (WID ("check_enable_workspaces"));

  gint hsize = g_settings_get_int (priv->compizcore_settings, COMPIZCORE_HSIZE_KEY);
  gint vsize = g_settings_get_int (priv->compizcore_settings, COMPIZCORE_VSIZE_KEY);

  if (hsize > 1 || vsize > 1)
    {
      if (!gtk_toggle_button_get_active (button))
        gtk_toggle_button_set_active (button, TRUE);
    }
  else
    gtk_toggle_button_set_active (button, FALSE);

  return FALSE;
}

static void
ext_enableworkspaces_changed_callback (GSettings* settings,
                                       guint key,
                                       gpointer user_data)
{
  g_idle_add((GSourceFunc) enable_workspaces_widget_refresh, user_data);
}                              

static void
on_enable_workspaces_changed (GtkToggleButton *button, gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;
  gint hsize = 1;
  gint vsize = 1;

  if (gtk_toggle_button_get_active (button))
  {
    hsize = vsize = 2;
  }

  g_settings_set_int (priv->compizcore_settings, COMPIZCORE_HSIZE_KEY, hsize);
  g_settings_set_int (priv->compizcore_settings, COMPIZCORE_VSIZE_KEY, hsize);
}

static void
enable_showdesktop_widget_refresh (gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;
  GtkToggleButton *button = GTK_TOGGLE_BUTTON (WID ("check_showdesktop_in_launcher"));
  gchar **favorites = NULL;
  gboolean show_desktop_found = FALSE;

  favorites = g_settings_get_strv (priv->unity_launcher_settings, UNITY_FAVORITES_KEY);
  while (*favorites != NULL)
    {
      if (g_strcmp0 (*favorites, SHOW_DESKTOP_UNITY_FAVORITE_STR) == 0)
        show_desktop_found = TRUE;
      favorites++;
    }

  if (show_desktop_found)
    gtk_toggle_button_set_active (button, TRUE);
  else
    gtk_toggle_button_set_active (button, FALSE);
}

static void
ext_enableshowdesktop_changed_callback (GSettings* settings,
                                        guint key,
                                        gpointer user_data)
{
  enable_showdesktop_widget_refresh (user_data);
}

static void
on_enable_showdesktop_changed (GtkToggleButton *button, gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;

  gchar** favorites;
  GPtrArray* newfavorites = g_ptr_array_new ();
  gboolean show_desktop_in_array = FALSE;

  favorites = g_settings_get_strv (priv->unity_launcher_settings, UNITY_FAVORITES_KEY);
  if (gtk_toggle_button_get_active (button))
    {

      while (*favorites != NULL)
        {
          // add the current element to the set
          g_ptr_array_add (newfavorites, (gpointer) g_strdup (*favorites));

          // if found running-apps, the show desktop element is added after that one
          if (g_strcmp0 (*favorites, "unity://running-apps") == 0)
            {
              favorites++;
              if (*favorites != NULL)
                {
                   // insert the additional element if not the favorite string
                   if (g_strcmp0 (*favorites, SHOW_DESKTOP_UNITY_FAVORITE_STR) != 0)
                     g_ptr_array_add (newfavorites, (gpointer) g_strdup (SHOW_DESKTOP_UNITY_FAVORITE_STR));
                   g_ptr_array_add (newfavorites, (gpointer) g_strdup (*favorites));
                   show_desktop_in_array = TRUE;
                }
              else
                break;
            }
          favorites++;
        }
        if (!show_desktop_in_array)
          g_ptr_array_add (newfavorites, (gpointer) g_strdup (SHOW_DESKTOP_UNITY_FAVORITE_STR));
      }
  else
    {
      while (*favorites != NULL)
        {
          if (g_strcmp0 (*favorites, SHOW_DESKTOP_UNITY_FAVORITE_STR) != 0)
            g_ptr_array_add (newfavorites, (gpointer) g_strdup (*favorites));
          favorites++;
        }
    }
  g_ptr_array_add (newfavorites, NULL);
  g_settings_set_strv (priv->unity_launcher_settings, UNITY_FAVORITES_KEY, (const gchar **)newfavorites->pdata);
  g_ptr_array_free (newfavorites, TRUE);
  
}

static gboolean
unity_own_setting_exists (CcAppearancePanel *self, const gchar* key_name)
{
  if (!self->priv->unity_own_settings)
    return FALSE;

  gchar** unity_keys;
  gchar** key;

  unity_keys = g_settings_list_keys (self->priv->unity_own_settings);

  for (key = unity_keys; *key; ++key)
    {
      if (g_strcmp0 (*key, key_name) == 0)
        return TRUE;
    }

  g_strfreev (unity_keys);
  return FALSE;
}

static void
menulocation_widget_refresh (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv = self->priv;

  gboolean has_setting = unity_own_setting_exists (self, UNITY_INTEGRATED_MENUS_KEY);
  gtk_widget_set_visible (WID ("unity_menus_box"), has_setting);
  gtk_widget_set_visible (WID ("menu_separator"), has_setting);

  if (!has_setting)
    return;

  gboolean value = g_settings_get_boolean (priv->unity_own_settings, UNITY_INTEGRATED_MENUS_KEY);

  if (value)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("unity_local_menus")), TRUE);
  else
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("unity_global_menus")), TRUE);
}

static void
ext_menulocation_changed_callback (GSettings* settings,
                                   guint key,
                                   gpointer user_data)
{
  menulocation_widget_refresh (CC_APPEARANCE_PANEL (user_data));
}

static void
on_menulocation_changed (GtkToggleButton *button, gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;
  gboolean local_menus = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("unity_local_menus")));

  g_settings_set_boolean (priv->unity_own_settings, UNITY_INTEGRATED_MENUS_KEY, local_menus);
  menulocation_widget_refresh (self);
}

static void
on_restore_defaults_page2_clicked (GtkButton *button, gpointer user_data)
{
  CcAppearancePanel *self = CC_APPEARANCE_PANEL (user_data);
  CcAppearancePanelPrivate *priv = self->priv;

  /* reset defaut for the profile and get the default */
  g_settings_reset (priv->unity_settings, UNITY_LAUNCHERHIDE_KEY);
  g_settings_reset (priv->unity_settings, UNITY_LAUNCHERSENSITIVITY_KEY);
  g_settings_reset (priv->unity_settings, UNITY_LAUNCHERREVEAL_KEY);
  g_settings_reset (priv->compizcore_settings, COMPIZCORE_HSIZE_KEY);
  g_settings_reset (priv->compizcore_settings, COMPIZCORE_VSIZE_KEY);

  if (unity_own_setting_exists (self, UNITY_INTEGRATED_MENUS_KEY))
    g_settings_reset (priv->unity_own_settings, UNITY_INTEGRATED_MENUS_KEY);

  GtkToggleButton *showdesktop = GTK_TOGGLE_BUTTON (WID ("check_showdesktop_in_launcher"));
  gtk_toggle_button_set_active(showdesktop, TRUE);
}

/* <hacks> */

/* Get scrolling in the right direction */
static gboolean
on_scale_scroll_event (GtkWidget      *widget,
                       GdkEventScroll *event)
{
  gdouble value;
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (widget));
  double min = gtk_adjustment_get_lower (adj);
  double max = gtk_adjustment_get_upper (adj);
  gdouble delta = max - min;

  value = gtk_adjustment_get_value (adj);

  if ((event->direction == GDK_SCROLL_UP) ||
     (event->direction == GDK_SCROLL_SMOOTH && event->delta_y < 0))
    {
      if (value + delta/8 > max)
        value = max;
      else
        value = value + delta/8;
      gtk_adjustment_set_value (adj, value);
    }
  else if ((event->direction == GDK_SCROLL_DOWN) ||
           (event->direction == GDK_SCROLL_SMOOTH && event->delta_y > 0))
    {
      if (value - delta/8 < min)
        value = min;
      else
        value = value - delta/8;
      gtk_adjustment_set_value (adj, value);
    }

  return TRUE;
}

/* </hacks> */

static void
setup_unity_settings (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv = self->priv;
  GtkAdjustment* iconsize_adj;
  GtkAdjustment* launcher_sensitivity_adj;
  GtkScale* iconsize_scale;
  GtkScale* launcher_sensitivity_scale;
  GSettingsSchema *schema;
  GSettingsSchemaSource* source;

  source = g_settings_schema_source_get_default ();
  schema = g_settings_schema_source_lookup (source, UNITY_OWN_GSETTINGS_SCHEMA, TRUE);
  if (schema)
    {
      priv->unity_own_settings = g_settings_new (UNITY_OWN_GSETTINGS_SCHEMA);
      g_settings_schema_unref (schema);
    }
  schema = g_settings_schema_source_lookup (source, UNITY_LAUNCHER_GSETTINGS_SCHEMA, TRUE);
  if (schema)
    {
      priv->unity_launcher_settings = g_settings_new (UNITY_LAUNCHER_GSETTINGS_SCHEMA);
      g_settings_schema_unref (schema);
    }
  schema = g_settings_schema_source_lookup (source, UNITY_GSETTINGS_SCHEMA, TRUE);
  if (schema)
    {
      priv->unity_settings = g_settings_new_with_path (UNITY_GSETTINGS_SCHEMA, UNITY_GSETTINGS_PATH);
      g_settings_schema_unref (schema);
    }
  schema = g_settings_schema_source_lookup (source, COMPIZCORE_GSETTINGS_SCHEMA, TRUE);
  if (schema)
    {
      priv->compizcore_settings = g_settings_new_with_path (COMPIZCORE_GSETTINGS_SCHEMA, COMPIZCORE_GSETTINGS_PATH);
      g_settings_schema_unref (schema);
    }

  if (!priv->unity_settings || !priv->compizcore_settings || !priv->unity_own_settings || !priv->unity_launcher_settings)
    return;

  /* Icon size change - we halve the sizes so we can only get even values*/
  iconsize_adj = gtk_adjustment_new (DEFAULT_ICONSIZE / 2, MIN_ICONSIZE / 2, MAX_ICONSIZE / 2, 1, 4, 0);
  iconsize_scale = GTK_SCALE (WID ("unity-iconsize-scale"));
  gtk_range_set_adjustment (GTK_RANGE (iconsize_scale), iconsize_adj);
  gtk_scale_add_mark (iconsize_scale, DEFAULT_ICONSIZE / 2, GTK_POS_BOTTOM, NULL);
  g_signal_connect (priv->unity_settings, "changed::" UNITY_ICONSIZE_KEY,
                    G_CALLBACK (ext_iconsize_changed_callback), iconsize_adj);

  g_signal_connect (G_OBJECT (iconsize_scale), "format-value",
                    G_CALLBACK (on_iconsize_format_value), NULL);
  g_signal_connect (iconsize_adj, "value_changed",
                    G_CALLBACK (on_iconsize_changed), priv->unity_settings);
  g_signal_connect (G_OBJECT (iconsize_scale), "scroll-event",
                    G_CALLBACK (on_scale_scroll_event), NULL);
  iconsize_widget_refresh (iconsize_adj, priv->unity_settings);

  /* Reveal spot setting */
  g_signal_connect (priv->unity_settings, "changed::" UNITY_LAUNCHERREVEAL_KEY,
                    G_CALLBACK (ext_reveallauncher_changed_callback), self);
  g_signal_connect (WID ("unity_reveal_spot_topleft"), "toggled",
                     G_CALLBACK (on_reveallauncher_changed), self);
  g_signal_connect (WID ("unity_reveal_spot_left"), "toggled",
                     G_CALLBACK (on_reveallauncher_changed), self);
  reveallauncher_widget_refresh (self);

  /* Launcher reveal */
  launcher_sensitivity_adj = gtk_adjustment_new (2, MIN_LAUNCHER_SENSIVITY, MAX_LAUNCHER_SENSIVITY, 0.1, 1, 0);
  launcher_sensitivity_scale = GTK_SCALE (WID ("unity-launcher-sensitivity"));
  gtk_range_set_adjustment (GTK_RANGE (launcher_sensitivity_scale), launcher_sensitivity_adj);
  gtk_scale_add_mark (launcher_sensitivity_scale, 2, GTK_POS_BOTTOM, NULL);
  g_signal_connect (priv->unity_settings, "changed::" UNITY_LAUNCHERSENSITIVITY_KEY,
                    G_CALLBACK (ext_launchersensitivity_changed_callback), launcher_sensitivity_adj);
  g_signal_connect (launcher_sensitivity_adj, "value_changed",
                    G_CALLBACK (on_launchersensitivity_changed), self);
  g_signal_connect (G_OBJECT (launcher_sensitivity_scale), "scroll-event",
                    G_CALLBACK (on_scale_scroll_event), NULL);
  launcher_sensitivity_widget_refresh (launcher_sensitivity_adj, priv->unity_settings);

  /* Autohide launcher setting */
  g_signal_connect (priv->unity_settings, "changed::" UNITY_LAUNCHERHIDE_KEY,
                    G_CALLBACK (ext_hidelauncher_changed_callback), self);
  g_signal_connect (WID ("unity_launcher_autohide"), "notify::active",
                    G_CALLBACK (on_hidelauncher_changed), self);
  hidelauncher_widget_refresh (self);

  /* Enabling workspaces */
  g_signal_connect (priv->compizcore_settings, "changed::" COMPIZCORE_HSIZE_KEY,
                    G_CALLBACK (ext_enableworkspaces_changed_callback), self);
  g_signal_connect (priv->compizcore_settings, "changed::" COMPIZCORE_VSIZE_KEY,
                    G_CALLBACK (ext_enableworkspaces_changed_callback), self);
  g_signal_connect (WID ("check_enable_workspaces"), "toggled",
                     G_CALLBACK (on_enable_workspaces_changed), self);
  enable_workspaces_widget_refresh (self);

  /* Enabling show desktop icon */
  g_signal_connect (priv->unity_launcher_settings, "changed::" UNITY_FAVORITES_KEY,
                    G_CALLBACK (ext_enableshowdesktop_changed_callback), self);
  g_signal_connect (WID ("check_showdesktop_in_launcher"), "toggled",
                     G_CALLBACK (on_enable_showdesktop_changed), self);
  enable_showdesktop_widget_refresh (self);

  /* Menu location */
  g_signal_connect (priv->unity_own_settings, "changed::" UNITY_INTEGRATED_MENUS_KEY,
                    G_CALLBACK (ext_menulocation_changed_callback), self);
  g_signal_connect (WID ("unity_global_menus"), "toggled",
                     G_CALLBACK (on_menulocation_changed), self);
  g_signal_connect (WID ("unity_local_menus"), "toggled",
                     G_CALLBACK (on_menulocation_changed), self);
  menulocation_widget_refresh (self);

  /* Restore defaut on second page */
  g_signal_connect (WID ("button-restore-unitybehavior"), "clicked",
                    G_CALLBACK (on_restore_defaults_page2_clicked), self);
}

static void
cc_appearance_panel_init (CcAppearancePanel *self)
{
  CcAppearancePanelPrivate *priv;
  gchar *objects_unity[] = { "style-liststore",
      "sources-liststore", "theme-list-store", "main-notebook", "sizegroup", NULL };
  GError *err = NULL;
  GtkWidget *widget;
  GtkListStore *store;
  GtkStyleContext *context;

  priv = self->priv = APPEARANCE_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();
  gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
  gtk_builder_add_objects_from_file (priv->builder,
                                       PKGDATADIR"/appearance.ui",
                                       objects_unity, &err);

  if (err)
    {
      g_warning ("Could not load ui: %s", err->message);
      g_error_free (err);
      return;
    }

  /* See shell_notify_cb for details */
  g_signal_connect (WID ("scrolledwindow1"), "realize",
                    G_CALLBACK (scrolled_realize_cb), self);

  priv->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (priv->settings);

  store = (GtkListStore*) gtk_builder_get_object (priv->builder,
                                                  "sources-liststore");

  priv->wallpapers_source = bg_wallpapers_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Wallpapers"),
                                     COL_SOURCE_TYPE, SOURCE_WALLPAPERS,
                                     COL_SOURCE, priv->wallpapers_source,
                                     -1);

  priv->pictures_source = bg_pictures_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Pictures Folder"),
                                     COL_SOURCE_TYPE, SOURCE_PICTURES,
                                     COL_SOURCE, priv->pictures_source,
                                     -1);

  priv->colors_source = bg_colors_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Colors & Gradients"),
                                     COL_SOURCE_TYPE, SOURCE_COLORS,
                                     COL_SOURCE, priv->colors_source,
                                     -1);

#ifdef HAVE_LIBSOCIALWEB
  priv->flickr_source = bg_flickr_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Flickr"),
                                     COL_SOURCE_TYPE, SOURCE_FLICKR,
                                     COL_SOURCE, priv->flickr_source,
                                     -1);
#endif


  /* add the top level widget */
  widget = WID ("main-notebook");

  gtk_container_add (GTK_CONTAINER (self), widget);
  gtk_widget_show_all (GTK_WIDGET (self));

  /* connect to source change signal */
  widget = WID ("sources-combobox");
  g_signal_connect (widget, "changed", G_CALLBACK (source_changed_cb), priv);

  /* select first item */
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

  /* connect to the background iconview change signal */
  widget = WID ("backgrounds-iconview");
  g_signal_connect (widget, "selection-changed",
                    G_CALLBACK (backgrounds_changed_cb),
                    self);

  /* Join treeview and buttons */
  widget = WID ("scrolledwindow1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
  widget = WID ("toolbar1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  g_signal_connect (WID ("add_button"), "clicked",
		    G_CALLBACK (add_button_clicked), self);
  g_signal_connect (WID ("remove_button"), "clicked",
		    G_CALLBACK (remove_button_clicked), self);

  /* Add drag and drop support for bg images */
  widget = WID ("scrolledwindow1");
  gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets (widget);
  g_signal_connect (widget, "drag-data-received",
		    G_CALLBACK (cc_appearance_panel_drag_uris), self);


  /* setup preview area */
  gtk_label_set_ellipsize (GTK_LABEL (WID ("background-label")), PANGO_ELLIPSIZE_END);
  widget = WID ("preview-area");
  g_signal_connect (widget, "draw", G_CALLBACK (preview_draw_cb),
                    self);

  priv->display_base = gdk_pixbuf_new_from_file (PKGDATADIR
                                                 "/display-base.png",
                                                 NULL);
  priv->display_overlay = gdk_pixbuf_new_from_file (PKGDATADIR
                                                    "/display-overlay.png",
                                                    NULL);

  g_signal_connect (WID ("style-combobox"), "changed",
                    G_CALLBACK (style_changed_cb), self);

  g_signal_connect (WID ("style-pcolor"), "color-set",
                    G_CALLBACK (color_changed_cb), self);
  g_signal_connect (WID ("style-scolor"), "color-set",
                    G_CALLBACK (color_changed_cb), self);
  g_signal_connect (WID ("swap-color-button"), "clicked",
                    G_CALLBACK (swap_colors_clicked), self);

  priv->copy_cancellable = g_cancellable_new ();

  priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  load_current_bg (self);

  update_preview (priv, NULL);

  /* Setup the edit box with our current settings */
  source_update_edit_box (priv, TRUE);

  /* Setup theme selector */
  setup_theme_selector (self);

  /* Setup unity settings */
  setup_unity_settings (self);
}

void
cc_appearance_panel_register (GIOModule *module)
{
  cc_appearance_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_APPEARANCE_PANEL,
                                  "appearance", 0);
}

