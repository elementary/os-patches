/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2012 Canonical Ltd.
 *
 * Contact: Alberto Mardegan <alberto.mardegan@canonical.com>
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
 * SECTION:ag-application
 * @short_description: information on the client applications of libaccounts.
 * @include: libaccounts-glib/ag-application.h
 *
 * The #AgApplication structure holds information on the client applications
 * registered with libaccounts.
 * It is instantiated by #AgManager with ag_manager_get_application() and
 * ag_manager_list_applications_by_service(), and destroyed with
 * ag_application_unref().
 *
 * <example>
 * <title>Querying application names for an
 * <structname>AgService</structname></title>
 * <programlisting>
 * AgManager *manager;
 * GList *services, *applications;
 * AgService *service;
 *
 * manager = ag_manager_new ();
 * services = ag_manager_list_services (manager);
 * g_assert (services != NULL);
 * service = (AgService *) services->data;
 * applications = ag_manager_list_applications_by_service (manager, service);
 *
 * g_print ("Service type: %s\n", ag_service_get_name (service));
 * for (applications; applications != NULL; applications = applications->next)
 * {
 *     const gchar *application_name = ag_application_get_name ((AgApplication *) applications->data);
 *     g_print ("  Application name: %s\n", application_name);
 * }
 * </programlisting>
 * </example>
 */

#include "config.h"
#include "ag-application.h"
#include "ag-internals.h"
#include "ag-service.h"
#include "ag-util.h"

#include <libxml/xmlreader.h>
#include <string.h>

struct _AgApplication {
    /*< private >*/
    gint ref_count;

    gchar *name;
    gchar *desktop_entry;
    gchar *description;
    gchar *i18n_domain;

    GDesktopAppInfo *desktop_app_info;
    gboolean desktop_app_info_loaded;

    /* the values of these hash tables are AgApplicationItem elements */
    GHashTable *services;
    GHashTable *service_types;
};

typedef struct {
    gchar *description;
    /* more fields could be added later on (for instance, supported features) */
} AgApplicationItem;

G_DEFINE_BOXED_TYPE (AgApplication, ag_application,
                     (GBoxedCopyFunc)ag_application_ref,
                     (GBoxedFreeFunc)ag_application_unref);

static void
_ag_application_item_free (AgApplicationItem *item)
{
    g_free (item->description);
    g_slice_free (AgApplicationItem, item);
}

static AgApplicationItem *
_ag_application_get_service_item (AgApplication *self, AgService *service)
{
    AgApplicationItem *item = NULL;

    if (self->services != NULL)
        item = g_hash_table_lookup (self->services, service->name);
    if (item == NULL && self->service_types != NULL)
    {
        item = g_hash_table_lookup (self->service_types,
                                    ag_service_get_service_type (service));
    }

    return item;
}

static inline void
_ag_application_ensure_desktop_app_info (AgApplication *self)
{
    if (!self->desktop_app_info_loaded)
    {
        const char *filename = self->desktop_entry != NULL ?
            self->desktop_entry : self->name;
        gchar *filename_tmp = NULL;
        if (!g_str_has_suffix (filename, ".desktop"))
        {
            filename_tmp = g_strconcat (filename, ".desktop", NULL);
            filename = filename_tmp;
        }

        self->desktop_app_info = g_desktop_app_info_new (filename);
        self->desktop_app_info_loaded = TRUE;
        g_free (filename_tmp);
    }
}

static gboolean
parse_item (xmlTextReaderPtr reader, GHashTable *hash_table,
            const gchar *item_tag)
{
    AgApplicationItem *item;
    xmlChar *xml_item_id;
    gchar *item_id;
    int ret, type;

    xml_item_id = xmlTextReaderGetAttribute (reader,
                                             (xmlChar *)"id");
    if (G_UNLIKELY (xml_item_id == NULL))
    {
        g_warning ("Found element %s with no \"id\" attribute",
                   item_tag);
        return FALSE;
    }
    item_id = g_strdup ((const gchar*)xml_item_id);
    xmlFree (xml_item_id);

    item = g_slice_new0 (AgApplicationItem);
    g_hash_table_insert (hash_table, item_id, item);

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        const gchar *name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, item_tag) == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "description") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &item->description);
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
parse_items (xmlTextReaderPtr reader,
             GHashTable **hash_table,
             const gchar *item_tag)
{
    const gchar *name;

    if (*hash_table == NULL)
    {
        *hash_table =
            g_hash_table_new_full (g_str_hash, g_str_equal,
                                   g_free,
                                   (GDestroyNotify)_ag_application_item_free);
    }

    int ret, type;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, item_tag) == 0)
            {
                ok = parse_item (reader, *hash_table, item_tag);
            }
            else
            {
                /* ignore unrecognized elements */
                ok = TRUE;
            }

            if (G_UNLIKELY (!ok)) return FALSE;
        }

        ret = xmlTextReaderNext (reader);
    }
    return TRUE;
}

static gboolean
parse_application (xmlTextReaderPtr reader, AgApplication *application)
{
    const gchar *name;
    int ret, type;

    if (!application->name)
    {
        xmlChar *_name = xmlTextReaderGetAttribute (reader,
                                                    (xmlChar *) "id");
        application->name = g_strdup ((const gchar *)_name);
        if (_name) xmlFree(_name);
    }

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, "application") == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "desktop-entry") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &application->desktop_entry);
            }
            else if (strcmp (name, "description") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &application->description);
            }
            else if (strcmp (name, "translations") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &application->i18n_domain);
            }
            else if (strcmp (name, "services") == 0)
            {
                ok = parse_items (reader, &application->services,
                                  "service");
            }
            else if (strcmp (name, "service-types") == 0)
            {
                ok = parse_items (reader, &application->service_types,
                                  "service-type");
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
read_application_file (xmlTextReaderPtr reader, AgApplication *application)
{
    const xmlChar *name;
    int ret;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = xmlTextReaderConstName (reader);
        if (G_LIKELY (name &&
                      strcmp ((const gchar *)name, "application") == 0))
        {
            return parse_application (reader, application);
        }

        ret = xmlTextReaderNext (reader);
    }
    return FALSE;
}

static gboolean
_ag_application_load_from_file (AgApplication *application)
{
    xmlTextReaderPtr reader;
    gchar *filepath;
    gboolean ret = FALSE;
    GError *error = NULL;
    gchar *file_data;
    gsize file_data_len;

    g_return_val_if_fail (application->name != NULL, FALSE);

    DEBUG_REFS ("Loading application %s", application->name);
    filepath = _ag_find_libaccounts_file (application->name,
                                          ".application",
                                          "AG_APPLICATIONS",
                                          APPLICATION_FILES_DIR);
    if (G_UNLIKELY (!filepath)) return FALSE;

    g_file_get_contents (filepath, &file_data, &file_data_len, &error);
    if (G_UNLIKELY (error))
    {
        g_warning ("Error reading %s: %s", filepath, error->message);
        g_error_free (error);
        g_free (filepath);
        return FALSE;
    }

    reader = xmlReaderForMemory (file_data, file_data_len, filepath, NULL, 0);
    g_free (filepath);
    if (G_UNLIKELY (reader == NULL))
        goto err_reader;

    ret = read_application_file (reader, application);

    xmlFreeTextReader (reader);
err_reader:
    g_free (file_data);
    return ret;
}

AgApplication *
_ag_application_new_from_file (const gchar *application_name)
{
    AgApplication *application;

    application = g_slice_new0 (AgApplication);
    application->ref_count = 1;
    application->name = g_strdup (application_name);

    if (!_ag_application_load_from_file (application))
    {
        ag_application_unref (application);
        application = NULL;
    }

    return application;
}

gboolean
_ag_application_supports_service (AgApplication *self, AgService *service)
{
    return _ag_application_get_service_item (self, service) != NULL;
}

/**
 * ag_application_get_name:
 * @self: the #AgApplication.
 *
 * Get the name of the #AgApplication.
 *
 * Returns: the name of @self.
 */
const gchar *
ag_application_get_name (AgApplication *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->name;
}

/**
 * ag_application_get_description:
 * @self: the #AgApplication.
 *
 * Get the description of the #AgApplication.
 *
 * Returns: the description of @self.
 */
const gchar *
ag_application_get_description (AgApplication *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    if (self->description == NULL)
    {
        _ag_application_ensure_desktop_app_info (self);
        if (self->desktop_app_info != NULL)
        {
            return g_app_info_get_description (G_APP_INFO
                                               (self->desktop_app_info));
        }
    }

    return self->description;
}

/**
 * ag_application_get_i18n_domain:
 * @self: the #AgApplication.
 *
 * Get the translation domain of the #AgApplication.
 *
 * Returns: the translation domain.
 */
const gchar *
ag_application_get_i18n_domain (AgApplication *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    return self->i18n_domain;
}

/**
 * ag_application_get_desktop_app_info:
 * @self: the #AgApplication.
 *
 * Get the #GDesktopAppInfo of the application.
 *
 * Returns: (transfer full): the #GDesktopAppInfo for @self, or %NULL if
 * failed.
 */
GDesktopAppInfo *
ag_application_get_desktop_app_info (AgApplication *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    _ag_application_ensure_desktop_app_info (self);
    return self->desktop_app_info != NULL ?
        g_object_ref (self->desktop_app_info) : NULL;
}

/**
 * ag_application_get_service_usage:
 * @self: the #AgApplication.
 * @service: an #AgService.
 *
 * Get the description from the application XML file, for the specified
 * service; if not found, get the service-type description instead.
 *
 * Returns: usage description of the service.
 */
const gchar *
ag_application_get_service_usage(AgApplication *self, AgService *service)
{
    AgApplicationItem *item;

    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (service != NULL, NULL);

    item = _ag_application_get_service_item (self, service);

    return (item != NULL) ? item->description : NULL;
}

/**
 * ag_application_ref:
 * @self: the #AgApplication.
 *
 * Increment the reference count of @self.
 *
 * Returns: @self.
 */
AgApplication *
ag_application_ref (AgApplication *self)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_atomic_int_inc (&self->ref_count);
    return self;
}

/**
 * ag_application_unref:
 * @self: the #AgApplication.
 *
 * Decrements the reference count of @self. The item is destroyed when the
 * count gets to 0.
 */
void
ag_application_unref (AgApplication *self)
{
    g_return_if_fail (self != NULL);
    if (g_atomic_int_dec_and_test (&self->ref_count))
    {
        g_free (self->name);
        g_free (self->desktop_entry);
        g_free (self->description);
        g_free (self->i18n_domain);

        if (self->desktop_app_info != NULL)
            g_object_unref (self->desktop_app_info);
        if (self->services != NULL)
            g_hash_table_unref (self->services);
        if (self->service_types != NULL)
            g_hash_table_unref (self->service_types);

        g_slice_free (AgApplication, self);
    }
}

