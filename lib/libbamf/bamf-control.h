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

#ifndef _BAMF_CONTROL_H_
#define _BAMF_CONTROL_H_

#include <glib-object.h>
#include "bamf-application.h"

G_BEGIN_DECLS

#define BAMF_TYPE_CONTROL (bamf_control_get_type ())

#define BAMF_CONTROL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
        BAMF_TYPE_CONTROL, BamfControl))

#define BAMF_CONTROL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),\
        BAMF_TYPE_CONTROL, BamfControlClass))

#define BAMF_IS_CONTROL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
        BAMF_TYPE_CONTROL))

#define BAMF_IS_CONTROL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
        BAMF_TYPE_CONTROL))

#define BAMF_CONTROL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
        BAMF_TYPE_CONTROL, BamfControlClass))

typedef struct _BamfControl        BamfControl;
typedef struct _BamfControlClass   BamfControlClass;
typedef struct _BamfControlPrivate BamfControlPrivate;

struct _BamfControl
{
  GObject parent;

  BamfControlPrivate *priv;
};

struct _BamfControlClass
{
  GObjectClass parent_class;

  /*< private >*/
  void (*_control_padding1) (void);
  void (*_control_padding2) (void);
  void (*_control_padding3) (void);
  void (*_control_padding4) (void);
  void (*_control_padding5) (void);
  void (*_control_padding6) (void);
};

GType bamf_control_get_type (void) G_GNUC_CONST;

BamfControl * bamf_control_get_default (void);

void          bamf_control_insert_desktop_file          (BamfControl *control,
                                                         const gchar *desktop_file);

void          bamf_control_create_local_desktop_file    (BamfControl *control,
                                                         BamfApplication *application);

void          bamf_control_register_application_for_pid (BamfControl *control,
                                                         const gchar *desktop_file,
                                                         gint32       pid);

void          bamf_control_set_approver_behavior        (BamfControl *control,
                                                         gint32       behavior);

G_END_DECLS
#endif
