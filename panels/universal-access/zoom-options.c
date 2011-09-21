/*
 * Copyright 2011 Inclusive Design Research Centre, OCAD University.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Joseph Scheuhammer <clown@alum.mit.edu>
 */

#include "zoom-options.h"
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <string.h>

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

struct _ZoomOptionsPrivate
{
  GtkBuilder *builder;
  GSettings *settings;

  GtkWidget *dialog;
};

G_DEFINE_TYPE (ZoomOptions, zoom_options, G_TYPE_OBJECT);

static void xhairs_color_opacity_changed_cb (GtkColorButton *button, ZoomOptionsPrivate *priv);
static void xhairs_length_add_marks (GtkScale *scale);

static void
mouse_mode_radiobutton_toggled_cb (GtkWidget *widget, ZoomOptionsPrivate *priv)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) == TRUE)
	  {
        g_settings_set_string (priv->settings, "mouse-tracking",
	                           gtk_buildable_get_name (GTK_BUILDABLE (widget)));
      }
}

static void
screen_position_radiobutton_toggled_cb (GtkWidget *widget, ZoomOptionsPrivate *priv)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) == TRUE)
	  {
        g_settings_set_string (priv->settings, "screen-position",
	                           gtk_buildable_get_name (GTK_BUILDABLE (widget)));
      }
}

static void
init_radio_group (GSList *radio_group,
                  gchar *key,
                  GCallback radiobutton_toggled_cb,
                  ZoomOptionsPrivate *priv)
{
    gchar *value;
    const gchar *name;

    value = g_settings_get_string (priv->settings, key);
	for (; radio_group != NULL; radio_group = radio_group->next)
	  {
	    name = gtk_buildable_get_name (GTK_BUILDABLE (radio_group->data));
	    if (strcmp (name, value) == 0)
	      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_group->data), TRUE);
	    else
	      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio_group->data), FALSE);

	    g_signal_connect (G_OBJECT (radio_group->data), "toggled",
                          G_CALLBACK(radiobutton_toggled_cb),
                          priv);
	  }
}

static void
init_xhairs_color_opacity (GtkColorButton *color_button, ZoomOptionsPrivate *priv)
{
    gchar *color_setting;
    GdkRGBA rgba;

    color_setting = g_settings_get_string (priv->settings, "cross-hairs-color");
    gdk_rgba_parse (&rgba, color_setting);

    rgba.alpha = g_settings_get_double (priv->settings, "cross-hairs-opacity");
    gtk_color_button_set_rgba (GTK_COLOR_BUTTON (color_button), &rgba);
}

static void
update_xhairs_color_cb (GSettings *settings, gchar *key, GtkColorButton *button)
{
    gchar *color;
    GdkColor rgb;

    color = g_settings_get_string (settings, key);
    gdk_color_parse (color, &rgb);

    gtk_color_button_set_color (button, &rgb);
}

static void
update_xhairs_opacity_cb (GSettings *settings, gchar *key, GtkColorButton *button)
{
    gdouble opacity;

    opacity = g_settings_get_double (settings, key);
    gtk_color_button_set_alpha (button, opacity * 65535);
}

static void
xhairs_color_opacity_changed_cb (GtkColorButton *button, ZoomOptionsPrivate *priv)
{
    GdkRGBA rgba;
    GdkColor rgb;
    gchar *color_string;
    gchar gsetting_val[8];

    gtk_color_button_get_color (button, &rgb);
    color_string = gdk_color_to_string (&rgb);

    // color_string is in the form '#rrrrggggbbbb'.  Convert to '#rrggbb'.
    // Start by copying the leading '#'.
    g_strlcpy (gsetting_val, color_string, 4);
    g_strlcpy (gsetting_val+3, color_string+5, 3);
    g_strlcpy (gsetting_val+5, color_string+9, 3);
    g_settings_set_string (priv->settings, "cross-hairs-color", gsetting_val);

    gtk_color_button_get_rgba (button, &rgba);
    g_settings_set_double (priv->settings, "cross-hairs-opacity", rgba.alpha);
}

static void xhairs_length_add_marks (GtkScale *scale)
{
    gint length, quarter_length;
    GtkAdjustment *scale_model;

    // Get maximum dimension of screen.
    length = MAX(gdk_screen_width(), gdk_screen_height());
    scale_model = gtk_range_get_adjustment (GTK_RANGE (scale));
    if (length < gtk_adjustment_get_upper(scale_model))
      {
        gtk_adjustment_set_upper (scale_model, length);
      }

    // The crosshair is made up of four lines in pairs (top, bottom) and
    // (left, right).  Stipulating: "quarter of the screen" means that the
    // length of one hair is 25% of the screen.
    quarter_length = length / 4;

    gtk_scale_add_mark (scale, quarter_length, GTK_POS_BOTTOM, _("1/4 Screen"));
    gtk_scale_add_mark (scale, quarter_length * 2 , GTK_POS_BOTTOM, _("1/2 Screen"));
    gtk_scale_add_mark (scale, quarter_length * 3, GTK_POS_BOTTOM, _("3/4 Screen"));
}

static void
zoom_option_close_dialog_cb (GtkWidget *closer, ZoomOptionsPrivate *priv)
{
    if (priv->dialog != NULL)
        gtk_widget_hide (priv->dialog);
}

static void
zoom_options_dispose (GObject *object)
{
  g_return_if_fail (object != NULL);
  g_return_if_fail (ZOOM_IS_OPTIONS (object));
  ZoomOptionsPrivate *priv = ZOOM_OPTIONS (object)->priv;

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
	}

  if (priv->settings)
    {
      g_object_unref (priv->settings);
      priv->settings = NULL;
    }

  if (priv->dialog)
    {
      gtk_widget_destroy (priv->dialog);
      priv->dialog = NULL;
    }

  G_OBJECT_CLASS (zoom_options_parent_class)->dispose (object);
}

static void
zoom_options_finalize (GObject *object)
{
  G_OBJECT_CLASS (zoom_options_parent_class)->finalize (object);
}

static void
zoom_options_class_init (ZoomOptionsClass *klass)
{
  GObjectClass    *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = zoom_options_dispose;
  object_class->finalize = zoom_options_finalize;

  g_type_class_add_private (klass, sizeof (ZoomOptionsPrivate));
}

static void
zoom_options_init (ZoomOptions *self)
{
  ZoomOptionsPrivate *priv;
  GtkWidget *w;
  GSList *radio_group;
  GError *err = NULL;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ZOOM_TYPE_OPTIONS, ZoomOptionsPrivate);

  priv->builder = gtk_builder_new ();
  gtk_builder_add_from_file (priv->builder,
                             GNOMECC_UI_DIR "/zoom-options.ui",
                             &err);
  if (err)
    {
      g_warning ("Could not load interface file: %s", err->message);
      g_error_free (err);

      g_object_unref (priv->builder);
      priv->builder = NULL;

      return;
    }

  priv->settings = g_settings_new ("org.gnome.desktop.a11y.magnifier");

  /* Magnification factor */
  w = WID (priv->builder, "magFactorSpinButton");
  g_settings_bind (priv->settings, "mag-factor",
				   gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
				   "value", G_SETTINGS_BIND_DEFAULT);

  /* Mouse tracking */
  w = WID (priv->builder, "proportional");
  radio_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (w));
  init_radio_group (radio_group, "mouse-tracking",
                    G_CALLBACK(mouse_mode_radiobutton_toggled_cb), priv);

  /* Screen position */
  w = WID (priv->builder, "full-screen");
  radio_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (w));
  init_radio_group (radio_group, "screen-position",
                    G_CALLBACK(screen_position_radiobutton_toggled_cb), priv);

  /* Lens mode */
  w = WID (priv->builder, "moveableLensCheckbox");
  g_settings_bind (priv->settings, "lens-mode", w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Clamp scrolling at screen edges */
  w = WID (priv->builder, "scrollAtEdges");
  g_settings_bind (priv->settings, "scroll-at-edges", w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Cross hairs: show/hide ... */
  w = WID (priv->builder, "xhairsEnabledSwitch");
  g_settings_bind (priv->settings, "show-cross-hairs", w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Cross hairs: color and opacity */
  w = WID (priv->builder, "xHairsPicker");
  init_xhairs_color_opacity (GTK_COLOR_BUTTON (w), priv);
  g_signal_connect (G_OBJECT (priv->settings), "changed::cross-hairs-color",
                    G_CALLBACK (update_xhairs_color_cb), w);
  g_signal_connect (G_OBJECT (priv->settings), "changed::cross-hairs-opacity",
                    G_CALLBACK (update_xhairs_opacity_cb), w);
  g_signal_connect (G_OBJECT (w), "color-set",
                    G_CALLBACK(xhairs_color_opacity_changed_cb),
                    priv);

  /* ... Cross hairs: thickness ... */
  w = WID (priv->builder, "xHairsThicknessSlider");
  g_settings_bind (priv->settings, "cross-hairs-thickness",
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Cross hairs: clip ... */
  w = WID (priv->builder, "xHairsClipCheckbox");
  g_settings_bind (priv->settings, "cross-hairs-clip", w, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* ... Cross hairs: length */
  w = WID (priv->builder, "xHairsLengthSlider");
  xhairs_length_add_marks (GTK_SCALE (w));
  g_settings_bind (priv->settings, "cross-hairs-length",
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Window itself ... */
  priv->dialog = WID (priv->builder, "magPrefsDialog");
  w = WID (priv->builder, "closeButton");
  g_signal_connect (G_OBJECT (w), "clicked",
                    G_CALLBACK (zoom_option_close_dialog_cb),
                    priv);
  g_signal_connect (G_OBJECT (priv->dialog), "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);
  zoom_options_present_dialog (self);
}

/**
 * zoom_options_present_dialog:
 * @self: the #ZoomOptions object
 *
 * Activate the dialog associated with this ZoomOptions.
 */
void
zoom_options_present_dialog (ZoomOptions *self)
{
  g_return_if_fail (ZOOM_IS_OPTIONS (self));

  if (self->priv->dialog != NULL)
    gtk_window_present (GTK_WINDOW (self->priv->dialog));
}
