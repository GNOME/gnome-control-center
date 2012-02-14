/*
 * Copyright © 2010 Red Hat, Inc.
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * Written by:
 *      Matthias Clasen <mclasen@redhat.com>
 *      Richard Hughes <richard@hughsie.com>
 */

#ifndef _CC_STRENGTH_BAR_H_
#define _CC_STRENGTH_BAR_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_STRENGTH_BAR            (cc_strength_bar_get_type ())
#define CC_STRENGTH_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_STRENGTH_BAR, \
                                                                           CcStrengthBar))
#define CC_STRENGTH_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_STRENGTH_BAR, \
                                                                        CcStrengthBarClass))
#define UM_IS_STRENGTH_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_STRENGTH_BAR))
#define UM_IS_STRENGTH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_STRENGTH_BAR))
#define CC_STRENGTH_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_STRENGTH_BAR, \
                                                                          CcStrengthBarClass))

typedef struct _CcStrengthBarClass CcStrengthBarClass;
typedef struct _CcStrengthBar CcStrengthBar;
typedef struct _CcStrengthBarPrivate CcStrengthBarPrivate;

struct _CcStrengthBarClass {
        GtkWidgetClass parent_class;
};

struct _CcStrengthBar {
        GtkWidget parent_instance;
        CcStrengthBarPrivate *priv;
};

GType      cc_strength_bar_get_type             (void) G_GNUC_CONST;

GtkWidget *cc_strength_bar_new                  (void);

void       cc_strength_bar_set_fraction         (CcStrengthBar *bar,
                                                 gdouble        fraction);
gdouble    cc_strength_bar_get_fraction         (CcStrengthBar *bar);

void       cc_strength_bar_set_segments         (CcStrengthBar *bar,
                                                 gint          segments);
gint       cc_strength_bar_get_segments         (CcStrengthBar *bar);

G_END_DECLS

#endif /* _CC_STRENGTH_BAR_H_ */
