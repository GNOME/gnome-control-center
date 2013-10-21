/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2013 Red Hat, Inc,
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by:
 *   Jasper St. Pierre <jstpierre@mecheye.net>
 *   Bogdan Ciobanu <bgdn.ciobanu@gmail.com>
 */

#define IMAGE_SIZE 96

#include "config.h"
#include "um-avatar-picker.h"

#include <glib/gi18n.h>
#include "online-avatars.h"

#ifdef HAVE_CHEESE
#include <cheese/cheese-avatar-widget.h>
#endif /* HAVE_CHEESE */

struct _UmAvatarPickerPrivate
{
    GtkWidget *stack;
    GtkWidget *select_button;
    GtkWidget *page_avatars;

    GtkWidget *avatars_online;
    GtkWidget *avatars_stock;

    GtkWidget *avatar_selected_widget;

    gboolean clearing_selection;

#ifdef HAVE_CHEESE
    GtkWidget *camera_widget;
#endif /* HAVE_CHEESE */
};
typedef struct _UmAvatarPickerPrivate UmAvatarPickerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (UmAvatarPicker, um_avatar_picker, GTK_TYPE_DIALOG);

static void
sync_select_sensivitity (UmAvatarPicker *picker)
{
    UmAvatarPickerPrivate *priv = um_avatar_picker_get_instance_private (picker);
    GtkWidget *current_page;
    gboolean has_selection;

    current_page = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
    if (current_page == priv->page_avatars) {
        has_selection = (priv->avatar_selected_widget != NULL);
    }
#ifdef HAVE_CHEESE
    else if (current_page == priv->camera_widget) {
        has_selection = (cheese_avatar_widget_get_picture (CHEESE_AVATAR_WIDGET (priv->camera_widget)) != NULL);
    }
#endif /* HAVE_CHEESE */
    else
        g_assert_not_reached ();

    gtk_widget_set_sensitive (priv->select_button, has_selection);
}

/* Avatars page */

static void
image_set_from_bytes (GtkImage *image,
                      GBytes   *bytes)
{
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *scaled_pixbuf = NULL;
    GError *error = NULL;

    if (!bytes) {
        GtkWidget *flow_box_child = gtk_widget_get_parent (GTK_WIDGET (image));
        gtk_widget_destroy (flow_box_child);
        return;
    }

    loader = gdk_pixbuf_loader_new ();
    if (!gdk_pixbuf_loader_write_bytes (loader, bytes, &error)) {
        g_warning ("Could not load image: %s\n", error->message);
        goto out;
    }

    if (!gdk_pixbuf_loader_close (loader, NULL)) {
        g_warning ("Could not load image: %s\n", error->message);
        goto out;
    }

    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    if (!pixbuf) {
        g_warning ("Could not load image\n");
        goto out;
    }

    scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, IMAGE_SIZE, IMAGE_SIZE,
                                             GDK_INTERP_BILINEAR);
    gtk_image_set_from_pixbuf (image, scaled_pixbuf);
    gtk_widget_show (GTK_WIDGET (image));

 out:
    g_clear_error (&error);
    g_object_unref (loader);
    if (scaled_pixbuf)
        g_object_unref (scaled_pixbuf);
}

static void
got_avatar_cb (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
    GtkImage *image = GTK_IMAGE (user_data);
    GBytes *bytes;
    bytes = get_avatar_from_online_account_finish (GOA_ACCOUNT (source_object), result, NULL);
    image_set_from_bytes (image, bytes);
}

static void
got_gravatar_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
    GtkImage *image = GTK_IMAGE (user_data);
    GBytes *bytes;
    bytes = get_gravatar_from_email_finish (result, NULL);
    image_set_from_bytes (image, bytes);
}

static void
fill_online_icons (UmAvatarPicker *picker)
{
    UmAvatarPickerPrivate *priv = um_avatar_picker_get_instance_private (picker);
    GHashTable *used_emails;
    GoaClient *client;
    GList *accounts, *l;

    client = goa_client_new_sync (NULL, NULL);
    if (!client)
        return;

    used_emails = g_hash_table_new (g_str_hash, g_str_equal);
    accounts = goa_client_get_accounts (client);

    for (l = accounts; l; l = l->next) {
        GoaObject *object = GOA_OBJECT (l->data);
        GoaAccount *account = goa_object_peek_account (object);
        GoaMail *mail = goa_object_peek_mail (object);
        GtkWidget *image;

        image = gtk_image_new ();
        gtk_widget_set_size_request (image, IMAGE_SIZE, IMAGE_SIZE);
        get_avatar_from_online_account (account, NULL, got_avatar_cb, image);
        gtk_container_add (GTK_CONTAINER (priv->avatars_online), image);

        if (mail) {
            const char *email = goa_mail_get_email_address (mail);
            if (!g_hash_table_contains (used_emails, email)) {
                image = gtk_image_new ();
                gtk_widget_set_size_request (image, IMAGE_SIZE, IMAGE_SIZE);
                get_gravatar_from_email (email, NULL, got_gravatar_cb, image);
                gtk_container_add (GTK_CONTAINER (priv->avatars_online), image);

                g_hash_table_add (used_emails, (gpointer) email);
            }
        }
    }

    g_list_free_full (accounts, (GDestroyNotify) g_object_unref);
    g_hash_table_destroy (used_emails);
}

static void
fill_stock_icons_from_dir (UmAvatarPicker *picker,
                           char           *path,
                           GDir           *dir)
{
    UmAvatarPickerPrivate *priv = um_avatar_picker_get_instance_private (picker);
    const char *face;
    GtkWidget *image;

    while ((face = g_dir_read_name (dir)) != NULL) {
        char *filename = g_build_filename (path, face, NULL);

        image = gtk_image_new_from_file (filename);
        gtk_widget_set_size_request (image, IMAGE_SIZE, IMAGE_SIZE);
        gtk_widget_show (image);

        gtk_container_add (GTK_CONTAINER (priv->avatars_stock), image);

        g_free (filename);
    }
}

static void
fill_stock_icons (UmAvatarPicker *picker)
{
    const char * const * dirs = g_get_system_data_dirs ();
    int i;

    for (i = 0; dirs[i] != NULL; i++) {
        char *path = g_build_filename (dirs[i], "pixmaps", "faces", NULL);
        GDir *dir = g_dir_open (path, 0, NULL);

        if (!dir)
            goto next;

        fill_stock_icons_from_dir (picker, path, dir);

    next:
        g_free (path);
        if (dir)
            g_dir_close (dir);
    }
}

static void
avatars_selection_changed (GtkFlowBox *box,
                           gpointer    user_data)
{
    UmAvatarPicker *picker = user_data;
    UmAvatarPickerPrivate *priv = um_avatar_picker_get_instance_private (picker);
    GtkFlowBox *boxen[3] = { (GtkFlowBox *) priv->avatars_stock, (GtkFlowBox *) priv->avatars_online, NULL };
    GtkFlowBox **widget;
    GList *l;

    if (priv->clearing_selection)
        return;

    /* The flow box is in single selection mode, so I know this
     * list will always contain either one item or NULL. */
    l = gtk_flow_box_get_selected_children (box);
    priv->avatar_selected_widget = l ? GTK_WIDGET (l->data) : NULL;
    g_list_free (l);

    priv->clearing_selection = TRUE;
    for (widget = boxen; *widget != NULL; widget++) {
        if (*widget == box)
            continue;

        gtk_flow_box_unselect_all (GTK_FLOW_BOX (*widget));
    }
    priv->clearing_selection = FALSE;

    sync_select_sensivitity (picker);
}

#ifdef HAVE_CHEESE
/* Camera page */

static void
camera_pixbuf_changed (GObject    *object,
                       GParamSpec *pspec,
                       gpointer    user_data)
{
    UmAvatarPicker *picker = UM_AVATAR_PICKER (user_data);
    sync_select_sensivitity (picker);
}
#endif

static void
um_avatar_picker_class_init (UmAvatarPickerClass *klass)
{
    gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/control-center/user-accounts/avatar-picker.ui");

    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), UmAvatarPicker, stack);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), UmAvatarPicker, select_button);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), UmAvatarPicker, page_avatars);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), UmAvatarPicker, avatars_online);
    gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), UmAvatarPicker, avatars_stock);
}

static void
um_avatar_picker_init (UmAvatarPicker *picker)
{
    UmAvatarPickerPrivate *priv = um_avatar_picker_get_instance_private (picker);

    gtk_window_set_modal (GTK_WINDOW (picker), TRUE);

    gtk_widget_init_template (GTK_WIDGET (picker));

    fill_stock_icons (picker);
    fill_online_icons (picker);

    g_signal_connect (priv->avatars_online, "selected-children-changed",
                      G_CALLBACK (avatars_selection_changed), picker);
    g_signal_connect (priv->avatars_stock, "selected-children-changed",
                      G_CALLBACK (avatars_selection_changed), picker);

#ifdef HAVE_CHEESE
    priv->camera_widget = cheese_avatar_widget_new ();
    gtk_container_add_with_properties (GTK_CONTAINER (priv->stack), priv->camera_widget,
                                       "name", "camera",
                                       "title", _("Camera"),
                                       NULL);
    g_signal_connect (priv->camera_widget, "notify::pixbuf",
                      G_CALLBACK (camera_pixbuf_changed), picker);
    gtk_widget_show (priv->camera_widget);
#endif /* HAVE_CHEESE */

    sync_select_sensivitity (picker);
}

GdkPixbuf *
um_avatar_picker_get_avatar (UmAvatarPicker *picker)
{
    UmAvatarPickerPrivate *priv = um_avatar_picker_get_instance_private (picker);
    GtkWidget *current_page;

    current_page = gtk_stack_get_visible_child (GTK_STACK (priv->stack));
    if (current_page == priv->page_avatars) {
        if (priv->avatar_selected_widget != NULL)
            return gtk_image_get_pixbuf (GTK_IMAGE (priv->avatar_selected_widget));
    }
#ifdef HAVE_CHEESE
    else if (current_page == priv->camera_widget) {
        return cheese_avatar_widget_get_picture (CHEESE_AVATAR_WIDGET (priv->camera_widget));
    }
#endif /* HAVE_CHEESE */
    else
        g_assert_not_reached ();

    return NULL;
}

GtkWidget *
um_avatar_picker_new (void)
{
    return g_object_new (UM_TYPE_AVATAR_PICKER, NULL);
}
