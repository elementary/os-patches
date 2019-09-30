/*
 * Copyright 2010 Canonical Ltd.
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

#ifndef _BAMF_FACTORY_H_
#define _BAMF_FACTORY_H_

#include <glib-object.h>
#include <libbamf/bamf-view.h>
#include <libbamf/bamf-application.h>

G_BEGIN_DECLS

#define BAMF_TYPE_FACTORY (bamf_factory_get_type ())

#define BAMF_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
        BAMF_TYPE_FACTORY, BamfFactory))

#define BAMF_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),\
        BAMF_TYPE_FACTORY, BamfFactoryClass))

#define BAMF_IS_FACTORY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj),\
        BAMF_TYPE_FACTORY))

#define BAMF_IS_FACTORY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),\
        BAMF_TYPE_FACTORY))

#define BAMF_FACTORY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),\
        BAMF_TYPE_FACTORY, BamfFactoryClass))

typedef struct _BamfFactory        BamfFactory;
typedef struct _BamfFactoryClass   BamfFactoryClass;
typedef struct _BamfFactoryPrivate BamfFactoryPrivate;

typedef enum
{
  BAMF_FACTORY_VIEW,
  BAMF_FACTORY_WINDOW,
  BAMF_FACTORY_APPLICATION,
  BAMF_FACTORY_TAB,
  BAMF_FACTORY_NONE
} BamfFactoryViewType;

struct _BamfFactory
{
  GObject parent;

  BamfFactoryPrivate *priv;
};

struct _BamfFactoryClass
{
  GObjectClass parent_class;
};

GType             bamf_factory_get_type              (void) G_GNUC_CONST;

BamfView        * _bamf_factory_view_for_path        (BamfFactory * factory,
                                                      const char * path);

BamfView        * _bamf_factory_view_for_path_type   (BamfFactory * factory,
                                                      const char * path,
                                                      BamfFactoryViewType type);

BamfView        * _bamf_factory_view_for_path_type_str (BamfFactory * factory,
                                                        const char * path,
                                                        const char * type);

BamfApplication * _bamf_factory_app_for_file         (BamfFactory * factory,
                                                     const char * path,
                                                     gboolean create);

BamfFactory     * _bamf_factory_get_default          (void);

G_END_DECLS

#endif
