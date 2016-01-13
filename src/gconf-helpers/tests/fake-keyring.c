/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2011 Red Hat, Inc.
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <glib.h>

/* libgnome-keyring is deprecated. */
#include "utils.h"
NM_PRAGMA_WARNING_DISABLE("-Wdeprecated-declarations")
#include <gnome-keyring.h>

#include "fake-keyring.h"

static GSList *secrets = NULL;

typedef struct {
	guint32 item_id;
	char *keyring;
	GnomeKeyringItemType stype;
	char *name;
	char *secret;
	GnomeKeyringAttributeList *attrs;
	char *user;
	char *server;
	char *protocol;
} Secret;

static Secret *
secret_new (GnomeKeyringItemType type,
            const char *keyring,
            const char *name,
            const char *secret,
            GnomeKeyringAttributeList *attrs,
            const char *user,
            const char *server,
            const char *protocol)
{
	Secret *s;
	static guint32 counter = 1;

	s = g_malloc0 (sizeof (Secret));
	s->item_id = counter++;
	s->keyring = g_strdup (keyring);
	s->name = g_strdup (name);
	s->secret = g_strdup (secret);
	s->stype = type;
	s->attrs = gnome_keyring_attribute_list_copy (attrs);
	s->user = g_strdup (user);
	s->server = g_strdup (server);
	s->protocol = g_strdup (protocol);
	return s;
}

static void
secret_free (Secret *s)
{
	g_free (s->keyring);
	g_free (s->name);
	g_free (s->secret);
	gnome_keyring_attribute_list_free (s->attrs);
	g_free (s->user);
	g_free (s->server);
	g_free (s->protocol);
	memset (s, 0, sizeof (*s));
	g_free (s);
}

static gboolean
match_attribute (GnomeKeyringAttribute *needle, GnomeKeyringAttributeList *haystack)
{
	int i;

	for (i = 0; i < haystack->len; i++) {
		GnomeKeyringAttribute *cmp = &(gnome_keyring_attribute_list_index (haystack, i));

		if (g_strcmp0 (needle->name, cmp->name))
			continue;
		if (needle->type != cmp->type)
			continue;
		if (needle->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
			if (g_strcmp0 (needle->value.string, cmp->value.string))
				continue;
		} else if (needle->type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
			if (needle->value.integer != cmp->value.integer)
				continue;
		} else
			g_assert_not_reached ();

		return TRUE;
	}

	return FALSE;
}

static Secret *
find_one_secret (const char *keyring,
                 GnomeKeyringItemType type,
                 GnomeKeyringAttributeList *attributes)
{
	GSList *iter;

	g_return_val_if_fail (attributes != NULL, NULL);

	for (iter = secrets; iter; iter = g_slist_next (iter)) {
		Secret *candidate = iter->data;
		gboolean same = TRUE;
		int i;

		if (candidate->stype != type)
			continue;
		if (!candidate->attrs || (candidate->attrs->len != attributes->len))
			continue;
		if (g_strcmp0 (keyring, candidate->keyring))
			continue;

		/* match all attributes */
		for (i = 0; i < attributes->len; i++) {
			GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (attributes, i));

			if (!match_attribute (attr, candidate->attrs)) {
				same = FALSE;
				break;
			}
		}

		if (same)
			return candidate;
	}
	return NULL;
}

GnomeKeyringResult
gnome_keyring_item_create_sync (const char *keyring,
                                GnomeKeyringItemType type,
                                const char *display_name,
                                GnomeKeyringAttributeList *attributes,
                                const char *secret,
                                gboolean update_if_exists,
                                guint32 *item_id)
{
	Secret *s;

	s = find_one_secret (keyring, type, attributes);
	if (s) {
		if (!update_if_exists)
			return GNOME_KEYRING_RESULT_OK;

		g_free (s->name);
		s->name = g_strdup (display_name);
		g_free (s->secret);
		s->secret = g_strdup (secret);
	} else {
		s = secret_new (type, keyring, display_name, secret, attributes, NULL, NULL, NULL);
		secrets = g_slist_append (secrets, s);
	}

	if (item_id)
		*item_id = s->item_id;

	return GNOME_KEYRING_RESULT_OK;
}

GnomeKeyringResult
gnome_keyring_item_delete_sync (const char *keyring, guint32 id)
{
	GSList *iter;

	for (iter = secrets; iter; iter = g_slist_next (iter)) {
		Secret *s = iter->data;

		if (s->item_id == id) {
			secrets = g_slist_remove (secrets, s);
			secret_free (s);
			break;
		}
	}

	return GNOME_KEYRING_RESULT_OK;
}

void
fake_keyring_clear (void)
{
	g_slist_foreach (secrets, (GFunc) secret_free, NULL);
	g_slist_free (secrets);
	secrets = NULL;
}

/* Copied from gnome-keyring */
static GnomeKeyringAttributeList *
make_attribute_list_va (va_list args)
{
	GnomeKeyringAttributeList *attributes;
	GnomeKeyringAttribute attribute;
	char *str;
	guint32 val;

	attributes = g_array_new (FALSE, FALSE, sizeof (GnomeKeyringAttribute));

	while ((attribute.name = va_arg (args, char *)) != NULL) {
		attribute.type = va_arg (args, GnomeKeyringAttributeType);

		switch (attribute.type) {
		case GNOME_KEYRING_ATTRIBUTE_TYPE_STRING:
			str = va_arg (args, char *);
			attribute.value.string = str;
			g_array_append_val (attributes, attribute);
			break;
		case GNOME_KEYRING_ATTRIBUTE_TYPE_UINT32:
			val = va_arg (args, guint32);
			attribute.value.integer = val;
			g_array_append_val (attributes, attribute);
			break;
		default:
			g_array_free (attributes, TRUE);
			return NULL;
		}
	}
	return attributes;
}

GnomeKeyringResult
gnome_keyring_find_itemsv_sync  (GnomeKeyringItemType type,
                                 GList **found,
                                 ...)
{
	GnomeKeyringAttributeList *attributes;
	va_list args;
	GSList *iter;

	g_return_val_if_fail (found, GNOME_KEYRING_RESULT_BAD_ARGUMENTS);

	va_start (args, found);
	attributes = make_attribute_list_va (args);
	va_end (args);

	g_return_val_if_fail (attributes != NULL, GNOME_KEYRING_RESULT_BAD_ARGUMENTS);

	for (iter = secrets; iter; iter = g_slist_next (iter)) {
		Secret *candidate = iter->data;
		gboolean same = TRUE;
		int i;

		if (candidate->stype != type)
			continue;
		if (!candidate->attrs)
			continue;

		/* match all attributes */
		for (i = 0; i < attributes->len; i++) {
			GnomeKeyringAttribute *attr = &(gnome_keyring_attribute_list_index (attributes, i));

			if (!match_attribute (attr, candidate->attrs)) {
				same = FALSE;
				break;
			}
		}

		if (same && found) {
			GnomeKeyringFound *f;

			f = g_malloc0 (sizeof (GnomeKeyringFound));
			f->item_id = candidate->item_id;
			f->keyring = g_strdup (candidate->keyring);
			f->attributes = gnome_keyring_attribute_list_copy (candidate->attrs);
			f->secret = g_strdup (candidate->secret);
			*found = g_list_append (*found, f);
		}
	}
	return GNOME_KEYRING_RESULT_OK;
}

GnomeKeyringResult
gnome_keyring_find_network_password_sync (const char *user,
                                          const char *domain,
                                          const char *server,
                                          const char *object,
                                          const char *protocol,
                                          const char *authtype,
                                          guint32 port,
                                          GList **results)
{
	GSList *iter;

	g_return_val_if_fail (results != NULL, GNOME_KEYRING_RESULT_OK);

	for (iter = secrets; iter; iter = g_slist_next (iter)) {
		Secret *candidate = iter->data;
		GnomeKeyringNetworkPasswordData *p;

		if (candidate->stype != GNOME_KEYRING_ITEM_NETWORK_PASSWORD)
			continue;
		if (g_strcmp0 (candidate->user, user))
			continue;
		if (g_strcmp0 (candidate->server, server))
			continue;
		if (g_strcmp0 (candidate->protocol, protocol))
			continue;

		p = g_malloc0 (sizeof (GnomeKeyringNetworkPasswordData));
		p->keyring = g_strdup (candidate->keyring);
		p->item_id = candidate->item_id;
		p->user = g_strdup (candidate->user);
		p->server = g_strdup (candidate->server);
		p->protocol = g_strdup (candidate->protocol);
		p->password = g_strdup (candidate->secret);
		*results = g_list_append (*results, p);
	}

	return GNOME_KEYRING_RESULT_OK;
}

/**********************************************************/
/* This copied from gnome-keyring and is LGPLv2+ */

void
gnome_keyring_network_password_free (GnomeKeyringNetworkPasswordData *data)
{
	if (!data)
		return;

	g_free (data->keyring);
	g_free (data->protocol);
	g_free (data->server);
	g_free (data->object);
	g_free (data->authtype);
	g_free (data->user);
	g_free (data->domain);
	g_free (data->password);

	g_free (data);
}

void
gnome_keyring_network_password_list_free (GList *list)
{
	g_list_foreach (list, (GFunc)gnome_keyring_network_password_free, NULL);
	g_list_free (list);
}

GnomeKeyringAttributeList *
gnome_keyring_attribute_list_copy (GnomeKeyringAttributeList *attributes)
{
	GnomeKeyringAttribute *array;
	GnomeKeyringAttributeList *copy;
	int i;

	if (attributes == NULL)
		return NULL;

	copy = g_array_sized_new (FALSE, FALSE, sizeof (GnomeKeyringAttribute), attributes->len);

	copy->len = attributes->len;
	memcpy (copy->data, attributes->data, sizeof (GnomeKeyringAttribute) * attributes->len);

	array = (GnomeKeyringAttribute *)copy->data;
	for (i = 0; i < copy->len; i++) {
		array[i].name = g_strdup (array[i].name);
		if (array[i].type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
			array[i].value.string = g_strdup (array[i].value.string);
		}
	}
	return copy;
}

void
gnome_keyring_attribute_list_free (GnomeKeyringAttributeList *attributes)
{
	GnomeKeyringAttribute *array;
	int i;

	if (attributes == NULL)
		return;

	array = (GnomeKeyringAttribute *)attributes->data;
	for (i = 0; i < attributes->len; i++) {
		g_free (array[i].name);
		if (array[i].type == GNOME_KEYRING_ATTRIBUTE_TYPE_STRING) {
			g_free (array[i].value.string);
		}
	}

	g_array_free (attributes, TRUE);
}

void
gnome_keyring_attribute_list_append_string (GnomeKeyringAttributeList *attributes,
                                            const char *name, const char *value)
{
	GnomeKeyringAttribute attribute;

	g_return_if_fail (attributes);
	g_return_if_fail (name);

	attribute.name = g_strdup (name);
	attribute.type = GNOME_KEYRING_ATTRIBUTE_TYPE_STRING;
	attribute.value.string = g_strdup (value);

	g_array_append_val (attributes, attribute);
}

void
gnome_keyring_found_free (GnomeKeyringFound *found)
{
	if (found == NULL)
		return;
	g_free (found->keyring);
	g_free (found->secret);
	gnome_keyring_attribute_list_free (found->attributes);
	g_free (found);
}

void
gnome_keyring_found_list_free (GList *found_list)
{
	g_list_foreach (found_list, (GFunc) gnome_keyring_found_free, NULL);
	g_list_free (found_list);
}

/* End copy from gnome-keyring */
/******************************************************/
