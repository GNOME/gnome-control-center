/* -*- mode: c; style: linux -*-
 * 
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Ondrej Holy <oholy@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CC_MOUSE_TEST_H
#define _CC_MOUSE_TEST_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_MOUSE_TEST cc_mouse_test_get_type ()

#define CC_MOUSE_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_MOUSE_TEST, CcMouseTest))
#define CC_MOUSE_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_MOUSE_TEST, CcMouseTestClass))
#define CC_IS_MOUSE_TEST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_MOUSE_TEST))
#define CC_IS_MOUSE_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_MOUSE_TEST))
#define CC_MOUSE_TEST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_MOUSE_TEST, CcMouseTestClass))

typedef struct _CcMouseTest CcMouseTest;
typedef struct _CcMouseTestClass CcMouseTestClass;
typedef struct _CcMouseTestPrivate CcMouseTestPrivate;

struct _CcMouseTest
{
  GtkAlignment parent;

  CcMouseTestPrivate *priv;
};

struct _CcMouseTestClass
{
  GtkAlignmentClass parent_class;
};

GType cc_mouse_test_get_type (void) G_GNUC_CONST;
GtkWidget *cc_mouse_test_new (void);

G_END_DECLS

#endif /* _CC_MOUSE_TEST_H */
