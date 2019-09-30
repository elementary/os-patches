/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of libaccounts-glib
 *
 * Copyright (C) 2009-2010 Nokia Corporation.
 * Copyright (C) 2012-2013 Canonical Ltd.
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
 * SECTION:ag-provider
 * @short_description: A representation of a provider.
 * @include: libaccounts-glib/ag-provider.h
 *
 * The #AgProvider structure represents an account provider. The structure is
 * not directly exposed to applications, but its fields are accessible via
 * getter methods. It can be instantiated by #AgManager with
 * ag_manager_get_provider() or ag_manager_list_providers().
 * The structure is reference counted. One must use ag_provider_unref() when
 * done with it.
 *
 * See the <link linkend="example-create-new-AgAccount">example of creating a
 * new <structname>AgAccount</structname></link> to see how #AgProvider can be
 * used.
 */

#include "config.h"

#include "ag-provider.h"

#include "ag-internals.h"
#include "ag-util.h"
#include <libxml/xmlreader.h>
#include <string.h>

G_DEFINE_BOXED_TYPE (AgProvider, ag_provider,
                     (GBoxedCopyFunc)ag_provider_ref,
                     (GBoxedFreeFunc)ag_provider_unref);

static gboolean
parse_template (xmlTextReaderPtr reader, AgProvider *provider)
{
    GHashTable *settings;
    gboolean ok;

    g_return_val_if_fail (provider->default_settings == NULL, FALSE);

    settings =
        g_hash_table_new_full (g_str_hash, g_str_equal,
                               g_free, (GDestroyNotify)g_variant_unref);

    ok = _ag_xml_parse_settings (reader, "", settings);
    if (G_UNLIKELY (!ok))
    {
        g_hash_table_destroy (settings);
        return FALSE;
    }

    provider->default_settings = settings;
    return TRUE;
}

static gboolean
parse_provider (xmlTextReaderPtr reader, AgProvider *provider)
{
    const gchar *name;
    int ret, type;

    if (!provider->name)
    {
        xmlChar *_name = xmlTextReaderGetAttribute (reader,
                                                    (xmlChar *) "id");
        provider->name = g_strdup((const gchar *)_name);
        if (_name) xmlFree(_name);
    }

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        name = (const gchar *)xmlTextReaderConstName (reader);
        if (G_UNLIKELY (!name)) return FALSE;

        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_END_ELEMENT &&
            strcmp (name, "provider") == 0)
            break;

        if (type == XML_READER_TYPE_ELEMENT)
        {
            gboolean ok;

            if (strcmp (name, "name") == 0 && !provider->display_name)
            {
                ok = _ag_xml_dup_element_data (reader, &provider->display_name);
                /* that's the only thing we are interested of: we can stop the
                 * parsing now. */
            }
            else if (strcmp (name, "description") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &provider->description);
            }
            else if (strcmp (name, "translations") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &provider->i18n_domain);
            }
            else if (strcmp (name, "icon") == 0)
            {
                ok = _ag_xml_dup_element_data (reader,
                                               &provider->icon_name);
            }
            else if (strcmp (name, "domains") == 0)
            {
                ok = _ag_xml_dup_element_data (reader, &provider->domains);
            }
            else if (strcmp (name, "plugin") == 0)
            {
                ok = _ag_xml_dup_element_data (reader, &provider->plugin_name);
            }
            else if (strcmp (name, "single-account") == 0)
            {
                ok = _ag_xml_get_boolean (reader, &provider->single_account);
            }
            else if (strcmp (name, "template") == 0)
            {
                ok = parse_template (reader, provider);
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
read_provider_file (xmlTextReaderPtr reader, AgProvider *provider)
{
    const xmlChar *name;
    int ret, type;

    ret = xmlTextReaderRead (reader);
    while (ret == 1)
    {
        type = xmlTextReaderNodeType (reader);
        if (type == XML_READER_TYPE_ELEMENT)
        {
            name = xmlTextReaderConstName (reader);
            if (G_LIKELY (name &&
                          strcmp ((const gchar *)name, "provider") == 0))
            {
                return parse_provider (reader, provider);
            }
        }

        ret = xmlTextReaderNext (reader);
    }
    return FALSE;
}

static AgProvider *
_ag_provider_new (void)
{
    AgProvider *provider;

    provider = g_slice_new0 (AgProvider);
    provider->ref_count = 1;

    return provider;
}

static gboolean
_ag_provider_load_from_file (AgProvider *provider)
{
    xmlTextReaderPtr reader;
    gchar *filepath;
    gboolean ret;
    GError *error = NULL;
    gsize len;

    g_return_val_if_fail (provider->name != NULL, FALSE);

    DEBUG_REFS ("Loading provider %s", provider->name);
    filepath = _ag_find_libaccounts_file (provider->name,
                                          ".provider",
                                          "AG_PROVIDERS",
                                          PROVIDER_FILES_DIR);
    if (G_UNLIKELY (!filepath)) return FALSE;

    g_file_get_contents (filepath, &provider->file_data,
                         &len, &error);
    if (G_UNLIKELY (error))
    {
        g_warning ("Error reading %s: %s", filepath, error->message);
        g_error_free (error);
        g_free (filepath);
        return FALSE;
    }

    g_free (filepath);

    /* TODO: cache the xmlReader */
    reader = xmlReaderForMemory (provider->file_data, len,
                                 NULL, NULL, 0);
    if (G_UNLIKELY (reader == NULL))
        return FALSE;

    ret = read_provider_file (reader, provider);

    xmlFreeTextReader (reader);
    return ret;
}

AgProvider *
_ag_provider_new_from_file (const gchar *provider_name)
{
    AgProvider *provider;

    provider = _ag_provider_new ();
    provider->name = g_strdup (provider_name);
    if (!_ag_provider_load_from_file (provider))
    {
        ag_provider_unref (provider);
        provider = NULL;
    }

    return provider;
}

GHashTable *
_ag_provider_load_default_settings (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);

    if (!provider->default_settings)
    {
        /* This can happen if the provider was created by the AccountManager by
         * loading the record from the DB.
         * Now we must reload the provider from its XML file.
         */
        if (!_ag_provider_load_from_file (provider))
        {
            g_warning ("Loading provider %s file failed", provider->name);
            return NULL;
        }
    }

    return provider->default_settings;
}

GVariant *
_ag_provider_get_default_setting (AgProvider *provider, const gchar *key)
{
    GHashTable *settings;

    g_return_val_if_fail (key != NULL, NULL);

    settings = _ag_provider_load_default_settings (provider);
    if (G_UNLIKELY (!settings))
        return NULL;

    return g_hash_table_lookup (settings, key);
}

/**
 * ag_provider_get_name:
 * @provider: the #AgProvider.
 *
 * Get the name of the #AgProvider.
 *
 * Returns: the name of @provider.
 */
const gchar *
ag_provider_get_name (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->name;
}

/**
 * ag_provider_get_i18n_domain:
 * @provider: the #AgProvider.
 *
 * Get the translation domain of the #AgProvider.
 *
 * Returns: the translation domain.
 */
const gchar *
ag_provider_get_i18n_domain (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->i18n_domain;
}

/**
 * ag_provider_get_icon_name:
 * @provider: the #AgProvider.
 *
 * Get the icon name of the #AgProvider.
 *
 * Returns: the icon_name.
 */
const gchar *
ag_provider_get_icon_name (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->icon_name;
}

/**
 * ag_provider_get_display_name:
 * @provider: the #AgProvider.
 *
 * Get the display name of the #AgProvider.
 *
 * Returns: the display name of @provider.
 */
const gchar *
ag_provider_get_display_name (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->display_name;
}

/**
 * ag_provider_get_description:
 * @provider: the #AgProvider.
 *
 * Get the description of the #AgProvider.
 *
 * Returns: the description of @provider, or %NULL upon failure.
 *
 * Since: 1.2
 */
const gchar *
ag_provider_get_description (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->description;
}

/**
 * ag_provider_get_domains_regex:
 * @provider: the #AgProvider.
 *
 * Get a regular expression matching all domains where this provider's accounts
 * can be used.
 *
 * Returns: a regular expression matching the domain names.
 *
 * Since: 1.1
 */
const gchar *
ag_provider_get_domains_regex (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->domains;
}

/**
 * ag_provider_match_domain:
 * @provider: the #AgProvider.
 * @domain: a domain name.
 *
 * Check whether @domain is supported by this provider, by matching it with the
 * regex returned by ag_provider_get_domains_regex().
 * If the provider does not define a regular expression to match the supported
 * domains, this function will return %FALSE.
 *
 * Returns: %TRUE if the given domain is supported, %FALSE otherwise.
 *
 * Since: 1.2
 */
gboolean
ag_provider_match_domain (AgProvider *provider, const gchar *domain)
{
    g_return_val_if_fail (provider != NULL, FALSE);
    g_return_val_if_fail (domain != NULL, FALSE);

    if (provider->domains == NULL)
        return FALSE;

    return g_regex_match_simple (provider->domains, domain, 0, 0);
}

/**
 * ag_provider_get_plugin_name:
 * @provider: the #AgProvider.
 *
 * Get the name of the account plugin which manages all accounts created from
 * this #AgProvider.
 * Some platforms might find it useful to store plugin names in the provider
 * XML files, especially when the same plugin can work for different providers.
 *
 * Returns: the plugin name for @provider, or %NULL if a plugin name is not
 * defined.
 *
 * Since: 1.5
 */
const gchar *
ag_provider_get_plugin_name (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->plugin_name;
}

/**
 * ag_provider_get_single_account:
 * @provider: the #AgProvider.
 *
 * Tell whether the provider doesn't support creating more than one account.
 * Note that libaccounts itself does not enforce preventing the creation of
 * multiple accounts when this flag is set: the flag is only informative, and
 * its implementation is left to the client.
 *
 * Returns: %FALSE if multiple accounts can be created from this provider,
 * %TRUE otherwise.
 *
 * Since: 1.14
 */
gboolean
ag_provider_get_single_account (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    return provider->single_account;
}

/**
 * ag_provider_get_file_contents:
 * @provider: the #AgProvider.
 * @contents: location to receive the pointer to the file contents.
 *
 * Gets the contents of the XML provider file.  The buffer returned in @contents
 * should not be modified or freed, and is guaranteed to be valid as long as
 * @provider is referenced.
 * If some error occurs, @contents is set to %NULL.
 */
void
ag_provider_get_file_contents (AgProvider *provider,
                              const gchar **contents)
{
    g_return_if_fail (provider != NULL);
    g_return_if_fail (contents != NULL);

    if (provider->file_data == NULL)
    {
        /* This can happen if the provider was created by the AccountManager by
         * loading the record from the DB.
         * Now we must reload the provider from its XML file.
         */
        if (!_ag_provider_load_from_file (provider))
            g_warning ("Loading provider %s file failed", provider->name);
    }

    *contents = provider->file_data;
}

/**
 * ag_provider_ref:
 * @provider: the #AgProvider.
 *
 * Adds a reference to @provider.
 *
 * Returns: @provider.
 */
AgProvider *
ag_provider_ref (AgProvider *provider)
{
    g_return_val_if_fail (provider != NULL, NULL);
    g_return_val_if_fail (provider->ref_count > 0, NULL);

    DEBUG_REFS ("Referencing provider %s (%d)",
                provider->name, provider->ref_count);
    provider->ref_count++;
    return provider;
}

/**
 * ag_provider_unref:
 * @provider: the #AgProvider.
 *
 * Used to unreference the #AgProvider structure.
 */
void
ag_provider_unref (AgProvider *provider)
{
    g_return_if_fail (provider != NULL);
    g_return_if_fail (provider->ref_count > 0);

    DEBUG_REFS ("Unreferencing provider %s (%d)",
                provider->name, provider->ref_count);
    provider->ref_count--;
    if (provider->ref_count == 0)
    {
        g_free (provider->name);
        g_free (provider->i18n_domain);
        g_free (provider->icon_name);
        g_free (provider->description);
        g_free (provider->display_name);
        g_free (provider->domains);
        g_free (provider->file_data);
        g_slice_free (AgProvider, provider);
    }
}

/**
 * ag_provider_list_free:
 * @list: (element-type AgProvider): a #GList of providers returned by some
 * function of this library.
 *
 * Frees the list @list.
 */
void
ag_provider_list_free (GList *list)
{
    g_list_foreach (list, (GFunc)ag_provider_unref, NULL);
    g_list_free (list);
}

