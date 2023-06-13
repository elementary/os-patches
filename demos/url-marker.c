/*
 * Copyright (C) 2009 Emmanuel Rodriguez <emmanuel.rodriguez@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <champlain/champlain.h>
#include <libsoup/soup.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* The data needed for constructing a marker */
typedef struct
{
  ChamplainMarkerLayer *layer;
  gdouble latitude;
  gdouble longitude;
} MarkerData;

/**
 * Returns a GdkPixbuf from a given SoupMessage. This function assumes that the
 * message has completed successfully.
 * If there's an error building the GdkPixbuf the function will return NULL and
 * set error accordingly.
 *
 * The GdkPixbuf has to be freed with g_object_unref.
 */
static GdkPixbuf *
pixbuf_new_from_message (SoupMessage *message,
    GError **error)
{
  const gchar *mime_type = NULL;
  GdkPixbufLoader *loader = NULL;
  GdkPixbuf *pixbuf = NULL;
  gboolean pixbuf_is_open = FALSE;

  *error = NULL;

  /*  Use a pixbuf loader that can load images of the same mime-type as the
      message.
   */
  mime_type = soup_message_headers_get_one (message->response_headers,
        "Content-Type");
  loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, error);
  if (loader != NULL)
    pixbuf_is_open = TRUE;
  if (*error != NULL)
    goto cleanup;


  gdk_pixbuf_loader_write (
      loader,
      (guchar *) message->response_body->data,
      message->response_body->length,
      error);
  if (*error != NULL)
    goto cleanup;

  gdk_pixbuf_loader_close (loader, error);
  pixbuf_is_open = FALSE;
  if (*error != NULL)
    goto cleanup;

  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
  if (pixbuf == NULL)
    goto cleanup;
  g_object_ref (G_OBJECT (pixbuf));

cleanup:
  if (pixbuf_is_open)
    gdk_pixbuf_loader_close (loader, NULL);

  if (loader != NULL)
    g_object_unref (G_OBJECT (loader));

  return pixbuf;
}


static ClutterActor *
texture_new_from_pixbuf (GdkPixbuf *pixbuf, GError **error)
{
  ClutterActor *texture = NULL;
  gfloat width, height;
  ClutterContent *content;
  
  content = clutter_image_new ();
  clutter_image_set_data (CLUTTER_IMAGE (content),
                          gdk_pixbuf_get_pixels (pixbuf),
                          gdk_pixbuf_get_has_alpha (pixbuf)
                            ? COGL_PIXEL_FORMAT_RGBA_8888
                            : COGL_PIXEL_FORMAT_RGB_888,
                          gdk_pixbuf_get_width (pixbuf),
                          gdk_pixbuf_get_height (pixbuf),
                          gdk_pixbuf_get_rowstride (pixbuf),
                          NULL);

  texture = clutter_actor_new ();
  clutter_content_get_preferred_size (content, &width, &height);
  clutter_actor_set_size (texture, width, height);
  clutter_actor_set_content (texture, content);
  clutter_content_invalidate (content);
  g_object_unref (content);

  return texture;
}


/**
 * Called when an image has been downloaded. This callback will transform the
 * image data (binary chunk sent by the remote web server) into a valid Clutter
 * actor (a texture) and will use this as the source image for a new marker.
 * The marker will then be added to an existing layer.
 *
 * This callback expects the parameter data to be a valid ChamplainMarkerLayer.
 */
static void
image_downloaded_cb (SoupSession *session,
    SoupMessage *message,
    gpointer data)
{
  MarkerData *marker_data = NULL;
  SoupURI *uri = NULL;
  char *url = NULL;
  GError *error = NULL;
  GdkPixbuf *pixbuf = NULL;
  ClutterActor *texture = NULL;
  ClutterActor *marker = NULL;

  if (data == NULL)
    goto cleanup;
  marker_data = (MarkerData *) data;

  /* Deal only with finished messages */
  uri = soup_message_get_uri (message);
  url = soup_uri_to_string (uri, FALSE);
  if (!SOUP_STATUS_IS_SUCCESSFUL (message->status_code))
    {
      g_print ("Download of %s failed with error code %d\n", url,
          message->status_code);
      goto cleanup;
    }

  pixbuf = pixbuf_new_from_message (message, &error);
  if (error != NULL)
    {
      g_print ("Failed to convert %s into an image: %s\n", url, error->message);
      goto cleanup;
    }

  /* Then transform the pixbuf into a texture */
  texture = texture_new_from_pixbuf (pixbuf, &error);
  if (error != NULL)
    {
      g_print ("Failed to convert %s into a texture: %s\n", url,
          error->message);
      goto cleanup;
    }

  /* Finally create a marker with the texture */
  marker = champlain_label_new_with_image (texture);
  texture = NULL;
  champlain_location_set_location (CHAMPLAIN_LOCATION (marker),
      marker_data->latitude, marker_data->longitude);
  champlain_marker_layer_add_marker (marker_data->layer, CHAMPLAIN_MARKER (marker));

cleanup:
  if (marker_data)
    g_object_unref (marker_data->layer);
  g_slice_free (MarkerData, marker_data);
  g_free (url);

  if (error != NULL)
    g_error_free (error);

  if (pixbuf != NULL)
    g_object_unref (G_OBJECT (pixbuf));

  if (texture != NULL)
    clutter_actor_destroy (CLUTTER_ACTOR (texture));
}


/**
 * Creates a marker at the given position with an image that's downloaded from
 * the given URL.
 *
 */
static void
create_marker_from_url (ChamplainMarkerLayer *layer,
    SoupSession *session,
    gdouble latitude,
    gdouble longitude,
    const gchar *url)
{
  SoupMessage *message;
  MarkerData *data;

  data = g_slice_new (MarkerData);
  data->layer = g_object_ref (layer);
  data->latitude = latitude;
  data->longitude = longitude;

  message = soup_message_new ("GET", url);
  soup_session_queue_message (session, message, image_downloaded_cb, data);
}


int
main (int argc, char *argv[])
{
  ClutterActor *view, *stage;
  ChamplainMarkerLayer *layer;
  SoupSession *session;

  if (clutter_init (&argc, &argv) != CLUTTER_INIT_SUCCESS)
    return 1;

  stage = clutter_stage_new ();
  clutter_actor_set_size (stage, 800, 600);
  g_signal_connect (stage, "destroy", G_CALLBACK (clutter_main_quit), NULL);

  /* Create the map view */
  view = champlain_view_new ();
  clutter_actor_set_size (CLUTTER_ACTOR (view), 800, 600);
  clutter_actor_add_child (stage, view);

  /* Create the markers and marker layer */
  layer = champlain_marker_layer_new_full (CHAMPLAIN_SELECTION_SINGLE);
  champlain_view_add_layer (CHAMPLAIN_VIEW (view), CHAMPLAIN_LAYER (layer));
  session = soup_session_new ();
  create_marker_from_url (layer, session, 48.218611, 17.146397,
      "https://gitlab.gnome.org/GNOME/libchamplain/raw/master/demos/icons/emblem-favorite.png");
  create_marker_from_url (layer, session, 48.21066, 16.31476,
      "https://gitlab.gnome.org/GNOME/libchamplain/raw/master/demos/icons/emblem-generic.png");
  create_marker_from_url (layer, session, 48.14838, 17.10791,
      "https://gitlab.gnome.org/GNOME/libchamplain/raw/master/demos/icons/emblem-important.png");

  /* Finish initialising the map view */
  g_object_set (G_OBJECT (view), "zoom-level", 10,
      "kinetic-mode", TRUE, NULL);
  champlain_view_center_on (CHAMPLAIN_VIEW (view), 48.22, 16.8);

  clutter_actor_show (stage);
  clutter_main ();

  g_object_unref (session);

  return 0;
}
