/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2008 William Jon McCann
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <canberra-gtk.h>
#include <libxml/tree.h>

#include <gsettings-desktop-schemas/gdesktop-enums.h>

#include "gvc-sound-theme-chooser.h"
#include "sound-theme-file-utils.h"

struct _GvcSoundThemeChooser
{
        GtkBox     parent_instance;
        GtkWidget *treeview;
        GtkWidget *selection_box;
        GSettings *settings;
        GSettings *sound_settings;
        char *current_theme;
        char *current_parent;
};

static void     gvc_sound_theme_chooser_class_init (GvcSoundThemeChooserClass *klass);
static void     gvc_sound_theme_chooser_init       (GvcSoundThemeChooser      *sound_theme_chooser);
static void     gvc_sound_theme_chooser_finalize   (GObject            *object);

G_DEFINE_TYPE (GvcSoundThemeChooser, gvc_sound_theme_chooser, GTK_TYPE_BOX)

#define KEY_SOUNDS_SCHEMA          "org.gnome.desktop.sound"
#define EVENT_SOUNDS_KEY           "event-sounds"
#define INPUT_SOUNDS_KEY           "input-feedback-sounds"
#define SOUND_THEME_KEY            "theme-name"

#define WM_SCHEMA                  "org.gnome.desktop.wm.preferences"
#define AUDIO_BELL_KEY             "audible-bell"

#define DEFAULT_ALERT_ID        "__default"
#define CUSTOM_THEME_NAME       "__custom"
#define NO_SOUNDS_THEME_NAME    "__no_sounds"
#define DEFAULT_THEME           "freedesktop"

enum {
        THEME_DISPLAY_COL,
        THEME_IDENTIFIER_COL,
        THEME_PARENT_ID_COL,
        THEME_NUM_COLS
};

enum {
        ALERT_DISPLAY_COL,
        ALERT_IDENTIFIER_COL,
        ALERT_SOUND_TYPE_COL,
        ALERT_NUM_COLS
};

enum {
        SOUND_TYPE_UNSET,
        SOUND_TYPE_OFF,
        SOUND_TYPE_DEFAULT_FROM_THEME,
        SOUND_TYPE_BUILTIN,
        SOUND_TYPE_CUSTOM
};

#define GVC_SOUND_SOUND    (xmlChar *) "sound"
#define GVC_SOUND_NAME     (xmlChar *) "name"
#define GVC_SOUND_FILENAME (xmlChar *) "filename"

/* Adapted from yelp-toc-pager.c */
static xmlChar *
xml_get_and_trim_names (xmlNodePtr node)
{
        xmlNodePtr cur;
        xmlChar *keep_lang = NULL;
        xmlChar *value;
        int j, keep_pri = INT_MAX;

        const gchar * const * langs = g_get_language_names ();

        value = NULL;

        for (cur = node->children; cur; cur = cur->next) {
                if (! xmlStrcmp (cur->name, GVC_SOUND_NAME)) {
                        xmlChar *cur_lang = NULL;
                        int cur_pri = INT_MAX;

                        cur_lang = xmlNodeGetLang (cur);

                        if (cur_lang) {
                                for (j = 0; langs[j]; j++) {
                                        if (g_str_equal (cur_lang, langs[j])) {
                                                cur_pri = j;
                                                break;
                                        }
                                }
                        } else {
                                cur_pri = INT_MAX - 1;
                        }

                        if (cur_pri <= keep_pri) {
                                if (keep_lang)
                                        xmlFree (keep_lang);
                                if (value)
                                        xmlFree (value);

                                value = xmlNodeGetContent (cur);

                                keep_lang = cur_lang;
                                keep_pri = cur_pri;
                        } else {
                                if (cur_lang)
                                        xmlFree (cur_lang);
                        }
                }
        }

        /* Delete all GVC_SOUND_NAME nodes */
        cur = node->children;
        while (cur) {
                xmlNodePtr this = cur;
                cur = cur->next;
                if (! xmlStrcmp (this->name, GVC_SOUND_NAME)) {
                        xmlUnlinkNode (this);
                        xmlFreeNode (this);
                }
        }

        return value;
}

static void
populate_model_from_node (GvcSoundThemeChooser *chooser,
                          GtkTreeModel         *model,
                          xmlNodePtr            node)
{
        xmlNodePtr child;
        xmlChar   *filename;
        xmlChar   *name;

        filename = NULL;
        name = xml_get_and_trim_names (node);
        for (child = node->children; child; child = child->next) {
                if (xmlNodeIsText (child)) {
                        continue;
                }

                if (xmlStrcmp (child->name, GVC_SOUND_FILENAME) == 0) {
                        filename = xmlNodeGetContent (child);
                } else if (xmlStrcmp (child->name, GVC_SOUND_NAME) == 0) {
                        /* EH? should have been trimmed */
                }
        }

        if (filename != NULL && name != NULL) {
                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                                   NULL,
                                                   G_MAXINT,
                                                   ALERT_IDENTIFIER_COL, filename,
                                                   ALERT_DISPLAY_COL, name,
                                                   ALERT_SOUND_TYPE_COL, _("Built-in"),
                                                   -1);
        }

        xmlFree (filename);
        xmlFree (name);
}

static void
populate_model_from_file (GvcSoundThemeChooser *chooser,
                          GtkTreeModel         *model,
                          const char           *filename)
{
        xmlDocPtr  doc;
        xmlNodePtr root;
        xmlNodePtr child;
        gboolean   exists;

        exists = g_file_test (filename, G_FILE_TEST_EXISTS);
        if (! exists) {
                return;
        }

        doc = xmlParseFile (filename);
        if (doc == NULL) {
                return;
        }

        root = xmlDocGetRootElement (doc);

        for (child = root->children; child; child = child->next) {
                if (xmlNodeIsText (child)) {
                        continue;
                }
                if (xmlStrcmp (child->name, GVC_SOUND_SOUND) != 0) {
                        continue;
                }

                populate_model_from_node (chooser, model, child);
        }

        xmlFreeDoc (doc);
}

static void
populate_model_from_dir (GvcSoundThemeChooser *chooser,
                         GtkTreeModel         *model,
                         const char           *dirname)
{
        GDir       *d;
        const char *name;

        d = g_dir_open (dirname, 0, NULL);
        if (d == NULL) {
                return;
        }

        while ((name = g_dir_read_name (d)) != NULL) {
                char *path;

                if (! g_str_has_suffix (name, ".xml")) {
                        continue;
                }

                path = g_build_filename (dirname, name, NULL);
                populate_model_from_file (chooser, model, path);
                g_free (path);
        }
}

static gboolean
save_alert_sounds (GvcSoundThemeChooser  *chooser,
                   const char            *id)
{
        const char *sounds[3] = { "bell-terminal", "bell-window-system", NULL };
        char *path;

        if (strcmp (id, DEFAULT_ALERT_ID) == 0) {
                delete_old_files (sounds);
                delete_disabled_files (sounds);
        } else {
                delete_old_files (sounds);
                delete_disabled_files (sounds);
                add_custom_file (sounds, id);
        }

        /* And poke the directory so the theme gets updated */
        path = custom_theme_dir_path (NULL);
        if (utime (path, NULL) != 0) {
                g_warning ("Failed to update mtime for directory '%s': %s",
                           path, g_strerror (errno));
        }
        g_free (path);

        return FALSE;
}


static void
update_alert_model (GvcSoundThemeChooser  *chooser,
                    const char            *id)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->treeview));
        g_assert (gtk_tree_model_get_iter_first (model, &iter));
        do {
                char    *this_id;

                gtk_tree_model_get (model, &iter,
                                    ALERT_IDENTIFIER_COL, &this_id,
                                    -1);

                if (strcmp (this_id, id) == 0) {
                        GtkTreeSelection *selection;

                        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (chooser->treeview));
                        gtk_tree_selection_select_iter (selection, &iter);
                }

                g_free (this_id);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
save_theme_name (GvcSoundThemeChooser *chooser,
                 const char           *theme_name)
{
        /* If the name is empty, use "freedesktop" */
        if (theme_name == NULL || *theme_name == '\0') {
                theme_name = DEFAULT_THEME;
        }

        /* special case for no sounds */
        if (strcmp (theme_name, NO_SOUNDS_THEME_NAME) == 0) {
                g_settings_set_boolean (chooser->sound_settings, EVENT_SOUNDS_KEY, FALSE);
                return;
        } else {
                g_settings_set_boolean (chooser->sound_settings, EVENT_SOUNDS_KEY, TRUE);
        }

        g_settings_set_string (chooser->sound_settings, SOUND_THEME_KEY, theme_name);
}

static gboolean
load_theme_file (const char *path,
                 char      **parent)
{
        GKeyFile *file;
        gboolean hidden;

        file = g_key_file_new ();
        if (g_key_file_load_from_file (file, path, G_KEY_FILE_KEEP_TRANSLATIONS, NULL) == FALSE) {
                g_key_file_free (file);
                return FALSE;
        }
        /* Don't add hidden themes to the list */
        hidden = g_key_file_get_boolean (file, "Sound Theme", "Hidden", NULL);
        if (!hidden) {
                /* Save the parent theme, if there's one */
                if (parent != NULL) {
                        *parent = g_key_file_get_string (file,
                                                         "Sound Theme",
                                                         "Inherits",
                                                         NULL);
                }
        }

        g_key_file_free (file);

        return TRUE;
}

static gboolean
load_theme_name (const char *name,
                 char      **parent)
{
        const char * const   *data_dirs;
        const char           *data_dir;
        char                 *path;
        guint                 i;
        gboolean              res;

        data_dir = g_get_user_data_dir ();
        path = g_build_filename (data_dir, "sounds", name, "index.theme", NULL);
        res = load_theme_file (path, parent);
        g_free (path);
        if (res)
                return TRUE;

        data_dirs = g_get_system_data_dirs ();
        for (i = 0; data_dirs[i] != NULL; i++) {
                path = g_build_filename (data_dirs[i], "sounds", name, "index.theme", NULL);
                res = load_theme_file (path, parent);
                g_free (path);
                if (res)
                        return TRUE;
        }

        return FALSE;
}

static void
update_alert (GvcSoundThemeChooser *chooser,
              const char           *alert_id)
{
        gboolean      is_custom;
        gboolean      is_default;
        gboolean      add_custom;
        gboolean      remove_custom;

        is_custom = strcmp (chooser->current_theme, CUSTOM_THEME_NAME) == 0;
        is_default = strcmp (alert_id, DEFAULT_ALERT_ID) == 0;

        /* So a few possibilities:
         * 1. Named theme, default alert selected: noop
         * 2. Named theme, alternate alert selected: create new custom with sound
         * 3. Custom theme, default alert selected: remove sound and possibly custom
         * 4. Custom theme, alternate alert selected: update custom sound
         */
        add_custom = FALSE;
        remove_custom = FALSE;
        if (! is_custom && is_default) {
                /* remove custom just in case */
                remove_custom = TRUE;
        } else if (! is_custom && ! is_default) {
                if (chooser->current_parent)
                        create_custom_theme (chooser->current_parent);
                else
                        create_custom_theme (DEFAULT_THEME);
                save_alert_sounds (chooser, alert_id);
                add_custom = TRUE;
        } else if (is_custom && is_default) {
                save_alert_sounds (chooser, alert_id);
                /* after removing files check if it is empty */
                if (custom_theme_dir_is_empty ()) {
                        remove_custom = TRUE;
                }
        } else if (is_custom && ! is_default) {
                save_alert_sounds (chooser, alert_id);
        }

        if (add_custom) {
                save_theme_name (chooser, CUSTOM_THEME_NAME);
        } else if (remove_custom) {
                delete_custom_theme_dir ();
                if (is_custom) {
                        save_theme_name (chooser, chooser->current_parent);
                }
        }

        update_alert_model (chooser, alert_id);
}

static void
play_preview_for_id (GvcSoundThemeChooser *chooser,
                     const char           *id)
{
        g_return_if_fail (id != NULL);

        /* special case: for the default item on custom themes
         * play the alert for the parent theme */
        if (strcmp (id, DEFAULT_ALERT_ID) == 0) {
                if (chooser->current_parent != NULL) {
                        ca_gtk_play_for_widget (GTK_WIDGET (chooser), 0,
                                                CA_PROP_APPLICATION_NAME, _("Sound Preferences"),
                                                CA_PROP_EVENT_ID, "bell-window-system",
                                                CA_PROP_CANBERRA_XDG_THEME_NAME, chooser->current_parent,
                                                CA_PROP_EVENT_DESCRIPTION, _("Testing event sound"),
                                                CA_PROP_CANBERRA_CACHE_CONTROL, "never",
                                                CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
#ifdef CA_PROP_CANBERRA_ENABLE
                                                CA_PROP_CANBERRA_ENABLE, "1",
#endif
                                                NULL);
                } else {
                        ca_gtk_play_for_widget (GTK_WIDGET (chooser), 0,
                                                CA_PROP_APPLICATION_NAME, _("Sound Preferences"),
                                                CA_PROP_EVENT_ID, "bell-window-system",
                                                CA_PROP_EVENT_DESCRIPTION, _("Testing event sound"),
                                                CA_PROP_CANBERRA_CACHE_CONTROL, "never",
                                                CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
#ifdef CA_PROP_CANBERRA_ENABLE
                                                CA_PROP_CANBERRA_ENABLE, "1",
#endif
                                                NULL);
                }
        } else {
                ca_gtk_play_for_widget (GTK_WIDGET (chooser), 0,
                                        CA_PROP_APPLICATION_NAME, _("Sound Preferences"),
                                        CA_PROP_MEDIA_FILENAME, id,
                                        CA_PROP_EVENT_DESCRIPTION, _("Testing event sound"),
                                        CA_PROP_CANBERRA_CACHE_CONTROL, "never",
                                        CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
#ifdef CA_PROP_CANBERRA_ENABLE
                                        CA_PROP_CANBERRA_ENABLE, "1",
#endif
                                        NULL);

        }
}

static void
on_treeview_row_activated (GtkTreeView          *treeview,
                           GtkTreePath          *path,
                           GtkTreeViewColumn    *column,
                           GvcSoundThemeChooser *chooser)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        char         *id;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (chooser->treeview));
        if (!gtk_tree_model_get_iter (model, &iter, path)) {
                return;
        }

        id = NULL;
        gtk_tree_model_get (model, &iter,
                            ALERT_IDENTIFIER_COL, &id,
                            -1);
        if (id == NULL) {
                return;
        }

        play_preview_for_id (chooser, id);
        update_alert (chooser, id);
        g_free (id);
}

static GtkWidget *
create_alert_treeview (GvcSoundThemeChooser *chooser)
{
        GtkListStore         *store;
        GtkWidget            *treeview;
        GtkCellRenderer      *renderer;
        GtkTreeViewColumn    *column;
        GtkTreeSelection     *selection;

        treeview = gtk_tree_view_new ();
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
        gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (treeview), TRUE);

        g_signal_connect (treeview,
                          "row-activated",
                          G_CALLBACK (on_treeview_row_activated),
                          chooser);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

        /* Setup the tree model, 3 columns:
         * - display name
         * - sound id
         * - sound type
         */
        store = gtk_list_store_new (ALERT_NUM_COLS,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);

        gtk_list_store_insert_with_values (store,
                                           NULL,
                                           G_MAXINT,
                                           ALERT_IDENTIFIER_COL, DEFAULT_ALERT_ID,
                                           ALERT_DISPLAY_COL, _("Default"),
                                           ALERT_SOUND_TYPE_COL, _("From theme"),
                                           -1);

        populate_model_from_dir (chooser, GTK_TREE_MODEL (store), SOUND_SET_DIR);

        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                                 GTK_TREE_MODEL (store));

        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Name"),
                                                           renderer,
                                                           "text", ALERT_DISPLAY_COL,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        return treeview;
}

static int
get_file_type (const char *sound_name,
               char      **linked_name)
{
        char *name, *filename;

        *linked_name = NULL;

        name = g_strdup_printf ("%s.disabled", sound_name);
        filename = custom_theme_dir_path (name);
        g_free (name);

        if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) != FALSE) {
                g_free (filename);
                return SOUND_TYPE_OFF;
        }
        g_free (filename);

        /* We only check for .ogg files because those are the
         * only ones we create */
        name = g_strdup_printf ("%s.ogg", sound_name);
        filename = custom_theme_dir_path (name);
        g_free (name);

        if (g_file_test (filename, G_FILE_TEST_IS_SYMLINK) != FALSE) {
                *linked_name = g_file_read_link (filename, NULL);
                g_free (filename);
                return SOUND_TYPE_CUSTOM;
        }
        g_free (filename);

        return SOUND_TYPE_BUILTIN;
}

static void
update_alerts_from_theme_name (GvcSoundThemeChooser *chooser,
                               const char           *name)
{
        if (strcmp (name, CUSTOM_THEME_NAME) != 0) {
                /* reset alert to default */
                update_alert (chooser, DEFAULT_ALERT_ID);
        } else {
                int   sound_type;
                char *linkname;

                linkname = NULL;
                sound_type = get_file_type ("bell-terminal", &linkname);
                g_debug ("Found link: %s", linkname);
                if (sound_type == SOUND_TYPE_CUSTOM) {
                        update_alert (chooser, linkname);
                }
        }
}

static void
update_theme (GvcSoundThemeChooser *chooser)
{
        gboolean     events_enabled;
        char        *last_theme;

        events_enabled = g_settings_get_boolean (chooser->sound_settings, EVENT_SOUNDS_KEY);

        last_theme = chooser->current_theme;
        if (events_enabled) {
                chooser->current_theme = g_settings_get_string (chooser->sound_settings, SOUND_THEME_KEY);
        } else {
                chooser->current_theme = g_strdup (NO_SOUNDS_THEME_NAME);
        }

        if (g_strcmp0 (last_theme, chooser->current_theme) != 0) {
                g_clear_pointer (&chooser->current_parent, g_free);
                if (load_theme_name (chooser->current_theme,
                                     &chooser->current_parent) == FALSE) {
                        g_free (chooser->current_theme);
                        chooser->current_theme = g_strdup (DEFAULT_THEME);
                        load_theme_name (DEFAULT_THEME,
                                         &chooser->current_parent);
                }
        }
        g_free (last_theme);

        gtk_widget_set_sensitive (chooser->selection_box, events_enabled);

        update_alerts_from_theme_name (chooser, chooser->current_theme);
}

static GObject *
gvc_sound_theme_chooser_constructor (GType                  type,
                                     guint                  n_construct_properties,
                                     GObjectConstructParam *construct_params)
{
        GObject              *object;
        GvcSoundThemeChooser *self;

        object = G_OBJECT_CLASS (gvc_sound_theme_chooser_parent_class)->constructor (type, n_construct_properties, construct_params);

        self = GVC_SOUND_THEME_CHOOSER (object);

        update_theme (self);

        return object;
}

static void
gvc_sound_theme_chooser_class_init (GvcSoundThemeChooserClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gvc_sound_theme_chooser_constructor;
        object_class->finalize = gvc_sound_theme_chooser_finalize;
}

static void
on_sound_settings_changed (GSettings            *settings,
                           const char           *key,
                           GvcSoundThemeChooser *chooser)
{
        if (strcmp (key, EVENT_SOUNDS_KEY) == 0) {
                update_theme (chooser);
        } else if (strcmp (key, SOUND_THEME_KEY) == 0) {
                update_theme (chooser);
        } else if (strcmp (key, INPUT_SOUNDS_KEY) == 0) {
                update_theme (chooser);
        }
}

static void
on_audible_bell_changed (GSettings            *settings,
                         const char           *key,
                         GvcSoundThemeChooser *chooser)
{
        update_theme (chooser);
}

static void
setup_list_size_constraint (GtkWidget *widget,
                            GtkWidget *to_size)
{
        GtkRequisition req;
        int            max_height;

        /* constrain height to be the tree height up to a max */
        max_height = (gdk_screen_get_height (gtk_widget_get_screen (widget))) / 4;

        gtk_widget_get_preferred_size (to_size, NULL, &req);

        gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (widget),
                                                    MIN (req.height, max_height));
}

static void
gvc_sound_theme_chooser_init (GvcSoundThemeChooser *chooser)
{
        GtkWidget   *box;
        GtkWidget   *label;
        GtkWidget   *scrolled_window;
        char        *str;

        gtk_orientable_set_orientation (GTK_ORIENTABLE (chooser),
                                        GTK_ORIENTATION_VERTICAL);

        chooser->settings = g_settings_new (WM_SCHEMA);
        chooser->sound_settings = g_settings_new (KEY_SOUNDS_SCHEMA);

        str = g_strdup_printf ("<b>%s</b>", _("C_hoose an alert sound:"));
        chooser->selection_box = box = gtk_frame_new (str);
        g_free (str);
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);

        gtk_widget_set_margin_top (box, 6);
        gtk_box_pack_start (GTK_BOX (chooser), box, TRUE, TRUE, 6);

        chooser->treeview = create_alert_treeview (chooser);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser->treeview);

        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        setup_list_size_constraint (scrolled_window, chooser->treeview);

        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                             GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (scrolled_window), chooser->treeview);
        gtk_widget_set_margin_top (scrolled_window, 6);
        gtk_container_add (GTK_CONTAINER (box), scrolled_window);

        g_signal_connect (G_OBJECT (chooser->sound_settings), "changed",
                          G_CALLBACK (on_sound_settings_changed), chooser);
        g_signal_connect (chooser->settings, "changed::" AUDIO_BELL_KEY,
                          G_CALLBACK (on_audible_bell_changed), chooser);
}

static void
gvc_sound_theme_chooser_finalize (GObject *object)
{
        GvcSoundThemeChooser *sound_theme_chooser;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_SOUND_THEME_CHOOSER (object));

        sound_theme_chooser = GVC_SOUND_THEME_CHOOSER (object);

        if (sound_theme_chooser != NULL) {
                g_object_unref (sound_theme_chooser->settings);
                g_object_unref (sound_theme_chooser->sound_settings);
        }

        G_OBJECT_CLASS (gvc_sound_theme_chooser_parent_class)->finalize (object);
}

GtkWidget *
gvc_sound_theme_chooser_new (void)
{
        GObject *chooser;
        chooser = g_object_new (GVC_TYPE_SOUND_THEME_CHOOSER,
                                "spacing", 6,
                                NULL);
        return GTK_WIDGET (chooser);
}
