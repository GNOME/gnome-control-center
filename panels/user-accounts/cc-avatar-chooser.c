/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <stdlib.h>

#include <adwaita.h>
#include <gio/gunixoutputstream.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <act/act.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include "cc-avatar-chooser.h"
#include "cc-crop-area.h"
#include "user-utils.h"

#define ROW_SPAN 5
#define AVATAR_CHOOSER_PIXEL_SIZE 80
#define PIXEL_SIZE 512

struct _CcAvatarChooser {
        GtkPopover parent;

        GtkWidget *transient_for;

        GtkWidget *crop_area;
        GtkWidget *user_flowbox;
        GtkWidget *flowbox;

        GnomeDesktopThumbnailFactory *thumb_factory;
        GListStore *faces;

        ActUser *user;
};

G_DEFINE_TYPE (CcAvatarChooser, cc_avatar_chooser, GTK_TYPE_POPOVER)

static void
crop_dialog_response (CcAvatarChooser *self,
                      gint             response_id,
                      GtkWidget       *dialog)
{
        g_autoptr(GdkPixbuf) pb = NULL;
        g_autoptr(GdkPixbuf) pb2 = NULL;

        if (response_id != GTK_RESPONSE_ACCEPT) {
                self->crop_area = NULL;
                gtk_window_destroy (GTK_WINDOW (dialog));
                return;
        }

        pb = cc_crop_area_create_pixbuf (CC_CROP_AREA (self->crop_area));
        pb2 = gdk_pixbuf_scale_simple (pb, PIXEL_SIZE, PIXEL_SIZE, GDK_INTERP_BILINEAR);

        set_user_icon_data (self->user, pb2);

        self->crop_area = NULL;
        gtk_window_destroy (GTK_WINDOW (dialog));

        gtk_popover_popdown (GTK_POPOVER (self));
}

static void
cc_avatar_chooser_crop (CcAvatarChooser *self,
                        GdkPixbuf       *pixbuf)
{
        GtkWidget *dialog;
        GtkWindow *toplevel;

        toplevel = (GtkWindow *)gtk_widget_get_native (GTK_WIDGET (self->transient_for));
        dialog = gtk_dialog_new_with_buttons ("",
                                              toplevel,
                                              GTK_DIALOG_USE_HEADER_BAR,
                                              _("_Cancel"),
                                              GTK_RESPONSE_CANCEL,
                                              _("Select"),
                                              GTK_RESPONSE_ACCEPT,
                                              NULL);
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-users");

        g_signal_connect_object (G_OBJECT (dialog), "response",
                                 G_CALLBACK (crop_dialog_response), self, G_CONNECT_SWAPPED);

        /* Content */
        self->crop_area = cc_crop_area_new ();
        cc_crop_area_set_min_size (CC_CROP_AREA (self->crop_area), 48, 48);
        cc_crop_area_set_paintable (CC_CROP_AREA (self->crop_area),
                                    GDK_PAINTABLE (gdk_texture_new_for_pixbuf (pixbuf)));
        gtk_box_prepend (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                         self->crop_area);
        gtk_widget_set_hexpand (self->crop_area, TRUE);
        gtk_widget_set_vexpand (self->crop_area, TRUE);

        gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);

}

static void
file_chooser_response (CcAvatarChooser *self,
                       gint             response,
                       GtkDialog       *chooser)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GdkPixbuf) pixbuf2 = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileInputStream) stream = NULL;

        if (response != GTK_RESPONSE_ACCEPT) {
                gtk_window_destroy (GTK_WINDOW (chooser));
                return;
        }

        file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));
        stream = g_file_read (file, NULL, &error);
        pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream),
                                             NULL, &error);
        if (pixbuf == NULL) {
                g_warning ("Failed to load %s: %s", g_file_get_uri (file), error->message);
        }

        pixbuf2 = gdk_pixbuf_apply_embedded_orientation (pixbuf);

        gtk_window_destroy (GTK_WINDOW (chooser));

        cc_avatar_chooser_crop (self, pixbuf2);
}

static void
cc_avatar_chooser_select_file (CcAvatarChooser *self)
{
        g_autoptr(GFile) folder = NULL;
        GtkWidget *chooser;
        GtkFileFilter *filter;
        GtkWindow *toplevel;

        toplevel = (GtkWindow*) gtk_widget_get_native (GTK_WIDGET (self->transient_for));
        chooser = gtk_file_chooser_dialog_new (_("Browse for more pictures"),
                                               toplevel,
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               _("_Cancel"), GTK_RESPONSE_CANCEL,
                                               _("_Open"), GTK_RESPONSE_ACCEPT,
                                               NULL);

        gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);

        folder = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));
        if (folder)
                gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
                                                     folder,
                                                     NULL);

        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (file_chooser_response), self, G_CONNECT_SWAPPED);

        gtk_popover_popdown (GTK_POPOVER (self));
        gtk_window_present (GTK_WINDOW (chooser));
}

static void
face_widget_activated (CcAvatarChooser *self,
                       GtkFlowBoxChild *child)
{
        const gchar *filename;
        GtkWidget   *image;

        image = gtk_flow_box_child_get_child (child);
        filename = g_object_get_data (G_OBJECT (image), "filename");

        act_user_set_icon_file (self->user, filename);

        gtk_popover_popdown (GTK_POPOVER (self));
}

static GtkWidget *
create_face_widget (gpointer item,
                    gpointer user_data)
{
        g_autofree gchar *image_path = NULL;
        g_autoptr(GdkPixbuf) source_pixbuf = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GtkWidget *image;

        image_path = g_file_get_path (G_FILE (item));

        source_pixbuf = gdk_pixbuf_new_from_file_at_size (image_path,
                                                          AVATAR_CHOOSER_PIXEL_SIZE,
                                                          AVATAR_CHOOSER_PIXEL_SIZE,
                                                          NULL);
        if (source_pixbuf == NULL) {
                image = gtk_image_new_from_icon_name ("image-missing");
                gtk_image_set_pixel_size (GTK_IMAGE (image), AVATAR_CHOOSER_PIXEL_SIZE);
                gtk_widget_set_visible (image, TRUE);

                g_object_set_data_full (G_OBJECT (image),
                                        "filename", g_steal_pointer (&image_path), g_free);

                return image;
        }

        pixbuf = round_image (source_pixbuf);
        image = gtk_image_new_from_pixbuf (pixbuf);
        gtk_image_set_pixel_size (GTK_IMAGE (image), AVATAR_CHOOSER_PIXEL_SIZE);
        gtk_widget_set_visible (image, TRUE);

        g_object_set_data_full (G_OBJECT (image),
                                "filename", g_steal_pointer (&image_path), g_free);

        return image;
}

static GStrv
get_settings_facesdirs (void)
{
        g_autoptr(GSettings) settings = g_settings_new ("org.gnome.desktop.interface");
        g_auto(GStrv) settings_dirs = g_settings_get_strv (settings, "avatar-directories");
        GPtrArray *facesdirs = g_ptr_array_new ();

        if (settings_dirs != NULL) {
                int i;
                for (i = 0; settings_dirs[i] != NULL; i++) {
                        char *path = settings_dirs[i];
                        if (g_strcmp0 (path, "") != 0)
                                g_ptr_array_add (facesdirs, g_strdup (path));
                }
        }
        g_ptr_array_add (facesdirs, NULL);

        return (GStrv) g_ptr_array_steal (facesdirs, NULL);
}

static GStrv
get_system_facesdirs (void)
{
        const char * const * data_dirs;
        GPtrArray *facesdirs;
        int i;

        facesdirs = g_ptr_array_new ();

        data_dirs = g_get_system_data_dirs ();
        for (i = 0; data_dirs[i] != NULL; i++) {
                char *path = g_build_filename (data_dirs[i], "pixmaps", "faces", NULL);
                g_ptr_array_add (facesdirs, path);
        }
        g_ptr_array_add (facesdirs, NULL);
        return (GStrv) g_ptr_array_steal (facesdirs, NULL);
}

static gboolean
add_faces_from_dirs (GListStore *faces, GStrv facesdirs, gboolean add_all)
{
        GFile *file;
        GFileType type;
        const gchar *target;
        guint i;
        gboolean added_faces = FALSE;

        for (i = 0; facesdirs[i] != NULL; i++) {
                g_autoptr(GFile) dir = NULL;
                g_autoptr(GFileEnumerator) enumerator = NULL;

                dir = g_file_new_for_path (facesdirs[i]);

                enumerator = g_file_enumerate_children (dir,
                                                        G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                        G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                        G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
                                                        G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                                        G_FILE_QUERY_INFO_NONE,
                                                        NULL, NULL);
                if (enumerator == NULL) {
                        continue;
                }

                while (TRUE) {
                        g_autoptr(GFileInfo) info = g_file_enumerator_next_file (enumerator, NULL, NULL);
                        if (info == NULL) {
                                break;
                        }

                        type = g_file_info_get_file_type (info);
                        if (type != G_FILE_TYPE_REGULAR &&
                            type != G_FILE_TYPE_SYMBOLIC_LINK) {
                                continue;
                        }

                        target = g_file_info_get_symlink_target (info);
                        if (target != NULL && g_str_has_prefix (target , "legacy/")) {
                                continue;
                        }

                        file = g_file_get_child (dir, g_file_info_get_name (info));
                        g_list_store_append (faces, file);

                        added_faces = TRUE;
                }

                g_file_enumerator_close (enumerator, NULL, NULL);

                if (added_faces && !add_all)
                        break;
        }
        return added_faces;
}


static void
setup_photo_popup (CcAvatarChooser *self)
{
        g_auto(GStrv) settings_facesdirs = NULL;

        self->faces = g_list_store_new (G_TYPE_FILE);
        gtk_flow_box_bind_model (GTK_FLOW_BOX (self->flowbox),
                                 G_LIST_MODEL (self->faces),
                                 create_face_widget,
                                 self,
                                 NULL);

        g_signal_connect_object (self->flowbox, "child-activated",
                                 G_CALLBACK (face_widget_activated), self, G_CONNECT_SWAPPED);

        settings_facesdirs = get_settings_facesdirs ();

        if (!add_faces_from_dirs (self->faces, settings_facesdirs, TRUE)) {
                g_auto(GStrv) system_facesdirs = get_system_facesdirs ();
                add_faces_from_dirs (self->faces, system_facesdirs, FALSE);
        }
}

CcAvatarChooser *
cc_avatar_chooser_new (GtkWidget *transient_for)
{
        CcAvatarChooser *self;

        self = g_object_new (CC_TYPE_AVATAR_CHOOSER,
                             NULL);
        self->transient_for = transient_for;
        self->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

        setup_photo_popup (self);

        return self;
}

static void
cc_avatar_chooser_dispose (GObject *object)
{
        CcAvatarChooser *self = CC_AVATAR_CHOOSER (object);

        g_clear_object (&self->thumb_factory);
        g_clear_object (&self->user);

        G_OBJECT_CLASS (cc_avatar_chooser_parent_class)->dispose (object);
}

static void
cc_avatar_chooser_init (CcAvatarChooser *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_avatar_chooser_class_init (CcAvatarChooserClass *klass)
{
        GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
        GObjectClass *oclass = G_OBJECT_CLASS (klass);

        gtk_widget_class_set_template_from_resource (wclass, "/org/gnome/control-center/user-accounts/cc-avatar-chooser.ui");

        gtk_widget_class_bind_template_child (wclass, CcAvatarChooser, user_flowbox);
        gtk_widget_class_bind_template_child (wclass, CcAvatarChooser, flowbox);

        gtk_widget_class_bind_template_callback (wclass, cc_avatar_chooser_select_file);

        oclass->dispose = cc_avatar_chooser_dispose;
}

static void
user_flowbox_activated (CcAvatarChooser *self)
{
        set_default_avatar (self->user);

        gtk_popover_popdown (GTK_POPOVER (self));
}

void
cc_avatar_chooser_set_user (CcAvatarChooser *self,
                            ActUser         *user)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        const gchar *name;
        GtkWidget *avatar;

        g_return_if_fail (self != NULL);

        if (self->user) {
                GtkWidget *child;

                child = gtk_widget_get_first_child (GTK_WIDGET (self->user_flowbox));
                while (child) {
                        GtkWidget *next = gtk_widget_get_next_sibling (child);

                        if (GTK_FLOW_BOX_CHILD (child))
                                gtk_flow_box_remove (GTK_FLOW_BOX (self->user_flowbox), child);

                        child = next;
                }

                g_clear_object (&self->user);
        }
        self->user = g_object_ref (user);

        name = act_user_get_real_name (user);
        if (name == NULL)
                name = act_user_get_user_name (user);
        avatar = adw_avatar_new (AVATAR_CHOOSER_PIXEL_SIZE, name, TRUE);
        gtk_flow_box_append (GTK_FLOW_BOX (self->user_flowbox), avatar);

        g_signal_connect_object (self->user_flowbox, "child-activated", G_CALLBACK (user_flowbox_activated), self, G_CONNECT_SWAPPED);
}

