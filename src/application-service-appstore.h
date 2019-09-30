/*
An object that stores the registration of all the application
indicators.  It also communicates this to the indicator visualization.

Copyright 2009 Canonical Ltd.

Authors:
    Ted Gould <ted@canonical.com>

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

#ifndef __APPLICATION_SERVICE_APPSTORE_H__
#define __APPLICATION_SERVICE_APPSTORE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define APPLICATION_SERVICE_APPSTORE_TYPE            (application_service_appstore_get_type ())
#define APPLICATION_SERVICE_APPSTORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), APPLICATION_SERVICE_APPSTORE_TYPE, ApplicationServiceAppstore))
#define APPLICATION_SERVICE_APPSTORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), APPLICATION_SERVICE_APPSTORE_TYPE, ApplicationServiceAppstoreClass))
#define IS_APPLICATION_SERVICE_APPSTORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), APPLICATION_SERVICE_APPSTORE_TYPE))
#define IS_APPLICATION_SERVICE_APPSTORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), APPLICATION_SERVICE_APPSTORE_TYPE))
#define APPLICATION_SERVICE_APPSTORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), APPLICATION_SERVICE_APPSTORE_TYPE, ApplicationServiceAppstoreClass))

typedef struct _ApplicationServiceAppstore      ApplicationServiceAppstore;
typedef struct _ApplicationServiceAppstoreClass ApplicationServiceAppstoreClass;
typedef struct _ApplicationServiceAppstorePrivate ApplicationServiceAppstorePrivate;

struct _ApplicationServiceAppstoreClass {
	GObjectClass parent_class;

	void (*application_added) (ApplicationServiceAppstore * appstore, gchar *, gint, gchar *, gchar *, gpointer);
	void (*application_removed) (ApplicationServiceAppstore * appstore, gint, gpointer);
	void (*application_icon_changed)(ApplicationServiceAppstore * appstore, gint, const gchar *, gpointer);
	void (*application_icon_theme_path_changed)(ApplicationServiceAppstore * appstore, gint, const gchar *, gpointer);
	void (*application_label_changed)(ApplicationServiceAppstore * appstore, gint, const gchar *, const gchar *, gpointer);
};

struct _ApplicationServiceAppstore {
	GObject parent;
	
	ApplicationServiceAppstorePrivate * priv;
};

ApplicationServiceAppstore * application_service_appstore_new (void);
GType application_service_appstore_get_type               (void);
void  application_service_appstore_application_add        (ApplicationServiceAppstore *   appstore,
                                                           const gchar *             dbus_name,
                                                           const gchar *             dbus_object);
void  application_service_appstore_application_remove     (ApplicationServiceAppstore *   appstore,
                                                           const gchar *             dbus_name,
                                                           const gchar *             dbus_object);
gchar** application_service_appstore_application_get_list (ApplicationServiceAppstore *   appstore);

G_END_DECLS

#endif
