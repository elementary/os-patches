/* cloudprovidersaccountpriv.h
 *
 * Copyright (C) 2017 Carlos Soriano <csoriano@gnome.org>
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

#ifndef CLOUD_PROVIDERS_ACCOUNT_PRIV_H
#define CLOUD_PROVIDERS_ACCOUNT_PRIV_H

#include <gio/gio.h>
#include "cloudprovidersaccount.h"
#include "cloudproviders-generated.h"
G_BEGIN_DECLS

CloudProvidersAccount*
cloud_providers_account_new (CloudProvidersDbusAccountProxy *proxy);

G_END_DECLS

#endif
