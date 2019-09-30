/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012 Canonical Ltd.
 * Copyright (C) 2012 Intel Corporation.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
 * Contact: Jussi Laako <jussi.laako@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * SECTION:ag-service
 * @short_description: A representation of a service.
 * @include: libaccounts-glib/ag-service.h
 *
 * The #AgService structure represents a service. The structure is not directly
 * exposed to applications, but its fields are accessible via getter methods.
 * It is instantiated by #AgManager, with ag_manager_get_service(),
 * ag_manager_list_services() or ag_manager_list_services_by_type().
 * The structure is reference counted. One must use ag_service_unref() when
 * done with it.
 */

#include "config.h"
#include "ag-service.h"

#include "ag-service-type.h"
#include "ag-internals.h"
#include "ag-util.h"
#include <libxml/xmlreader.h>
#include <string.h>

G_DEFINE_BOXED_TYPE (AgService, ag_service,
                     (GBoxedCopyFunc)ag_service_ref,
                     (GBoxedFreeFunc)ag_service_unref);

static gboolean
parse_template (xmlTextReaderPtr reader, AgService *service)
{
    GHashTable *settings;
    gboolean ok;

    g_return_val_if_fail (service->default_settings == NULL, FALSE);

    settings =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free, (GDestroyNotify)g_variant_unref);

    ok = _ag_xml_parse_settings (reader, "", settings);
    if (G_UNLIKELY (!ok))
    {
        g_hash_table_destroy (settings);
        return FALSE;
    }

    service->default_settings = settings;
    return TRUE;
}

static gboolean
parse_preview (G_GNUC_UNUSED xmlTextReaderPtr reader,
               G_GNUC_UNUSED AgService *service)
{
    /* TODO: implement */
    return TRUE;
}

static gboolean
parse_service (xmlTextReaderPtr reader, AgService *service)
{
    const gchar *name;
    int ret, type;

    if (!service->name)
    {
        xmlChar *_name = xmlTextReaderGetAttribute (reader,
                                                    (xmlChar *) "id");
        service->name = g_strdup ((const gchar *)_name);
        if (_name) xmlFree(_name);
    }

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, "service") == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "type") == 0 && !service->type)
            {
                ok = _ag_xml_dup_element_data (reader, &service->type);
            }
            else if (strcmp (name, "name") == 0 && !service->display_name)
            {
                ok = _ag_xml_dup_element_data (reader, &service->display_name);
            }
            else if (strcmp (name, "description") == 0)
            {
                ok = _ag_xml_dup_element_data (reader, &service->description);
            }
            else if (strcmp (name, "provider") == 0 && !service->provider)
            {
                ok = _ag_xml_dup_element_data (reader, &service->provider);
            }
            else if (strcmp (name, "icon") == 0)
            {
                ok = _ag_xml_dup_element_data (reader, &service->icon_name);
            }
            else if (strcmp (name, "translations") == 0)
            {
                ok = _ag_xml_dup_element_data (reader, &service->i18n_domain);
            }

            else if (strcmp (name, "template") == 0)
            {
                ok = parse_template (reader, service);
            }
            else if (strcmp (name, "preview") == 0)
            {
                ok = parse_preview (reader, service);
            }
            else if (strcmp (name, "type_data") == 0)
            {
                static const gchar element[] = "<type_data";
                gsize offset;

                /* find the offset in the file where this element begins */
                offset = xmlTextReaderByteConsumed(reader);
                while (offset > 0)
                {
                    if (strncmp (service->file_data + offset, element,
                                 sizeof (element)) == 0)
                    {
                        service->type_data_offset = offset;
                        break;
                    }
                    offset--;
                }

                /* this element is placed after all the elements we are
                 * interested in: we can stop the parsing now */
                return TRUE;
            }
            else if (strcmp (name, "tags") == 0)
            {
                ok = _ag_xml_parse_element_list (reader, "tag",
                                                 &service->tags);
            }
            else
                ok = TRUE;

            if (G_UNLIKELY (!ok)) return FALSE;
        }

        ret = xmlTextReaderNext (reader);
    }
    return TRUE;
}

static gboolean
read_service_file (xmlTextReaderPtr reader, AgService *service)
{
    const xmlChar *name;
    int ret;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = xmlTextReaderConstName (reader);
        if (G_LIKELY (name &&
                      strcmp ((const gchar *)name, "service") == 0))
        {
            return parse_service (reader, service);
        }

        ret = xmlTextReaderNext (reader);
    }
    return FALSE;
}

static void
copy_tags_from_type (AgService *service)
{
    AgServiceType *type;
    GList *type_tags, *tag_list;

    service->tags = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           g_free, NULL);
    type = _ag_service_type_new_from_file (service->type);
    if (G_UNLIKELY (type == NULL)) return;

    type_tags = ag_service_type_get_tags (type);
    for (tag_list = type_tags; tag_list != NULL; tag_list = tag_list->next)
        g_hash_table_insert (service->tags,
                             g_strdup (tag_list->data), NULL);
    g_list_free (type_tags);
    ag_service_type_unref (type);
}

AgService *
_ag_service_new (void)
{
    AgService *service;

    service = g_slice_new0 (AgService);
    service->ref_count = 1;

    return service;
}

static gboolean
_ag_service_load_from_file (AgService *service)
{
    xmlTextReaderPtr reader;
    gchar *filepath;
    gboolean ret;
    GError *error = NULL;
    gsize len;

    g_return_val_if_fail (service->name != NULL, FALSE);

    DEBUG_REFS ("Loading service %s", service->name);
    filepath = _ag_find_libaccounts_file (service->name,
                                          ".service",
                                          "AG_SERVICES",
                                          SERVICE_FILES_DIR);
    if (G_UNLIKELY (!filepath)) return FALSE;

    g_file_get_contents (filepath, &service->file_data,
                         &len, &error);
    if (G_UNLIKELY (error))
    {
        g_warning ("Error reading %s: %s", filepath, error->message);
        g_error_free (error);
        g_free (filepath);
        return FALSE;
    }

    /* TODO: cache the xmlReader */
    reader = xmlReaderForMemory (service->file_data, len,
                                 filepath, NULL, 0);
    g_free (filepath);
    if (G_UNLIKELY (reader == NULL))
        return FALSE;

    ret = read_service_file (reader, service);

    xmlFreeTextReader (reader);
    return ret;
}

AgService *
_ag_service_new_from_file (const gchar *service_name)
{
    AgService *service;

    service = _ag_service_new ();
    service->name = g_strdup (service_name);
    if (!_ag_service_load_from_file (service))
    {
        ag_service_unref (service);
        service = NULL;
    }

    return service;
}

AgService *
_ag_service_new_from_memory (const gchar *service_name, const gchar *service_type,
                             const gint service_id)
{
    AgService *service;

    service = _ag_service_new ();
    service->name = g_strdup (service_name);
    service->type = g_strdup (service_type);
    service->id = service_id;

    return service;
}

GHashTable *
_ag_service_load_default_settings (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);

    if (!service->default_settings)
    {
        /* This can happen if the service was created by the AccountManager by
         * loading the record from the DB.
         * Now we must reload the service from its XML file.
         */
        if (!_ag_service_load_from_file (service))
        {
            g_warning ("Loading service %s file failed", service->name);
            return NULL;
        }
    }

    return service->default_settings;
}

GVariant *
_ag_service_get_default_setting (AgService *service, const gchar *key)
{
    GHashTable *settings;

    g_return_val_if_fail (key != NULL, NULL);

    settings = _ag_service_load_default_settings (service);
    if (G_UNLIKELY (!settings))
        return NULL;

    return g_hash_table_lookup (settings, key);
}

/**
 * ag_service_get_name:
 * @service: the #AgService.
 *
 * Gets the name of the #AgService.
 *
 * Returns: the name of @service.
 */
const gchar *
ag_service_get_name (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    return service->name;
}

/**
 * ag_service_get_display_name:
 * @service: the #AgService.
 *
 * Gets the display name of the #AgService.
 *
 * Returns: the display name of @service.
 */
const gchar *
ag_service_get_display_name (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    if (service->display_name == NULL && !service->file_data)
        _ag_service_load_from_file (service);
    return service->display_name;
}

/**
 * ag_service_get_description:
 * @service: the #AgService.
 *
 * Gets the description of the #AgService.
 *
 * Returns: the description of @service, or %NULL upon failure.
 *
 * Since: 1.2
 */
const gchar *
ag_service_get_description (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    if (service->description == NULL && !service->file_data)
        _ag_service_load_from_file (service);
    return service->description;
}

/**
 * ag_service_get_service_type:
 * @service: the #AgService.
 *
 * Gets the service type of the #AgService.
 *
 * Returns: the type of @service.
 */
const gchar *
ag_service_get_service_type (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    if (service->type == NULL && !service->file_data)
        _ag_service_load_from_file (service);
    return service->type;
}

/**
 * ag_service_get_provider:
 * @service: the #AgService.
 *
 * Gets the provider name of the #AgService.
 *
 * Returns: the name of the provider of @service.
 */
const gchar *
ag_service_get_provider (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    if (service->provider == NULL && !service->file_data)
        _ag_service_load_from_file (service);
    return service->provider;
}

/**
 * ag_service_get_icon_name:
 * @service: the #AgService.
 *
 * Gets the icon name of the #AgService.
 *
 * Returns: the name of the icon of @service.
 */
const gchar *
ag_service_get_icon_name (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);

    if (!service->file_data)
        _ag_service_load_from_file (service);

    return service->icon_name;
}

/**
 * ag_service_get_i18n_domain:
 * @service: the #AgService.
 *
 * Gets the translation domain of the #AgService.
 *
 * Returns: the name of the translation catalog.
 */
const gchar *
ag_service_get_i18n_domain (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);

    if (!service->file_data)
        _ag_service_load_from_file (service);

    return service->i18n_domain;
}

/**
 * ag_service_has_tag:
 * @service: the #AgService.
 * @tag: tag to check for.
 *
 * Checks if the #AgService has the requested tag.
 *
 * Returns: TRUE if #AgService has the tag, FALSE otherwise
 */
gboolean ag_service_has_tag (AgService *service, const gchar *tag)
{
    g_return_val_if_fail (service != NULL, FALSE);

    if (!service->file_data)
        _ag_service_load_from_file (service);

    if (service->tags == NULL)
        copy_tags_from_type (service);

    return g_hash_table_lookup_extended (service->tags, tag, NULL, NULL);
}

/**
 * ag_service_get_tags:
 * @service: the #AgService.
 *
 * Get list of tags specified for the #AgService. If the service has not
 * defined tags, tags from the service type will be returned.
 *
 * Returns: (transfer container) (element-type utf8): #GList of tags for
 * @service. The list must be freed with g_list_free(). Entries are owned by
 * the #AgService type and must not be free'd.
 */
GList *ag_service_get_tags (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);

    if (!service->file_data)
        _ag_service_load_from_file (service);

    if (service->tags == NULL)
        copy_tags_from_type (service);

    return g_hash_table_get_keys (service->tags);
}

/**
 * ag_service_get_file_contents:
 * @service: the #AgService.
 * @contents: location to receive the pointer to the file contents.
 * @data_offset: pointer to receive the offset of the type data.
 *
 * Gets the contents of the XML service file.  The buffer returned in @contents
 * should not be modified or freed, and is guaranteed to be valid as long as
 * @service is referenced. If @data_offset is not %NULL, it is set to the
 * offset where the &lt;type_data&gt; element can be found.
 * If some error occurs, @contents is set to %NULL.
 */
void
ag_service_get_file_contents (AgService *service,
                              const gchar **contents,
                              gsize *data_offset)
{
    g_return_if_fail (service != NULL);
    g_return_if_fail (contents != NULL);

    if (service->file_data == NULL)
    {
        /* This can happen if the service was created by the AccountManager by
         * loading the record from the DB.
         * Now we must reload the service from its XML file.
         */
        if (!_ag_service_load_from_file (service))
            g_warning ("Loading service %s file failed", service->name);
    }

    *contents = service->file_data;

    if (data_offset)
        *data_offset = service->type_data_offset;
}

/**
 * ag_service_ref:
 * @service: the #AgService.
 *
 * Adds a reference to @service.
 *
 * Returns: @service.
 */
AgService *
ag_service_ref (AgService *service)
{
    g_return_val_if_fail (service != NULL, NULL);
    g_return_val_if_fail (service->ref_count > 0, NULL);

    DEBUG_REFS ("Referencing service %s (%d)",
                service->name, service->ref_count);
    service->ref_count++;
    return service;
}

/**
 * ag_service_unref:
 * @service: the #AgService.
 *
 * Used to unreference the #AgService structure.
 */
void
ag_service_unref (AgService *service)
{
    g_return_if_fail (service != NULL);
    g_return_if_fail (service->ref_count > 0);

    DEBUG_REFS ("Unreferencing service %s (%d)",
                service->name, service->ref_count);
    service->ref_count--;
    if (service->ref_count == 0)
    {
        g_free (service->name);
        g_free (service->display_name);
        g_free (service->description);
        g_free (service->icon_name);
        g_free (service->i18n_domain);
        g_free (service->type);
        g_free (service->provider);
        g_free (service->file_data);
        if (service->default_settings)
            g_hash_table_unref (service->default_settings);
        if (service->tags)
            g_hash_table_destroy (service->tags);
        g_slice_free (AgService, service);
    }
}

/**
 * ag_service_list_free:
 * @list: (element-type AgService): a #GList of services returned by some
 * function of this library.
 *
 * Frees the list @list.
 */
void
ag_service_list_free (GList *list)
{
    g_list_foreach (list, (GFunc)ag_service_unref, NULL);
    g_list_free (list);
}

