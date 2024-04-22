/*
 * Copyright 2024 GNOME Foundation Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <adwaita.h>
#include <glib.h>

#include "cc-cannot-add-user-dialog.h"

struct _CcCannotAddUserDialog {
    AdwDialog parent_instance;
};

G_DEFINE_TYPE (CcCannotAddUserDialog, cc_cannot_add_user_dialog, ADW_TYPE_DIALOG);

static void
cc_cannot_add_user_dialog_init (CcCannotAddUserDialog *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_cannot_add_user_dialog_class_init (CcCannotAddUserDialogClass *klass)
{
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/control-center/system/users/cc-cannot-add-user-dialog.ui");
}

CcCannotAddUserDialog *
cc_cannot_add_user_dialog_new (void)
{
    return g_object_new (CC_TYPE_CANNOT_ADD_USER_DIALOG, NULL);
}
