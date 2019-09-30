/*
 * Copyright 2013 Canonical Ltd.
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
 *
 * Authors:
 *     Conor Curran <conor.curran@canonical.com>
 *     Mirco Müller <mirco.mueller@canonical.com>
 *     Lars Uebernickel <lars.uebernickel@canonical.com>
 */

#include "config.h"

#include "idomediaplayermenuitem.h"
#include "idoactionhelper.h"

#define ALBUM_ART_SIZE 60

typedef GtkMenuItemClass IdoMediaPlayerMenuItemClass;

struct _IdoMediaPlayerMenuItem
{
  GtkMenuItem parent;

  GCancellable *cancellable;
  GtkWidget* player_label;
  GtkWidget* player_icon;
  GtkWidget* metadata_widget;
  GtkWidget* album_art;
  GtkWidget* artist_label;
  GtkWidget* piece_label;
  GtkWidget* container_label;

  gboolean running;
};

G_DEFINE_TYPE (IdoMediaPlayerMenuItem, ido_media_player_menu_item, GTK_TYPE_MENU_ITEM);

static void
ido_media_player_menu_item_dispose (GObject *object)
{
  IdoMediaPlayerMenuItem *self = IDO_MEDIA_PLAYER_MENU_ITEM (object);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  G_OBJECT_CLASS (ido_media_player_menu_item_parent_class)->dispose (object);
}

static gboolean
ido_media_player_menu_item_draw (GtkWidget *widget,
                                 cairo_t   *cr)
{
  IdoMediaPlayerMenuItem *self = IDO_MEDIA_PLAYER_MENU_ITEM (widget);

  GTK_WIDGET_CLASS (ido_media_player_menu_item_parent_class)->draw (widget, cr);

  /* draw a triangle next to the application name if the app is running */
  if (self->running)
    {
      const int arrow_width = 5;
      const int half_arrow_height = 4;

      GdkRGBA color;
      GtkAllocation allocation;
      GtkAllocation label_allocation;
      int x;
      int y;

      gtk_style_context_get_color (gtk_widget_get_style_context (widget),
                                   gtk_widget_get_state (widget),
                                   &color);

      gtk_widget_get_allocation (widget, &allocation);
      gtk_widget_get_allocation (self->player_label, &label_allocation);
      x = allocation.x;
      y = label_allocation.y - allocation.y + label_allocation.height / 2;

      cairo_move_to (cr, x, y - half_arrow_height);
      cairo_line_to (cr, x, y + half_arrow_height);
      cairo_line_to (cr, x + arrow_width, y);
      cairo_close_path (cr);

      gdk_cairo_set_source_rgba (cr, &color);
      cairo_fill (cr);
    }

  return FALSE;
}

static void
ido_media_player_menu_item_class_init (IdoMediaPlayerMenuItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = ido_media_player_menu_item_dispose;

  widget_class->draw = ido_media_player_menu_item_draw;
}

static GtkWidget *
track_info_label_new ()
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_width_chars (GTK_LABEL (label), 25);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 25);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);

  return label;
}

static void
ido_media_player_menu_item_init (IdoMediaPlayerMenuItem *self)
{
  GtkWidget *grid;

  self->cancellable = g_cancellable_new ();

  self->player_icon = gtk_image_new();
  gtk_widget_set_margin_right (self->player_icon, 6);
  gtk_widget_set_halign (self->player_icon, GTK_ALIGN_START);

  self->player_label = gtk_label_new (NULL);
  gtk_widget_set_halign (self->player_label, GTK_ALIGN_START);
  gtk_widget_set_hexpand (self->player_label, TRUE);

  self->album_art = gtk_image_new();
  gtk_widget_set_size_request (self->album_art, ALBUM_ART_SIZE, ALBUM_ART_SIZE);
  gtk_widget_set_margin_right (self->album_art, 8);

  self->artist_label = track_info_label_new ();
  self->piece_label = track_info_label_new ();

  self->container_label = track_info_label_new ();
  gtk_widget_set_vexpand (self->container_label, TRUE);
  gtk_widget_set_valign (self->container_label, GTK_ALIGN_START);

  self->metadata_widget = gtk_grid_new ();
  gtk_grid_attach (GTK_GRID (self->metadata_widget), self->album_art, 0, 0, 1, 4);
  gtk_grid_attach (GTK_GRID (self->metadata_widget), self->piece_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (self->metadata_widget), self->artist_label, 1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (self->metadata_widget), self->container_label, 1, 2, 1, 1);

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 8);
  gtk_grid_attach (GTK_GRID (grid), self->player_icon, 0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), self->player_label, 1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), self->metadata_widget, 0, 1, 2, 1);

  gtk_container_add (GTK_CONTAINER (self), grid);
  gtk_widget_show_all (grid);

  /* hide metadata by defalut (player is not running) */
  gtk_widget_hide (self->metadata_widget);
}

static void
ido_media_player_menu_item_set_player_name (IdoMediaPlayerMenuItem *self,
                                            const gchar            *name)
{
  g_return_if_fail (IDO_IS_MEDIA_PLAYER_MENU_ITEM (self));

  gtk_label_set_label (GTK_LABEL (self->player_label), name);
}

static void
ido_media_player_menu_item_set_player_icon (IdoMediaPlayerMenuItem *self,
                                            GIcon                  *icon)
{
  g_return_if_fail (IDO_IS_MEDIA_PLAYER_MENU_ITEM (self));

  gtk_image_set_from_gicon (GTK_IMAGE (self->player_icon), icon, GTK_ICON_SIZE_MENU);
}

static void
ido_media_player_menu_item_set_is_running (IdoMediaPlayerMenuItem *self,
                                           gboolean                running)
{
  g_return_if_fail (IDO_IS_MEDIA_PLAYER_MENU_ITEM (self));

  if (self->running != running)
    {
      self->running = running;
      gtk_widget_queue_draw (GTK_WIDGET (self));
    }
}

static void
album_art_received (GObject      *object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  IdoMediaPlayerMenuItem *self = user_data;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = gdk_pixbuf_new_from_stream_finish (result, &error);
  if (pixbuf == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("unable to fetch album art: %s", error->message);

      g_error_free (error);
      return;
    }

  gtk_image_set_from_pixbuf (GTK_IMAGE (self->album_art), pixbuf);
  g_object_unref (pixbuf);
}

static void
album_art_file_opened (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  IdoMediaPlayerMenuItem *self = user_data;
  GFileInputStream *input;
  GError *error = NULL;

  input = g_file_read_finish (G_FILE (object), result, &error);
  if (input == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("unable to fetch album art: %s", error->message);

      g_error_free (error);
      return;
    }

  gdk_pixbuf_new_from_stream_at_scale_async (G_INPUT_STREAM (input),
                                             ALBUM_ART_SIZE, ALBUM_ART_SIZE, TRUE,
                                             self->cancellable,
                                             album_art_received, self);

  g_object_unref (input);
}

static void
ido_media_player_menu_item_set_album_art (IdoMediaPlayerMenuItem *self,
                                          const gchar            *url)
{
  GFile *file;

  g_return_if_fail (IDO_IS_MEDIA_PLAYER_MENU_ITEM (self));

  gtk_image_clear (GTK_IMAGE (self->album_art));

  if (url == NULL)
    return;

  file = g_file_new_for_uri (url);
  g_file_read_async (file, G_PRIORITY_DEFAULT, self->cancellable, album_art_file_opened, self);

  g_object_unref (file);
}

static void
gtk_label_set_markup_printf_escaped (GtkLabel    *label,
                                     const gchar *format,
                                     ...)
{
  va_list args;
  gchar *str;

  va_start (args, format);
  str = g_markup_vprintf_escaped (format, args);
  gtk_label_set_markup (label, str);
  va_end (args);

  g_free (str);
}

static void
ido_media_player_menu_item_set_metadata (IdoMediaPlayerMenuItem *self,
                                         const gchar            *title,
                                         const gchar            *artist,
                                         const gchar            *album,
                                         const gchar            *art_url)
{
  g_return_if_fail (IDO_IS_MEDIA_PLAYER_MENU_ITEM (self));

  /* hide if there's no metadata */
  if (title == NULL || *title == '\0')
    {
      gtk_label_set_label (GTK_LABEL (self->piece_label), NULL);
      gtk_label_set_label (GTK_LABEL (self->artist_label), NULL);
      gtk_label_set_label (GTK_LABEL (self->container_label), NULL);
      ido_media_player_menu_item_set_album_art (self, NULL);
      gtk_widget_hide (self->metadata_widget);
    }
  else
    {
      gtk_label_set_markup_printf_escaped (GTK_LABEL (self->piece_label), "<small>%s</small>", title);
      gtk_label_set_markup_printf_escaped (GTK_LABEL (self->artist_label), "<small>%s</small>", artist);
      gtk_label_set_markup_printf_escaped (GTK_LABEL (self->container_label), "<small>%s</small>", album);
      ido_media_player_menu_item_set_album_art (self, art_url);
      gtk_widget_show (self->metadata_widget);
    }
}

static void
ido_media_player_menu_item_state_changed (IdoActionHelper *helper,
                                          GVariant        *state,
                                          gpointer         user_data)
{
  IdoMediaPlayerMenuItem *widget;
  gboolean running = FALSE;
  const gchar *title = NULL;
  const gchar *artist = NULL;
  const gchar *album = NULL;
  const gchar *art_url = NULL;

  g_variant_lookup (state, "running", "b", &running);
  g_variant_lookup (state, "title", "&s", &title);
  g_variant_lookup (state, "artist", "&s", &artist);
  g_variant_lookup (state, "album", "&s", &album);
  g_variant_lookup (state, "art-url", "&s", &art_url);

  widget = IDO_MEDIA_PLAYER_MENU_ITEM (ido_action_helper_get_widget (helper));
  ido_media_player_menu_item_set_is_running (widget, running);
  ido_media_player_menu_item_set_metadata (widget, title, artist, album, art_url);
}

GtkMenuItem *
ido_media_player_menu_item_new_from_model (GMenuItem    *menuitem,
                                           GActionGroup *actions)
{
  GtkMenuItem *widget;
  gchar *label;
  gchar *action;
  GVariant *v;

  widget = g_object_new (IDO_TYPE_MEDIA_PLAYER_MENU_ITEM, NULL);

  if (g_menu_item_get_attribute (menuitem, "label", "s", &label))
    {
      ido_media_player_menu_item_set_player_name (IDO_MEDIA_PLAYER_MENU_ITEM (widget), label);
      g_free (label);
    }

  if ((v = g_menu_item_get_attribute_value (menuitem, "icon", NULL)))
    {
      GIcon *icon;

      icon = g_icon_deserialize (v);
      if (icon)
        {
          ido_media_player_menu_item_set_player_icon (IDO_MEDIA_PLAYER_MENU_ITEM (widget), icon);
          g_object_unref (icon);
        }

      g_variant_unref (v);
    }

  if (g_menu_item_get_attribute (menuitem, "action", "s", &action))
    {
      IdoActionHelper *helper;

      helper = ido_action_helper_new (GTK_WIDGET (widget), actions, action, NULL);
      g_signal_connect (helper, "action-state-changed",
                        G_CALLBACK (ido_media_player_menu_item_state_changed), NULL);

      g_signal_connect_object (widget, "activate",
                               G_CALLBACK (ido_action_helper_activate),
                               helper, G_CONNECT_SWAPPED);

      g_signal_connect_swapped (widget, "destroy", G_CALLBACK (g_object_unref), helper);

      g_free (action);
    }

  return widget;
}
