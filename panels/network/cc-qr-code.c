/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-qr-code.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-qr-code"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "cc-qr-code.h"
#include "qrcodegen.c"

/**
 * SECTION: cc-qr_code
 * @title: CcQrCode
 * @short_description: A Simple QR Code wrapper around libqrencode
 * @include: "cc-qr-code.h"
 *
 * Generate a QR image from a given text.
 */

#define BYTES_PER_R8G8B8 3

struct _CcQrCode
{
  GObject          parent_instance;

  gchar           *text;
  GdkTexture      *texture;
  gint             size;
};

G_DEFINE_TYPE (CcQrCode, cc_qr_code, G_TYPE_OBJECT)


static void
cc_qr_code_finalize (GObject *object)
{
  CcQrCode *self = (CcQrCode *)object;

  g_clear_object (&self->texture);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (cc_qr_code_parent_class)->finalize (object);
}

static void
cc_qr_code_class_init (CcQrCodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_qr_code_finalize;
}

static void
cc_qr_code_init (CcQrCode *self)
{
}

CcQrCode *
cc_qr_code_new (void)
{
  return g_object_new (CC_TYPE_QR_CODE, NULL);
}

gboolean
cc_qr_code_set_text (CcQrCode    *self,
                     const gchar *text)
{
  g_return_val_if_fail (CC_IS_QR_CODE (self), FALSE);
  g_return_val_if_fail (!text || *text, FALSE);

  if (g_strcmp0 (text, self->text) == 0)
    return FALSE;

  g_clear_object (&self->texture);
  g_free (self->text);
  self->text = g_strdup (text);

  return TRUE;
}

static void
cc_fill_pixel (GByteArray *array,
               guint8      value,
               int         pixel_size)
{
  guint i;

  for (i = 0; i < pixel_size; i++) {
    g_byte_array_append (array, &value, 1); /* R */
    g_byte_array_append (array, &value, 1); /* G */
    g_byte_array_append (array, &value, 1); /* B */
  }
}

GdkPaintable *
cc_qr_code_get_paintable (CcQrCode *self,
                          gint      size)
{
  uint8_t qr_code[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  uint8_t temp_buf[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  g_autoptr(GBytes) bytes = NULL;
  GByteArray *qr_matrix;
  gint pixel_size, qr_size, total_size;
  gint column, row, i;
  gboolean success = FALSE;

  g_return_val_if_fail (CC_IS_QR_CODE (self), NULL);
  g_return_val_if_fail (size > 0, NULL);

  if (!self->text || !*self->text)
    {
      g_warn_if_reached ();
      cc_qr_code_set_text (self, "invalid text");
    }

  if (self->texture && self->size == size)
    return GDK_PAINTABLE (self->texture);

  self->size  = size;

  success = qrcodegen_encodeText (self->text,
                                  temp_buf,
                                  qr_code,
                                  qrcodegen_Ecc_LOW,
                                  qrcodegen_VERSION_MIN,
                                  qrcodegen_VERSION_MAX,
                                  qrcodegen_Mask_AUTO,
                                  FALSE);

  if (!success)
    return NULL;

  qr_size = qrcodegen_getSize(qr_code);
  pixel_size = MAX (1, size / (qr_size));
  total_size = qr_size * pixel_size;
  qr_matrix = g_byte_array_sized_new (total_size * total_size * pixel_size * BYTES_PER_R8G8B8);

  for (column = 0; column < total_size; column++)
    {
      for (i = 0; i < pixel_size; i++)
        {
          for (row = 0; row < total_size / pixel_size; row++)
            {
              if (qrcodegen_getModule (qr_code, column, row))
                cc_fill_pixel (qr_matrix, 0x00, pixel_size);
              else
                cc_fill_pixel (qr_matrix, 0xff, pixel_size);
            }
        }
    }

  bytes = g_byte_array_free_to_bytes (qr_matrix);

  g_clear_object (&self->texture);
  self->texture = gdk_memory_texture_new (total_size,
                                          total_size,
                                          GDK_MEMORY_R8G8B8,
                                          bytes,
                                          total_size * BYTES_PER_R8G8B8);

  return GDK_PAINTABLE (self->texture);
}
