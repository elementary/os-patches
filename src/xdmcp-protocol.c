/*
 * Copyright (C) 2010-2011 Robert Ancell.
 * Author: Robert Ancell <robert.ancell@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See http://www.gnu.org/copyleft/gpl.html the full text of the
 * license.
 */

#include <string.h>
#include <gio/gio.h>

#include "xdmcp-protocol.h"
#include "x-authority.h"

typedef struct
{
    const guint8 *data;
    guint16 remaining;
    gboolean overflow;
} PacketReader;

static guint8
read_card8 (PacketReader *reader)
{
    if (reader->remaining < 1)
    {
        reader->overflow = TRUE;
        return 0;
    }

    guint8 value = reader->data[0];
    reader->data++;
    reader->remaining--;

    return value;
}

static guint16
read_card16 (PacketReader *reader)
{
    return read_card8 (reader) << 8 | read_card8 (reader);
}

static guint32
read_card32 (PacketReader *reader)
{
    return read_card8 (reader) << 24 | read_card8 (reader) << 16 | read_card8 (reader) << 8 | read_card8 (reader);
}

static void
read_data (PacketReader *reader, XDMCPData *data)
{
    data->length = read_card16 (reader);
    data->data = g_malloc (sizeof (guint8) * data->length);
    for (guint16 i = 0; i < data->length; i++)
        data->data[i] = read_card8 (reader);
}

static gchar *
read_string (PacketReader *reader)
{
    guint16 length = read_card16 (reader);
    gchar *string = g_malloc (sizeof (gchar) * (length + 1));
    guint16 i;
    for (i = 0; i < length; i++)
        string[i] = (gchar) read_card8 (reader);
    string[i] = '\0';

    return string;
}

static gchar **
read_string_array (PacketReader *reader)
{
    guint8 n_strings = read_card8 (reader);
    gchar **strings = g_malloc (sizeof (gchar *) * (n_strings + 1));
    guint8 i;
    for (i = 0; i < n_strings; i++)
        strings[i] = read_string (reader);
    strings[i] = NULL;

    return strings;
}

typedef struct
{
    guint8 *data;
    guint16 remaining;
    gboolean overflow;
} PacketWriter;

static void
write_card8 (PacketWriter *writer, guint8 value)
{
    if (writer->remaining < 1)
    {
        writer->overflow = TRUE;
        return;
    }

    writer->data[0] = value;
    writer->data++;
    writer->remaining--;
}

static void
write_card16 (PacketWriter *writer, guint16 value)
{
    write_card8 (writer, value >> 8);
    write_card8 (writer, value & 0xFF);
}

static void
write_card32 (PacketWriter *writer, guint32 value)
{
    write_card8 (writer, (value >> 24) & 0xFF);
    write_card8 (writer, (value >> 16) & 0xFF);
    write_card8 (writer, (value >> 8) & 0xFF);
    write_card8 (writer, value & 0xFF);
}

static void
write_data (PacketWriter *writer, const XDMCPData *value)
{
    write_card16 (writer, value->length);
    for (guint16 i = 0; i < value->length; i++)
        write_card8 (writer, value->data[i]);
}

static void
write_string (PacketWriter *writer, const gchar *value)
{
    write_card16 (writer, strlen (value));
    for (const gchar *c = value; *c; c++)
        write_card8 (writer, *c);
}

static void
write_string_array (PacketWriter *writer, gchar **values)
{
    write_card8 (writer, g_strv_length (values));
    for (gchar **value = values; *value; value++)
        write_string (writer, *value);
}

XDMCPPacket *
xdmcp_packet_alloc (XDMCPOpcode opcode)
{
    XDMCPPacket *packet = g_malloc0 (sizeof (XDMCPPacket));
    packet->opcode = opcode;

    return packet;
}

XDMCPPacket *
xdmcp_packet_decode (const guint8 *data, gsize data_length)
{
    PacketReader reader;
    reader.data = data;
    reader.remaining = data_length;
    reader.overflow = FALSE;

    guint16 version = read_card16 (&reader);
    guint16 opcode = read_card16 (&reader);
    guint16 length = read_card16 (&reader);

    if (reader.overflow)
    {
        g_warning ("Ignoring short packet"); // FIXME: Use GError
        return NULL;
    }
    if (version != XDMCP_VERSION)
    {
        g_warning ("Ignoring packet from unknown version %d", version);
        return NULL;
    }
    if (length != reader.remaining)
    {
        g_warning ("Ignoring packet of wrong length. Opcode %d expected %d octets, got %d", opcode, length, reader.remaining);
        return NULL;
    }

    XDMCPPacket *packet = xdmcp_packet_alloc (opcode);
    gboolean failed = FALSE;
    switch (packet->opcode)
    {
    case XDMCP_BroadcastQuery:
    case XDMCP_Query:
    case XDMCP_IndirectQuery:
        packet->Query.authentication_names = read_string_array (&reader);
        break;
    case XDMCP_ForwardQuery:
        read_data (&reader, &packet->ForwardQuery.client_address);
        read_data (&reader, &packet->ForwardQuery.client_port);
        packet->ForwardQuery.authentication_names = read_string_array (&reader);
        break;
    case XDMCP_Willing:
        packet->Willing.authentication_name = read_string (&reader);
        packet->Willing.hostname = read_string (&reader);
        packet->Willing.status = read_string (&reader);
        break;
    case XDMCP_Unwilling:
        packet->Unwilling.hostname = read_string (&reader);
        packet->Unwilling.status = read_string (&reader);
        break;
    case XDMCP_Request:
        packet->Request.display_number = read_card16 (&reader);
        packet->Request.n_connections = read_card8 (&reader);
        packet->Request.connections = g_malloc (sizeof (XDMCPConnection) * packet->Request.n_connections);
        for (int i = 0; i < packet->Request.n_connections; i++)
            packet->Request.connections[i].type = read_card16 (&reader);
        if (read_card8 (&reader) != packet->Request.n_connections)
        {
            g_warning ("Number of connection types does not match number of connection addresses");
            failed = TRUE;
        }
        for (int i = 0; i < packet->Request.n_connections; i++)
            read_data (&reader, &packet->Request.connections[i].address);
        packet->Request.authentication_name = read_string (&reader);
        read_data (&reader, &packet->Request.authentication_data);
        packet->Request.authorization_names = read_string_array (&reader);
        packet->Request.manufacturer_display_id = read_string (&reader);
        break;
    case XDMCP_Accept:
        packet->Accept.session_id = read_card32 (&reader);
        packet->Accept.authentication_name = read_string (&reader);
        read_data (&reader, &packet->Accept.authentication_data);
        packet->Accept.authorization_name = read_string (&reader);
        read_data (&reader, &packet->Accept.authorization_data);
        break;
    case XDMCP_Decline:
        packet->Decline.status = read_string (&reader);
        packet->Decline.authentication_name = read_string (&reader);
        read_data (&reader, &packet->Decline.authentication_data);
        break;
    case XDMCP_Manage:
        packet->Manage.session_id = read_card32 (&reader);
        packet->Manage.display_number = read_card16 (&reader);
        packet->Manage.display_class = read_string (&reader);
        break;
    case XDMCP_Refuse:
        packet->Refuse.session_id = read_card32 (&reader);
        break;
    case XDMCP_Failed:
        packet->Failed.session_id = read_card32 (&reader);
        packet->Failed.status = read_string (&reader);
        break;
    case XDMCP_KeepAlive:
        packet->KeepAlive.display_number = read_card16 (&reader);
        packet->KeepAlive.session_id = read_card32 (&reader);
        break;
    case XDMCP_Alive:
        packet->Alive.session_running = read_card8 (&reader) == 0 ? FALSE : TRUE;
        packet->Alive.session_id = read_card32 (&reader);
        break;
    default:
        g_warning ("Unable to encode unknown opcode %d", packet->opcode);
        failed = TRUE;
        break;
    }

    if (!failed)
    {
        if (reader.overflow)
        {
            g_warning ("Short packet received");
            failed = TRUE;
        }
        else if (reader.remaining != 0)
        {
            g_warning ("Extra data on end of message");
            failed = TRUE;
        }
    }
    if (failed)
    {
        xdmcp_packet_free (packet);
        return NULL;
    }

    return packet;
}

gssize
xdmcp_packet_encode (XDMCPPacket *packet, guint8 *data, gsize max_length)
{
    if (max_length < 6)
        return -1;

    PacketWriter writer;
    writer.data = data + 6;
    writer.remaining = max_length - 6;
    writer.overflow = FALSE;

    switch (packet->opcode)
    {
    case XDMCP_BroadcastQuery:
    case XDMCP_Query:
    case XDMCP_IndirectQuery:
        write_string_array (&writer, packet->Query.authentication_names);
        break;
    case XDMCP_ForwardQuery:
        write_data (&writer, &packet->ForwardQuery.client_address);
        write_data (&writer, &packet->ForwardQuery.client_port);
        write_string_array (&writer, packet->ForwardQuery.authentication_names);
        break;
    case XDMCP_Willing:
        write_string (&writer, packet->Willing.authentication_name);
        write_string (&writer, packet->Willing.hostname);
        write_string (&writer, packet->Willing.status);
        break;
    case XDMCP_Unwilling:
        write_string (&writer, packet->Unwilling.hostname);
        write_string (&writer, packet->Unwilling.status);
        break;
    case XDMCP_Request:
        write_card16 (&writer, packet->Request.display_number);
        write_card8 (&writer, packet->Request.n_connections);
        for (int i = 0; i < packet->Request.n_connections; i++)
            write_card16 (&writer, packet->Request.connections[i].type);
        write_card8 (&writer, packet->Request.n_connections);
        for (int i = 0; i < packet->Request.n_connections; i++)
            write_data (&writer, &packet->Request.connections[i].address);
        write_string (&writer, packet->Request.authentication_name);
        write_data (&writer, &packet->Request.authentication_data);
        write_string_array (&writer, packet->Request.authorization_names);
        write_string (&writer, packet->Request.manufacturer_display_id);
        break;
    case XDMCP_Accept:
        write_card32 (&writer, packet->Accept.session_id);
        write_string (&writer, packet->Accept.authentication_name);
        write_data (&writer, &packet->Accept.authentication_data);
        write_string (&writer, packet->Accept.authorization_name);
        write_data (&writer, &packet->Accept.authorization_data);
        break;
    case XDMCP_Decline:
        write_string (&writer, packet->Decline.status);
        write_string (&writer, packet->Decline.authentication_name);
        write_data (&writer, &packet->Decline.authentication_data);
        break;
    case XDMCP_Manage:
        write_card32 (&writer, packet->Manage.session_id);
        write_card16 (&writer, packet->Manage.display_number);
        write_string (&writer, packet->Manage.display_class);
        break;
    case XDMCP_Refuse:
        write_card32 (&writer, packet->Refuse.session_id);
        break;
    case XDMCP_Failed:
        write_card32 (&writer, packet->Failed.session_id);
        write_string (&writer, packet->Failed.status);
        break;
    case XDMCP_KeepAlive:
        write_card16 (&writer, packet->KeepAlive.display_number);
        write_card32 (&writer, packet->KeepAlive.session_id);
        break;
    case XDMCP_Alive:
        write_card8 (&writer, packet->Alive.session_running ? 1 : 0);
        write_card32 (&writer, packet->Alive.session_id);
        break;
    }

    guint16 length = max_length - 6 - writer.remaining;

    /* Write header */
    writer.data = data;
    writer.remaining = 6;
    writer.overflow = FALSE;
    write_card16(&writer, XDMCP_VERSION);
    write_card16(&writer, packet->opcode);
    write_card16(&writer, length);

    if (writer.overflow)
    {
        g_warning ("Overflow writing response");
        return -1;
    }

    return length + 6;
}

static gchar *
data_tostring (XDMCPData *data)
{
    g_autoptr(GString) s = g_string_new ("");
    for (guint16 i = 0; i < data->length; i++)
        g_string_append_printf (s, "%02X", data->data[i]);

    return g_steal_pointer (&s->str);
}

static gchar *
string_list_tostring (gchar **strings)
{
    g_autoptr(GString) s = g_string_new ("");
    for (gchar **i = strings; *i; i++)
    {
        if (i != strings)
           g_string_append (s, " ");
        g_string_append_printf (s, "'%s'", *i);
    }

    return g_steal_pointer (&s->str);
}

gchar *
xdmcp_packet_tostring (XDMCPPacket *packet)
{
    switch (packet->opcode)
    {
    case XDMCP_BroadcastQuery:
    {
        g_autofree gchar *names = string_list_tostring (packet->Query.authentication_names);
        return g_strdup_printf ("BroadcastQuery(authentication_names=[%s])", names);
    }
    case XDMCP_Query:
    {
        g_autofree gchar *names = string_list_tostring (packet->Query.authentication_names);
        return g_strdup_printf ("Query(authentication_names=[%s])", names);
    }
    case XDMCP_IndirectQuery:
    {
        g_autofree gchar *names = string_list_tostring (packet->Query.authentication_names);
        return g_strdup_printf ("IndirectQuery(authentication_names=[%s])", names);
    }
    case XDMCP_ForwardQuery:
    {
        g_autofree gchar *address_text = data_tostring (&packet->ForwardQuery.client_address);
        g_autofree gchar *port_text = data_tostring (&packet->ForwardQuery.client_port);
        g_autofree gchar *names_text = string_list_tostring (packet->ForwardQuery.authentication_names);
        return g_strdup_printf ("ForwardQuery(client_address=%s client_port=%s authentication_names=[%s])",
                                address_text, port_text, names_text);
    }
    case XDMCP_Willing:
        return g_strdup_printf ("Willing(authentication_name='%s' hostname='%s' status='%s')",
                                packet->Willing.authentication_name, packet->Willing.hostname, packet->Willing.status);
    case XDMCP_Unwilling:
        return g_strdup_printf ("Unwilling(hostname='%s' status='%s')",
                                packet->Unwilling.hostname, packet->Unwilling.status);
    case XDMCP_Request:
    {
        g_autofree gchar *names_text = string_list_tostring (packet->Request.authorization_names);
        g_autofree gchar *data_text = data_tostring (&packet->Request.authentication_data);
        g_autoptr(GString) connections_text = g_string_new ("");
        for (gint i = 0; i < packet->Request.n_connections; i++)
        {
            XDMCPConnection *connection = &packet->Request.connections[i];

            if (i != 0)
               g_string_append (connections_text, " ");

            GSocketFamily family = G_SOCKET_FAMILY_INVALID;
            if (connection->type == XAUTH_FAMILY_INTERNET && connection->address.length == 4)
                family = G_SOCKET_FAMILY_IPV4;
            else if (connection->type == XAUTH_FAMILY_INTERNET6 && connection->address.length == 16)
                family = G_SOCKET_FAMILY_IPV6;

            if (family != G_SOCKET_FAMILY_INVALID)
            {
                g_autoptr(GInetAddress) address = g_inet_address_new_from_bytes (connection->address.data, family);
                g_autofree gchar *text = g_inet_address_to_string (address);
                g_string_append (connections_text, text);
            }
            else
            {
                g_autofree gchar *text = data_tostring (&connection->address);
                g_string_append_printf (connections_text, "(%d, %s)", connection->type, text);
            }
        }
        return g_strdup_printf ("Request(display_number=%d connections=[%s] authentication_name='%s' authentication_data=%s authorization_names=[%s] manufacturer_display_id='%s')",
                                packet->Request.display_number, connections_text->str, packet->Request.authentication_name, data_text,
                                names_text, packet->Request.manufacturer_display_id);
    }
    case XDMCP_Accept:
    {
        g_autofree gchar *authentication_text = data_tostring (&packet->Accept.authentication_data);
        g_autofree gchar *authorization_text = data_tostring (&packet->Accept.authorization_data);
        return g_strdup_printf ("Accept(session_id=%d authentication_name='%s' authentication_data=%s authorization_name='%s' authorization_data=%s)",
                                packet->Accept.session_id, packet->Accept.authentication_name, authentication_text,
                                packet->Accept.authorization_name, authorization_text);
    }
    case XDMCP_Decline:
    {
        g_autofree gchar *t = data_tostring (&packet->Decline.authentication_data);
        return g_strdup_printf ("Decline(status='%s' authentication_name='%s' authentication_data=%s)",
                                packet->Decline.status, packet->Decline.authentication_name, t);
    }
    case XDMCP_Manage:
        return g_strdup_printf ("Manage(session_id=%d display_number=%d display_class='%s')",
                                packet->Manage.session_id, packet->Manage.display_number, packet->Manage.display_class);
    case XDMCP_Refuse:
        return g_strdup_printf ("Refuse(session_id=%d)", packet->Refuse.session_id);
    case XDMCP_Failed:
        return g_strdup_printf ("Failed(session_id=%d status='%s')", packet->Failed.session_id, packet->Failed.status);
    case XDMCP_KeepAlive:
        return g_strdup_printf ("KeepAlive(display_number=%d session_id=%d)",
                                packet->KeepAlive.display_number, packet->KeepAlive.session_id);
    case XDMCP_Alive:
        return g_strdup_printf ("Alive(session_running=%s session_id=%d)",
                                packet->Alive.session_running ? "true" : "false", packet->Alive.session_id);
    default:
        return g_strdup_printf ("XDMCPPacket(opcode=%d)", packet->opcode);
    }
}

void
xdmcp_packet_free (XDMCPPacket *packet)
{
    if (packet == NULL)
        return;

    switch (packet->opcode)
    {
    case XDMCP_BroadcastQuery:
    case XDMCP_Query:
    case XDMCP_IndirectQuery:
        g_strfreev (packet->Query.authentication_names);
        break;
    case XDMCP_ForwardQuery:
        g_free (packet->ForwardQuery.client_address.data);
        g_free (packet->ForwardQuery.client_port.data);
        g_strfreev (packet->ForwardQuery.authentication_names);
        break;
    case XDMCP_Willing:
        g_free (packet->Willing.authentication_name);
        g_free (packet->Willing.hostname);
        g_free (packet->Willing.status);
        break;
    case XDMCP_Unwilling:
        g_free (packet->Unwilling.hostname);
        g_free (packet->Unwilling.status);
        break;
    case XDMCP_Request:
        for (gint i = 0; i < packet->Request.n_connections; i++)
            g_free (packet->Request.connections[i].address.data);
        g_free (packet->Request.connections);
        g_free (packet->Request.authentication_name);
        g_free (packet->Request.authentication_data.data);
        g_strfreev (packet->Request.authorization_names);
        g_free (packet->Request.manufacturer_display_id);
        break;
    case XDMCP_Accept:
        g_free (packet->Accept.authentication_name);
        g_free (packet->Accept.authentication_data.data);
        g_free (packet->Accept.authorization_name);
        g_free (packet->Accept.authorization_data.data);
        break;
    case XDMCP_Decline:
        g_free (packet->Decline.status);
        g_free (packet->Decline.authentication_name);
        g_free (packet->Decline.authentication_data.data);
        break;
    case XDMCP_Manage:
        g_free (packet->Manage.display_class);
        break;
    case XDMCP_Refuse:
        break;
    case XDMCP_Failed:
        g_free (packet->Failed.status);
        break;
    case XDMCP_KeepAlive:
        break;
    case XDMCP_Alive:
        break;
    }
    g_free (packet);
}
