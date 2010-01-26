/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 William Jon McCann
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>

#include "cc-theme-save-dialog.h"

#define CC_THEME_SAVE_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_THEME_SAVE_DIALOG, CcThemeSaveDialogPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))
#define BACKGROUND_KEY "/desktop/gnome/background/picture_filename"

struct CcThemeSaveDialogPrivate
{
        GtkWidget          *entry;
        GtkWidget          *text_view;
        GtkWidget          *save_button;
        GtkWidget          *save_background_checkbutton;
        GnomeThemeMetaInfo *info;
};

enum {
        PROP_0,
};

static void     cc_theme_save_dialog_class_init  (CcThemeSaveDialogClass *klass);
static void     cc_theme_save_dialog_init        (CcThemeSaveDialog      *theme_save_dialog);
static void     cc_theme_save_dialog_finalize    (GObject                   *object);

G_DEFINE_TYPE (CcThemeSaveDialog, cc_theme_save_dialog, GTK_TYPE_DIALOG)

enum {
  INVALID_THEME_NAME
};

GQuark
cc_theme_save_dialog_error_quark (void)
{
        static GQuark error_quark = 0;

        if (error_quark == 0)
                error_quark = g_quark_from_static_string ("cc-theme-save-dialog");

        return error_quark;
}

void
cc_theme_save_dialog_set_theme_info (CcThemeSaveDialog  *dialog,
                                     GnomeThemeMetaInfo *info)
{
        g_return_if_fail (CC_IS_THEME_SAVE_DIALOG (dialog));
        dialog->priv->info = info;
}

static void
cc_theme_save_dialog_set_property (GObject        *object,
                                   guint           prop_id,
                                   const GValue   *value,
                                   GParamSpec     *pspec)
{
        CcThemeSaveDialog *self;

        self = CC_THEME_SAVE_DIALOG (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_theme_save_dialog_get_property (GObject        *object,
                                   guint           prop_id,
                                   GValue         *value,
                                   GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

/* taken from gnome-desktop-item.c */
static gchar *
escape_string_and_dup (const char *s)
{
        char *return_value, *p;
        const char *q;
        int len = 0;

        if (s == NULL)
                return g_strdup("");

        q = s;
        while (*q) {
                len++;
                if (strchr ("\n\r\t\\", *q) != NULL)
                        len++;
                q++;
        }
        return_value = p = (char *) g_malloc (len + 1);
        do {
                switch (*s) {
                case '\t':
                        *p++ = '\\';
                        *p++ = 't';
                        break;
                case '\n':
                        *p++ = '\\';
                        *p++ = 'n';
                        break;
                case '\r':
                        *p++ = '\\';
                        *p++ = 'r';
                        break;
                case '\\':
                        *p++ = '\\';
                        *p++ = '\\';
                        break;
                default:
                        *p++ = *s;
                }
        }
        while (*s++);
        return return_value;
}

static gboolean
check_theme_name (const char  *theme_name,
                  GError      **error)
{
        if (theme_name == NULL) {
                g_set_error (error,
                             CC_THEME_SAVE_DIALOG_ERROR,
                             INVALID_THEME_NAME,
                             _("Theme name must be present"));
                return FALSE;
        }
        return TRUE;
}

static char *
str_remove_slash (const char *src)
{
        const char *i;
        char *rtn;
        gint len = 0;
        i = src;

        while (*i) {
                if (*i != '/')
                        len++;
                i++;
        }

        rtn = (char *) g_malloc (len + 1);
        while (*src) {
                if (*src != '/') {
                        *rtn = *src;
                        rtn++;
                }
                src++;
        }
        *rtn = '\0';
        return rtn - len;
}

static gboolean
setup_directory_structure (CcThemeSaveDialog *dialog,
                           const char        *theme_name,
                           GError           **error)
{
        char    *dir;
        char    *theme_name_dir;
        gboolean retval = TRUE;

        theme_name_dir = str_remove_slash (theme_name);

        dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
        if (!g_file_test (dir, G_FILE_TEST_EXISTS))
                g_mkdir (dir, 0775);
        g_free (dir);

        dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, NULL);
        if (!g_file_test (dir, G_FILE_TEST_EXISTS))
                g_mkdir (dir, 0775);
        g_free (dir);

        dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, "index.theme", NULL);
        g_free (theme_name_dir);

        if (g_file_test (dir, G_FILE_TEST_EXISTS)) {
                GtkDialog *ask_dialog;
                GtkWidget *button;
                int        response;

                ask_dialog = (GtkDialog *) gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                                   GTK_DIALOG_MODAL,
                                                                   GTK_MESSAGE_QUESTION,
                                                                   GTK_BUTTONS_CANCEL,
                                                                   _("The theme already exists. Would you like to replace it?"));
                button = gtk_dialog_add_button (ask_dialog,
                                                _("_Overwrite"),
                                                GTK_RESPONSE_ACCEPT);
                gtk_button_set_image (GTK_BUTTON (button),
                                      gtk_image_new_from_stock (GTK_STOCK_SAVE, GTK_ICON_SIZE_BUTTON));
                response = gtk_dialog_run (ask_dialog);
                gtk_widget_destroy (GTK_WIDGET (ask_dialog));
                retval = (response != GTK_RESPONSE_CANCEL);
        }
        g_free (dir);

        return retval;
}

static gboolean
write_theme_to_disk (CcThemeSaveDialog   *dialog,
                     GnomeThemeMetaInfo  *theme_info,
                     const char          *theme_name,
                     const char          *theme_description,
                     gboolean             save_background,
                     GError             **error)
{
        char          *dir;
        char          *theme_name_dir;
        GFile         *tmp_file;
        GFile         *target_file;
        GOutputStream *output;
        char          *str;
        char          *current_background;
        GConfClient   *client;
        const char    *theme_header =
                "[Desktop Entry]\n"
                "Name=%s\n"
                "Type=X-GNOME-Metatheme\n"
                "Comment=%s\n"
                "\n"
                "[X-GNOME-Metatheme]\n"
                "GtkTheme=%s\n"
                "MetacityTheme=%s\n"
                "IconTheme=%s\n";

        theme_name_dir = str_remove_slash (theme_name);
        dir = g_build_filename (g_get_home_dir (), ".themes", theme_name_dir, "index.theme~", NULL);
        g_free (theme_name_dir);

        tmp_file = g_file_new_for_path (dir);
        dir [strlen (dir) - 1] = '\000';
        target_file = g_file_new_for_path (dir);
        g_free (dir);

        /* start making the theme file */
        str = g_strdup_printf (theme_header,
                               theme_name,
                               theme_description,
                               theme_info->gtk_theme_name,
                               theme_info->metacity_theme_name,
                               theme_info->icon_theme_name);

        output = G_OUTPUT_STREAM (g_file_replace (tmp_file, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL));
        g_output_stream_write (output, str, strlen (str), NULL, NULL);
        g_free (str);

        if (theme_info->gtk_color_scheme) {
                char *a, *tmp;
                tmp = g_strdup (theme_info->gtk_color_scheme);
                for (a = tmp; *a != '\0'; a++)
                        if (*a == '\n')
                                *a = ',';
                str = g_strdup_printf ("GtkColorScheme=%s\n", tmp);
                g_output_stream_write (output, str, strlen (str), NULL, NULL);

                g_free (str);
                g_free (tmp);
        }

        if (theme_info->cursor_theme_name) {
#ifdef HAVE_XCURSOR
                str = g_strdup_printf ("CursorTheme=%s\n"
                                       "CursorSize=%i\n",
                                       theme_info->cursor_theme_name,
                                       theme_info->cursor_size);
#else
                str = g_strdup_printf ("CursorFont=%s\n", theme_info->cursor_theme_name);
#endif
                g_output_stream_write (output, str, strlen (str), NULL, NULL);
                g_free (str);
        }

        if (theme_info->notification_theme_name) {
                str = g_strdup_printf ("NotificationTheme=%s\n", theme_info->notification_theme_name);
                g_output_stream_write (output, str, strlen (str), NULL, NULL);
                g_free (str);
        }

        if (save_background) {
                client = gconf_client_get_default ();
                current_background = gconf_client_get_string (client, BACKGROUND_KEY, NULL);

                if (current_background != NULL) {
                        str = g_strdup_printf ("BackgroundImage=%s\n", current_background);

                        g_output_stream_write (output, str, strlen (str), NULL, NULL);

                        g_free (current_background);
                        g_free (str);
                }
                g_object_unref (client);
        }

        g_file_move (tmp_file, target_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
        g_output_stream_close (output, NULL, NULL);

        g_object_unref (tmp_file);
        g_object_unref (target_file);

        return TRUE;
}

static gboolean
save_theme_to_disk (CcThemeSaveDialog   *dialog,
                    GnomeThemeMetaInfo  *theme_info,
                    const char          *theme_name,
                    const char          *theme_description,
                    gboolean             save_background,
                    GError             **error)
{
        if (!check_theme_name (theme_name, error))
                return FALSE;

        if (!setup_directory_structure (dialog, theme_name, error))
                return FALSE;

        if (!write_theme_to_disk (dialog, theme_info, theme_name, theme_description, save_background, error))
                return FALSE;

        return TRUE;
}

static gboolean
do_save (CcThemeSaveDialog *dialog)
{
        GtkTextBuffer *buffer;
        GtkTextIter    start_iter;
        GtkTextIter    end_iter;
        char          *buffer_text;
        char          *theme_description = NULL;
        char          *theme_name = NULL;
        gboolean       save_background;
        GError        *error = NULL;
        gboolean       ret;

        ret = FALSE;
        theme_name = escape_string_and_dup (gtk_entry_get_text (GTK_ENTRY (dialog->priv->entry)));
        buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->priv->text_view));

        gtk_text_buffer_get_start_iter (buffer, &start_iter);
        gtk_text_buffer_get_end_iter (buffer, &end_iter);
        buffer_text = gtk_text_buffer_get_text (buffer, &start_iter, &end_iter, FALSE);
        theme_description = escape_string_and_dup (buffer_text);
        g_free (buffer_text);

        save_background = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->save_background_checkbutton));

        ret = save_theme_to_disk (dialog, dialog->priv->info, theme_name, theme_description, save_background, &error);
        g_free (theme_name);
        g_free (theme_description);
        g_clear_error (&error);

        return ret;
}

static void
cc_theme_save_dialog_response (GtkDialog *dialog,
                               gint       response_id)
{
        switch (response_id) {
        case GTK_RESPONSE_OK:
                if (!do_save (CC_THEME_SAVE_DIALOG (dialog))) {
                        g_signal_stop_emission_by_name (dialog, "response");
                        gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
                }
                break;
        default:
                break;
        }
}

static void
on_entry_text_changed (GtkEditable       *editable,
                       CcThemeSaveDialog *dialog)

{
        const char *text;

        text = gtk_entry_get_text (GTK_ENTRY (editable));

        gtk_widget_set_sensitive (dialog->priv->save_button,
                                  text != NULL && text[0] != '\000');
}

static GObject *
cc_theme_save_dialog_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
        CcThemeSaveDialog *dialog;
        GtkBuilder        *builder;
        GtkWidget         *widget;
        GtkWidget         *box;
        GtkTextBuffer     *text_buffer;
        GError            *error;

        dialog = CC_THEME_SAVE_DIALOG (G_OBJECT_CLASS (cc_theme_save_dialog_parent_class)->constructor (type,
                                                                                                        n_construct_properties,
                                                                                                        construct_properties));


        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR
                                   "/appearance.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return NULL;
        }


        dialog->priv->entry = WID ("save_dialog_entry");
        dialog->priv->text_view = WID ("save_dialog_textview");
        dialog->priv->save_background_checkbutton = WID ("save_background_checkbutton");

        g_signal_connect (dialog->priv->entry,
                          "changed",
                          (GCallback) on_entry_text_changed,
                          dialog);

        gtk_widget_set_size_request (dialog->priv->text_view, 300, 100);

        text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->priv->text_view));
        gtk_text_buffer_set_text (text_buffer, "", 0);

        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
        dialog->priv->save_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_SAVE, GTK_RESPONSE_OK);

        gtk_entry_set_text (GTK_ENTRY (dialog->priv->entry), "");
        on_entry_text_changed (GTK_EDITABLE (dialog->priv->entry), dialog);
        gtk_widget_grab_focus (dialog->priv->entry);

        widget = WID ("save_dialog_table");
        box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_widget_reparent (widget, box);
        gtk_widget_show (widget);

        g_object_unref (builder);

        return G_OBJECT (dialog);
}

static void
cc_theme_save_dialog_class_init (CcThemeSaveDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

        object_class->get_property = cc_theme_save_dialog_get_property;
        object_class->set_property = cc_theme_save_dialog_set_property;
        object_class->constructor = cc_theme_save_dialog_constructor;
        object_class->finalize = cc_theme_save_dialog_finalize;

        dialog_class->response = cc_theme_save_dialog_response;

        g_type_class_add_private (klass, sizeof (CcThemeSaveDialogPrivate));
}

static void
cc_theme_save_dialog_init (CcThemeSaveDialog *dialog)
{
        dialog->priv = CC_THEME_SAVE_DIALOG_GET_PRIVATE (dialog);
}

static void
cc_theme_save_dialog_finalize (GObject *object)
{
        CcThemeSaveDialog *theme_save_dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_THEME_SAVE_DIALOG (object));

        theme_save_dialog = CC_THEME_SAVE_DIALOG (object);

        g_return_if_fail (theme_save_dialog->priv != NULL);

        G_OBJECT_CLASS (cc_theme_save_dialog_parent_class)->finalize (object);
}

GtkWidget *
cc_theme_save_dialog_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_THEME_SAVE_DIALOG,
                               NULL);

        return GTK_WIDGET (object);
}
