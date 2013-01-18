/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * libmbim-glib -- GLib/GIO based library to control MBIM devices
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>

#include "mbim-message.h"
#include "mbim-message-private.h"
#include "mbim-error-types.h"
#include "mbim-enum-types.h"

/**
 * SECTION:mbim-message
 * @title: MbimMessage
 * @short_description: Generic MBIM message handling routines
 *
 * #MbimMessage is a generic type representing a MBIM message of any kind
 * (request, response, indication).
 **/

/*****************************************************************************/

GType
mbim_message_get_type (void)
{
    static volatile gsize g_define_type_id__volatile = 0;

    if (g_once_init_enter (&g_define_type_id__volatile)) {
        GType g_define_type_id =
            g_boxed_type_register_static (g_intern_static_string ("MbimMessage"),
                                          (GBoxedCopyFunc) mbim_message_ref,
                                          (GBoxedFreeFunc) mbim_message_unref);

        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

    return g_define_type_id__volatile;
}

/*****************************************************************************/

static GByteArray *
_mbim_message_allocate (MbimMessageType message_type,
                        guint32         transaction_id,
                        guint32         additional_size)
{
    GByteArray *self;
    guint32 len;

    /* Compute size of the basic empty message and allocate heap for it */
    len = sizeof (struct header) + additional_size;
    self = g_byte_array_sized_new (len);
    g_byte_array_set_size (self, len);

    /* Set MBIM header */
    ((struct header *)(self->data))->type           = GUINT32_TO_LE (message_type);
    ((struct header *)(self->data))->length         = GUINT32_TO_LE (len);
    ((struct header *)(self->data))->transaction_id = GUINT32_TO_LE (transaction_id);

    return self;
}

/*****************************************************************************/
/* Generic message interface */

/**
 * mbim_message_ref:
 * @self: a #MbimMessage.
 *
 * Atomically increments the reference count of @self by one.
 *
 * Returns: (transfer full) the new reference to @self.
 */
MbimMessage *
mbim_message_ref (MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return (MbimMessage *) g_byte_array_ref (self);
}

/**
 * mbim_message_unref:
 * @self: a #MbimMessage.
 *
 * Atomically decrements the reference count of @self by one.
 * If the reference count drops to 0, @self is completely disposed.
 */
void
mbim_message_unref (MbimMessage *self)
{
    g_return_if_fail (self != NULL);

    g_byte_array_unref (self);
}

/**
 * mbim_message_get_message_type:
 * @self: a #MbimMessage.
 *
 * Gets the message type.
 *
 * Returns: a #MbimMessageType.
 */
MbimMessageType
mbim_message_get_message_type (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, MBIM_MESSAGE_TYPE_INVALID);

    return MBIM_MESSAGE_GET_MESSAGE_TYPE (self);
}

/**
 * mbim_message_get_message_length:
 * @self: a #MbimMessage.
 *
 * Gets the whole message length.
 *
 * Returns: the length of the message.
 */
guint32
mbim_message_get_message_length (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, 0);

    return MBIM_MESSAGE_GET_MESSAGE_LENGTH (self);
}

/**
 * mbim_message_get_transaction_id:
 * @self: a #MbimMessage.
 *
 * Gets the transaction ID of the message.
 *
 * Returns: the transaction ID.
 */
guint32
mbim_message_get_transaction_id (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, 0);

    return MBIM_MESSAGE_GET_TRANSACTION_ID (self);
}

/**
 * mbim_message_new:
 * @data: contents of the message.
 * @data_length: length of the message.
 *
 * Create a #MbimMessage with the given contents.
 *
 * Returns: (transfer full): a newly created #MbimMessage, which should be freed with mbim_message_unref().
 */
MbimMessage *
mbim_message_new (const guint8 *data,
                  guint32       data_length)
{
    MbimMessage *out;

    /* Create output MbimMessage */
    out = g_byte_array_sized_new (data_length);
    g_byte_array_append (out, data, data_length);

    return out;
}

/**
 * mbim_message_dup:
 * @self: a #MbimMessage to duplicate.
 *
 * Create a #MbimMessage with the same contents as @self.
 *
 * Returns: (transfer full): a newly created #MbimMessage, which should be freed with mbim_message_unref().
 */
MbimMessage *
mbim_message_dup (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, NULL);

    return mbim_message_new (((GByteArray *)self)->data,
                             MBIM_MESSAGE_GET_MESSAGE_LENGTH (self));
}

/**
 * mbim_message_get_raw:
 * @self: a #MbimMessage.
 * @length: (out): return location for the size of the output buffer.
 * @error: return location for error or %NULL.
 *
 * Gets the whole raw data buffer of the #MbimMessage.
 *
 * Returns: (transfer none): The raw data buffer, or #NULL if @error is set.
 */
const guint8 *
mbim_message_get_raw (const MbimMessage  *self,
                      guint32            *length,
                      GError            **error)
{
    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (length != NULL, NULL);

    if (!self->data || !self->len) {
        g_set_error (error,
                     MBIM_CORE_ERROR,
                     MBIM_CORE_ERROR_FAILED,
                     "Message is empty");
        return NULL;
    }

    *length = (guint32) self->len;
    return self->data;
}

/**
 * mbim_message_get_printable:
 * @self: a #MbimMessage.
 * @line_prefix: prefix string to use in each new generated line.
 *
 * Gets a printable string with the contents of the whole MBIM message.
 *
 * Returns: (transfer full): a newly allocated string, which should be freed with g_free().
 */
gchar *
mbim_message_get_printable (const MbimMessage *self,
                            const gchar       *line_prefix)
{
    GString *printable;

    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail (line_prefix != NULL, NULL);

    if (!line_prefix)
        line_prefix = "";

    printable = g_string_new ("");
    g_string_append_printf (printable,
                            "%sHeader:\n"
                            "%s  length      = %u\n"
                            "%s  type        = %s (0x%08x)\n"
                            "%s  transaction = %u\n",
                            line_prefix,
                            line_prefix, MBIM_MESSAGE_GET_MESSAGE_LENGTH (self),
                            line_prefix, mbim_message_type_get_string (MBIM_MESSAGE_GET_MESSAGE_TYPE (self)), MBIM_MESSAGE_GET_MESSAGE_TYPE (self),
                            line_prefix, MBIM_MESSAGE_GET_TRANSACTION_ID (self));

    switch (MBIM_MESSAGE_GET_MESSAGE_TYPE (self)) {
    case MBIM_MESSAGE_TYPE_INVALID:
        g_warn_if_reached ();
        break;

    case MBIM_MESSAGE_TYPE_OPEN:
        g_string_append_printf (printable,
                                "%sContents:\n"
                                "%s  max_control_transfer = %u\n",
                                line_prefix,
                                line_prefix, mbim_message_open_get_max_control_transfer (self));
        break;

    case MBIM_MESSAGE_TYPE_CLOSE:
        break;

    case MBIM_MESSAGE_TYPE_OPEN_DONE: {
        MbimStatusError status;

        status = mbim_message_open_done_get_status_code (self);
        g_string_append_printf (printable,
                                "%sContents:\n"
                                "%s  status error = '%s' (0x%08x)\n",
                                line_prefix,
                                line_prefix, mbim_status_error_get_string (status), status);
        break;
    }

    case MBIM_MESSAGE_TYPE_CLOSE_DONE: {
        MbimStatusError status;

        status = mbim_message_close_done_get_status_code (self);
        g_string_append_printf (printable,
                                "%sContents:\n"
                                "%s  status error = '%s' (0x%08x)\n",
                                line_prefix,
                                line_prefix, mbim_status_error_get_string (status), status);
        break;
    }

    case MBIM_MESSAGE_TYPE_HOST_ERROR:
    case MBIM_MESSAGE_TYPE_FUNCTION_ERROR: {
        MbimProtocolError error;

        error = mbim_message_error_get_error_status_code (self);
        g_string_append_printf (printable,
                                "%sContents:\n"
                                "%s  error = '%s' (0x%08x)\n",
                                line_prefix,
                                line_prefix, mbim_protocol_error_get_string (error), error);
        break;
    }

    case MBIM_MESSAGE_TYPE_COMMAND:
    case MBIM_MESSAGE_TYPE_COMMAND_DONE:
    case MBIM_MESSAGE_TYPE_INDICATION:
        g_string_append_printf (printable,
                                "%sFragment header:\n"
                                "%s  total   = %u\n"
                                "%s  current = %u\n",
                                line_prefix,
                                line_prefix, _mbim_message_fragment_get_total (self),
                                line_prefix, _mbim_message_fragment_get_current (self));
        break;
    }

    return g_string_free (printable, FALSE);
}

/*****************************************************************************/
/* Fragment interface */

#define MBIM_MESSAGE_IS_FRAGMENT(self)                                  \
    (MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_COMMAND || \
     MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_COMMAND_DONE || \
     MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_INDICATION)

#define MBIM_MESSAGE_FRAGMENT_GET_TOTAL(self)                           \
	GUINT32_FROM_LE (((struct full_message *)(self->data))->message.fragment.fragment_header.total)

#define MBIM_MESSAGE_FRAGMENT_GET_CURRENT(self)                         \
	GUINT32_FROM_LE (((struct full_message *)(self->data))->message.fragment.fragment_header.current)


gboolean
_mbim_message_is_fragment (const MbimMessage *self)
{
    return MBIM_MESSAGE_IS_FRAGMENT (self);
}

guint32
_mbim_message_fragment_get_total (const MbimMessage  *self)
{
    g_assert (MBIM_MESSAGE_IS_FRAGMENT (self));

    return MBIM_MESSAGE_FRAGMENT_GET_TOTAL (self);
}

guint32
_mbim_message_fragment_get_current (const MbimMessage  *self)
{
    g_assert (MBIM_MESSAGE_IS_FRAGMENT (self));

    return MBIM_MESSAGE_FRAGMENT_GET_CURRENT (self);
}

const guint8 *
_mbim_message_fragment_get_payload (const MbimMessage *self,
                                    guint32           *length)
{
    g_assert (MBIM_MESSAGE_IS_FRAGMENT (self));

    *length = (MBIM_MESSAGE_GET_MESSAGE_LENGTH (self) - \
               sizeof (struct header) -                 \
               sizeof (struct fragment_header));
    return ((struct full_message *)(self->data))->message.fragment.buffer;
}

MbimMessage *
_mbim_message_fragment_collector_init (const MbimMessage  *fragment,
                                       GError            **error)
{
   g_assert (MBIM_MESSAGE_IS_FRAGMENT (fragment));

   /* Collector must start with fragment #0 */
   if (MBIM_MESSAGE_FRAGMENT_GET_CURRENT (fragment) != 0) {
       g_set_error (error,
                    MBIM_PROTOCOL_ERROR,
                    MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE,
                    "Expecting fragment '0/%u', got '%u/%u'",
                    MBIM_MESSAGE_FRAGMENT_GET_TOTAL (fragment),
                    MBIM_MESSAGE_FRAGMENT_GET_CURRENT (fragment),
                    MBIM_MESSAGE_FRAGMENT_GET_TOTAL (fragment));
       return NULL;
   }

   return mbim_message_dup (fragment);
}

gboolean
_mbim_message_fragment_collector_add (MbimMessage        *self,
                                      const MbimMessage  *fragment,
                                      GError            **error)
{
    guint32 buffer_len;
    const guint8 *buffer;

    g_assert (MBIM_MESSAGE_IS_FRAGMENT (self));
    g_assert (MBIM_MESSAGE_IS_FRAGMENT (fragment));

    /* We can only add a fragment if it is the next one we're expecting.
     * Otherwise, we return an error. */
    if (MBIM_MESSAGE_FRAGMENT_GET_CURRENT (self) != (MBIM_MESSAGE_FRAGMENT_GET_CURRENT (fragment) - 1)) {
        g_set_error (error,
                     MBIM_PROTOCOL_ERROR,
                     MBIM_PROTOCOL_ERROR_FRAGMENT_OUT_OF_SEQUENCE,
                     "Expecting fragment '%u/%u', got '%u/%u'",
                     MBIM_MESSAGE_FRAGMENT_GET_CURRENT (self) + 1,
                     MBIM_MESSAGE_FRAGMENT_GET_TOTAL (self),
                     MBIM_MESSAGE_FRAGMENT_GET_CURRENT (fragment),
                     MBIM_MESSAGE_FRAGMENT_GET_TOTAL (fragment));
        return FALSE;
    }

    buffer = _mbim_message_fragment_get_payload (fragment, &buffer_len);
    if (buffer_len) {
        /* Concatenate information buffers */
        g_byte_array_append (self, buffer, buffer_len);
        /* Update the whole message length */
        ((struct header *)(self->data))->length =
            GUINT32_TO_LE (MBIM_MESSAGE_GET_MESSAGE_LENGTH (self) + buffer_len);
    }

    /* Update the current fragment info in the main message; skip endian changes */
    ((struct full_message *)(self->data))->message.fragment.fragment_header.current =
        ((struct full_message *)(fragment->data))->message.fragment.fragment_header.current;

    return TRUE;
}

gboolean
_mbim_message_fragment_collector_complete (MbimMessage *self)
{
    g_assert (MBIM_MESSAGE_IS_FRAGMENT (self));

    if (MBIM_MESSAGE_FRAGMENT_GET_CURRENT (self) != (MBIM_MESSAGE_FRAGMENT_GET_TOTAL (self) - 1))
        /* Not complete yet */
        return FALSE;

    /* Reset current & total */
    ((struct full_message *)(self->data))->message.fragment.fragment_header.current = 0;
    ((struct full_message *)(self->data))->message.fragment.fragment_header.total = GUINT32_TO_LE (1);
    return TRUE;
}

struct fragment_info *
_mbim_message_split_fragments (const MbimMessage *self,
                               guint32            max_fragment_size,
                               guint             *n_fragments)
{
    GArray *array;
    guint32 total_message_length;
    guint32 total_payload_length;
    guint32 fragment_header_length;
    guint32 fragment_payload_length;
    guint32 total_fragments;
    guint i;
    const guint8 *data;
    guint32 data_length;

    /* A message which is longer than the maximum fragment size needs to be
     * split in different fragments before sending it. */

    total_message_length = mbim_message_get_message_length (self);

    /* If a single fragment is enough, don't try to split */
    if (total_message_length <= max_fragment_size)
        return NULL;

    /* Total payload length is the total length minus the headers of the
     * input message */
    fragment_header_length = sizeof (struct header) + sizeof (struct fragment_header);
    total_payload_length = total_message_length - fragment_header_length;

    /* Fragment payload length is the maximum amount of data that can fit in a
     * single fragment */
    fragment_payload_length = max_fragment_size - fragment_header_length;

    /* We can now compute the number of fragments that we'll get */
    total_fragments = total_payload_length / fragment_payload_length;
    if (total_payload_length % fragment_payload_length)
        total_fragments++;

    array = g_array_sized_new (FALSE,
                               FALSE,
                               sizeof (struct fragment_info),
                               total_fragments);

    /* Initialize data walkers */
    data = ((struct full_message *)(((GByteArray *)self)->data))->message.fragment.buffer;
    data_length = total_payload_length;

    /* Create fragment infos */
    for (i = 0; i < total_fragments; i++) {
        struct fragment_info info;

        /* Set data info */
        info.data = data;
        info.data_length = (data_length > fragment_payload_length ? fragment_payload_length : data_length);

        /* Set header info */
        info.header.type             = GUINT32_TO_LE (MBIM_MESSAGE_GET_MESSAGE_TYPE (self));
        info.header.length           = GUINT32_TO_LE (fragment_header_length + info.data_length);
        info.header.transaction_id   = GUINT32_TO_LE (MBIM_MESSAGE_GET_TRANSACTION_ID (self));
        info.fragment_header.total   = GUINT32_TO_LE (total_fragments);
        info.fragment_header.current = GUINT32_TO_LE (i);

        g_array_insert_val (array, i, info);

        /* Update walkers */
        data = &data[info.data_length];
        data_length -= info.data_length;
    }

    g_warn_if_fail (data_length == 0);

    *n_fragments = total_fragments;
    return (struct fragment_info *) g_array_free (array, FALSE);
}

/*****************************************************************************/
/* 'Open' message interface */

/**
 * mbim_message_open_new:
 * @transaction_id: transaction ID.
 * @max_control_transfer: maximum control transfer.
 *
 * Create a new #MbimMessage of type %MBIM_MESSAGE_TYPE_OPEN with the specified
 * parameters.
 *
 * Returns: (transfer full): a newly created #MbimMessage. The returned value
 * should be freed with mbim_message_unref().
 */
MbimMessage *
mbim_message_open_new (guint32 transaction_id,
                       guint32 max_control_transfer)
{
    GByteArray *self;

    self = _mbim_message_allocate (MBIM_MESSAGE_TYPE_OPEN,
                                   transaction_id,
                                   sizeof (struct open_message));

    /* Open header */
    ((struct full_message *)(self->data))->message.open.max_control_transfer = GUINT32_TO_LE (max_control_transfer);

    return (MbimMessage *)self;
}

/**
 * mbim_message_open_get_max_control_transfer:
 * @self: a #MbimMessage.
 *
 * Get the maximum control transfer set to be used in the #MbimMessage of type
 * %MBIM_MESSAGE_TYPE_OPEN.
 *
 * Returns: the maximum control transfer.
 */
guint32
mbim_message_open_get_max_control_transfer (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, 0);
    g_return_val_if_fail (MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_OPEN, 0);

    return GUINT32_FROM_LE (((struct full_message *)(self->data))->message.open.max_control_transfer);
}

/*****************************************************************************/
/* 'Open Done' message interface */

/**
 * mbim_message_open_done_get_status_code:
 * @self: a #MbimMessage.
 *
 * Get status code from the %MBIM_MESSAGE_TYPE_OPEN_DONE message.
 *
 * Returns: a #MbimStatusError.
 */
MbimStatusError
mbim_message_open_done_get_status_code (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, MBIM_STATUS_ERROR_FAILURE);
    g_return_val_if_fail (MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_OPEN_DONE, MBIM_STATUS_ERROR_FAILURE);

    return (MbimStatusError) GUINT32_FROM_LE (((struct full_message *)(self->data))->message.open_done.status_code);
}

/**
 * mbim_message_open_done_get_result:
 * @self: a #MbimMessage.
 *
 * Gets the result of the 'Open' operation in the %MBIM_MESSAGE_TYPE_OPEN_DONE message.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if @error is set.
 */
gboolean
mbim_message_open_done_get_result (const MbimMessage  *self,
                                   GError            **error)
{
    MbimStatusError status;

    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_OPEN_DONE, FALSE);

    status = (MbimStatusError) GUINT32_FROM_LE (((struct full_message *)(self->data))->message.open_done.status_code);
    if (status == MBIM_STATUS_ERROR_NONE)
        return TRUE;

    g_set_error (error,
                 MBIM_STATUS_ERROR,
                 status,
                 mbim_status_error_get_string (status));
    return FALSE;
}

/*****************************************************************************/
/* 'Close' message interface */

/**
 * mbim_message_close_new:
 * @transaction_id: transaction ID.
 *
 * Create a new #MbimMessage of type %MBIM_MESSAGE_TYPE_CLOSE with the specified
 * parameters.
 *
 * Returns: (transfer full): a newly created #MbimMessage. The returned value
 * should be freed with mbim_message_unref().
 */
MbimMessage *
mbim_message_close_new (guint32 transaction_id)
{
    return (MbimMessage *) _mbim_message_allocate (MBIM_MESSAGE_TYPE_CLOSE,
                                                   transaction_id,
                                                   0);
}

/*****************************************************************************/
/* 'Close Done' message interface */

/**
 * mbim_message_close_done_get_status_code:
 * @self: a #MbimMessage.
 *
 * Get status code from the %MBIM_MESSAGE_TYPE_CLOSE_DONE message.
 *
 * Returns: a #MbimStatusError.
 */
MbimStatusError
mbim_message_close_done_get_status_code (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, MBIM_STATUS_ERROR_FAILURE);
    g_return_val_if_fail (MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_CLOSE_DONE, MBIM_STATUS_ERROR_FAILURE);

    return (MbimStatusError) GUINT32_FROM_LE (((struct full_message *)(self->data))->message.close_done.status_code);
}

/**
 * mbim_message_close_done_get_result:
 * @self: a #MbimMessage.
 *
 * Gets the result of the 'Close' operation in the %MBIM_MESSAGE_TYPE_CLOSE_DONE message.
 *
 * Returns: %TRUE if the operation succeeded, %FALSE if @error is set.
 */
gboolean
mbim_message_close_done_get_result (const MbimMessage  *self,
                                    GError            **error)
{
    MbimStatusError status;

    g_return_val_if_fail (self != NULL, FALSE);
    g_return_val_if_fail (MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_CLOSE_DONE, FALSE);

    status = (MbimStatusError) GUINT32_FROM_LE (((struct full_message *)(self->data))->message.close_done.status_code);
    if (status == MBIM_STATUS_ERROR_NONE)
        return TRUE;

    g_set_error (error,
                 MBIM_STATUS_ERROR,
                 status,
                 mbim_status_error_get_string (status));
    return FALSE;
}

/*****************************************************************************/
/* 'Error' message interface */

/**
 * mbim_message_error_new:
 * @transaction_id: transaction ID.
 * @error_status_code: a #MbimProtocolError.
 *
 * Create a new #MbimMessage of type %MBIM_MESSAGE_TYPE_HOST_ERROR with the specified
 * parameters.
 *
 * Returns: (transfer full): a newly created #MbimMessage. The returned value
 * should be freed with mbim_message_unref().
 */
MbimMessage *
mbim_message_error_new (guint32           transaction_id,
                        MbimProtocolError error_status_code)
{
    GByteArray *self;

    self = _mbim_message_allocate (MBIM_MESSAGE_TYPE_HOST_ERROR,
                                   transaction_id,
                                   sizeof (struct open_message));

    /* Open header */
    ((struct full_message *)(self->data))->message.error.error_status_code = GUINT32_TO_LE (error_status_code);

    return (MbimMessage *)self;
}

/**
 * mbim_message_error_get_error_status_code:
 * @self: a #MbimMessage.
 *
 * Get the error code in a %MBIM_MESSAGE_TYPE_HOST_ERROR or
 * %MBIM_MESSAGE_TYPE_FUNCTION_ERROR message.
 *
 * Returns: a #MbimProtocolError.
 */
MbimProtocolError
mbim_message_error_get_error_status_code (const MbimMessage *self)
{
    g_return_val_if_fail (self != NULL, MBIM_PROTOCOL_ERROR_INVALID);
    g_return_val_if_fail ((MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_HOST_ERROR ||
                           MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_FUNCTION_ERROR),
                          MBIM_PROTOCOL_ERROR_INVALID);

    return (MbimProtocolError) GUINT32_FROM_LE (((struct full_message *)(self->data))->message.error.error_status_code);
}

/**
 * mbim_message_error_get_error:
 * @self: a #MbimMessage.
 *
 * Get the error in a %MBIM_MESSAGE_TYPE_HOST_ERROR or
 * %MBIM_MESSAGE_TYPE_FUNCTION_ERROR message.
 *
 * Returns: a newly allocated #GError, which should be freed with g_error_free().
 */
GError *
mbim_message_error_get_error (const MbimMessage *self)
{
    MbimProtocolError error_status_code;


    g_return_val_if_fail (self != NULL, NULL);
    g_return_val_if_fail ((MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_HOST_ERROR ||
                           MBIM_MESSAGE_GET_MESSAGE_TYPE (self) == MBIM_MESSAGE_TYPE_FUNCTION_ERROR),
                          NULL);

    error_status_code = (MbimProtocolError) GUINT32_FROM_LE (((struct full_message *)(self->data))->message.error.error_status_code);

    return g_error_new (MBIM_PROTOCOL_ERROR,
                        error_status_code,
                        "MBIM protocol error: %s",
                        mbim_protocol_error_get_string (error_status_code));
}
