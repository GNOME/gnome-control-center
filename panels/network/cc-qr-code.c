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

struct _CcQrCode
{
  GObject          parent_instance;

  gchar           *text;
  cairo_surface_t *surface;
  gint             size;
  gint             scale;
};

G_DEFINE_TYPE (CcQrCode, cc_qr_code, G_TYPE_OBJECT)


static void
cc_qr_code_finalize (GObject *object)
{
  CcQrCode *self = (CcQrCode *)object;

  g_clear_pointer (&self->surface, cairo_surface_destroy);
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

  /* Clear cairo surface that is cached */
  g_clear_pointer (&self->surface, cairo_surface_destroy);
  g_free (self->text);
  self->text = g_strdup (text);

  return TRUE;
}

static void
cc_cairo_fill_pixel (cairo_t *cr,
                     int      x,
                     int      y,
                     int      padding,
                     int      scale)
{
  cairo_rectangle (cr,
                   x * scale + padding,
                   y * scale + padding,
                   scale, scale);
  cairo_fill (cr);
}

cairo_surface_t *
cc_qr_code_get_surface (CcQrCode *self,
                        gint      size,
                        gint      scale)
{
  uint8_t qr_code[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  uint8_t temp_buf[qrcodegen_BUFFER_LEN_FOR_VERSION (qrcodegen_VERSION_MAX)];
  cairo_t *cr;
  gint pixel_size, padding, qr_size;
  gboolean success = FALSE;

  g_return_val_if_fail (CC_IS_QR_CODE (self), NULL);
  g_return_val_if_fail (size > 0, NULL);
  g_return_val_if_fail (scale > 0, NULL);

  if (!self->text || !*self->text)
    {
      g_warn_if_reached ();
      cc_qr_code_set_text (self, "invalid text");
    }

  if (self->surface &&
      self->size == size &&
      self->scale == scale)
    return self->surface;

  self->size  = size;
  self->scale = scale;
  g_clear_pointer (&self->surface, cairo_surface_destroy);

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

  self->surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, size * scale, size * scale);
  cairo_surface_set_device_scale (self->surface, scale, scale);
  cr = cairo_create (self->surface);
  cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);

  /* Draw white background */
  cairo_set_source_rgba (cr, 1, 1, 1, 1);
  cairo_rectangle (cr, 0, 0, size * scale, size * scale);
  cairo_fill (cr);

  qr_size = qrcodegen_getSize(qr_code);
  pixel_size = MAX (1, size / (qr_size));
  padding = (size - qr_size * pixel_size) / 2;

  /* If subpixel size is big and margin is pretty small,
   * increase the margin */
  if (pixel_size > 4 && padding < 12)
    {
      pixel_size--;
      padding = (size - qr_size * pixel_size) / 2;
    }

  /* Now draw the black QR code pixels */
  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  for (int row = 0; row < qr_size; row++)
    {
      for (int column = 0; column < qr_size; column++)
        {
          if (qrcodegen_getModule (qr_code, row, column))
            cc_cairo_fill_pixel (cr, column, row, padding, pixel_size);
        }
    }

  cairo_destroy (cr);

  return self->surface;
}
