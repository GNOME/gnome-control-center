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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <canberra-gtk.h>
#include <pulse/pulseaudio.h>

#include "gvc-balance-bar.h"
#include "gvc-channel-map-private.h"

#define SCALE_SIZE 128
#define ADJUSTMENT_MAX_NORMAL 65536.0 /* PA_VOLUME_NORM */

struct _GvcBalanceBar
{
        GtkBox         parent_instance;

        GvcChannelMap *channel_map;
        GvcBalanceType btype;
        GtkWidget     *scale_box;
        GtkWidget     *start_box;
        GtkWidget     *end_box;
        GtkWidget     *label;
        GtkWidget     *scale;
        GtkAdjustment *adjustment;
        GtkSizeGroup  *size_group;
        gboolean       symmetric;
        gboolean       click_lock;
};

enum
{
        PROP_0,
        PROP_CHANNEL_MAP,
        PROP_BALANCE_TYPE,
};

static void     gvc_balance_bar_class_init (GvcBalanceBarClass *klass);
static void     gvc_balance_bar_init       (GvcBalanceBar      *balance_bar);
static void     gvc_balance_bar_finalize   (GObject            *object);

static gboolean on_scale_button_press_event   (GtkWidget      *widget,
                                               GdkEventButton *event,
                                               GvcBalanceBar  *bar);
static gboolean on_scale_button_release_event (GtkWidget      *widget,
                                               GdkEventButton *event,
                                               GvcBalanceBar  *bar);
static gboolean on_scale_scroll_event         (GtkWidget      *widget,
                                               GdkEventScroll *event,
                                               GvcBalanceBar  *bar);
static void on_adjustment_value_changed       (GtkAdjustment *adjustment,
                                               GvcBalanceBar *bar);

G_DEFINE_TYPE (GvcBalanceBar, gvc_balance_bar, GTK_TYPE_BOX)

static GtkWidget *
_scale_box_new (GvcBalanceBar *bar)
{
        GtkWidget            *box;
        GtkWidget            *sbox;
        GtkWidget            *ebox;
        GtkAdjustment        *adjustment = bar->adjustment;
        char                 *str_lower, *str_upper;
        gdouble              lower, upper;

        bar->scale_box = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        bar->scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, bar->adjustment);
        gtk_widget_show (bar->scale);
        gtk_scale_set_has_origin (GTK_SCALE (bar->scale), FALSE);
        gtk_widget_set_size_request (bar->scale, SCALE_SIZE, -1);

        bar->start_box = sbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (sbox);
        gtk_box_pack_start (GTK_BOX (box), sbox, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (sbox), bar->label, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (box), bar->scale, TRUE, TRUE, 0);

        switch (bar->btype) {
        case BALANCE_TYPE_RL:
                str_lower = g_strdup_printf ("<small>%s</small>", C_("balance", "Left"));
                str_upper = g_strdup_printf ("<small>%s</small>", C_("balance", "Right"));
                break;
        case BALANCE_TYPE_FR:
                str_lower = g_strdup_printf ("<small>%s</small>", C_("balance", "Rear"));
                str_upper = g_strdup_printf ("<small>%s</small>", C_("balance", "Front"));
                break;
        case BALANCE_TYPE_LFE:
                str_lower = g_strdup_printf ("<small>%s</small>", C_("balance", "Minimum"));
                str_upper = g_strdup_printf ("<small>%s</small>", C_("balance", "Maximum"));
                break;
        default:
                g_assert_not_reached ();
        }

        lower = gtk_adjustment_get_lower (adjustment);
        gtk_scale_add_mark (GTK_SCALE (bar->scale), lower,
                            GTK_POS_BOTTOM, str_lower);
        g_free (str_lower);
        upper = gtk_adjustment_get_upper (adjustment);
        gtk_scale_add_mark (GTK_SCALE (bar->scale), upper,
                            GTK_POS_BOTTOM, str_upper);
        g_free (str_upper);

        if (bar->btype != BALANCE_TYPE_LFE) {
                gtk_scale_add_mark (GTK_SCALE (bar->scale),
                                    (upper - lower)/2 + lower,
                                    GTK_POS_BOTTOM, NULL);
        }

        bar->end_box = ebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_show (ebox);
        gtk_box_pack_start (GTK_BOX (box), ebox, FALSE, FALSE, 0);

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

void
gvc_balance_bar_set_size_group (GvcBalanceBar *bar,
                                GtkSizeGroup  *group,
                                gboolean       symmetric)
{
        g_return_if_fail (GVC_IS_BALANCE_BAR (bar));

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

static const char *
btype_to_string (guint btype)
{
        switch (btype) {
        case BALANCE_TYPE_RL:
                return "Balance";
        case BALANCE_TYPE_FR:
                return "Fade";
                break;
        case BALANCE_TYPE_LFE:
                return "LFE";
        default:
                g_assert_not_reached ();
        }
        return NULL;
}

static void
update_level_from_map (GvcBalanceBar *bar,
                       GvcChannelMap *map)
{
        const gdouble *volumes;
        gdouble val;

        g_debug ("Volume changed (for %s bar)", btype_to_string (bar->btype));

        volumes = gvc_channel_map_get_volume (map);
        switch (bar->btype) {
        case BALANCE_TYPE_RL:
                val = volumes[BALANCE];
                break;
        case BALANCE_TYPE_FR:
                val = volumes[FADE];
                break;
        case BALANCE_TYPE_LFE:
                val = volumes[LFE];
                break;
        default:
                g_assert_not_reached ();
        }

        gtk_adjustment_set_value (bar->adjustment, val);
}

static void
on_channel_map_volume_changed (GvcChannelMap  *map,
                               gboolean        set,
                               GvcBalanceBar  *bar)
{
        update_level_from_map (bar, map);
}

static void
gvc_balance_bar_set_channel_map (GvcBalanceBar *bar,
                                 GvcChannelMap *map)
{
        g_return_if_fail (GVC_BALANCE_BAR (bar));

        if (bar->channel_map != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (bar->channel_map),
                                                      on_channel_map_volume_changed, bar);
                g_object_unref (bar->channel_map);
        }
        bar->channel_map = g_object_ref (map);

        update_level_from_map (bar, map);

        g_signal_connect (G_OBJECT (map), "volume-changed",
                          G_CALLBACK (on_channel_map_volume_changed), bar);

        g_object_notify (G_OBJECT (bar), "channel-map");
}

static void
gvc_balance_bar_set_balance_type (GvcBalanceBar *bar,
                                  GvcBalanceType btype)
{
        GtkWidget *frame;

        g_return_if_fail (GVC_BALANCE_BAR (bar));

        bar->btype = btype;
        if (bar->btype != BALANCE_TYPE_LFE) {
                bar->adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                            -1.0,
                                                                            1.0,
                                                                            0.5,
                                                                            0.5,
                                                                            0.0));
        } else {
                bar->adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                            0.0,
                                                                            ADJUSTMENT_MAX_NORMAL,
                                                                            ADJUSTMENT_MAX_NORMAL/100.0,
                                                                            ADJUSTMENT_MAX_NORMAL/10.0,
                                                                            0.0));
        }

        g_object_ref_sink (bar->adjustment);
        g_signal_connect (bar->adjustment,
                          "value-changed",
                          G_CALLBACK (on_adjustment_value_changed),
                          bar);

        switch (btype) {
        case BALANCE_TYPE_RL:
                bar->label = gtk_label_new_with_mnemonic (_("_Balance:"));
                break;
        case BALANCE_TYPE_FR:
                bar->label = gtk_label_new_with_mnemonic (_("_Fade:"));
                break;
        case BALANCE_TYPE_LFE:
                bar->label = gtk_label_new_with_mnemonic (_("_Subwoofer:"));
                break;
        default:
                g_assert_not_reached ();
        }
        gtk_widget_show (bar->label);
        gtk_widget_set_halign (bar->label, GTK_ALIGN_START);
        gtk_widget_set_valign (bar->label, GTK_ALIGN_START);

        /* frame */
        frame = gtk_frame_new (NULL);
        gtk_widget_show (frame);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (bar), frame, TRUE, TRUE, 0);

        /* box with scale */
        bar->scale_box = _scale_box_new (bar);
        gtk_widget_show (bar->scale_box);
        gtk_container_add (GTK_CONTAINER (frame), bar->scale_box);

        gtk_widget_set_direction (bar->scale, GTK_TEXT_DIR_LTR);
        gtk_label_set_mnemonic_widget (GTK_LABEL (bar->label),
                                       bar->scale);

        g_object_notify (G_OBJECT (bar), "balance-type");
}

static void
gvc_balance_bar_set_property (GObject       *object,
                              guint          prop_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
        GvcBalanceBar *self = GVC_BALANCE_BAR (object);

        switch (prop_id) {
        case PROP_CHANNEL_MAP:
                gvc_balance_bar_set_channel_map (self, g_value_get_object (value));
                break;
        case PROP_BALANCE_TYPE:
                gvc_balance_bar_set_balance_type (self, g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_balance_bar_get_property (GObject     *object,
                              guint        prop_id,
                              GValue      *value,
                              GParamSpec  *pspec)
{
        GvcBalanceBar *self = GVC_BALANCE_BAR (object);

        switch (prop_id) {
        case PROP_CHANNEL_MAP:
                g_value_set_object (value, self->channel_map);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
gvc_balance_bar_constructor (GType                  type,
                             guint                  n_construct_properties,
                             GObjectConstructParam *construct_params)
{
        return G_OBJECT_CLASS (gvc_balance_bar_parent_class)->constructor (type, n_construct_properties, construct_params);
}

static void
gvc_balance_bar_class_init (GvcBalanceBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gvc_balance_bar_constructor;
        object_class->finalize = gvc_balance_bar_finalize;
        object_class->set_property = gvc_balance_bar_set_property;
        object_class->get_property = gvc_balance_bar_get_property;

        g_object_class_install_property (object_class,
                                         PROP_CHANNEL_MAP,
                                         g_param_spec_object ("channel-map",
                                                              "channel map",
                                                              "The channel map",
                                                              GVC_TYPE_CHANNEL_MAP,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_BALANCE_TYPE,
                                         g_param_spec_int ("balance-type",
                                                           "balance type",
                                                           "Whether the balance is right-left or front-rear",
                                                           BALANCE_TYPE_RL, NUM_BALANCE_TYPES - 1, BALANCE_TYPE_RL,
                                                           G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}


static gboolean
on_scale_button_press_event (GtkWidget      *widget,
                             GdkEventButton *event,
                             GvcBalanceBar  *bar)
{
        bar->click_lock = TRUE;

        return FALSE;
}

static gboolean
on_scale_button_release_event (GtkWidget      *widget,
                               GdkEventButton *event,
                               GvcBalanceBar  *bar)
{
        bar->click_lock = FALSE;

        return FALSE;
}

static gboolean
on_scale_scroll_event (GtkWidget      *widget,
                       GdkEventScroll *event,
                       GvcBalanceBar  *bar)
{
        gdouble value;
        gdouble dx, dy;

        value = gtk_adjustment_get_value (bar->adjustment);

        if (!gdk_event_get_scroll_deltas ((GdkEvent*)event, &dx, &dy)) {
                dx = 0.0;
                dy = 0.0;

                switch (event->direction) {
                case GDK_SCROLL_UP:
                case GDK_SCROLL_RIGHT:
                        dy = 1.0;
                        break;
                case GDK_SCROLL_DOWN:
                case GDK_SCROLL_LEFT:
                        dy = -1.0;
                        break;
                default:
                        ;
                }
        }

        if (bar->btype == BALANCE_TYPE_LFE) {
                if (dy > 0) {
                        if (value + dy * ADJUSTMENT_MAX_NORMAL/100.0 > ADJUSTMENT_MAX_NORMAL)
                                value = ADJUSTMENT_MAX_NORMAL;
                        else
                                value = value + dy * ADJUSTMENT_MAX_NORMAL/100.0;
                } else if (dy < 0) {
                        if (value + dy * ADJUSTMENT_MAX_NORMAL/100.0 < 0)
                                value = 0.0;
                        else
                                value = value + dy * ADJUSTMENT_MAX_NORMAL/100.0;
                }
        } else {
                if (dy > 0) {
                        if (value + dy * 0.01 > 1.0)
                                value = 1.0;
                        else
                                value = value + dy * 0.01;
                } else if (dy < 0) {
                        if (value + dy * 0.01 < -1.0)
                                value = -1.0;
                        else
                                value = value + dy * 0.01;
                }
        }
        gtk_adjustment_set_value (bar->adjustment, value);

        return TRUE;
}

static void
on_adjustment_value_changed (GtkAdjustment *adjustment,
                             GvcBalanceBar *bar)
{
        gdouble                val;
        pa_cvolume             cv;
        const pa_channel_map  *pa_map;

        if (bar->channel_map == NULL)
                return;

        cv = *gvc_channel_map_get_cvolume (bar->channel_map);
        val = gtk_adjustment_get_value (adjustment);

        pa_map = gvc_channel_map_get_pa_channel_map (bar->channel_map);

        switch (bar->btype) {
        case BALANCE_TYPE_RL:
                pa_cvolume_set_balance (&cv, pa_map, val);
                break;
        case BALANCE_TYPE_FR:
                pa_cvolume_set_fade (&cv, pa_map, val);
                break;
        case BALANCE_TYPE_LFE:
                pa_cvolume_set_position (&cv, pa_map, PA_CHANNEL_POSITION_LFE, val);
                break;
        }

        gvc_channel_map_volume_changed (bar->channel_map, &cv, TRUE);
}

static void
gvc_balance_bar_init (GvcBalanceBar *bar)
{
}

static void
gvc_balance_bar_finalize (GObject *object)
{
        GvcBalanceBar *bar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GVC_IS_BALANCE_BAR (object));

        bar = GVC_BALANCE_BAR (object);

        g_return_if_fail (bar != NULL);

        if (bar->channel_map != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (bar->channel_map),
                                                      on_channel_map_volume_changed, bar);
                g_object_unref (bar->channel_map);
        }

        G_OBJECT_CLASS (gvc_balance_bar_parent_class)->finalize (object);
}

GtkWidget *
gvc_balance_bar_new (const GvcChannelMap *channel_map, GvcBalanceType btype)
{
        GObject *bar;
        bar = g_object_new (GVC_TYPE_BALANCE_BAR,
                            "channel-map", channel_map,
                            "balance-type", btype,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            NULL);
        return GTK_WIDGET (bar);
}
