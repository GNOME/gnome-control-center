/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

#include <pulse/pulseaudio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <canberra-gtk.h>

#include "gvc-channel-bar.h"
#include "gvc-mixer-control.h"

#define SCALE_SIZE 128
#define ADJUSTMENT_MAX_NORMAL gvc_mixer_control_get_vol_max_norm(NULL)
#define ADJUSTMENT_MAX_AMPLIFIED gvc_mixer_control_get_vol_max_amplified(NULL)
#define ADJUSTMENT_MAX (bar->is_amplified ? ADJUSTMENT_MAX_AMPLIFIED : ADJUSTMENT_MAX_NORMAL)
#define SCROLLSTEP (ADJUSTMENT_MAX / 100.0 * 5.0)

struct _GvcChannelBar
{
        GtkBox         parent_instance;

        GtkOrientation orientation;
        GtkWidget     *scale_box;
        GtkWidget     *start_box;
        GtkWidget     *end_box;
        GtkWidget     *image;
        GtkWidget     *label;
        GtkWidget     *low_image;
        GtkWidget     *scale;
        GtkWidget     *high_image;
        GtkWidget     *mute_switch;
        GtkAdjustment *adjustment;
        GtkAdjustment *zero_adjustment;
        gboolean       show_mute;
        gboolean       is_muted;
        char          *name;
        char          *icon_name;
        char          *low_icon_name;
        char          *high_icon_name;
        GtkSizeGroup  *size_group;
        gboolean       symmetric;
        gboolean       click_lock;
        gboolean       is_amplified;
        guint32        base_volume;
};

enum
{
        PROP_0,
        PROP_ORIENTATION,
        PROP_SHOW_MUTE,
        PROP_IS_MUTED,
        PROP_ADJUSTMENT,
        PROP_NAME,
        PROP_ICON_NAME,
        PROP_LOW_ICON_NAME,
        PROP_HIGH_ICON_NAME,
        PROP_IS_AMPLIFIED,
        PROP_ELLIPSIZE
};

static void     gvc_channel_bar_class_init    (GvcChannelBarClass *klass);
static void     gvc_channel_bar_init          (GvcChannelBar      *channel_bar);
static void     gvc_channel_bar_finalize      (GObject            *object);

static gboolean on_scale_button_press_event   (GtkWidget      *widget,
                                               GdkEventButton *event,
                                               GvcChannelBar  *bar);
static gboolean on_scale_button_release_event (GtkWidget      *widget,
                                               GdkEventButton *event,
                                               GvcChannelBar  *bar);
static gboolean on_scale_scroll_event         (GtkWidget      *widget,
                                               GdkEventScroll *event,
                                               GvcChannelBar  *bar);

G_DEFINE_TYPE (GvcChannelBar, gvc_channel_bar, GTK_TYPE_BOX)

static GtkWidget *
_scale_box_new (GvcChannelBar *bar)
{
        GtkWidget            *box;
        GtkWidget            *sbox;
        GtkWidget            *ebox;

        if (bar->orientation == GTK_ORIENTATION_VERTICAL) {
                bar->scale_box = box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

                bar->scale = gtk_scale_new (GTK_ORIENTATION_VERTICAL, bar->adjustment);
                gtk_widget_show (bar->scale);

                gtk_widget_set_size_request (bar->scale, -1, SCALE_SIZE);
                gtk_range_set_inverted (GTK_RANGE (bar->scale), TRUE);

                bar->start_box = sbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_show (sbox);
                gtk_box_pack_start (GTK_BOX (box), sbox, FALSE, FALSE, 0);

                gtk_box_pack_start (GTK_BOX (sbox), bar->image, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (sbox), bar->label, FALSE, FALSE, 0);

                gtk_box_pack_start (GTK_BOX (sbox), bar->high_image, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (box), bar->scale, TRUE, TRUE, 0);

                bar->end_box = ebox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_show (ebox);
                gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, FALSE, 0);

                gtk_box_pack_start (GTK_BOX (ebox), bar->low_image, FALSE, FALSE, 0);

                gtk_box_pack_start (GTK_BOX (ebox), bar->mute_switch, FALSE, FALSE, 0);
        } else {
                bar->scale_box = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
                gtk_box_pack_start (GTK_BOX (box), bar->image, FALSE, FALSE, 0);

                bar->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, bar->adjustment);
                gtk_widget_show (bar->scale);

                gtk_widget_set_size_request (bar->scale, SCALE_SIZE, -1);

                bar->start_box = sbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
                gtk_widget_show (sbox);
                gtk_box_pack_start (GTK_BOX (box), sbox, FALSE, FALSE, 0);

                gtk_box_pack_end (GTK_BOX (sbox), bar->low_image, FALSE, FALSE, 0);
                gtk_widget_show (bar->low_image);

                gtk_box_pack_start (GTK_BOX (sbox), bar->label, TRUE, TRUE, 0);
                gtk_box_pack_start (GTK_BOX (box), bar->scale, TRUE, TRUE, 0);

                bar->end_box = ebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
                gtk_widget_show (ebox);
                gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, FALSE, 0);

                gtk_box_pack_start (GTK_BOX (ebox), bar->high_image, FALSE, FALSE, 0);
                gtk_widget_show (bar->high_image);
                gtk_box_pack_start (GTK_BOX (ebox), bar->mute_switch, FALSE, FALSE, 0);
                gtk_widget_show (bar->mute_switch);
        }

        ca_gtk_widget_disable_sounds (bar->scale, FALSE);
        gtk_widget_add_events (bar->scale, GDK_SCROLL_MASK);

        g_signal_connect (G_OBJECT (bar->scale), "button-press-event",
                          G_CALLBACK (on_scale_button_press_event), bar);
        g_signal_connect (G_OBJECT (bar->scale), "button-release-event",
                          G_CALLBACK (on_scale_button_release_event), bar);
        g_signal_connect (G_OBJECT (bar->scale), "scroll-event",
                          G_CALLBACK (on_scale_scroll_event), bar);

        if (bar->size_group != NULL) {
                gtk_size_group_add_widget (bar->size_group, sbox);

                if (bar->symmetric) {
                        gtk_size_group_add_widget (bar->size_group, ebox);
                }
        }

        gtk_scale_set_draw_value (GTK_SCALE (bar->scale), FALSE);

        return box;
}

static void
update_image (GvcChannelBar *bar)
{
        gtk_image_set_from_icon_name (GTK_IMAGE (bar->image),
                                      bar->icon_name,
                                      GTK_ICON_SIZE_DIALOG);

        if (bar->icon_name != NULL) {
                gtk_widget_show (bar->image);
        } else {
                gtk_widget_hide (bar->image);
        }
}

static void
update_label (GvcChannelBar *bar)
{
        if (bar->name != NULL) {
                gtk_label_set_text_with_mnemonic (GTK_LABEL (bar->label),
                                                  bar->name);
                gtk_label_set_mnemonic_widget (GTK_LABEL (bar->label),
                                               bar->scale);
                gtk_widget_show (bar->label);
        } else {
                gtk_label_set_text (GTK_LABEL (bar->label), NULL);
                gtk_widget_hide (bar->label);
        }
}

static void
update_layout (GvcChannelBar *bar)
{
        GtkWidget *box;
        GtkWidget *frame;

        if (bar->scale == NULL) {
                return;
        }

        box = bar->scale_box;
        frame = gtk_widget_get_parent (box);

        g_object_ref (bar->image);
        g_object_ref (bar->label);
        g_object_ref (bar->mute_switch);
        g_object_ref (bar->low_image);
        g_object_ref (bar->high_image);

        gtk_container_remove (GTK_CONTAINER (bar->start_box), bar->image);
        gtk_container_remove (GTK_CONTAINER (bar->start_box), bar->label);
        gtk_container_remove (GTK_CONTAINER (bar->end_box), bar->mute_switch);

        if (bar->orientation == GTK_ORIENTATION_VERTICAL) {
                gtk_container_remove (GTK_CONTAINER (bar->start_box), bar->low_image);
                gtk_container_remove (GTK_CONTAINER (bar->end_box), bar->high_image);
        } else {
                gtk_container_remove (GTK_CONTAINER (bar->end_box), bar->low_image);
                gtk_container_remove (GTK_CONTAINER (bar->start_box), bar->high_image);
        }

        gtk_container_remove (GTK_CONTAINER (box), bar->start_box);
        gtk_container_remove (GTK_CONTAINER (box), bar->scale);
        gtk_container_remove (GTK_CONTAINER (box), bar->end_box);
        gtk_container_remove (GTK_CONTAINER (frame), box);

        bar->scale_box = _scale_box_new (bar);
        gtk_widget_show (bar->scale_box);
        gtk_container_add (GTK_CONTAINER (frame), bar->scale_box);

        g_object_unref (bar->image);
        g_object_unref (bar->label);
        g_object_unref (bar->mute_switch);
        g_object_unref (bar->low_image);
        g_object_unref (bar->high_image);
}

void
gvc_channel_bar_set_size_group (GvcChannelBar *bar,
                                GtkSizeGroup  *group,
                                gboolean       symmetric)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        bar->size_group = group;
        bar->symmetric = symmetric;

        if (bar->size_group != NULL) {
                gtk_size_group_add_widget (bar->size_group,
                                           bar->start_box);

                if (bar->symmetric) {
                        gtk_size_group_add_widget (bar->size_group,
                                                   bar->end_box);
                }
        }
        gtk_widget_queue_draw (GTK_WIDGET (bar));
}

void
gvc_channel_bar_set_name (GvcChannelBar  *bar,
                          const char     *name)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        g_free (bar->name);
        bar->name = g_strdup (name);
        update_label (bar);
        g_object_notify (G_OBJECT (bar), "name");
}

void
gvc_channel_bar_set_icon_name (GvcChannelBar  *bar,
                               const char     *name)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        g_free (bar->icon_name);
        bar->icon_name = g_strdup (name);
        update_image (bar);
        g_object_notify (G_OBJECT (bar), "icon-name");
}

void
gvc_channel_bar_set_low_icon_name   (GvcChannelBar *bar,
                                     const char    *name)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (name != NULL && strcmp (bar->low_icon_name, name) != 0) {
                g_free (bar->low_icon_name);
                bar->low_icon_name = g_strdup (name);
                gtk_image_set_from_icon_name (GTK_IMAGE (bar->low_image),
                                              bar->low_icon_name,
                                              GTK_ICON_SIZE_MENU);
                g_object_notify (G_OBJECT (bar), "low-icon-name");
        }
}

void
gvc_channel_bar_set_high_icon_name  (GvcChannelBar *bar,
                                     const char    *name)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (name != NULL && strcmp (bar->high_icon_name, name) != 0) {
                g_free (bar->high_icon_name);
                bar->high_icon_name = g_strdup (name);
                gtk_image_set_from_icon_name (GTK_IMAGE (bar->high_image),
                                              bar->high_icon_name,
                                              GTK_ICON_SIZE_MENU);
                g_object_notify (G_OBJECT (bar), "high-icon-name");
        }
}

void
gvc_channel_bar_set_orientation (GvcChannelBar  *bar,
                                 GtkOrientation  orientation)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (orientation != bar->orientation) {
                bar->orientation = orientation;
                update_layout (bar);
                g_object_notify (G_OBJECT (bar), "orientation");
        }
}

static void
gvc_channel_bar_set_adjustment (GvcChannelBar *bar,
                                GtkAdjustment *adjustment)
{
        g_return_if_fail (GVC_CHANNEL_BAR (bar));
        g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

        if (bar->adjustment != NULL) {
                g_object_unref (bar->adjustment);
        }
        bar->adjustment = g_object_ref_sink (adjustment);

        if (bar->scale != NULL) {
                gtk_range_set_adjustment (GTK_RANGE (bar->scale), adjustment);
        }

        g_object_notify (G_OBJECT (bar), "adjustment");
}

GtkAdjustment *
gvc_channel_bar_get_adjustment (GvcChannelBar *bar)
{
        g_return_val_if_fail (GVC_IS_CHANNEL_BAR (bar), NULL);

        return bar->adjustment;
}

static gboolean
on_scale_button_press_event (GtkWidget      *widget,
                             GdkEventButton *event,
                             GvcChannelBar  *bar)
{
        bar->click_lock = TRUE;

        return FALSE;
}

static gboolean
on_scale_button_release_event (GtkWidget      *widget,
                               GdkEventButton *event,
                               GvcChannelBar  *bar)
{
        GtkAdjustment *adj;
        gdouble value;

        bar->click_lock = FALSE;

        adj = gtk_range_get_adjustment (GTK_RANGE (widget));

        value = gtk_adjustment_get_value (adj);

        /* this means the adjustment moved away from zero and
         * therefore we should unmute and set the volume. */
        gvc_channel_bar_set_is_muted (bar, (value == 0.0));

        /* Play a sound! */
        ca_gtk_play_for_widget (GTK_WIDGET (bar), 0,
                                CA_PROP_EVENT_ID, "audio-volume-change",
                                CA_PROP_EVENT_DESCRIPTION, "foobar event happened",
                                CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
                                NULL);

        return FALSE;
}

gboolean
gvc_channel_bar_scroll (GvcChannelBar *bar, GdkEventScroll *event)
{
        GtkAdjustment *adj;
        gdouble value;
        GdkScrollDirection direction;
        gdouble dx, dy;

        g_return_val_if_fail (bar != NULL, FALSE);
        g_return_val_if_fail (GVC_IS_CHANNEL_BAR (bar), FALSE);

        direction = event->direction;

        if (bar->orientation == GTK_ORIENTATION_VERTICAL) {
                if (direction == GDK_SCROLL_LEFT || direction == GDK_SCROLL_RIGHT)
                        return FALSE;
        } else {
                /* Switch direction for RTL */
                if (gtk_widget_get_direction (GTK_WIDGET (bar)) == GTK_TEXT_DIR_RTL) {
                        if (direction == GDK_SCROLL_RIGHT)
                                direction = GDK_SCROLL_LEFT;
                        else if (direction == GDK_SCROLL_LEFT)
                                direction = GDK_SCROLL_RIGHT;
                }
                /* Switch side scroll to vertical */
                if (direction == GDK_SCROLL_RIGHT)
                        direction = GDK_SCROLL_UP;
                else if (direction == GDK_SCROLL_LEFT)
                        direction = GDK_SCROLL_DOWN;
        }

	if (!gdk_event_get_scroll_deltas ((GdkEvent*)event, &dx, &dy)) {
		dx = 0.0;
		dy = 0.0;

		switch (direction) {
		case GDK_SCROLL_UP:
		case GDK_SCROLL_LEFT:
			dy = 1.0;
			break;
		case GDK_SCROLL_DOWN:
		case GDK_SCROLL_RIGHT:
			dy = -1.0;
			break;
		default:
			;
		}
	}

        adj = gtk_range_get_adjustment (GTK_RANGE (bar->scale));
        if (adj == bar->zero_adjustment) {
                if (dy > 0)
                        gvc_channel_bar_set_is_muted (bar, FALSE);
                return TRUE;
        }

        value = gtk_adjustment_get_value (adj);

        if (dy > 0) {
                if (value + dy * SCROLLSTEP > ADJUSTMENT_MAX)
                        value = ADJUSTMENT_MAX;
                else
                        value = value + dy * SCROLLSTEP;
        } else if (dy < 0) {
                if (value + dy * SCROLLSTEP < 0)
                        value = 0.0;
                else
                        value = value + dy * SCROLLSTEP;
        }

        gvc_channel_bar_set_is_muted (bar, (value == 0.0));
        adj = gtk_range_get_adjustment (GTK_RANGE (bar->scale));
        gtk_adjustment_set_value (adj, value);

        return TRUE;
}

static gboolean
on_scale_scroll_event (GtkWidget      *widget,
                       GdkEventScroll *event,
                       GvcChannelBar  *bar)
{
        return gvc_channel_bar_scroll (bar, event);
}

static void
on_zero_adjustment_value_changed (GtkAdjustment *adjustment,
                                  GvcChannelBar *bar)
{
        gdouble value;

        if (bar->click_lock != FALSE) {
                return;
        }

        value = gtk_adjustment_get_value (bar->zero_adjustment);
        gtk_adjustment_set_value (bar->adjustment, value);


        if (bar->show_mute == FALSE) {
                /* this means the adjustment moved away from zero and
                 * therefore we should unmute and set the volume. */
                gvc_channel_bar_set_is_muted (bar, value > 0.0);
        }
}

static void
update_mute_switch (GvcChannelBar *bar)
{
        if (bar->show_mute) {
                gtk_widget_show (bar->mute_switch);
                gtk_switch_set_active (GTK_SWITCH (bar->mute_switch),
                                       !bar->is_muted);
        } else {
                gtk_widget_hide (bar->mute_switch);
        }

        if (bar->is_muted) {
                /* If we aren't showing the mute button then
                 * move slider to the zero.  But we don't want to
                 * change the adjustment.  */
                g_signal_handlers_block_by_func (bar->zero_adjustment,
                                                 on_zero_adjustment_value_changed,
                                                 bar);
                gtk_adjustment_set_value (bar->zero_adjustment, 0);
                g_signal_handlers_unblock_by_func (bar->zero_adjustment,
                                                   on_zero_adjustment_value_changed,
                                                   bar);
                gtk_range_set_adjustment (GTK_RANGE (bar->scale),
                                          bar->zero_adjustment);
        } else {
                /* no longer muted so restore the original adjustment
                 * and tell the front-end that the value changed */
                gtk_range_set_adjustment (GTK_RANGE (bar->scale),
                                          bar->adjustment);
                gtk_adjustment_value_changed (bar->adjustment);
        }
}

void
gvc_channel_bar_set_is_muted (GvcChannelBar *bar,
                              gboolean       is_muted)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (is_muted != bar->is_muted) {
                /* Update our internal state before telling the
                 * front-end about our changes */
                bar->is_muted = is_muted;
                update_mute_switch (bar);
                g_object_notify (G_OBJECT (bar), "is-muted");
        }
}

gboolean
gvc_channel_bar_get_is_muted  (GvcChannelBar *bar)
{
        g_return_val_if_fail (GVC_IS_CHANNEL_BAR (bar), FALSE);
        return bar->is_muted;
}

void
gvc_channel_bar_set_show_mute (GvcChannelBar *bar,
                               gboolean       show_mute)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (show_mute != bar->show_mute) {
                bar->show_mute = show_mute;
                g_object_notify (G_OBJECT (bar), "show-mute");
                update_mute_switch (bar);
        }
}

gboolean
gvc_channel_bar_get_show_mute (GvcChannelBar *bar)
{
        g_return_val_if_fail (GVC_IS_CHANNEL_BAR (bar), FALSE);
        return bar->show_mute;
}

void
gvc_channel_bar_set_is_amplified (GvcChannelBar *bar, gboolean amplified)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        bar->is_amplified = amplified;
        gtk_adjustment_set_upper (bar->adjustment, ADJUSTMENT_MAX);
        gtk_adjustment_set_upper (bar->zero_adjustment, ADJUSTMENT_MAX);
        gtk_scale_clear_marks (GTK_SCALE (bar->scale));

        if (amplified) {
                g_autofree gchar *str = NULL;

                if (bar->base_volume == ADJUSTMENT_MAX_NORMAL) {
                        str = g_strdup_printf ("<small>%s</small>", C_("volume", "100%"));
                        gtk_scale_add_mark (GTK_SCALE (bar->scale), ADJUSTMENT_MAX_NORMAL,
                                            GTK_POS_BOTTOM, str);
                } else {
                        str = g_strdup_printf ("<small>%s</small>", C_("volume", "Unamplified"));
                        gtk_scale_add_mark (GTK_SCALE (bar->scale), bar->base_volume,
                                            GTK_POS_BOTTOM, str);
                        /* Only show 100% if it's higher than the base volume */
                        if (bar->base_volume < ADJUSTMENT_MAX_NORMAL) {
                                str = g_strdup_printf ("<small>%s</small>", C_("volume", "100%"));
                                gtk_scale_add_mark (GTK_SCALE (bar->scale), ADJUSTMENT_MAX_NORMAL,
                                                    GTK_POS_BOTTOM, str);
                        }
                }

                /* Ideally we would use baseline alignment for all
                 * these widgets plus the scale but neither GtkScale
                 * nor GtkSwitch support baseline alignment yet. */

                gtk_widget_set_valign (bar->mute_switch, GTK_ALIGN_START);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_misc_set_alignment (GTK_MISC (bar->low_image), 0.5, 0.15);
                gtk_misc_set_alignment (GTK_MISC (bar->high_image), 0.5, 0.15);
G_GNUC_END_IGNORE_DEPRECATIONS
                gtk_widget_set_valign (bar->label, GTK_ALIGN_START);
        } else {
                gtk_widget_set_valign (bar->mute_switch, GTK_ALIGN_CENTER);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_misc_set_alignment (GTK_MISC (bar->low_image), 0.5, 0.5);
                gtk_misc_set_alignment (GTK_MISC (bar->high_image), 0.5, 0.5);
G_GNUC_END_IGNORE_DEPRECATIONS
                gtk_widget_set_valign (bar->label, GTK_ALIGN_CENTER);
        }
}

gboolean
gvc_channel_bar_get_ellipsize (GvcChannelBar *bar)
{
        g_return_val_if_fail (GVC_IS_CHANNEL_BAR (bar), FALSE);

        return gtk_label_get_ellipsize (GTK_LABEL (bar->label)) != PANGO_ELLIPSIZE_NONE;
}

void
gvc_channel_bar_set_ellipsize (GvcChannelBar *bar,
                               gboolean       ellipsized)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (ellipsized)
                gtk_label_set_ellipsize (GTK_LABEL (bar->label), PANGO_ELLIPSIZE_END);
	else
                gtk_label_set_ellipsize (GTK_LABEL (bar->label), PANGO_ELLIPSIZE_NONE);
}

void
gvc_channel_bar_set_base_volume (GvcChannelBar *bar,
                                 pa_volume_t    base_volume)
{
        g_return_if_fail (GVC_IS_CHANNEL_BAR (bar));

        if (base_volume == 0) {
                bar->base_volume = ADJUSTMENT_MAX_NORMAL;
                return;
        }

        /* Note that you need to call _is_amplified() afterwards to update the marks */
        bar->base_volume = base_volume;
}

static void
gvc_channel_bar_set_property (GObject       *object,
                              guint          prop_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
        GvcChannelBar *self = GVC_CHANNEL_BAR (object);

        switch (prop_id) {
        case PROP_ORIENTATION:
                gvc_channel_bar_set_orientation (self, g_value_get_enum (value));
                break;
        case PROP_IS_MUTED:
                gvc_channel_bar_set_is_muted (self, g_value_get_boolean (value));
                break;
        case PROP_SHOW_MUTE:
                gvc_channel_bar_set_show_mute (self, g_value_get_boolean (value));
                break;
        case PROP_NAME:
                gvc_channel_bar_set_name (self, g_value_get_string (value));
                break;
        case PROP_ICON_NAME:
                gvc_channel_bar_set_icon_name (self, g_value_get_string (value));
                break;
        case PROP_LOW_ICON_NAME:
                gvc_channel_bar_set_low_icon_name (self, g_value_get_string (value));
                break;
        case PROP_HIGH_ICON_NAME:
                gvc_channel_bar_set_high_icon_name (self, g_value_get_string (value));
                break;
        case PROP_ADJUSTMENT:
                gvc_channel_bar_set_adjustment (self, g_value_get_object (value));
                break;
        case PROP_IS_AMPLIFIED:
                gvc_channel_bar_set_is_amplified (self, g_value_get_boolean (value));
                break;
        case PROP_ELLIPSIZE:
                gvc_channel_bar_set_ellipsize (self, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_channel_bar_get_property (GObject     *object,
                              guint        prop_id,
                              GValue      *value,
                              GParamSpec  *pspec)
{
        GvcChannelBar *self = GVC_CHANNEL_BAR (object);

        switch (prop_id) {
        case PROP_ORIENTATION:
                g_value_set_enum (value, self->orientation);
                break;
        case PROP_IS_MUTED:
                g_value_set_boolean (value, self->is_muted);
                break;
        case PROP_SHOW_MUTE:
                g_value_set_boolean (value, self->show_mute);
                break;
        case PROP_NAME:
                g_value_set_string (value, self->name);
                break;
        case PROP_ICON_NAME:
                g_value_set_string (value, self->icon_name);
                break;
        case PROP_LOW_ICON_NAME:
                g_value_set_string (value, self->low_icon_name);
                break;
        case PROP_HIGH_ICON_NAME:
                g_value_set_string (value, self->high_icon_name);
                break;
        case PROP_ADJUSTMENT:
                g_value_set_object (value, gvc_channel_bar_get_adjustment (self));
                break;
        case PROP_IS_AMPLIFIED:
                g_value_set_boolean (value, self->is_amplified);
                break;
        case PROP_ELLIPSIZE:
                g_value_set_boolean (value, gvc_channel_bar_get_ellipsize (self));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gvc_channel_bar_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_params)
{
        GObject       *object;
        GvcChannelBar *self;

        object = G_OBJECT_CLASS (gvc_channel_bar_parent_class)->constructor (type, n_construct_properties, construct_params);

        self = GVC_CHANNEL_BAR (object);

        update_mute_switch (self);

        return object;
}

static void
gvc_channel_bar_class_init (GvcChannelBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gvc_channel_bar_constructor;
        object_class->finalize = gvc_channel_bar_finalize;
        object_class->set_property = gvc_channel_bar_set_property;
        object_class->get_property = gvc_channel_bar_get_property;

        g_object_class_install_property (object_class,
                                         PROP_ORIENTATION,
                                         g_param_spec_enum ("orientation",
                                                            "Orientation",
                                                            "The orientation of the scale",
                                                            GTK_TYPE_ORIENTATION,
                                                            GTK_ORIENTATION_HORIZONTAL,
                                                            G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_IS_MUTED,
                                         g_param_spec_boolean ("is-muted",
                                                               "is muted",
                                                               "Whether stream is muted",
                                                               FALSE,
                                                               G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_SHOW_MUTE,
                                         g_param_spec_boolean ("show-mute",
                                                               "show mute",
                                                               "Whether stream is muted",
                                                               FALSE,
                                                               G_PARAM_READWRITE|G_PARAM_CONSTRUCT));

        g_object_class_install_property (object_class,
                                         PROP_ADJUSTMENT,
                                         g_param_spec_object ("adjustment",
                                                              "Adjustment",
                                                              "The GtkAdjustment that contains the current value of this scale button object",
                                                              GTK_TYPE_ADJUSTMENT,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_NAME,
                                         g_param_spec_string ("name",
                                                              "Name",
                                                              "Name to display for this stream",
                                                              NULL,
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_ICON_NAME,
                                         g_param_spec_string ("icon-name",
                                                              "Icon Name",
                                                              "Name of icon to display for this stream",
                                                              NULL,
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_LOW_ICON_NAME,
                                         g_param_spec_string ("low-icon-name",
                                                              "Icon Name",
                                                              "Name of icon to display for this stream",
                                                              "audio-volume-low-symbolic",
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_HIGH_ICON_NAME,
                                         g_param_spec_string ("high-icon-name",
                                                              "Icon Name",
                                                              "Name of icon to display for this stream",
                                                              "audio-volume-high-symbolic",
                                                              G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_IS_AMPLIFIED,
                                         g_param_spec_boolean ("is-amplified",
                                                               "Is amplified",
                                                               "Whether the stream is digitally amplified",
                                                               FALSE,
                                                               G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
        g_object_class_install_property (object_class,
                                         PROP_ELLIPSIZE,
                                         g_param_spec_boolean ("ellipsize",
                                                               "Label is ellipsized",
                                                               "Whether the label is ellipsized",
                                                               FALSE,
                                                               G_PARAM_READWRITE|G_PARAM_CONSTRUCT));
}

static void
on_mute_switch_toggled (GtkSwitch     *sw,
                        GParamSpec *pspec,
                        GvcChannelBar *bar)
{
        gboolean is_muted;
        is_muted = gtk_switch_get_active (sw);
        gvc_channel_bar_set_is_muted (bar, !is_muted);
}

static void
gvc_channel_bar_init (GvcChannelBar *bar)
{
        GtkWidget *frame;

        bar->base_volume = ADJUSTMENT_MAX_NORMAL;
        bar->low_icon_name = g_strdup ("audio-volume-low-symbolic");
        bar->high_icon_name = g_strdup ("audio-volume-high-symbolic");

        bar->orientation = GTK_ORIENTATION_HORIZONTAL;
        bar->adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                    0.0,
                                                                    ADJUSTMENT_MAX_NORMAL,
                                                                    ADJUSTMENT_MAX_NORMAL/100.0,
                                                                    ADJUSTMENT_MAX_NORMAL/10.0,
                                                                    0.0));
        g_object_ref_sink (bar->adjustment);

        bar->zero_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                         0.0,
                                                                         ADJUSTMENT_MAX_NORMAL,
                                                                         ADJUSTMENT_MAX_NORMAL/100.0,
                                                                         ADJUSTMENT_MAX_NORMAL/10.0,
                                                                         0.0));
        g_object_ref_sink (bar->zero_adjustment);

        g_signal_connect (bar->zero_adjustment,
                          "value-changed",
                          G_CALLBACK (on_zero_adjustment_value_changed),
                          bar);

        bar->mute_switch = gtk_switch_new ();
        gtk_widget_hide (bar->mute_switch);
        g_signal_connect (bar->mute_switch,
                          "notify::active",
                          G_CALLBACK (on_mute_switch_toggled),
                          bar);

        bar->low_image = gtk_image_new_from_icon_name ("audio-volume-low-symbolic",
                                                             GTK_ICON_SIZE_MENU);
        gtk_widget_hide (bar->low_image);
        gtk_style_context_add_class (gtk_widget_get_style_context (bar->low_image), "dim-label");
        bar->high_image = gtk_image_new_from_icon_name ("audio-volume-high-symbolic",
                                                              GTK_ICON_SIZE_MENU);
        gtk_widget_hide (bar->high_image);
        gtk_style_context_add_class (gtk_widget_get_style_context (bar->high_image), "dim-label");

        bar->image = gtk_image_new ();

        bar->label = gtk_label_new (NULL);
        gtk_widget_set_halign (bar->label, GTK_ALIGN_START);

        /* frame */
        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (bar), frame, TRUE, TRUE, 0);

        /* box with scale */
        bar->scale_box = _scale_box_new (bar);
        gtk_widget_show (bar->scale_box);

        gtk_container_add (GTK_CONTAINER (frame), bar->scale_box);
}

static void
gvc_channel_bar_finalize (GObject *object)
{
        GvcChannelBar *channel_bar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_CHANNEL_BAR (object));

        channel_bar = GVC_CHANNEL_BAR (object);

        g_return_if_fail (channel_bar != NULL);

        g_free (channel_bar->name);
        g_free (channel_bar->icon_name);
        g_free (channel_bar->low_icon_name);
        g_free (channel_bar->high_icon_name);

        G_OBJECT_CLASS (gvc_channel_bar_parent_class)->finalize (object);
}

GtkWidget *
gvc_channel_bar_new (void)
{
        GObject *bar;
        bar = g_object_new (GVC_TYPE_CHANNEL_BAR,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            NULL);
        return GTK_WIDGET (bar);
}
