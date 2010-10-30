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
 * Author: Matthias Clasen
 */

#ifndef _UM_STRENGTH_BAR_H_
#define _UM_STRENGTH_BAR_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_STRENGTH_BAR            (um_strength_bar_get_type ())
#define UM_STRENGTH_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_STRENGTH_BAR, \
                                                                           UmStrengthBar))
#define UM_STRENGTH_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_STRENGTH_BAR, \
                                                                        UmStrengthBarClass))
#define UM_IS_STRENGTH_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_STRENGTH_BAR))
#define UM_IS_STRENGTH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_STRENGTH_BAR))
#define UM_STRENGTH_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_STRENGTH_BAR, \
                                                                          UmStrengthBarClass))

typedef struct _UmStrengthBarClass UmStrengthBarClass;
typedef struct _UmStrengthBar UmStrengthBar;
typedef struct _UmStrengthBarPrivate UmStrengthBarPrivate;

struct _UmStrengthBarClass {
        GtkWidgetClass parent_class;
};

struct _UmStrengthBar {
        GtkWidget parent_instance;
        UmStrengthBarPrivate *priv;
};

GType      um_strength_bar_get_type             (void) G_GNUC_CONST;

GtkWidget *um_strength_bar_new                  (void);
void       um_strength_bar_set_strength         (UmStrengthBar *bar,
                                                 gdouble        strength);
gdouble    um_strength_bar_get_strength         (UmStrengthBar *bar);

G_END_DECLS

#endif /* _UM_STRENGTH_BAR_H_ */
