#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <gio/gio.h>

#include "x-common.h"
#include "xdmcp-client.h"

#define MAXIMUM_REQUEST_LENGTH 65535

typedef enum
{
    XDMCP_BroadcastQuery = 1,
    XDMCP_Query          = 2,
    XDMCP_IndirectQuery  = 3,
    XDMCP_ForwardQuery   = 4,
    XDMCP_Willing        = 5,
    XDMCP_Unwilling      = 6,
    XDMCP_Request        = 7,
    XDMCP_Accept         = 8,
    XDMCP_Decline        = 9,
    XDMCP_Manage         = 10,
    XDMCP_Refuse         = 11,
    XDMCP_Failed         = 12,
    XDMCP_KeepAlive      = 13,
    XDMCP_Alive          = 14
} XDMCPOpcode;

typedef struct
{
    gchar *host;
    gint port;
    GSocket *socket;
    gchar *authentication_names;
    gchar *authorization_name;
    gint authorization_data_length;
    guint8 *authorization_data;
} XDMCPClientPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XDMCPClient, xdmcp_client, G_TYPE_OBJECT)

enum {
    XDMCP_CLIENT_WILLING,
    XDMCP_CLIENT_UNWILLING,
    XDMCP_CLIENT_ACCEPT,
    XDMCP_CLIENT_DECLINE,
    XDMCP_CLIENT_FAILED,
    XDMCP_CLIENT_ALIVE,
    XDMCP_CLIENT_LAST_SIGNAL
};
static guint xdmcp_client_signals[XDMCP_CLIENT_LAST_SIGNAL] = { 0 };

static void
xdmcp_write (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (client);

    g_autoptr(GError) error = NULL;
    gssize n_written = g_socket_send (priv->socket, (const gchar *) buffer, buffer_length, NULL, &error);
    if (n_written < 0)
        g_warning ("Failed to send XDMCP request: %s", error->message);
    else if (n_written != buffer_length)
        g_warning ("Partial write for XDMCP request, wrote %zi, expected %zi", n_written, buffer_length);
}

static void
decode_willing (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPWilling *message = g_malloc0 (sizeof (XDMCPWilling));

    gsize offset = 0;
    guint16 length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authentication_name = read_string (buffer, buffer_length, length, &offset);
    length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->hostname = read_string (buffer, buffer_length, length, &offset);
    length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->status = read_string (buffer, buffer_length, length, &offset);

    g_signal_emit (client, xdmcp_client_signals[XDMCP_CLIENT_WILLING], 0, message);

    g_free (message->authentication_name);
    g_free (message->hostname);
    g_free (message->status);
    g_free (message);
}

static void
decode_unwilling (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPUnwilling *message = g_malloc0 (sizeof (XDMCPUnwilling));

    gsize offset = 0;
    guint16 length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->hostname = read_string (buffer, buffer_length, length, &offset);
    length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->status = read_string (buffer, buffer_length, length, &offset);

    g_signal_emit (client, xdmcp_client_signals[XDMCP_CLIENT_UNWILLING], 0, message);

    g_free (message->hostname);
    g_free (message->status);
    g_free (message);
}

static void
decode_accept (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPAccept *message = g_malloc (sizeof (XDMCPAccept));

    gsize offset = 0;
    message->session_id = read_card32 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    guint16 length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authentication_name = read_string (buffer, buffer_length, length, &offset);
    message->authentication_data_length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authentication_data = read_string8 (buffer, buffer_length, message->authentication_data_length, &offset);
    length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authorization_name = read_string (buffer, buffer_length, length, &offset);
    message->authorization_data_length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authorization_data = read_string8 (buffer, buffer_length, length, &offset);

    g_signal_emit (client, xdmcp_client_signals[XDMCP_CLIENT_ACCEPT], 0, message);

    g_free (message->authentication_name);
    g_free (message->authentication_data);
    g_free (message->authorization_name);
    g_free (message->authorization_data);
    g_free (message);
}

static void
decode_decline (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPDecline *message = g_malloc0 (sizeof (XDMCPDecline));

    gsize offset = 0;
    guint16 length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->status = read_string (buffer, buffer_length, length, &offset);
    length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authentication_name = read_string (buffer, buffer_length, length, &offset);
    message->authentication_data_length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->authentication_data = read_string8 (buffer, buffer_length, message->authentication_data_length, &offset);

    g_signal_emit (client, xdmcp_client_signals[XDMCP_CLIENT_DECLINE], 0, message);

    g_free (message->status);
    g_free (message->authentication_name);
    g_free (message->authentication_data);
    g_free (message);
}

static void
decode_failed (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPFailed *message = g_malloc0 (sizeof (XDMCPFailed));

    gsize offset = 0;
    message->session_id = read_card32 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    guint16 length = read_card16 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);
    message->status = read_string (buffer, buffer_length, length, &offset);

    g_signal_emit (client, xdmcp_client_signals[XDMCP_CLIENT_FAILED], 0, message);

    g_free (message->status);
    g_free (message);
}

static void
decode_alive (XDMCPClient *client, const guint8 *buffer, gssize buffer_length)
{
    XDMCPAlive *message;
    gsize offset = 0;

    message = g_malloc0 (sizeof (XDMCPAlive));

    message->session_running = read_card8 (buffer, buffer_length, &offset) != 0 ? TRUE : FALSE;
    message->session_id = read_card32 (buffer, buffer_length, X_BYTE_ORDER_MSB, &offset);

    g_signal_emit (client, xdmcp_client_signals[XDMCP_CLIENT_ALIVE], 0, message);

    g_free (message);
}

static gboolean
xdmcp_data_cb (GIOChannel *channel, GIOCondition condition, gpointer data)
{
    XDMCPClient *client = data;

    guint8 buffer[MAXIMUM_REQUEST_LENGTH];
    gssize n_read = recv (g_io_channel_unix_get_fd (channel), buffer, MAXIMUM_REQUEST_LENGTH, 0);
    if (n_read < 0)
        g_warning ("Error reading from XDMCP socket: %s", strerror (errno));
    else if (n_read == 0)
    {
        g_debug ("EOF");
        return FALSE;
    }
    else
    {
        gsize offset = 0;
        guint16 version = read_card16 (buffer, n_read, X_BYTE_ORDER_MSB, &offset);
        guint16 opcode = read_card16 (buffer, n_read, X_BYTE_ORDER_MSB, &offset);
        guint16 length = read_card16 (buffer, n_read, X_BYTE_ORDER_MSB, &offset);

        if (version != 1)
        {
            g_debug ("Ignoring XDMCP version %d message", version);
            return TRUE;
        }
        if (6 + length > n_read)
        {
            g_debug ("Ignoring XDMCP message of length %zi with invalid length field %d", n_read, length);
            return TRUE;
        }
        switch (opcode)
        {
        case XDMCP_Willing:
            decode_willing (client, buffer + offset, n_read - offset);
            break;

        case XDMCP_Unwilling:
            decode_unwilling (client, buffer + offset, n_read - offset);
            break;

        case XDMCP_Accept:
            decode_accept (client, buffer + offset, n_read - offset);
            break;

        case XDMCP_Decline:
            decode_decline (client, buffer + offset, n_read - offset);
            break;

        case XDMCP_Failed:
            decode_failed (client, buffer + offset, n_read - offset);
            break;

        case XDMCP_Alive:
            decode_alive (client, buffer + offset, n_read - offset);
            break;

        default:
            g_debug ("Ignoring unknown XDMCP opcode %d", opcode);
            break;
        }
    }

    return TRUE;
}

XDMCPClient *
xdmcp_client_new (void)
{
    return g_object_new (xdmcp_client_get_type (), NULL);
}

void
xdmcp_client_set_hostname (XDMCPClient *client, const gchar *hostname)
{
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (client);
    g_free (priv->host);
    priv->host = g_strdup (hostname);
}

void
xdmcp_client_set_port (XDMCPClient *client, guint16 port)
{
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (client);
    priv->port = port;
}

gboolean
xdmcp_client_start (XDMCPClient *client)
{
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (client);

    if (priv->socket)
        return TRUE;

    g_autoptr(GError) error = NULL;
    priv->socket = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &error);
    if (error)
        g_warning ("Error creating XDMCP socket: %s", error->message);
    if (!priv->socket)
        return FALSE;

    GSocketConnectable *address = g_network_address_new (priv->host, priv->port);
    GSocketAddressEnumerator *enumerator = g_socket_connectable_enumerate (address);
    while (TRUE)
    {
        g_autoptr(GError) e = NULL;
        g_autoptr(GSocketAddress) socket_address = g_socket_address_enumerator_next (enumerator, NULL, &e);
        if (e)
            g_warning ("Failed to get socket address: %s", e->message);
        if (!socket_address)
            return FALSE;

        if (!g_socket_connect (priv->socket, socket_address, NULL, &e))
        {
            g_warning ("Unable to connect XDMCP socket: %s", error->message);
            continue;
        }

        g_io_add_watch (g_io_channel_unix_new (g_socket_get_fd (priv->socket)), G_IO_IN, xdmcp_data_cb, client);

        return TRUE;
    }
}

GInetAddress *
xdmcp_client_get_local_address (XDMCPClient *client)
{
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (client);

    if (!priv->socket)
        return NULL;

    GSocketAddress *socket_address = g_socket_get_local_address (priv->socket, NULL);
    return g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (socket_address));
}

static void
xdmcp_client_init (XDMCPClient *client)
{
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (client);
    priv->port = XDMCP_PORT;
}

static void
send_query (XDMCPClient *client, guint16 opcode, gchar **authentication_names)
{
    guint8 buffer[MAXIMUM_REQUEST_LENGTH];
    gsize offset = 0;

    gsize length = 1;
    gsize n_names = 0;
    for (gchar **name = authentication_names; authentication_names && *name; name++)
    {
        length += 2 + strlen (*name);
        n_names++;
    }

    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_VERSION, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, opcode, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, length, &offset);
    write_card8 (buffer, MAXIMUM_REQUEST_LENGTH, n_names, &offset);
    for (gchar **name = authentication_names; authentication_names && *name; name++)
    {
        write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, strlen (*name), &offset);
        write_string (buffer, MAXIMUM_REQUEST_LENGTH, *name, &offset);
    }
    xdmcp_write (client, buffer, offset);
}

void
xdmcp_client_send_query (XDMCPClient *client, gchar **authentication_names)
{
    send_query (client, XDMCP_Query, authentication_names);
}

void
xdmcp_client_send_broadcast_query (XDMCPClient *client, gchar **authentication_names)
{
    send_query (client, XDMCP_BroadcastQuery, authentication_names);
}

void
xdmcp_client_send_indirect_query (XDMCPClient *client, gchar **authentication_names)
{
    send_query (client, XDMCP_IndirectQuery, authentication_names);
}

void
xdmcp_client_send_request (XDMCPClient *client,
                           guint16 display_number,
                           GInetAddress **addresses,
                           const gchar *authentication_name,
                           const guint8 *authentication_data, guint16 authentication_data_length,
                           gchar **authorization_names, const gchar *mfid)
{
    gsize length = 11 + strlen (authentication_name) + authentication_data_length + strlen (mfid);
    gsize n_addresses = 0;
    for (GInetAddress **address = addresses; *address; address++)
    {
        gssize native_address_length = g_inet_address_get_native_size (*address);
        length += 4 + native_address_length;
        n_addresses++;
    }
    gsize n_names = 0;
    for (gchar **name = authorization_names; *name; name++)
    {
        length += 2 + strlen (*name);
        n_names++;
    }

    guint8 buffer[MAXIMUM_REQUEST_LENGTH];
    gsize offset = 0;

    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_VERSION, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_Request, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, length, &offset);

    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, display_number, &offset);
    write_card8 (buffer, MAXIMUM_REQUEST_LENGTH, n_addresses, &offset);
    for (GInetAddress **address = addresses; *address; address++)
        write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, 0, &offset); /* FamilyInternet */
    write_card8 (buffer, MAXIMUM_REQUEST_LENGTH, n_addresses, &offset);
    for (GInetAddress **address = addresses; *address; address++)
    {
        gssize native_address_length = g_inet_address_get_native_size (*address);
        const guint8 *native_address = g_inet_address_to_bytes (*address);
        write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, native_address_length, &offset);
        write_string8 (buffer, MAXIMUM_REQUEST_LENGTH, native_address, native_address_length, &offset);
    }
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, strlen (authentication_name), &offset);
    write_string (buffer, MAXIMUM_REQUEST_LENGTH, authentication_name, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, authentication_data_length, &offset);
    write_string8 (buffer, MAXIMUM_REQUEST_LENGTH, authentication_data, authentication_data_length, &offset);
    write_card8 (buffer, MAXIMUM_REQUEST_LENGTH, n_names, &offset);
    for (gchar **name = authorization_names; *name; name++)
    {
        write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, strlen (*name), &offset);
        write_string (buffer, MAXIMUM_REQUEST_LENGTH, *name, &offset);
    }
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, strlen (mfid), &offset);
    write_string (buffer, MAXIMUM_REQUEST_LENGTH, mfid, &offset);

    xdmcp_write (client, buffer, offset);
}

void
xdmcp_client_send_manage (XDMCPClient *client, guint32 session_id, guint16 display_number, const gchar *display_class)
{
    guint8 buffer[MAXIMUM_REQUEST_LENGTH];
    gsize offset = 0;

    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_VERSION, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_Manage, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, 8 + strlen (display_class), &offset);

    write_card32 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, session_id, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, display_number, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, strlen (display_class), &offset);
    write_string (buffer, MAXIMUM_REQUEST_LENGTH, display_class, &offset);

    xdmcp_write (client, buffer, offset);
}

void
xdmcp_client_send_keep_alive (XDMCPClient *client, guint16 display_number, guint32 session_id)
{
    guint8 buffer[MAXIMUM_REQUEST_LENGTH];
    gsize offset = 0;

    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_VERSION, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, XDMCP_KeepAlive, &offset);
    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, 6, &offset);

    write_card16 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, display_number, &offset);
    write_card32 (buffer, MAXIMUM_REQUEST_LENGTH, X_BYTE_ORDER_MSB, session_id, &offset);

    xdmcp_write (client, buffer, offset);
}

static void
xdmcp_client_finalize (GObject *object)
{
    XDMCPClient *self = XDMCP_CLIENT (object);
    XDMCPClientPrivate *priv = xdmcp_client_get_instance_private (self);

    g_clear_pointer (&priv->host, g_free);
    g_clear_object (&priv->socket);
    g_clear_pointer (&priv->authorization_name, g_free);
    g_clear_pointer (&priv->authorization_data, g_free);
}

static void
xdmcp_client_class_init (XDMCPClientClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = xdmcp_client_finalize;

    xdmcp_client_signals[XDMCP_CLIENT_WILLING] =
        g_signal_new (XDMCP_CLIENT_SIGNAL_WILLING,
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XDMCPClientClass, willing),
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    xdmcp_client_signals[XDMCP_CLIENT_UNWILLING] =
        g_signal_new (XDMCP_CLIENT_SIGNAL_UNWILLING,
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XDMCPClientClass, unwilling),
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    xdmcp_client_signals[XDMCP_CLIENT_ACCEPT] =
        g_signal_new (XDMCP_CLIENT_SIGNAL_ACCEPT,
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XDMCPClientClass, accept),
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    xdmcp_client_signals[XDMCP_CLIENT_DECLINE] =
        g_signal_new (XDMCP_CLIENT_SIGNAL_DECLINE,
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XDMCPClientClass, decline),
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    xdmcp_client_signals[XDMCP_CLIENT_FAILED] =
        g_signal_new (XDMCP_CLIENT_SIGNAL_FAILED,
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XDMCPClientClass, failed),
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    xdmcp_client_signals[XDMCP_CLIENT_ALIVE] =
        g_signal_new (XDMCP_CLIENT_SIGNAL_ALIVE,
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (XDMCPClientClass, alive),
                      NULL, NULL,
                      NULL,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
}
