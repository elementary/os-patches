/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2015 Red Hat, Inc.
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
 *  - Ondrej Holy <oholy@redhat.com>
 */

#include <gtk/gtk.h>
#include <act/act.h>
#include <sys/stat.h>

#include "user-image.h"


struct _MctUserImage
{
  GtkImage parent_instance;

  ActUser *user;
};

G_DEFINE_TYPE (MctUserImage, mct_user_image, GTK_TYPE_IMAGE)

static GdkPixbuf *
round_image (GdkPixbuf *pixbuf)
{
  GdkPixbuf *dest = NULL;
  cairo_surface_t *surface;
  cairo_t *cr;
  gint size;

  size = gdk_pixbuf_get_width (pixbuf);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
  cr = cairo_create (surface);

  /* Clip a circle */
  cairo_arc (cr, size / 2, size / 2, size / 2, 0, 2 * G_PI);
  cairo_clip (cr);
  cairo_new_path (cr);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  dest = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return dest;
}

static cairo_surface_t *
render_user_icon (ActUser *user,
                  gint     icon_size,
                  gint     scale)
{
  g_autoptr(GdkPixbuf) source_pixbuf = NULL;
  GdkPixbuf *pixbuf = NULL;
  GError *error;
  const gchar *icon_file;
  cairo_surface_t *surface = NULL;

  g_return_val_if_fail (ACT_IS_USER (user), NULL);
  g_return_val_if_fail (icon_size > 12, NULL);

  icon_file = act_user_get_icon_file (user);
  pixbuf = NULL;
  if (icon_file)
    {
      source_pixbuf = gdk_pixbuf_new_from_file_at_size (icon_file,
                                                        icon_size * scale,
                                                        icon_size * scale,
                                                        NULL);
      if (source_pixbuf)
        pixbuf = round_image (source_pixbuf);
    }

  if (pixbuf != NULL)
    goto out;

  error = NULL;
  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     "avatar-default",
                                     icon_size * scale,
                                     GTK_ICON_LOOKUP_FORCE_SIZE,
                                     &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

out:

  if (pixbuf != NULL)
    {
      surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
      g_object_unref (pixbuf);
    }

  return surface;
}

static void
render_image (MctUserImage *image)
{
  cairo_surface_t *surface;
  gint scale, pixel_size;

  if (image->user == NULL)
    return;

  pixel_size = gtk_image_get_pixel_size (GTK_IMAGE (image));
  scale = gtk_widget_get_scale_factor (GTK_WIDGET (image));
  surface = render_user_icon (image->user,
                              pixel_size > 0 ? pixel_size : 48,
                              scale);
  gtk_image_set_from_surface (GTK_IMAGE (image), surface);
  cairo_surface_destroy (surface);
}

void
mct_user_image_set_user (MctUserImage *image,
                         ActUser      *user)
{
  g_clear_object (&image->user);
  image->user = g_object_ref (user);

  render_image (image);
}

static void
mct_user_image_finalize (GObject *object)
{
  MctUserImage *image = MCT_USER_IMAGE (object);

  g_clear_object (&image->user);

  G_OBJECT_CLASS (mct_user_image_parent_class)->finalize (object);
}

static void
mct_user_image_class_init (MctUserImageClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = mct_user_image_finalize;
}

static void
mct_user_image_init (MctUserImage *image)
{
  g_signal_connect_swapped (image, "notify::scale-factor", G_CALLBACK (render_image), image);
  g_signal_connect_swapped (image, "notify::pixel-size", G_CALLBACK (render_image), image);
}

GtkWidget *
mct_user_image_new (void)
{
  return g_object_new (MCT_TYPE_USER_IMAGE, NULL);
}
