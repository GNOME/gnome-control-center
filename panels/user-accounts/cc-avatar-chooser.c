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

#include <gio/gunixoutputstream.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <act/act.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#ifdef HAVE_CHEESE
#include <cheese-avatar-chooser.h>
#include <cheese-camera-device.h>
#include <cheese-camera-device-monitor.h>
#endif /* HAVE_CHEESE */

#include "cc-avatar-chooser.h"
#include "cc-crop-area.h"
#include "user-utils.h"

#define ROW_SPAN 5
#define AVATAR_CHOOSER_PIXEL_SIZE 80
#define PIXEL_SIZE 512

struct _CcAvatarChooser {
        GtkPopover parent;

        GtkWidget *popup_button;
        GtkWidget *crop_area;
        GtkWidget *user_flowbox;
        GtkWidget *flowbox;
        GtkWidget *take_picture_button;

#ifdef HAVE_CHEESE
        CheeseCameraDeviceMonitor *monitor;
        GCancellable *cancellable;
        guint num_cameras;
#endif /* HAVE_CHEESE */

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
                gtk_widget_destroy (dialog);
                return;
        }

        pb = cc_crop_area_get_picture (CC_CROP_AREA (self->crop_area));
        pb2 = gdk_pixbuf_scale_simple (pb, PIXEL_SIZE, PIXEL_SIZE, GDK_INTERP_BILINEAR);

        set_user_icon_data (self->user, pb2);

        self->crop_area = NULL;
        gtk_widget_destroy (dialog);

        gtk_popover_popdown (GTK_POPOVER (self));
}

static void
cc_avatar_chooser_crop (CcAvatarChooser *self,
                        GdkPixbuf       *pixbuf)
{
        GtkWidget *dialog;

        dialog = gtk_dialog_new_with_buttons ("",
                                              GTK_WINDOW (gtk_widget_get_toplevel (self->popup_button)),
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
        gtk_widget_show (self->crop_area);
        cc_crop_area_set_min_size (CC_CROP_AREA (self->crop_area), 48, 48);
        cc_crop_area_set_constrain_aspect (CC_CROP_AREA (self->crop_area), TRUE);
        cc_crop_area_set_picture (CC_CROP_AREA (self->crop_area), pixbuf);
        gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                            self->crop_area,
                            TRUE, TRUE, 8);

        gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 300);

        gtk_widget_show (dialog);
}

static void
file_chooser_response (CcAvatarChooser *self,
                       gint             response,
                       GtkDialog       *chooser)
{
        g_autofree gchar *fileuri = NULL;
        g_autoptr(GError) error = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        g_autoptr(GdkPixbuf) pixbuf2 = NULL;
        g_autoptr(GFile) file = NULL;
        g_autoptr(GFileInputStream) stream = NULL;

        if (response != GTK_RESPONSE_ACCEPT) {
                gtk_widget_destroy (GTK_WIDGET (chooser));
                return;
        }

        fileuri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
        file = g_file_new_for_uri (fileuri);

        stream = g_file_read (file, NULL, &error);
        pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream),
                                             NULL, &error);
        if (pixbuf == NULL) {
                g_warning ("Failed to load %s: %s", fileuri, error->message);
        }

        pixbuf2 = gdk_pixbuf_apply_embedded_orientation (pixbuf);

        gtk_widget_destroy (GTK_WIDGET (chooser));

        cc_avatar_chooser_crop (self, pixbuf2);
}

static void
update_preview (GtkFileChooser               *chooser,
                GnomeDesktopThumbnailFactory *thumb_factory)
{
        g_autofree gchar *uri = NULL;

        uri = gtk_file_chooser_get_uri (chooser);

        if (uri) {
                g_autoptr(GdkPixbuf) pixbuf = NULL;
                g_autofree char *mime_type = NULL;
                g_autoptr(GFile) file = NULL;
                g_autoptr(GFileInfo) file_info = NULL;
                GtkWidget *preview;

                preview = gtk_file_chooser_get_preview_widget (chooser);

                file = g_file_new_for_uri (uri);
                file_info = g_file_query_info (file,
                                               "standard::*",
                                               G_FILE_QUERY_INFO_NONE,
                                               NULL, NULL);

                if (file_info != NULL &&
                    g_file_info_get_file_type (file_info) != G_FILE_TYPE_DIRECTORY) {
                        mime_type = g_strdup (g_file_info_get_content_type (file_info));
                }

                if (mime_type) {
                        pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumb_factory,
                                                                                     uri,
                                                                                     mime_type);
                }

                gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
                                                   GTK_RESPONSE_ACCEPT,
                                                   (pixbuf != NULL));

                if (pixbuf != NULL) {
                        gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
                }
                else {
                        gtk_image_set_from_icon_name (GTK_IMAGE (preview),
                                                      "dialog-question",
                                                      GTK_ICON_SIZE_DIALOG);
                }
        }

        gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static void
cc_avatar_chooser_select_file (CcAvatarChooser *self)
{
        GtkWidget *chooser;
        const gchar *folder;
        GtkWidget *preview;
        GtkFileFilter *filter;

        chooser = gtk_file_chooser_dialog_new (_("Browse for more pictures"),
                                               GTK_WINDOW (gtk_widget_get_toplevel (self->popup_button)),
                                               GTK_FILE_CHOOSER_ACTION_OPEN,
                                               _("_Cancel"), GTK_RESPONSE_CANCEL,
                                               _("_Open"), GTK_RESPONSE_ACCEPT,
                                               NULL);

        gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);

        preview = gtk_image_new ();
        gtk_widget_set_size_request (preview, 128, -1);
        gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview);
        gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (chooser), FALSE);
        gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
        gtk_widget_show (preview);

        /* Preview has to be generated after default handler of "selection-changed"
         * signal, otherwise dialog response sensitivity is rewritten (Bug 547988).
         * Preview also has to be generated on "selection-changed" signal to reflect
         * all changes (Bug 660877). */
        g_signal_connect_after (chooser, "selection-changed",
                                G_CALLBACK (update_preview), self->thumb_factory);

        folder = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
        if (folder)
                gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
                                                     folder);

        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

        g_signal_connect_object (chooser, "response",
                                 G_CALLBACK (file_chooser_response), self, G_CONNECT_SWAPPED);

        gtk_window_present (GTK_WINDOW (chooser));
}

#ifdef HAVE_CHEESE
static gboolean
destroy_chooser (GtkWidget *chooser)
{
        gtk_widget_destroy (chooser);
        return FALSE;
}

static void
webcam_response_cb (CcAvatarChooser  *self,
                    int               response,
                    GtkDialog        *dialog)
{
        if (response == GTK_RESPONSE_ACCEPT) {
                g_autoptr(GdkPixbuf) pb = NULL;
                g_autoptr(GdkPixbuf) pb2 = NULL;

                g_object_get (G_OBJECT (dialog), "pixbuf", &pb, NULL);
                pb2 = gdk_pixbuf_scale_simple (pb, PIXEL_SIZE, PIXEL_SIZE, GDK_INTERP_BILINEAR);

                set_user_icon_data (self->user, pb2);
        }
        if (response != GTK_RESPONSE_DELETE_EVENT &&
            response != GTK_RESPONSE_NONE)
                g_idle_add ((GSourceFunc) destroy_chooser, dialog);

        gtk_popover_popdown (GTK_POPOVER (self));
}

static void
webcam_icon_selected (CcAvatarChooser *self)
{
        GtkWidget *window;

        window = cheese_avatar_chooser_new ();
        gtk_window_set_transient_for (GTK_WINDOW (window),
                                      GTK_WINDOW (gtk_widget_get_toplevel (self->popup_button)));
        gtk_window_set_modal (GTK_WINDOW (window), TRUE);
        g_signal_connect_object (G_OBJECT (window), "response",
                                 G_CALLBACK (webcam_response_cb), self, G_CONNECT_SWAPPED);
        gtk_widget_show (window);
}

static void
update_photo_menu_status (CcAvatarChooser *self)
{
        if (self->num_cameras == 0)
                gtk_widget_set_visible (self->take_picture_button, FALSE);
        else
                gtk_widget_set_sensitive (self->take_picture_button, TRUE);
}

static void
device_added (CcAvatarChooser *self)
{
        self->num_cameras++;
        update_photo_menu_status (self);
}

static void
device_removed (CcAvatarChooser *self)
{
        self->num_cameras--;
        update_photo_menu_status (self);
}

#endif /* HAVE_CHEESE */

static void
face_widget_activated (CcAvatarChooser *self,
                       GtkFlowBoxChild *child)
{
        const gchar *filename;
        GtkWidget   *image;

        image = gtk_bin_get_child (GTK_BIN (child));
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
        if (source_pixbuf == NULL)
                return NULL;

        pixbuf = round_image (source_pixbuf);
        image = gtk_image_new_from_pixbuf (pixbuf);
        gtk_image_set_pixel_size (GTK_IMAGE (image), AVATAR_CHOOSER_PIXEL_SIZE);
        gtk_widget_show (image);

        g_object_set_data_full (G_OBJECT (image),
                                "filename", g_steal_pointer (&image_path), g_free);

        return image;
}

#ifdef HAVE_CHEESE
static void
setup_cheese_camera_device_monitor (CcAvatarChooser *self)
{
        g_signal_connect_object (G_OBJECT (self->monitor), "added", G_CALLBACK (device_added), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (G_OBJECT (self->monitor), "removed", G_CALLBACK (device_removed), self, G_CONNECT_SWAPPED);
        cheese_camera_device_monitor_coldplug (self->monitor);
        update_photo_menu_status (self);
}

static void
cheese_camera_device_monitor_new_cb (GObject *source,
                                     GAsyncResult *result,
                                     gpointer user_data)
{
        CcAvatarChooser *self = user_data;
        GObject *ret;

        ret = g_async_initable_new_finish (G_ASYNC_INITABLE (source), result, NULL);
        if (ret == NULL)
                return;

        self->monitor = CHEESE_CAMERA_DEVICE_MONITOR (ret);
        setup_cheese_camera_device_monitor (self);
}
#endif /* HAVE_CHEESE */

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

#ifdef HAVE_CHEESE
        gtk_widget_set_visible (self->take_picture_button, TRUE);

        self->cancellable = g_cancellable_new ();
        g_async_initable_new_async (CHEESE_TYPE_CAMERA_DEVICE_MONITOR,
                                    G_PRIORITY_DEFAULT,
                                    self->cancellable,
                                    cheese_camera_device_monitor_new_cb,
                                    self,
                                    NULL);
#endif /* HAVE_CHEESE */
}

static void
popup_icon_menu (CcAvatarChooser *self)
{
        gtk_popover_popup (GTK_POPOVER (self));
}

static gboolean
on_popup_button_button_pressed (CcAvatarChooser *self,
                                GdkEventButton  *event)
{
        if (event->button == 1) {
                if (!gtk_widget_get_visible (GTK_WIDGET (self))) {
                        popup_icon_menu (self);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->popup_button), TRUE);
                } else {
                        gtk_popover_popdown (GTK_POPOVER (self));
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->popup_button), FALSE);
                }

                return TRUE;
        }

        return FALSE;
}

CcAvatarChooser *
cc_avatar_chooser_new (GtkWidget *button)
{
        CcAvatarChooser *self;

        self = g_object_new (CC_TYPE_AVATAR_CHOOSER,
                             "relative-to", button,
                             NULL);

        self->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

        /* Set up the popup */
        self->popup_button = button;
        setup_photo_popup (self);
        g_signal_connect_object (button, "toggled",
                                 G_CALLBACK (popup_icon_menu), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (button, "button-press-event",
                                 G_CALLBACK (on_popup_button_button_pressed), self, G_CONNECT_SWAPPED);

        return self;
}

static void
cc_avatar_chooser_dispose (GObject *object)
{
        CcAvatarChooser *self = CC_AVATAR_CHOOSER (object);

        g_clear_object (&self->thumb_factory);
#ifdef HAVE_CHEESE
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->monitor);
#endif
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
        gtk_widget_class_bind_template_child (wclass, CcAvatarChooser, take_picture_button);

        gtk_widget_class_bind_template_callback (wclass, cc_avatar_chooser_select_file);
#ifdef HAVE_CHEESE
        gtk_widget_class_bind_template_callback (wclass, webcam_icon_selected);
#endif

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
        g_autoptr(GdkPixbuf) source_pixbuf = NULL;
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        GtkWidget *image;

        g_return_if_fail (self != NULL);

        if (self->user) {
                gtk_container_foreach (GTK_CONTAINER (self->user_flowbox), (GtkCallback) gtk_widget_destroy, NULL);
                g_clear_object (&self->user);
        }
        self->user = g_object_ref (user);

        source_pixbuf = generate_default_avatar (user, AVATAR_CHOOSER_PIXEL_SIZE);
        pixbuf = round_image (source_pixbuf);
        image = gtk_image_new_from_pixbuf (pixbuf);
        gtk_image_set_pixel_size (GTK_IMAGE (image), AVATAR_CHOOSER_PIXEL_SIZE);
        gtk_widget_show (image);
        gtk_container_add (GTK_CONTAINER (self->user_flowbox), image);
        g_signal_connect_object (self->user_flowbox, "child-activated", G_CALLBACK (user_flowbox_activated), self, G_CONNECT_SWAPPED);
}

