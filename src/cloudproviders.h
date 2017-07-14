/* cloudproviders.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 * Copyright (C) 2017 Julius Haertl <jus@bitgrid.net>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CLOUD_PROVIDERS_H
#define CLOUD_PROVIDERS_H

#include <gio/gio.h>
#include "cloudproviderproxy.h"

G_BEGIN_DECLS

#define TYPE_CLOUD_PROVIDERS             (cloud_providers_get_type())
#define CLOUD_PROVIDERS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CLOUD_PROVIDERS, CloudProviders))
#define CLOUD_PROVIDERS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CLOUD_PROVIDERS, CloudProvidersClass))
#define IS_CLOUD_PROVIDERS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CLOUD_PROVIDERS))
#define IS_CLOUD_PROVIDERS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CLOUD_PROVIDERS))
#define CLOUD_PROVIDERS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CLOUD_PROVIDERS, CloudProvidersClass))

typedef struct _CloudProviders CloudProviders;
typedef struct _CloudProvidersClass CloudProvidersClass;

struct _CloudProvidersClass
{
  GObjectClass parent_class;
};

struct _CloudProviders
{
  GObject parent_instance;
};

GType cloud_providers_get_type          (void) G_GNUC_CONST;
CloudProviders *cloud_providers_dup_singleton (void);
void cloud_providers_update (CloudProviders *self);
GList *cloud_providers_get_providers (CloudProviders *self);
G_END_DECLS

#endif /* CLOUD_PROVIDERS_H */
