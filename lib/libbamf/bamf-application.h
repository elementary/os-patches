/*
 * Copyright 2010-2011 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of either or both of the following licenses:
 *
 * 1) the GNU Lesser General Public License version 3, as published by the
 * Free Software Foundation; and/or
 * 2) the GNU Lesser General Public License version 2.1, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the applicable version of the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of both the GNU Lesser General Public
 * License version 3 and version 2.1 along with this program.  If not, see
 * <http://www.gnu.org/licenses/>
 *
 * Authored by: Jason Smith <jason.smith@canonical.com>
 *              Neil Jagdish Patel <neil.patel@canonical.com>
 *              Marco Trevisan (Treviño) <3v1n0@ubuntu.com>
 *
 */

#ifndef _BAMF_APPLICATION_H_
#define _BAMF_APPLICATION_H_

#include <glib-object.h>
#include <libbamf/bamf-view.h>

G_BEGIN_DECLS

#define BAMF_TYPE_APPLICATION (bamf_application_get_type ())

#define BAMF_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
        BAMF_TYPE_APPLICATION, BamfApplication))

#define BAMF_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),\
        BAMF_TYPE_APPLICATION, BamfApplicationClass))

#define BAMF_IS_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
        BAMF_TYPE_APPLICATION))

#define BAMF_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
        BAMF_TYPE_APPLICATION))

#define BAMF_APPLICATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
        BAMF_TYPE_APPLICATION, BamfApplicationClass))

#define BAMF_APPLICATION_SIGNAL_DESKTOP_FILE_UPDATED "desktop-file-updated"
#define BAMF_APPLICATION_SIGNAL_WINDOW_ADDED         "window-added"
#define BAMF_APPLICATION_SIGNAL_WINDOW_REMOVED       "window-removed"

typedef struct _BamfApplication        BamfApplication;
typedef struct _BamfApplicationClass   BamfApplicationClass;
typedef struct _BamfApplicationPrivate BamfApplicationPrivate;

struct _BamfApplication
{
  BamfView parent;

  BamfApplicationPrivate *priv;
};

struct _BamfApplicationClass
{
  BamfViewClass parent_class;

  /*< private >*/
  void (*_application_padding1) (void);
  void (*_application_padding2) (void);
  void (*_application_padding3) (void);
  void (*_application_padding4) (void);
  void (*_application_padding5) (void);
  void (*_application_padding6) (void);
};

GType             bamf_application_get_type             (void) G_GNUC_CONST;

const gchar     * bamf_application_get_application_type (BamfApplication *application);

const gchar     * bamf_application_get_desktop_file     (BamfApplication *application);

gchar          ** bamf_application_get_supported_mime_types (BamfApplication *application);

GList           * bamf_application_get_windows          (BamfApplication *application);

GArray          * bamf_application_get_xids             (BamfApplication *application);

BamfView        * bamf_application_get_focusable_child  (BamfApplication *application);

gboolean          bamf_application_get_show_menu_stubs  (BamfApplication *application);


/* Deprecated symbols */
G_GNUC_DEPRECATED
gboolean bamf_application_get_application_menu (BamfApplication *application,
                                                gchar **name,
                                                gchar **object_path);

G_END_DECLS

#endif
