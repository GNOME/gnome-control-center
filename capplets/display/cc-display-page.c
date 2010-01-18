/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 - 2010 Red Hat, Inc.
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

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus-glib.h>

#include "scrollarea.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-rr.h>
#include <libgnomeui/gnome-rr-config.h>
#include <libgnomeui/gnome-rr-labeler.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "cc-display-page.h"

#define CC_DISPLAY_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PAGE, CcDisplayPagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

#define SHOW_ICON_KEY "/apps/gnome_settings_daemon/xrandr/show_notification_icon"

struct CcDisplayPagePrivate
{
        GnomeRRScreen   *screen;
        GnomeRRConfig   *current_configuration;
        GnomeRRLabeler  *labeler;
        GnomeOutputInfo *current_output;

        GtkWidget       *current_monitor_event_box;
        GtkWidget       *current_monitor_label;
        GtkWidget       *monitor_on_radio;
        GtkWidget       *monitor_off_radio;
        GtkListStore    *resolution_store;
        GtkWidget       *resolution_combo;
        GtkWidget       *refresh_combo;
        GtkWidget       *rotation_combo;
        GtkWidget       *panel_checkbox;
        GtkWidget       *clone_checkbox;
        GtkWidget       *show_icon_checkbox;

        /* We store the event timestamp when the Apply button is clicked */
        GtkWidget       *apply_button;
        guint32          apply_button_clicked_timestamp;

        GtkWidget       *area;
        gboolean         ignore_gui_changes;

        /* These are used while we are waiting for the ApplyConfiguration method to be executed over D-bus */
        DBusGConnection *connection;
        DBusGProxy      *proxy;
        DBusGProxyCall  *proxy_call;

        enum {
                APPLYING_VERSION_1,
                APPLYING_VERSION_2
        } apply_configuration_state;
};

enum {
        PROP_0,
};

static void     cc_display_page_class_init     (CcDisplayPageClass *klass);
static void     cc_display_page_init           (CcDisplayPage      *display_page);
static void     cc_display_page_finalize       (GObject             *object);

static void     rebuild_gui (CcDisplayPage *page);
static void     on_clone_changed (GtkWidget *box, CcDisplayPage *page);
static void     on_rate_changed (GtkComboBox *box, CcDisplayPage *page);
static gboolean output_overlaps (GnomeOutputInfo *output, GnomeRRConfig *config);
static void     select_current_output_from_dialog_position (CcDisplayPage *page);
static void     monitor_on_off_toggled_cb (GtkToggleButton *toggle, CcDisplayPage *page);
static void     get_geometry (GnomeOutputInfo *output, int *w, int *h);
static void     apply_configuration_returned_cb (DBusGProxy *proxy, DBusGProxyCall *call_id, CcDisplayPage *page);
static gboolean get_clone_size (GnomeRRScreen *screen, int *width, int *height);
static gboolean output_info_supports_mode (CcDisplayPage *page, GnomeOutputInfo *info, int width, int height);

G_DEFINE_TYPE (CcDisplayPage, cc_display_page, CC_TYPE_PAGE)


typedef struct
{
    int grab_x;
    int grab_y;
    int output_x;
    int output_y;
} GrabInfo;

static void
cc_display_page_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_display_page_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
error_message (CcDisplayPage *page,
               const char    *primary_text,
               const char    *secondary_text)
{
        GtkWidget *dialog;
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        dialog = gtk_message_dialog_new ((toplevel) ? GTK_WINDOW (toplevel) : NULL,
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_ERROR,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", primary_text);

        if (secondary_text != NULL)
                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s", secondary_text);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static gboolean
do_free (gpointer data)
{
        g_free (data);
        return FALSE;
}

static gchar *
idle_free (gchar *s)
{
        g_idle_add (do_free, s);

        return s;
}

static void
layout_set_font (PangoLayout *layout,
                 const char  *font)
{
        PangoFontDescription *desc;
        desc = pango_font_description_from_string (font);

        if (desc != NULL) {
                pango_layout_set_font_description (layout, desc);

                pango_font_description_free (desc);
        }
}

static void
clear_combo (GtkWidget *widget)
{
        GtkTreeModel *model;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        gtk_list_store_clear (GTK_LIST_STORE (model));
}

typedef struct
{
        const char *text;
        gboolean    found;
        GtkTreeIter iter;
} ForeachInfo;

static gboolean
foreach (GtkTreeModel *model,
         GtkTreePath  *path,
         GtkTreeIter  *iter,
         ForeachInfo  *info)
{
        char *text = NULL;

        gtk_tree_model_get (model, iter, 0, &text, -1);

        g_assert (text != NULL);

        if (strcmp (info->text, text) == 0) {
                info->found = TRUE;
                info->iter = *iter;
                return TRUE;
        }

        return FALSE;
}

static void
add_key (GtkWidget      *widget,
         const char     *text,
         int             width,
         int             height,
         int             rate,
         GnomeRRRotation rotation)
{
        ForeachInfo   info;
        GtkTreeModel *model;
        gboolean      retval;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        info.text = text;
        info.found = FALSE;

        gtk_tree_model_foreach (model,
                                (GtkTreeModelForeachFunc) foreach,
                                &info);

        if (!info.found) {
                GtkTreeIter iter;
                gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                                   &iter,
                                                   -1,
                                                   0, text,
                                                   1, width,
                                                   2, height,
                                                   3, rate,
                                                   4, width * height,
                                                   5, rotation,
                                                   -1);

                retval = TRUE;
        } else {
                retval = FALSE;
        }
}

static gboolean
combo_select (GtkWidget  *widget,
              const char *text)
{
        GtkTreeModel *model;
        ForeachInfo   info;

        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        info.text = text;
        info.found = FALSE;

        gtk_tree_model_foreach (model,
                                (GtkTreeModelForeachFunc) foreach,
                                &info);

        if (!info.found)
                return FALSE;

        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget),
                                       &info.iter);
        return TRUE;
}

static GnomeRRMode **
get_current_modes (CcDisplayPage *page)
{
        GnomeRROutput *output;

        if (page->priv->current_configuration->clone) {
                return gnome_rr_screen_list_clone_modes (page->priv->screen);
        } else {
                if (!page->priv->current_output)
                        return NULL;

                output = gnome_rr_screen_get_output_by_name (page->priv->screen,
                                                             page->priv->current_output->name);

                if (output == NULL)
                        return NULL;

                return gnome_rr_output_list_modes (output);
        }
}

static void
rebuild_rotation_combo (CcDisplayPage *page)
{
        typedef struct {
                GnomeRRRotation rotation;
                const char *    name;
        } RotationInfo;
        static const RotationInfo rotations[] = {
                { GNOME_RR_ROTATION_0, N_("Normal") },
                { GNOME_RR_ROTATION_90, N_("Left") },
                { GNOME_RR_ROTATION_270, N_("Right") },
                { GNOME_RR_ROTATION_180, N_("Upside Down") },
        };
        const char     *selection;
        GnomeRRRotation current;
        int             i;

        clear_combo (page->priv->rotation_combo);

        gtk_widget_set_sensitive (page->priv->rotation_combo,
                                  page->priv->current_output
                                  && page->priv->current_output->on);

        if (!page->priv->current_output)
                return;

        current = page->priv->current_output->rotation;

        selection = NULL;
        for (i = 0; i < G_N_ELEMENTS (rotations); ++i) {
                const RotationInfo *info = &(rotations[i]);

                page->priv->current_output->rotation = info->rotation;

                /* NULL-GError --- FIXME: we should say why this rotation is not available! */
                if (gnome_rr_config_applicable (page->priv->current_configuration,
                                                page->priv->screen,
                                                NULL)) {
                        add_key (page->priv->rotation_combo,
                                 _(info->name),
                                 0,
                                 0,
                                 0,
                                 info->rotation);

                        if (info->rotation == current)
                                selection = _(info->name);
                }
        }

        page->priv->current_output->rotation = current;

        if (!(selection
              && combo_select (page->priv->rotation_combo, selection)))
                combo_select (page->priv->rotation_combo, _("Normal"));
}

static char *
make_rate_string (int hz)
{
        return g_strdup_printf (_("%d Hz"), hz);
}

static void
rebuild_rate_combo (CcDisplayPage *page)
{
        GHashTable   *rates;
        GnomeRRMode **modes;
        int           best;
        int           i;

        clear_combo (page->priv->refresh_combo);

        gtk_widget_set_sensitive (page->priv->refresh_combo,
                                  page->priv->current_output
                                  && page->priv->current_output->on);

        if (!page->priv->current_output
            || !(modes = get_current_modes (page)))
                return;

        rates = g_hash_table_new_full (g_str_hash,
                                       g_str_equal,
                                       (GFreeFunc) g_free,
                                       NULL);

        best = -1;
        for (i = 0; modes[i] != NULL; ++i) {
                GnomeRRMode *mode = modes[i];
                int          width;
                int          height;
                int          rate;

                width = gnome_rr_mode_get_width (mode);
                height = gnome_rr_mode_get_height (mode);
                rate = gnome_rr_mode_get_freq (mode);

                if (width == page->priv->current_output->width         &&
                    height == page->priv->current_output->height) {
                        add_key (page->priv->refresh_combo,
                                 idle_free (make_rate_string (rate)),
                                 0,
                                 0,
                                 rate,
                                 -1);

                        if (rate > best)
                                best = rate;
                }
        }

        if (!combo_select (page->priv->refresh_combo,
                           idle_free (make_rate_string (page->priv->current_output->rate))))
                combo_select (page->priv->refresh_combo,
                              idle_free (make_rate_string (best)));
}

static int
count_active_outputs (CcDisplayPage *page)
{
        int i;
        int count = 0;

        for (i = 0; page->priv->current_configuration->outputs[i] != NULL; ++i) {
                GnomeOutputInfo *output;

                output = page->priv->current_configuration->outputs[i];

                if (output->on)
                        count++;
        }

        return count;
}

#if 0
static int
count_all_outputs (GnomeRRConfig *config)
{
        int i;

        for (i = 0; config->outputs[i] != NULL; i++)
                ;

        return i;
}
#endif

/* Computes whether "Mirror Screens" (clone mode) is supported based on these criteria:
 *
 * 1. There is an available size for cloning.
 *
 * 2. There are 2 or more connected outputs that support that size.
 */
static gboolean
mirror_screens_is_supported (CcDisplayPage *page)
{
        int      clone_width;
        int      clone_height;
        gboolean have_clone_size;
        gboolean mirror_is_supported;

        mirror_is_supported = FALSE;

        have_clone_size = get_clone_size (page->priv->screen,
                                          &clone_width,
                                          &clone_height);

        if (have_clone_size) {
                int i;
                int num_outputs_with_clone_size;

                num_outputs_with_clone_size = 0;

                for (i = 0; page->priv->current_configuration->outputs[i] != NULL; i++) {
                        GnomeOutputInfo *output = page->priv->current_configuration->outputs[i];

                        /* We count the connected outputs that support the clone size.  It
                         * doesn't matter if those outputs aren't actually On currently; we
                         * will turn them on in on_clone_changed().
                         */
                        if (output->connected
                            && output_info_supports_mode (page,
                                                          output,
                                                          clone_width,
                                                          clone_height))
                                num_outputs_with_clone_size++;
                }

                if (num_outputs_with_clone_size >= 2)
                        mirror_is_supported = TRUE;
        }

        return mirror_is_supported;
}

static void
rebuild_mirror_screens (CcDisplayPage *page)
{
        gboolean mirror_is_active;
        gboolean mirror_is_supported;

        g_signal_handlers_block_by_func (page->priv->clone_checkbox,
                                         G_CALLBACK (on_clone_changed),
                                         page);

        mirror_is_active = page->priv->current_configuration
                && page->priv->current_configuration->clone;

        /* If mirror_is_active, then it *must* be possible to turn mirroring off */
        mirror_is_supported = mirror_is_active
                || mirror_screens_is_supported (page);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->priv->clone_checkbox),
                                      mirror_is_active);
        gtk_widget_set_sensitive (page->priv->clone_checkbox,
                                  mirror_is_supported);

        g_signal_handlers_unblock_by_func (page->priv->clone_checkbox,
                                           G_CALLBACK (on_clone_changed),
                                           page);
}

static void
rebuild_current_monitor_label (CcDisplayPage *page)
{
        char    *str;
        char    *tmp;
        GdkColor color;
        gboolean use_color;

        if (page->priv->current_output) {
                if (page->priv->current_configuration->clone) {
                        tmp = g_strdup (_("Mirror Screens"));
                } else {
                        tmp = g_strdup_printf (_("Monitor: %s"),
                                               page->priv->current_output->display_name);
                }

                str = g_strdup_printf ("<b>%s</b>", tmp);
                gnome_rr_labeler_get_color_for_output (page->priv->labeler,
                                                       page->priv->current_output,
                                                       &color);
                use_color = TRUE;
                g_free (tmp);
        } else {
                str = g_strdup_printf ("<b>%s</b>", _("Monitor"));
                use_color = FALSE;
        }

        gtk_label_set_markup (GTK_LABEL (page->priv->current_monitor_label), str);
        g_free (str);

        if (use_color) {
                GdkColor black = { 0, 0, 0, 0 };

                gtk_widget_modify_bg (page->priv->current_monitor_event_box,
                                      page->priv->current_monitor_event_box->state,
                                      &color);

                /* Make the label explicitly black.  We don't want it to follow the
                 * theme's colors, since the label is always shown against a light
                 * pastel background.  See bgo#556050
                 */
                gtk_widget_modify_fg (page->priv->current_monitor_label,
                                      GTK_WIDGET_STATE (page->priv->current_monitor_label),
                                      &black);
        } else {
                /* Remove any modifications we did on the label's color */
                GtkRcStyle *reset_rc_style;

                reset_rc_style = gtk_rc_style_new ();
                gtk_widget_modify_style (page->priv->current_monitor_label,
                                         reset_rc_style); /* takes ownership of, and destroys, the rc style */
        }

        gtk_event_box_set_visible_window (GTK_EVENT_BOX (page->priv->current_monitor_event_box), use_color);
}

static void
rebuild_on_off_radios (CcDisplayPage *page)
{
        gboolean sensitive;
        gboolean on_active;
        gboolean off_active;

        g_signal_handlers_block_by_func (page->priv->monitor_on_radio,
                                         G_CALLBACK (monitor_on_off_toggled_cb),
                                         page);
        g_signal_handlers_block_by_func (page->priv->monitor_off_radio,
                                         G_CALLBACK (monitor_on_off_toggled_cb),
                                         page);

        sensitive = FALSE;
        on_active = FALSE;
        off_active = FALSE;

        if (!page->priv->current_configuration->clone && page->priv->current_output) {
                if (count_active_outputs (page) > 1 || !page->priv->current_output->on)
                        sensitive = TRUE;
                else
                        sensitive = FALSE;

                on_active = page->priv->current_output->on;
                off_active = !on_active;
        }

        gtk_widget_set_sensitive (page->priv->monitor_on_radio, sensitive);
        gtk_widget_set_sensitive (page->priv->monitor_off_radio, sensitive);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->priv->monitor_on_radio),
                                      on_active);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->priv->monitor_off_radio),
                                      off_active);

        g_signal_handlers_unblock_by_func (page->priv->monitor_on_radio,
                                           G_CALLBACK (monitor_on_off_toggled_cb),
                                           page);
        g_signal_handlers_unblock_by_func (page->priv->monitor_off_radio,
                                           G_CALLBACK (monitor_on_off_toggled_cb),
                                           page);
}

static char *
make_resolution_string (int width,
                        int height)
{
        return g_strdup_printf (_("%d x %d"), width, height);
}

static void
find_best_mode (GnomeRRMode **modes,
                int          *out_width,
                int          *out_height)
{
        int i;

        *out_width = 0;
        *out_height = 0;

        for (i = 0; modes[i] != NULL; i++) {
                int w, h;

                w = gnome_rr_mode_get_width (modes[i]);
                h = gnome_rr_mode_get_height (modes[i]);

                if (w * h > *out_width * *out_height) {
                        *out_width = w;
                        *out_height = h;
                }
        }
}

static void
rebuild_resolution_combo (CcDisplayPage *page)
{
        int           i;
        GnomeRRMode **modes;
        const char   *current;

        clear_combo (page->priv->resolution_combo);

        if (!(modes = get_current_modes (page))
            || !page->priv->current_output
            || !page->priv->current_output->on) {
                gtk_widget_set_sensitive (page->priv->resolution_combo, FALSE);
                return;
        }

        g_assert (page->priv->current_output != NULL);
        g_assert (page->priv->current_output->width != 0 && page->priv->current_output->height != 0);

        gtk_widget_set_sensitive (page->priv->resolution_combo, TRUE);

        for (i = 0; modes[i] != NULL; ++i) {
                int width;
                int height;

                width = gnome_rr_mode_get_width (modes[i]);
                height = gnome_rr_mode_get_height (modes[i]);

                add_key (page->priv->resolution_combo,
                         idle_free (make_resolution_string (width, height)),
                         width, height, 0, -1);
        }

        current = idle_free (make_resolution_string (page->priv->current_output->width,
                                                     page->priv->current_output->height));

        if (!combo_select (page->priv->resolution_combo, current)) {
                int best_w, best_h;

                find_best_mode (modes, &best_w, &best_h);
                combo_select (page->priv->resolution_combo,
                              idle_free (make_resolution_string (best_w, best_h)));
        }
}

static void
rebuild_gui (CcDisplayPage *page)
{
        gboolean sensitive;

        /* We would break spectacularly if we recursed, so
         * just assert if that happens
         */
        g_assert (page->priv->ignore_gui_changes == FALSE);

        page->priv->ignore_gui_changes = TRUE;

        sensitive = page->priv->current_output ? TRUE : FALSE;

#if 0
        g_debug ("rebuild gui, is on: %d", page->priv->current_output->on);
#endif

        rebuild_mirror_screens (page);
        rebuild_current_monitor_label (page);
        rebuild_on_off_radios (page);
        rebuild_resolution_combo (page);
        rebuild_rate_combo (page);
        rebuild_rotation_combo (page);

#if 0
        g_debug ("sensitive: %d, on: %d", sensitive, page->priv->current_output->on);
#endif
        gtk_widget_set_sensitive (page->priv->panel_checkbox, sensitive);

        page->priv->ignore_gui_changes = FALSE;
}

static gboolean
get_mode (GtkWidget       *widget,
          int             *width,
          int             *height,
          int             *freq,
          GnomeRRRotation *rot)
{
        GtkTreeIter   iter;
        GtkTreeModel *model;
        GtkComboBox  *box = GTK_COMBO_BOX (widget);
        int           dummy;

        if (!gtk_combo_box_get_active_iter (box, &iter))
                return FALSE;

        if (!width)
                width = &dummy;

        if (!height)
                height = &dummy;

        if (!freq)
                freq = &dummy;

        if (!rot)
                rot = (GnomeRRRotation *)&dummy;

        model = gtk_combo_box_get_model (box);
        gtk_tree_model_get (model, &iter,
                            1, width,
                            2, height,
                            3, freq,
                            5, rot,
                            -1);

        return TRUE;

}

static void
on_rotation_changed (GtkComboBox   *box,
                     CcDisplayPage *page)
{
        GnomeRRRotation rotation;

        if (!page->priv->current_output)
                return;

        if (get_mode (page->priv->rotation_combo, NULL, NULL, NULL, &rotation))
                page->priv->current_output->rotation = rotation;

        foo_scroll_area_invalidate (FOO_SCROLL_AREA (page->priv->area));
}

static void
on_rate_changed (GtkComboBox   *box,
                 CcDisplayPage *page)
{
        int rate;

        if (!page->priv->current_output)
                return;

        if (get_mode (page->priv->refresh_combo, NULL, NULL, &rate, NULL))
                page->priv->current_output->rate = rate;

        foo_scroll_area_invalidate (FOO_SCROLL_AREA (page->priv->area));
}

static void
select_resolution_for_current_output (CcDisplayPage *page)
{
        GnomeRRMode **modes;
        int width, height;

        if (page->priv->current_output->pref_width != 0
            && page->priv->current_output->pref_height != 0) {
                page->priv->current_output->width = page->priv->current_output->pref_width;
                page->priv->current_output->height = page->priv->current_output->pref_height;
                return;
        }

        modes = get_current_modes (page);
        if (!modes)
                return;

        find_best_mode (modes, &width, &height);

        page->priv->current_output->width = width;
        page->priv->current_output->height = height;
}

static void
monitor_on_off_toggled_cb (GtkToggleButton *toggle,
                           CcDisplayPage    *page)
{
        gboolean is_on;

        if (!page->priv->current_output)
                return;

        if (!gtk_toggle_button_get_active (toggle))
                return;

        if (GTK_WIDGET (toggle) == page->priv->monitor_on_radio) {
                is_on = TRUE;
        } else if (GTK_WIDGET (toggle) == page->priv->monitor_off_radio) {
                is_on = FALSE;
        } else {
                g_assert_not_reached ();
                return;
        }

        page->priv->current_output->on = is_on;

        if (is_on) {
                select_resolution_for_current_output (page); /* The refresh rate will be picked in rebuild_rate_combo() */
        }

        rebuild_gui (page);
        foo_scroll_area_invalidate (FOO_SCROLL_AREA (page->priv->area));
}

static void
realign_outputs_after_resolution_change (CcDisplayPage   *page,
                                         GnomeOutputInfo *output_that_changed,
                                         int              old_width,
                                         int              old_height)
{
        /* We find the outputs that were below or to the right of the output that
         * changed, and realign them; we also do that for outputs that shared the
         * right/bottom edges with the output that changed.  The outputs that are
         * above or to the left of that output don't need to change.
         */

        int i;
        int old_right_edge;
        int old_bottom_edge;
        int dx;
        int dy;

        g_assert (page->priv->current_configuration != NULL);

        if (output_that_changed->width == old_width && output_that_changed->height == old_height)
                return;

        old_right_edge = output_that_changed->x + old_width;
        old_bottom_edge = output_that_changed->y + old_height;

        dx = output_that_changed->width - old_width;
        dy = output_that_changed->height - old_height;

        for (i = 0; page->priv->current_configuration->outputs[i] != NULL; i++) {
                GnomeOutputInfo *output;
                int              output_width;
                int              output_height;

                output = page->priv->current_configuration->outputs[i];

                if (output == output_that_changed || !output->connected)
                        continue;

                get_geometry (output, &output_width, &output_height);

                if (output->x >= old_right_edge)
                        output->x += dx;
                else if (output->x + output_width == old_right_edge)
                        output->x = output_that_changed->x + output_that_changed->width - output_width;

                if (output->y >= old_bottom_edge)
                        output->y += dy;
                else if (output->y + output_height == old_bottom_edge)
                        output->y = output_that_changed->y + output_that_changed->height - output_height;
        }
}

static void
on_resolution_changed (GtkComboBox   *box,
                       CcDisplayPage *page)
{
        int old_width;
        int old_height;
        int width;
        int height;

        if (!page->priv->current_output)
                return;

        old_width = page->priv->current_output->width;
        old_height = page->priv->current_output->height;

        if (get_mode (page->priv->resolution_combo, &width, &height, NULL, NULL)) {
                page->priv->current_output->width = width;
                page->priv->current_output->height = height;

                if (width == 0 || height == 0)
                        page->priv->current_output->on = FALSE;
                else
                        page->priv->current_output->on = TRUE;
        }

        realign_outputs_after_resolution_change (page,
                                                 page->priv->current_output,
                                                 old_width,
                                                 old_height);

        rebuild_rate_combo (page);
        rebuild_rotation_combo (page);

        foo_scroll_area_invalidate (FOO_SCROLL_AREA (page->priv->area));
}

static void
lay_out_outputs_horizontally (CcDisplayPage *page)
{
        int i;
        int x;

        /* Lay out all the monitors horizontally when "mirror screens" is turned
         * off, to avoid having all of them overlapped initially.  We put the
         * outputs turned off on the right-hand side.
         */

        x = 0;

        /* First pass, all "on" outputs */

        for (i = 0; page->priv->current_configuration->outputs[i]; ++i) {
                GnomeOutputInfo *output;

                output = page->priv->current_configuration->outputs[i];
                if (output->connected && output->on) {
                        output->x = x;
                        output->y = 0;
                        x += output->width;
                }
        }

        /* Second pass, all the black screens */

        for (i = 0; page->priv->current_configuration->outputs[i]; ++i) {
                GnomeOutputInfo *output;

                output = page->priv->current_configuration->outputs[i];
                if (!(output->connected && output->on)) {
                        output->x = x;
                        output->y = 0;
                        x += output->width;
                }
        }

}

/* FIXME: this function is copied from gnome-settings-daemon/plugins/xrandr/gsd-xrandr-manager.c.
 * Do we need to put this function in gnome-desktop for public use?
 */
static gboolean
get_clone_size (GnomeRRScreen *screen,
                int           *width,
                int           *height)
{
        GnomeRRMode **modes = gnome_rr_screen_list_clone_modes (screen);
        int           best_w;
        int           best_h;
        int           i;

        best_w = 0;
        best_h = 0;

        for (i = 0; modes[i] != NULL; ++i) {
                GnomeRRMode *mode = modes[i];
                int          w;
                int          h;

                w = gnome_rr_mode_get_width (mode);
                h = gnome_rr_mode_get_height (mode);

                if (w * h > best_w * best_h) {
                        best_w = w;
                        best_h = h;
                }
        }

        if (best_w > 0 && best_h > 0) {
                if (width)
                        *width = best_w;
                if (height)
                        *height = best_h;

                return TRUE;
        }

        return FALSE;
}

static gboolean
output_info_supports_mode (CcDisplayPage   *page,
                           GnomeOutputInfo *info,
                           int              width,
                           int              height)
{
        GnomeRROutput *output;
        GnomeRRMode  **modes;
        int            i;

        if (!info->connected)
                return FALSE;

        output = gnome_rr_screen_get_output_by_name (page->priv->screen,
                                                     info->name);
        if (!output)
                return FALSE;

        modes = gnome_rr_output_list_modes (output);

        for (i = 0; modes[i]; i++) {
                if (gnome_rr_mode_get_width (modes[i]) == width
                    && gnome_rr_mode_get_height (modes[i]) == height)
                        return TRUE;
        }

        return FALSE;
}

static void
on_clone_changed (GtkWidget     *box,
                  CcDisplayPage *page)
{

        page->priv->current_configuration->clone =
                gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page->priv->clone_checkbox));

        if (page->priv->current_configuration->clone) {
                int i;
                int width;
                int height;

                for (i = 0; page->priv->current_configuration->outputs[i]; ++i) {
                        if (page->priv->current_configuration->outputs[i]->connected) {
                                page->priv->current_output = page->priv->current_configuration->outputs[i];
                                break;
                        }
                }

                /* Turn on all the connected screens that support the best clone mode.
                 * The user may hit "Mirror Screens", but he shouldn't have to turn on
                 * all the required outputs as well.
                 */

                get_clone_size (page->priv->screen, &width, &height);

                for (i = 0; page->priv->current_configuration->outputs[i]; i++) {
                        if (output_info_supports_mode (page,
                                                       page->priv->current_configuration->outputs[i],
                                                       width,
                                                       height)) {
                                page->priv->current_configuration->outputs[i]->on = TRUE;
                                page->priv->current_configuration->outputs[i]->width = width;
                                page->priv->current_configuration->outputs[i]->height = height;
                        }
                }
        } else {
                if (output_overlaps (page->priv->current_output,
                                     page->priv->current_configuration))
                        lay_out_outputs_horizontally (page);
        }

        rebuild_gui (page);
}

static void
get_geometry (GnomeOutputInfo *output,
              int             *w,
              int             *h)
{
        if (output->on) {
                *h = output->height;
                *w = output->width;
        } else {
                *h = output->pref_height;
                *w = output->pref_width;
        }

        if ((output->rotation & GNOME_RR_ROTATION_90) || (output->rotation & GNOME_RR_ROTATION_270)) {
                int tmp;

                tmp = *h;
                *h = *w;
                *w = tmp;
        }
}

#define SPACE 15
#define MARGIN  15

static GList *
list_connected_outputs (CcDisplayPage *page,
                        int           *total_w,
                        int           *total_h)
{
        int    i;
        int    dummy;
        GList *result = NULL;

        if (!total_w)
                total_w = &dummy;
        if (!total_h)
                total_h = &dummy;

        *total_w = 0;
        *total_h = 0;
        for (i = 0; page->priv->current_configuration->outputs[i] != NULL; ++i) {
                GnomeOutputInfo *output;

                output = page->priv->current_configuration->outputs[i];

                if (output->connected) {
                        int w;
                        int h;

                        result = g_list_prepend (result, output);

                        get_geometry (output, &w, &h);

                        *total_w += w;
                        *total_h += h;
                }
        }

        return g_list_reverse (result);
}

static int
get_n_connected (CcDisplayPage *page)
{
        GList *connected_outputs;
        int    n;

        connected_outputs = list_connected_outputs (page, NULL, NULL);
        n = g_list_length (connected_outputs);
        g_list_free (connected_outputs);

        return n;
}

static double
compute_scale (CcDisplayPage *page)
{
        int          available_w;
        int          available_h;
        int          total_w;
        int          total_h;
        int          n_monitors;
        GdkRectangle viewport;
        GList       *connected_outputs;

        foo_scroll_area_get_viewport (FOO_SCROLL_AREA (page->priv->area), &viewport);

        connected_outputs = list_connected_outputs (page, &total_w, &total_h);

        n_monitors = g_list_length (connected_outputs);

        g_list_free (connected_outputs);

        available_w = viewport.width - 2 * MARGIN - (n_monitors - 1) * SPACE;
        available_h = viewport.height - 2 * MARGIN - (n_monitors - 1) * SPACE;

        return MIN ((double)available_w / total_w, (double)available_h / total_h);
}

typedef struct Edge
{
        GnomeOutputInfo *output;
        int              x1;
        int              y1;
        int              x2;
        int              y2;
} Edge;

typedef struct Snap
{
        Edge *snapper;              /* Edge that should be snapped */
        Edge *snappee;
        int   dy;
        int   dx;
} Snap;

static void
add_edge (GnomeOutputInfo *output,
          int              x1,
          int              y1,
          int              x2,
          int              y2,
          GArray          *edges)
{
        Edge e;

        e.x1 = x1;
        e.x2 = x2;
        e.y1 = y1;
        e.y2 = y2;
        e.output = output;

        g_array_append_val (edges, e);
}

static void
list_edges_for_output (GnomeOutputInfo *output,
                       GArray          *edges)
{
        int x, y, w, h;

        x = output->x;
        y = output->y;
        get_geometry (output, &w, &h);

        /* Top, Bottom, Left, Right */
        add_edge (output, x, y, x + w, y, edges);
        add_edge (output, x, y + h, x + w, y + h, edges);
        add_edge (output, x, y, x, y + h, edges);
        add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (GnomeRRConfig *config,
            GArray        *edges)
{
        int i;

        for (i = 0; config->outputs[i]; ++i) {
                GnomeOutputInfo *output = config->outputs[i];

                if (output->connected)
                        list_edges_for_output (output, edges);
        }
}

static gboolean
overlap (int s1,
         int e1,
         int s2,
         int e2)
{
        return (!(e1 < s2 || s1 >= e2));
}

static gboolean
horizontal_overlap (Edge *snapper,
                    Edge *snappee)
{
        if (snapper->y1 != snapper->y2 || snappee->y1 != snappee->y2)
                return FALSE;

        return overlap (snapper->x1, snapper->x2, snappee->x1, snappee->x2);
}

static gboolean
vertical_overlap (Edge *snapper,
                  Edge *snappee)
{
        if (snapper->x1 != snapper->x2 || snappee->x1 != snappee->x2)
                return FALSE;

        return overlap (snapper->y1, snapper->y2, snappee->y1, snappee->y2);
}

static void
add_snap (GArray *snaps,
          Snap    snap)
{
        if (ABS (snap.dx) <= 200 || ABS (snap.dy) <= 200)
                g_array_append_val (snaps, snap);
}

static void
add_edge_snaps (Edge   *snapper,
                Edge   *snappee,
                GArray *snaps)
{
        Snap snap;

        snap.snapper = snapper;
        snap.snappee = snappee;

        if (horizontal_overlap (snapper, snappee)) {
                snap.dx = 0;
                snap.dy = snappee->y1 - snapper->y1;

                add_snap (snaps, snap);
        } else if (vertical_overlap (snapper, snappee)) {
                snap.dy = 0;
                snap.dx = snappee->x1 - snapper->x1;

                add_snap (snaps, snap);
        }

        /* Corner snaps */
        /* 1->1 */
        snap.dx = snappee->x1 - snapper->x1;
        snap.dy = snappee->y1 - snapper->y1;

        add_snap (snaps, snap);

        /* 1->2 */
        snap.dx = snappee->x2 - snapper->x1;
        snap.dy = snappee->y2 - snapper->y1;

        add_snap (snaps, snap);

        /* 2->2 */
        snap.dx = snappee->x2 - snapper->x2;
        snap.dy = snappee->y2 - snapper->y2;

        add_snap (snaps, snap);

        /* 2->1 */
        snap.dx = snappee->x1 - snapper->x2;
        snap.dy = snappee->y1 - snapper->y2;

        add_snap (snaps, snap);
}

static void
list_snaps (GnomeOutputInfo *output,
            GArray          *edges,
            GArray          *snaps)
{
        int i;

        for (i = 0; i < edges->len; ++i) {
                Edge *output_edge;

                output_edge = &(g_array_index (edges, Edge, i));

                if (output_edge->output == output) {
                        int j;

                        for (j = 0; j < edges->len; ++j) {
                                Edge *edge;

                                edge = &(g_array_index (edges, Edge, j));

                                if (edge->output != output)
                                        add_edge_snaps (output_edge, edge, snaps);
                        }
                }
        }
}

#if 0
static void
print_edge (Edge *edge)
{
        g_debug ("(%d %d %d %d)", edge->x1, edge->y1, edge->x2, edge->y2);
}
#endif

static gboolean
corner_on_edge (int   x,
                int   y,
                Edge *e)
{
        if (x == e->x1 && x == e->x2 && y >= e->y1 && y <= e->y2)
                return TRUE;

        if (y == e->y1 && y == e->y2 && x >= e->x1 && x <= e->x2)
                return TRUE;

        return FALSE;
}

static gboolean
edges_align (Edge *e1,
             Edge *e2)
{
        if (corner_on_edge (e1->x1, e1->y1, e2))
                return TRUE;

        if (corner_on_edge (e2->x1, e2->y1, e1))
                return TRUE;

        return FALSE;
}

static gboolean
output_is_aligned (GnomeOutputInfo *output,
                   GArray          *edges)
{
        gboolean result = FALSE;
        int i;

        for (i = 0; i < edges->len; ++i) {
                Edge *output_edge;

                output_edge = &(g_array_index (edges, Edge, i));

                if (output_edge->output == output) {
                        int j;

                        for (j = 0; j < edges->len; ++j) {
                                Edge *edge;

                                edge = &(g_array_index (edges, Edge, j));

                                /* We are aligned if an output edge matches
                                 * an edge of another output
                                 */
                                if (edge->output != output_edge->output) {
                                        if (edges_align (output_edge, edge)) {
                                                result = TRUE;
                                                goto done;
                                        }
                                }
                        }
                }
        }
 done:

        return result;
}

static void
get_output_rect (GnomeOutputInfo *output,
                 GdkRectangle    *rect)
{
        int w;
        int h;

        get_geometry (output, &w, &h);

        rect->width = w;
        rect->height = h;
        rect->x = output->x;
        rect->y = output->y;
}

static gboolean
output_overlaps (GnomeOutputInfo *output,
                 GnomeRRConfig   *config)
{
        int i;
        GdkRectangle output_rect;

        get_output_rect (output, &output_rect);

        for (i = 0; config->outputs[i]; ++i) {
                GnomeOutputInfo *other = config->outputs[i];

                if (other != output && other->connected) {
                        GdkRectangle other_rect;

                        get_output_rect (other, &other_rect);
                        if (gdk_rectangle_intersect (&output_rect, &other_rect, NULL))
                                return TRUE;
                }
        }

        return FALSE;
}

static gboolean
gnome_rr_config_is_aligned (GnomeRRConfig *config,
                            GArray        *edges)
{
        int      i;
        gboolean result = TRUE;

        for (i = 0; config->outputs[i]; ++i) {
                GnomeOutputInfo *output = config->outputs[i];

                if (output->connected) {
                        if (!output_is_aligned (output, edges))
                                return FALSE;

                        if (output_overlaps (output, config))
                                return FALSE;
                }
        }

        return result;
}

struct GrabInfo
{
        int grab_x;
        int grab_y;
        int output_x;
        int output_y;
};

static gboolean
is_corner_snap (const Snap *s)
{
        return s->dx != 0 && s->dy != 0;
}

static int
compare_snaps (gconstpointer v1,
               gconstpointer v2)
{
        const Snap *s1 = v1;
        const Snap *s2 = v2;
        int         sv1 = MAX (ABS (s1->dx), ABS (s1->dy));
        int         sv2 = MAX (ABS (s2->dx), ABS (s2->dy));
        int         d;

        d = sv1 - sv2;

        /* This snapping algorithm is good enough for rock'n'roll, but
         * this is probably a better:
         *
         *    First do a horizontal/vertical snap, then
         *    with the new coordinates from that snap,
         *    do a corner snap.
         *
         * Right now, it's confusing that corner snapping
         * depends on the distance in an axis that you can't actually see.
         *
         */
        if (d == 0) {
                if (is_corner_snap (s1) && !is_corner_snap (s2))
                        return -1;
                else if (is_corner_snap (s2) && !is_corner_snap (s1))
                        return 1;
                else
                        return 0;
        } else {
                return d;
        }
}

static void
on_output_event (FooScrollArea      *area,
                 FooScrollAreaEvent *event,
                 GnomeOutputInfo    *output)
{
        CcDisplayPage *page;

        page = g_object_get_data (G_OBJECT (area), "page");

        if (event->type == FOO_BUTTON_PRESS) {
                GrabInfo *info;

                page->priv->current_output = output;

                rebuild_gui (page);

                if (!page->priv->current_configuration->clone
                    && get_n_connected (page) > 1) {

                        foo_scroll_area_begin_grab (area,
                                                    (FooScrollAreaEventFunc) on_output_event,
                                                    output);

                        info = g_new0 (GrabInfo, 1);
                        info->grab_x = event->x;
                        info->grab_y = event->y;
                        info->output_x = output->x;
                        info->output_y = output->y;

                        output->user_data = info;
                }

                foo_scroll_area_invalidate (area);
        } else {
                if (foo_scroll_area_is_grabbed (area)) {
                        GrabInfo *info = output->user_data;
                        double    scale = compute_scale (page);
                        int       old_x;
                        int       old_y;
                        int       new_x;
                        int       new_y;
                        int       i;
                        GArray   *edges;
                        GArray   *snaps;
                        GArray   *new_edges;

                        old_x = output->x;
                        old_y = output->y;
                        new_x = info->output_x + (event->x - info->grab_x) / scale;
                        new_y = info->output_y + (event->y - info->grab_y) / scale;

                        output->x = new_x;
                        output->y = new_y;

                        edges = g_array_new (TRUE, TRUE, sizeof (Edge));
                        snaps = g_array_new (TRUE, TRUE, sizeof (Snap));
                        new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

                        list_edges (page->priv->current_configuration, edges);
                        list_snaps (output, edges, snaps);

                        g_array_sort (snaps, compare_snaps);

                        output->x = info->output_x;
                        output->y = info->output_y;

                        for (i = 0; i < snaps->len; ++i) {
                                Snap   *snap;
                                GArray *new_edges;

                                snap = &(g_array_index (snaps, Snap, i));
                                new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

                                output->x = new_x + snap->dx;
                                output->y = new_y + snap->dy;

                                g_array_set_size (new_edges, 0);
                                list_edges (page->priv->current_configuration, new_edges);

                                if (gnome_rr_config_is_aligned (page->priv->current_configuration, new_edges)) {
                                        g_array_free (new_edges, TRUE);
                                        break;
                                } else {
                                        output->x = info->output_x;
                                        output->y = info->output_y;
                                }
                        }

                        g_array_free (new_edges, TRUE);
                        g_array_free (snaps, TRUE);
                        g_array_free (edges, TRUE);

                        if (event->type == FOO_BUTTON_RELEASE) {
                                foo_scroll_area_end_grab (area);

                                g_free (output->user_data);
                                output->user_data = NULL;

#if 0
                                g_debug ("new position: %d %d %d %d", output->x, output->y, output->width, output->height);
#endif
                        }

                        foo_scroll_area_invalidate (area);
                }
        }
}

#if 0
static void
on_canvas_event (FooScrollArea      *area,
                 FooScrollAreaEvent *event,
                 gpointer            data)
{
        CcDisplayPage *page;

        page = g_object_get_data (G_OBJECT (area), "page");

        if (event->type == FOO_BUTTON_PRESS) {
                page->priv->current_output = NULL;

                rebuild_gui (page);

                foo_scroll_area_invalidate (area);
        }
}
#endif

static PangoLayout *
get_display_name (CcDisplayPage   *page,
                  GnomeOutputInfo *output)
{
        const char *text;

        if (page->priv->current_configuration->clone) {
                /* Translators:  this is the feature where what you see on your laptop's
                 * screen is the same as your external monitor.  Here, "Mirror" is being
                 * used as an adjective, not as a verb.  For example, the Spanish
                 * translation could be "Pantallas en Espejo", *not* "Espejar Pantallas".
                 */
                text = _("Mirror Screens");
        } else
                text = output->display_name;

        return gtk_widget_create_pango_layout (
                                               GTK_WIDGET (page->priv->area), text);
}

static void
paint_background (FooScrollArea *area,
                  cairo_t       *cr)
{
        GdkRectangle viewport;
        GtkWidget   *widget;

        widget = GTK_WIDGET (area);

        foo_scroll_area_get_viewport (area, &viewport);

        cairo_set_source_rgb (cr,
                              widget->style->base[GTK_STATE_SELECTED].red / 65535.0,
                              widget->style->base[GTK_STATE_SELECTED].green / 65535.0,
                              widget->style->base[GTK_STATE_SELECTED].blue / 65535.0);

        cairo_rectangle (cr,
                         viewport.x,
                         viewport.y,
                         viewport.width,
                         viewport.height);

        cairo_fill_preserve (cr);

#if 0
        foo_scroll_area_add_input_from_fill (area, cr, (FooScrollAreaEventFunc) on_canvas_event, NULL);
#endif

        cairo_set_source_rgb (cr,
                              widget->style->dark[GTK_STATE_SELECTED].red / 65535.0,
                              widget->style->dark[GTK_STATE_SELECTED].green / 65535.0,
                              widget->style->dark[GTK_STATE_SELECTED].blue / 65535.0);

        cairo_stroke (cr);
}

static void
paint_output (CcDisplayPage *page,
              cairo_t       *cr,
              int            i)
{
        int              w, h;
        double           scale;
        double           x, y;
        int              total_w;
        int              total_h;
        GList           *connected_outputs;
        GnomeOutputInfo *output;
        PangoLayout     *layout;
        PangoRectangle   ink_extent;
        PangoRectangle   log_extent;
        GdkRectangle     viewport;
        GdkColor         output_color;
        double           r, g, b;
        double           available_w;
        double           factor;

        scale = compute_scale (page);
        connected_outputs = list_connected_outputs (page,
                                                    &total_w,
                                                    &total_h);
        output = g_list_nth (connected_outputs, i)->data;
        layout = get_display_name (page, output);

        cairo_save (cr);

        foo_scroll_area_get_viewport (FOO_SCROLL_AREA (page->priv->area), &viewport);

        get_geometry (output, &w, &h);

#if 0
        g_debug ("%s (%p) geometry %d %d %d", output->name, output,
                 w, h, output->rate);
#endif

        viewport.height -= 2 * MARGIN;
        viewport.width -= 2 * MARGIN;

        x = output->x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0;
        y = output->y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0;

#if 0
        g_debug ("scaled: %f %f", x, y);

        g_debug ("scale: %f", scale);

        g_debug ("%f %f %f %f", x, y, w * scale + 0.5, h * scale + 0.5);
#endif

        cairo_save (cr);

        cairo_translate (cr,
                         x + (w * scale + 0.5) / 2,
                         y + (h * scale + 0.5) / 2);

        /* rotation is already applied in get_geometry */

        if (output->rotation & GNOME_RR_REFLECT_X)
                cairo_scale (cr, -1, 1);

        if (output->rotation & GNOME_RR_REFLECT_Y)
                cairo_scale (cr, 1, -1);

        cairo_translate (cr,
                         - x - (w * scale + 0.5) / 2,
                         - y - (h * scale + 0.5) / 2);


        cairo_rectangle (cr, x, y, w * scale + 0.5, h * scale + 0.5);
        cairo_clip_preserve (cr);

        gnome_rr_labeler_get_color_for_output (page->priv->labeler, output, &output_color);
        r = output_color.red / 65535.0;
        g = output_color.green / 65535.0;
        b = output_color.blue / 65535.0;

        if (!output->on) {
                /* If the output is turned off, just darken the selected color */
                r *= 0.2;
                g *= 0.2;
                b *= 0.2;
        }

        cairo_set_source_rgba (cr, r, g, b, 1.0);

        foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (page->priv->area),
                                             cr,
                                             (FooScrollAreaEventFunc) on_output_event,
                                             output);
        cairo_fill (cr);

        if (output == page->priv->current_output) {
                cairo_rectangle (cr, x + 2, y + 2, w * scale + 0.5 - 4, h * scale + 0.5 - 4);

                cairo_set_line_width (cr, 4);
                cairo_set_source_rgba (cr, 0.33, 0.43, 0.57, 1.0);
                cairo_stroke (cr);
        }

        cairo_rectangle (cr, x + 0.5, y + 0.5, w * scale + 0.5 - 1, h * scale + 0.5 - 1);

        cairo_set_line_width (cr, 1);
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

        cairo_stroke (cr);
        cairo_set_line_width (cr, 2);

        layout_set_font (layout, "Sans Bold 12");
        pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);

        available_w = w * scale + 0.5 - 6; /* Same as the inner rectangle's width, minus 1 pixel of padding on each side */
        if (available_w < ink_extent.width)
                factor = available_w / ink_extent.width;
        else
                factor = 1.0;

        cairo_move_to (cr,
                       x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                       y + ((h * scale + 0.5) - factor * log_extent.height) / 2);

        cairo_scale (cr, factor, factor);

        if (output->on)
                cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        else
                cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

        pango_cairo_show_layout (cr, layout);

        cairo_restore (cr);

        g_object_unref (layout);
}

static void
on_area_paint (FooScrollArea *area,
               cairo_t       *cr,
               GdkRectangle  *extent,
               GdkRegion     *region,
               CcDisplayPage *page)
{
        double scale;
        GList *connected_outputs = NULL;
        GList *list;

        paint_background (area, cr);

        if (!page->priv->current_configuration)
                return;

        scale = compute_scale (page);
        connected_outputs = list_connected_outputs (page, NULL, NULL);

#if 0
        g_debug ("scale: %f", scale);
#endif

        for (list = connected_outputs; list != NULL; list = list->next) {
                paint_output (page,
                              cr,
                              g_list_position (connected_outputs, list));

                if (page->priv->current_configuration->clone)
                        break;
        }
}

static void
make_text_combo (GtkWidget *widget,
                 int        sort_column)
{
        GtkCellRenderer *cell;
        GtkComboBox     *box = GTK_COMBO_BOX (widget);
        GtkListStore    *store;

        store = gtk_list_store_new (6,
                                    G_TYPE_STRING,          /* Text */
                                    G_TYPE_INT,             /* Width */
                                    G_TYPE_INT,             /* Height */
                                    G_TYPE_INT,             /* Frequency */
                                    G_TYPE_INT,             /* Width * Height */
                                    G_TYPE_INT);            /* Rotation */

        gtk_cell_layout_clear (GTK_CELL_LAYOUT (widget));

        gtk_combo_box_set_model (box, GTK_TREE_MODEL (store));

        cell = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (box), cell, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (box), cell,
                                        "text", 0,
                                        NULL);

        if (sort_column != -1) {
                gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                                      sort_column,
                                                      GTK_SORT_DESCENDING);
        }
}

static void
compute_virtual_size_for_configuration (GnomeRRConfig *config,
                                        int           *ret_width,
                                        int           *ret_height)
{
        int i;
        int width;
        int height;

        width = height = 0;

        for (i = 0; config->outputs[i] != NULL; i++) {
                GnomeOutputInfo *output;

                output = config->outputs[i];

                if (output->on) {
                        width = MAX (width, output->x + output->width);
                        height = MAX (height, output->y + output->height);
                }
        }

        *ret_width = width;
        *ret_height = height;
}

static void
check_required_virtual_size (CcDisplayPage *page)
{
        int req_width;
        int req_height;
        int min_width;
        int max_width;
        int min_height;
        int max_height;

        compute_virtual_size_for_configuration (page->priv->current_configuration,
                                                &req_width,
                                                &req_height);

        gnome_rr_screen_get_ranges (page->priv->screen,
                                    &min_width,
                                    &max_width,
                                    &min_height,
                                    &max_height);

#if 0
        g_debug ("X Server supports:");
        g_debug ("min_width = %d, max_width = %d", min_width, max_width);
        g_debug ("min_height = %d, max_height = %d", min_height, max_height);

        g_debug ("Requesting size of %dx%d", req_width, req_height);
#endif

        if (!(min_width <= req_width && req_width <= max_width
              && min_height <= req_height && req_height <= max_height)) {
                /* FIXME: present a useful dialog, maybe even before the user tries to Apply */
#if 0
                g_debug ("Your X server needs a larger Virtual size!");
#endif
        }
}

static void
begin_version2_apply_configuration (CcDisplayPage *page,
                                    GdkWindow     *parent_window,
                                    guint32        timestamp)
{
        XID parent_window_xid;

        parent_window_xid = GDK_WINDOW_XID (parent_window);

        page->priv->proxy = dbus_g_proxy_new_for_name (page->priv->connection,
                                                       "org.gnome.SettingsDaemon",
                                                       "/org/gnome/SettingsDaemon/XRANDR",
                                                       "org.gnome.SettingsDaemon.XRANDR_2");
        g_assert (page->priv->proxy != NULL); /* that call does not fail unless we pass bogus names */

        page->priv->apply_configuration_state = APPLYING_VERSION_2;
        page->priv->proxy_call = dbus_g_proxy_begin_call (page->priv->proxy,
                                                          "ApplyConfiguration",
                                                          (DBusGProxyCallNotify) apply_configuration_returned_cb,
                                                          page,
                                                          NULL,
                                                          G_TYPE_INT64,
                                                          (gint64) parent_window_xid,
                                                          G_TYPE_INT64,
                                                          (gint64) timestamp,
                                                          G_TYPE_INVALID,
                                                          G_TYPE_INVALID);
        /* FIXME: we don't check for page->priv->proxy_call == NULL, which could happen if
         * the connection was disconnected.  This is left as an exercise for the
         * reader.
         */
}

static void
begin_version1_apply_configuration (CcDisplayPage *page)
{
        page->priv->proxy = dbus_g_proxy_new_for_name (page->priv->connection,
                                                "org.gnome.SettingsDaemon",
                                                "/org/gnome/SettingsDaemon/XRANDR",
                                                "org.gnome.SettingsDaemon.XRANDR");
        g_assert (page->priv->proxy != NULL); /* that call does not fail unless we pass bogus names */

        page->priv->apply_configuration_state = APPLYING_VERSION_1;
        page->priv->proxy_call = dbus_g_proxy_begin_call (page->priv->proxy,
                                                          "ApplyConfiguration",
                                                          (DBusGProxyCallNotify) apply_configuration_returned_cb,
                                                          page,
                                                          NULL,
                                                          G_TYPE_INVALID,
                                                          G_TYPE_INVALID);
        /* FIXME: we don't check for page->priv->proxy_call == NULL, which could happen if
         * the connection was disconnected.  This is left as an exercise for the
         * reader.
         */
}

static void
ensure_current_configuration_is_saved (void)
{
        GnomeRRScreen *rr_screen;
        GnomeRRConfig *rr_config;

        /* Normally, gnome_rr_config_save() creates a backup file based on the
         * old monitors.xml.  However, if *that* file didn't exist, there is
         * nothing from which to create a backup.  So, here we'll save the
         * current/unchanged configuration and then let our caller call
         * gnome_rr_config_save() again with the new/changed configuration, so
         * that there *will* be a backup file in the end.
         */

        rr_screen = gnome_rr_screen_new (gdk_screen_get_default (),
                                         NULL,
                                         NULL,
                                         NULL); /* NULL-GError */
        if (!rr_screen)
                return;

        rr_config = gnome_rr_config_new_current (rr_screen);
        gnome_rr_config_save (rr_config, NULL); /* NULL-GError */

        gnome_rr_config_free (rr_config);
        gnome_rr_screen_destroy (rr_screen);
}

/* Callback for dbus_g_proxy_begin_call() */
static void
apply_configuration_returned_cb (DBusGProxy     *proxy,
                                 DBusGProxyCall *call_id,
                                 CcDisplayPage  *page)
{
        gboolean success;
        GError  *error;

        g_assert (call_id == page->priv->proxy_call);

        error = NULL;
        success = dbus_g_proxy_end_call (proxy, call_id, &error, G_TYPE_INVALID);

        if (!success) {
                if (page->priv->apply_configuration_state == APPLYING_VERSION_2
                    && g_error_matches (error, DBUS_GERROR, DBUS_GERROR_UNKNOWN_METHOD)) {
                        g_error_free (error);

                        g_object_unref (page->priv->proxy);
                        page->priv->proxy = NULL;

                        begin_version1_apply_configuration (page);
                        return;
                } else {
                        /* We don't pop up an error message; gnome-settings-daemon already does that
                         * in case the selected RANDR configuration could not be applied.
                         */
                        g_error_free (error);
                }
        }

        g_object_unref (page->priv->proxy);
        page->priv->proxy = NULL;

        dbus_g_connection_unref (page->priv->connection);
        page->priv->connection = NULL;
        page->priv->proxy_call = NULL;

        gtk_widget_set_sensitive (GTK_WIDGET (page), TRUE);
}

static void
apply (CcDisplayPage *page)
{
        GError    *error;
        GtkWidget *toplevel;


        gnome_rr_config_sanitize (page->priv->current_configuration);

        check_required_virtual_size (page);

        foo_scroll_area_invalidate (FOO_SCROLL_AREA (page->priv->area));

        ensure_current_configuration_is_saved ();

        error = NULL;
        if (!gnome_rr_config_save (page->priv->current_configuration, &error)) {
                error_message (page,
                               _("Could not save the monitor configuration"),
                               error->message);
                g_error_free (error);
                return;
        }

        g_assert (page->priv->connection == NULL);
        g_assert (page->priv->proxy == NULL);
        g_assert (page->priv->proxy_call == NULL);

        page->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (page->priv->connection == NULL) {
                error_message (page,
                               _("Could not get session bus while applying display configuration"),
                               error->message);
                g_error_free (error);
                return;
        }

        gtk_widget_set_sensitive (GTK_WIDGET (page), FALSE);

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = GTK_WIDGET (page);
        }

        begin_version2_apply_configuration (page,
                                            gtk_widget_get_window (toplevel),
                                            page->priv->apply_button_clicked_timestamp);
}

#if 0
/* Returns whether the graphics driver doesn't advertise RANDR 1.2 features, and just 1.0 */
static gboolean
driver_is_randr_10 (GnomeRRConfig *config)
{
        /* In the Xorg code, see xserver/randr/rrinfo.c:RRScanOldConfig().  It gets
         * called when the graphics driver doesn't support RANDR 1.2 yet, just 1.0.
         * In that case, the X server's base code (which supports RANDR 1.2) will
         * simulate having a single output called "default".  For drivers that *do*
         * support RANDR 1.2, the separate outputs will be named differently, we
         * hope.
         *
         * This heuristic is courtesy of Dirk Mueller <dmueller@suse.de>
         *
         * FIXME: however, we don't even check for XRRQueryVersion() returning 1.2, neither
         * here nor in gnome-desktop/libgnomedesktop*.c.  Do we need to check for that,
         * or is gnome_rr_screen_new()'s return value sufficient?
         */

        return (count_all_outputs (config) == 1 && strcmp (config->outputs[0]->name, "default") == 0);
}
#endif

static void
on_detect_displays (GtkWidget     *widget,
                    CcDisplayPage *page)
{
        GError *error;

        error = NULL;
        if (!gnome_rr_screen_refresh (page->priv->screen, &error)) {
                if (error != NULL) {
                        error_message (page,
                                       _("Could not detect displays"),
                                       error->message);
                        g_error_free (error);
                }
        }
}


static void
on_show_icon_toggled (GtkToggleButton *togglebutton,
                      CcDisplayPage   *page)
{
        GConfClient *client;
        client = gconf_client_get_default ();
        gconf_client_set_bool (client,
                               SHOW_ICON_KEY,
                               gtk_toggle_button_get_active (togglebutton),
                               NULL);
        g_object_unref (client);
}

static GnomeOutputInfo *
get_nearest_output (GnomeRRConfig *configuration,
                    int            x,
                    int            y)
{
        int i;
        int nearest_index;
        int nearest_dist;

        nearest_index = -1;
        nearest_dist = G_MAXINT;

        for (i = 0; configuration->outputs[i] != NULL; i++) {
                GnomeOutputInfo *output;
                int              dist_x;
                int              dist_y;

                output = configuration->outputs[i];

                if (!(output->connected && output->on))
                        continue;

                if (x < output->x)
                        dist_x = output->x - x;
                else if (x >= output->x + output->width)
                        dist_x = x - (output->x + output->width) + 1;
                else
                        dist_x = 0;

                if (y < output->y)
                        dist_y = output->y - y;
                else if (y >= output->y + output->height)
                        dist_y = y - (output->y + output->height) + 1;
                else
                        dist_y = 0;

                if (MIN (dist_x, dist_y) < nearest_dist) {
                        nearest_dist = MIN (dist_x, dist_y);
                        nearest_index = i;
                }
        }

        if (nearest_index != -1)
                return configuration->outputs[nearest_index];
        else
                return NULL;

}

/* Gets the output that contains the largest intersection with the window.
 * Logic stolen from gdk_screen_get_monitor_at_window().
 */
static GnomeOutputInfo *
get_output_for_window (GnomeRRConfig *configuration,
                       GdkWindow     *window)
{
        GdkRectangle win_rect;
        int          i;
        int          largest_area;
        int          largest_index;

        gdk_window_get_geometry (window,
                                 &win_rect.x,
                                 &win_rect.y,
                                 &win_rect.width,
                                 &win_rect.height,
                                 NULL);
        gdk_window_get_origin (window, &win_rect.x, &win_rect.y);

        largest_area = 0;
        largest_index = -1;

        for (i = 0; configuration->outputs[i] != NULL; i++) {
                GnomeOutputInfo *output;
                GdkRectangle     output_rect;
                GdkRectangle     intersection;

                output = configuration->outputs[i];

                output_rect.x      = output->x;
                output_rect.y      = output->y;
                output_rect.width  = output->width;
                output_rect.height = output->height;

                if (output->connected
                    && gdk_rectangle_intersect (&win_rect,
                                                &output_rect,
                                                &intersection)) {
                        int area;

                        area = intersection.width * intersection.height;
                        if (area > largest_area) {
                                largest_area = area;
                                largest_index = i;
                        }
                }
        }

        if (largest_index != -1)
                return configuration->outputs[largest_index];
        else
                return get_nearest_output (configuration,
                                           win_rect.x + win_rect.width / 2,
                                           win_rect.y + win_rect.height / 2);
}

/* We select the current output, i.e. select the one being edited, based on
 * which output is showing the configuration dialog.
 */
static void
select_current_output_from_dialog_position (CcDisplayPage *page)
{
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = GTK_WIDGET (page);
        }

        if (GTK_WIDGET_REALIZED (toplevel))
                page->priv->current_output = get_output_for_window (page->priv->current_configuration,
                                                                    gtk_widget_get_window (toplevel));
        else
                page->priv->current_output = NULL;

        rebuild_gui (page);
}

/* This is a GtkWidget show handler.  We wait for the page
 * to be mapped, and then we select the output which corresponds to the
 * monitor on which the dialog is being shown.
 */
static void
on_show (GtkWidget     *widget,
         CcDisplayPage *page)
{
        select_current_output_from_dialog_position (page);
}

static void
on_apply_button_clicked (GtkButton     *button,
                         CcDisplayPage *page)
{
        /* We simply store the timestamp at which the Apply button was clicked.
         * We'll just wait for the dialog to return from gtk_dialog_run(), and
         * *then* use the timestamp when applying the RANDR configuration.
         */

        page->priv->apply_button_clicked_timestamp = gtk_get_current_event_time ();

        apply (page);
}

static void
on_screen_changed (GnomeRRScreen *scr,
                   CcDisplayPage *page)
{
        GnomeRRConfig *current;

        current = gnome_rr_config_new_current (page->priv->screen);

        if (page->priv->current_configuration)
                gnome_rr_config_free (page->priv->current_configuration);

        page->priv->current_configuration = current;
        page->priv->current_output = NULL;

        if (page->priv->labeler) {
                gnome_rr_labeler_hide (page->priv->labeler);
                g_object_unref (page->priv->labeler);
        }

        page->priv->labeler = gnome_rr_labeler_new (page->priv->current_configuration);

        select_current_output_from_dialog_position (page);
}

static void
on_viewport_changed (FooScrollArea *scroll_area,
                     GdkRectangle  *old_viewport,
                     GdkRectangle  *new_viewport)
{
        foo_scroll_area_set_size (scroll_area,
                                  new_viewport->width,
                                  new_viewport->height);

        foo_scroll_area_invalidate (scroll_area);
}

static void
setup_page (CcDisplayPage *page)
{
        GtkBuilder      *builder;
        GtkWidget       *widget;
        GtkWidget       *align;
        GError          *error;
        GConfClient     *client;

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   UIDIR
                                   "/display-capplet.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }

        page->priv->screen = gnome_rr_screen_new (gdk_screen_get_default (),
                                                  (GnomeRRScreenChanged) on_screen_changed,
                                                  page,
                                                  &error);
        if (page->priv->screen == NULL) {
                error_message (NULL,
                               _("Could not get screen information"),
                               error->message);
                g_error_free (error);
                g_object_unref (builder);
                return;
        }

        g_signal_connect_after (page,
                                "show",
                                G_CALLBACK (on_show),
                                page);

        page->priv->current_monitor_event_box = WID ("current_monitor_event_box");
        page->priv->current_monitor_label = WID ("current_monitor_label");

        page->priv->monitor_on_radio = WID ("monitor_on_radio");
        page->priv->monitor_off_radio = WID ("monitor_off_radio");
        g_signal_connect (page->priv->monitor_on_radio,
                          "toggled",
                          G_CALLBACK (monitor_on_off_toggled_cb),
                          page);
        g_signal_connect (page->priv->monitor_off_radio,
                          "toggled",
                          G_CALLBACK (monitor_on_off_toggled_cb),
                          page);

        page->priv->resolution_combo = WID ("resolution_combo");
        g_signal_connect (page->priv->resolution_combo,
                          "changed",
                          G_CALLBACK (on_resolution_changed),
                          page);

        page->priv->refresh_combo = WID ("refresh_combo");
        g_signal_connect (page->priv->refresh_combo,
                          "changed",
                          G_CALLBACK (on_rate_changed),
                          page);

        page->priv->rotation_combo = WID ("rotation_combo");
        g_signal_connect (page->priv->rotation_combo,
                          "changed",
                          G_CALLBACK (on_rotation_changed),
                          page);

        page->priv->clone_checkbox = WID ("clone_checkbox");
        g_signal_connect (page->priv->clone_checkbox,
                          "toggled",
                          G_CALLBACK (on_clone_changed),
                          page);

        g_signal_connect (WID ("detect_displays_button"),
                          "clicked",
                          G_CALLBACK (on_detect_displays),
                          page);

        page->priv->show_icon_checkbox = WID ("show_notification_icon");

        client = gconf_client_get_default ();

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page->priv->show_icon_checkbox),
                                      gconf_client_get_bool (client,
                                                             SHOW_ICON_KEY,
                                                             NULL));
        g_object_unref (client);

        g_signal_connect (page->priv->show_icon_checkbox,
                          "toggled",
                          G_CALLBACK (on_show_icon_toggled),
                          page);

        page->priv->panel_checkbox = WID ("panel_checkbox");

        make_text_combo (page->priv->resolution_combo, 4);
        make_text_combo (page->priv->refresh_combo, 3);
        make_text_combo (page->priv->rotation_combo, -1);

        g_assert (page->priv->panel_checkbox);

        /* Scroll Area */
        page->priv->area = (GtkWidget *)foo_scroll_area_new ();

        g_object_set_data (G_OBJECT (page->priv->area), "page", page);

        /* FIXME: this should be computed dynamically */
        foo_scroll_area_set_min_size (FOO_SCROLL_AREA (page->priv->area),
                                      -1,
                                      200);
        gtk_widget_show (page->priv->area);
        g_signal_connect (page->priv->area,
                          "paint",
                          G_CALLBACK (on_area_paint),
                          page);
        g_signal_connect (page->priv->area,
                          "viewport_changed",
                          G_CALLBACK (on_viewport_changed),
                          page);

        align = WID ("align");
        gtk_container_add (GTK_CONTAINER (align), page->priv->area);

        page->priv->apply_button = WID ("apply-button");
        g_signal_connect (page->priv->apply_button,
                          "clicked",
                          G_CALLBACK (on_apply_button_clicked),
                          page);

        on_screen_changed (page->priv->screen, page);

        widget = WID ("main_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);

        g_object_unref (builder);
}

static GObject *
cc_display_page_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_properties)
{
        CcDisplayPage      *display_page;

        display_page = CC_DISPLAY_PAGE (G_OBJECT_CLASS (cc_display_page_parent_class)->constructor (type,
                                                                                                    n_construct_properties,
                                                                                                    construct_properties));

        g_object_set (display_page,
                      "display-name", _("Display"),
                      "id", "general",
                      NULL);

        setup_page (display_page);

        return G_OBJECT (display_page);
}

static void
cc_display_page_class_init (CcDisplayPageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_display_page_get_property;
        object_class->set_property = cc_display_page_set_property;
        object_class->constructor = cc_display_page_constructor;
        object_class->finalize = cc_display_page_finalize;

        g_type_class_add_private (klass, sizeof (CcDisplayPagePrivate));
}

static void
cc_display_page_init (CcDisplayPage *page)
{
        page->priv = CC_DISPLAY_PAGE_GET_PRIVATE (page);
}

static void
cc_display_page_finalize (GObject *object)
{
        CcDisplayPage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_DISPLAY_PAGE (object));

        page = CC_DISPLAY_PAGE (object);

        g_return_if_fail (page->priv != NULL);

        gnome_rr_screen_destroy (page->priv->screen);

        G_OBJECT_CLASS (cc_display_page_parent_class)->finalize (object);
}

CcPage *
cc_display_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_DISPLAY_PAGE, NULL);

        return CC_PAGE (object);
}
