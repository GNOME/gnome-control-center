/*
 * Copyright © 2009 Bastien Nocera <hadess@hadess.net>
 *
 * Licensed under the GNU General Public License Version 2
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CC_CROP_AREA_H_
#define _CC_CROP_AREA_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_CROP_AREA (cc_crop_area_get_type ())
#define CC_CROP_AREA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_CROP_AREA, \
                                                                           CcCropArea))
#define CC_CROP_AREA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_CROP_AREA, \
                                                                        CcCropAreaClass))
#define CC_IS_CROP_AREA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_CROP_AREA))
#define CC_IS_CROP_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_CROP_AREA))
#define CC_CROP_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_CROP_AREA, \
                                                                          CcCropAreaClass))

typedef struct _CcCropAreaClass CcCropAreaClass;
typedef struct _CcCropArea CcCropArea;
typedef struct _CcCropAreaPrivate CcCropAreaPrivate;

struct _CcCropAreaClass {
        GtkDrawingAreaClass parent_class;
};

struct _CcCropArea {
        GtkDrawingArea parent_instance;
        CcCropAreaPrivate *priv;
};

GType      cc_crop_area_get_type             (void) G_GNUC_CONST;

GtkWidget *cc_crop_area_new                  (void);
GdkPixbuf *cc_crop_area_get_picture          (CcCropArea *area);
void       cc_crop_area_set_picture          (CcCropArea *area,
                                              GdkPixbuf  *pixbuf);
void       cc_crop_area_set_min_size         (CcCropArea *area,
                                              gint        width,
                                              gint        height);
void       cc_crop_area_set_constrain_aspect (CcCropArea *area,
                                              gboolean    constrain);

G_END_DECLS

#endif /* _CC_CROP_AREA_H_ */
