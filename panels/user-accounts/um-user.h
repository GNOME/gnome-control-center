/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
 * Copyright (C) 2007-2008 William Jon McCann <mccann@jhu.edu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Facade object for user data, owned by UmUserManager
 */

#ifndef __UM_USER__
#define __UM_USER__

#include <sys/types.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "um-account-type.h"

G_BEGIN_DECLS

#define UM_TYPE_USER (um_user_get_type ())
#define UM_USER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), UM_TYPE_USER, UmUser))
#define UM_IS_USER(object) (G_TYPE_CHECK_INSTANCE_TYPE ((object), UM_TYPE_USER))

typedef enum {
        UM_PASSWORD_MODE_REGULAR,
        UM_PASSWORD_MODE_SET_AT_LOGIN,
        UM_PASSWORD_MODE_NONE,
        UM_PASSWORD_MODE_DISABLED,
        UM_PASSWORD_MODE_ENABLED
} UmPasswordMode;

typedef struct _UmUser UmUser;

GType          um_user_get_type            (void) G_GNUC_CONST;

UmUser        *um_user_new_from_object_path (const gchar *path);
const gchar   *um_user_get_object_path      (UmUser *user);

uid_t          um_user_get_uid             (UmUser   *user);
const gchar   *um_user_get_user_name       (UmUser   *user);
const gchar   *um_user_get_real_name       (UmUser   *user);
const gchar   *um_user_get_display_name    (UmUser   *user);
gint           um_user_get_account_type    (UmUser   *user);
const gchar   *um_user_get_email           (UmUser   *user);
const gchar   *um_user_get_language        (UmUser   *user);
const gchar   *um_user_get_location        (UmUser   *user);
const gchar   *um_user_get_home_directory  (UmUser   *user);
const gchar   *um_user_get_shell           (UmUser   *user);
gulong         um_user_get_login_frequency (UmUser   *user);
gint           um_user_get_password_mode   (UmUser   *user);
const gchar   *um_user_get_password_hint   (UmUser   *user);
const gchar   *um_user_get_icon_file       (UmUser   *user);
gboolean       um_user_get_locked          (UmUser   *user);
gboolean       um_user_get_automatic_login (UmUser   *user);
gboolean       um_user_is_system_account   (UmUser   *user);
gboolean       um_user_is_local_account    (UmUser   *user);

void           um_user_set_user_name       (UmUser      *user,
                                            const gchar *user_name);
void           um_user_set_real_name       (UmUser      *user,
                                            const gchar *real_name);
void           um_user_set_email           (UmUser      *user,
                                            const gchar *email);
void           um_user_set_language        (UmUser      *user,
                                            const gchar *language);
void           um_user_set_location        (UmUser      *user,
                                            const gchar *location);
void           um_user_set_icon_file       (UmUser      *user,
                                            const gchar *filename);
void           um_user_set_icon_data       (UmUser      *user,
                                            GdkPixbuf   *pixbuf);
void           um_user_set_account_type    (UmUser      *user,
                                            gint         account_type);
void           um_user_set_automatic_login (UmUser      *user,
                                            gboolean     enabled);
void           um_user_set_password        (UmUser      *user,
                                            int          password_mode,
                                            const gchar *plain,
                                            const gchar *password_hint);
gboolean       um_user_is_logged_in        (UmUser   *user);

GdkPixbuf     *um_user_render_icon         (UmUser   *user,
                                            gboolean  framed,
                                            gint      icon_size);
gint           um_user_collate             (UmUser   *user1,
                                            UmUser   *user2);

void           um_user_show_short_display_name (UmUser *user);
void           um_user_show_full_display_name  (UmUser *user);

G_END_DECLS

#endif
