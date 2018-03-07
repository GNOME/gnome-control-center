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

#define WID(w) (GtkWidget *) gtk_builder_get_object (priv->builder, w)

#define POSITION_MODEL_VALUE_COLUMN     2
#define FONT_SCALE                      1.25

struct _ZoomOptionsPrivate
{
  GtkBuilder *builder;
  GSettings *settings;
  GSettings *application_settings;

  GtkWidget *position_combobox;
  GtkWidget *follow_mouse_radio;
  GtkWidget *screen_part_radio;
  GtkWidget *centered_radio;
  GtkWidget *push_radio;
  GtkWidget *proportional_radio;
  GtkWidget *extend_beyond_checkbox;
  GtkWidget *brightness_slider;
  GtkWidget *contrast_slider;

  GtkWidget *dialog;
};

G_DEFINE_TYPE (ZoomOptions, zoom_options, G_TYPE_OBJECT);

static gchar *brightness_keys[] = {
  "brightness-red",
  "brightness-green",
  "brightness-blue",
  NULL
};

static gchar *contrast_keys[] = {
  "contrast-red",
  "contrast-green",
  "contrast-blue",
  NULL
};

static void set_enable_screen_part_ui (GtkWidget *widget, ZoomOptionsPrivate *priv);
static void mouse_tracking_notify_cb (GSettings *settings, const gchar *key, ZoomOptionsPrivate *priv);
static void scale_label (GtkBin *toggle, PangoAttrList *attrs);
static void xhairs_color_opacity_changed (GtkColorButton *button, ZoomOptionsPrivate *priv);
static void xhairs_length_add_marks (GtkScale *scale);
static void effects_slider_set_value (GtkRange *slider, GSettings *settings);
static void brightness_slider_notify_cb (GSettings *settings, const gchar *key, ZoomOptionsPrivate *priv);
static void contrast_slider_notify_cb (GSettings *settings, const gchar *key, ZoomOptionsPrivate *priv);
static void effects_slider_changed (GtkRange *slider, ZoomOptionsPrivate *priv);

static void
mouse_tracking_radio_toggled_cb (GtkWidget *widget, ZoomOptionsPrivate *priv)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) == TRUE)
	  {
        g_settings_set_string (priv->settings, "mouse-tracking",
	                           gtk_buildable_get_name (GTK_BUILDABLE (widget)));
      }
}

static void
init_mouse_mode_radio_group (GSList *mode_group, ZoomOptionsPrivate *priv)
{
    gchar *mode;
    gchar *name;

    mode = g_settings_get_string (priv->settings, "mouse-tracking");
	for (; mode_group != NULL; mode_group = mode_group->next)
	  {
	    name = (gchar *) gtk_buildable_get_name (GTK_BUILDABLE (mode_group->data));
	    if (g_strcmp0 (name, mode) == 0)
	      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_group->data), TRUE);
	    else
	      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (mode_group->data), FALSE);

	    g_signal_connect (G_OBJECT (mode_group->data), "toggled",
                          G_CALLBACK(mouse_tracking_radio_toggled_cb),
                          priv);
	  }
}

static void
init_screen_part_section (ZoomOptionsPrivate *priv, PangoAttrList *pango_attrs)
{
  gboolean lens_mode;
  GSList *mouse_mode_group;

  priv->follow_mouse_radio = WID ("moveableLens");
  priv->screen_part_radio = WID ("screenPart");
  priv->centered_radio = WID ("centered");
  priv->push_radio = WID ("push");
  priv->proportional_radio = WID ("proportional");
  priv->extend_beyond_checkbox = WID ("scrollAtEdges");

  /* Scale the labels of the toggles */
  scale_label (GTK_BIN(priv->follow_mouse_radio), pango_attrs);
  scale_label (GTK_BIN(priv->screen_part_radio), pango_attrs);
  scale_label (GTK_BIN(priv->centered_radio), pango_attrs);
  scale_label (GTK_BIN(priv->push_radio), pango_attrs);
  scale_label (GTK_BIN(priv->proportional_radio), pango_attrs);
  scale_label (GTK_BIN(priv->extend_beyond_checkbox), pango_attrs);

  lens_mode = g_settings_get_boolean (priv->settings, "lens-mode");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->follow_mouse_radio), lens_mode);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->screen_part_radio), !lens_mode);

  mouse_mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (priv->centered_radio));
  init_mouse_mode_radio_group (mouse_mode_group, priv);
  set_enable_screen_part_ui (priv->screen_part_radio, priv);

  g_settings_bind (priv->settings, "lens-mode",
                   priv->follow_mouse_radio, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (priv->settings, "scroll-at-edges",
                   priv->extend_beyond_checkbox, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (G_OBJECT (priv->screen_part_radio), "toggled",
                    G_CALLBACK (set_enable_screen_part_ui), priv);

  g_signal_connect (G_OBJECT (priv->settings), "changed::mouse-tracking",
                    G_CALLBACK (mouse_tracking_notify_cb), priv);
}

static void
set_enable_screen_part_ui (GtkWidget *widget, ZoomOptionsPrivate *priv)
{
    gboolean screen_part;

    /* If the "screen part" radio is not checked, then the "follow mouse" radio
     * is checked (== lens mode). Set mouse tracking back to the default.
     */
    screen_part = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->screen_part_radio));
    if (!screen_part)
      {
        g_settings_set_string (priv->settings,
                               "mouse-tracking", "proportional");
      }

    gtk_widget_set_sensitive (priv->centered_radio, screen_part);
    gtk_widget_set_sensitive (priv->push_radio, screen_part);
    gtk_widget_set_sensitive (priv->proportional_radio, screen_part);
    gtk_widget_set_sensitive (priv->extend_beyond_checkbox, screen_part);
}

static void
mouse_tracking_notify_cb (GSettings             *settings,
                          const gchar           *key,
                          ZoomOptionsPrivate    *priv)
{
  gchar *tracking;

  tracking = g_settings_get_string (settings, key);
  if (g_strcmp0 (tracking, "proportional") == 0)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->proportional_radio), TRUE);
    }
  else if (g_strcmp0 (tracking, "centered") == 0)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->centered_radio), TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->push_radio), TRUE);
    }
}

static void
scale_label (GtkBin *toggle, PangoAttrList *attrs)
{
  GtkWidget *label;

  label = gtk_bin_get_child (toggle);
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
}

static void
screen_position_combo_changed_cb (GtkWidget *combobox, ZoomOptions *options)
{
  ZoomOptionsPrivate *priv = options->priv;
  gchar *combo_value = NULL;
  GtkTreeIter iter;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combobox), &iter);

  gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (combobox)), &iter,
                      POSITION_MODEL_VALUE_COLUMN, &combo_value,
                      -1);

  if (g_strcmp0 (combo_value, ""))
    {
      g_settings_set_string (priv->settings, "screen-position", combo_value);
    }

  g_free (combo_value);
}

static void
screen_position_notify_cb (GSettings *settings,
                           const gchar *key,
                           ZoomOptions *options)
{
  ZoomOptionsPrivate *priv = options->priv;
  gchar *position;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkComboBox *combobox;
  gboolean valid;
  gchar *combo_value;

  position = g_settings_get_string (settings, key);
  position = g_settings_get_string (priv->settings, key);
  combobox = GTK_COMBO_BOX (WID ("screen_position_combo_box"));
  model = gtk_combo_box_get_model (combobox);

  /* Find the matching screen position value in the combobox model.  If nothing
   * matches, leave the combobox as is.
   */
  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
        gtk_tree_model_get (model, &iter,
                            POSITION_MODEL_VALUE_COLUMN, &combo_value,
                            -1);
        if (!g_strcmp0 (combo_value, position))
          {
            g_signal_handlers_block_by_func (combobox, screen_position_combo_changed_cb, priv);
            gtk_combo_box_set_active_iter (combobox, &iter);
            g_signal_handlers_unblock_by_func (combobox, screen_position_combo_changed_cb, priv);
            g_free (combo_value);
            break;
          }

        g_free (combo_value);
        valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
init_xhairs_color_opacity (GtkColorButton *color_button, GSettings *settings)
{
    gchar *color_setting;
    GdkRGBA rgba;

    color_setting = g_settings_get_string (settings, "cross-hairs-color");
    gdk_rgba_parse (&rgba, color_setting);

    rgba.alpha = g_settings_get_double (settings, "cross-hairs-opacity");
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_button), &rgba);
}

static void
xhairs_color_notify_cb (GSettings *settings, gchar *key, GtkColorButton *button)
{
    init_xhairs_color_opacity (button, settings);
}

static void
xhairs_opacity_notify_cb (GSettings *settings, gchar *key, GtkColorButton *button)
{
    GdkRGBA rgba;
    gdouble opacity;

    opacity = g_settings_get_double (settings, key);
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);
    rgba.alpha = opacity * 65535;
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), &rgba);
}

#define TO_HEX(x) (int) ((gdouble) x * 255.0)
static void
xhairs_color_opacity_changed (GtkColorButton *button, ZoomOptionsPrivate *priv)
{
    GdkRGBA rgba;
    gchar *color_string;

    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);
    color_string = g_strdup_printf ("#%02x%02x%02x",
                                    TO_HEX(rgba.red),
                                    TO_HEX(rgba.green),
                                    TO_HEX(rgba.blue));

    g_settings_set_string (priv->settings, "cross-hairs-color", color_string);
    g_free (color_string);

    g_settings_set_double (priv->settings, "cross-hairs-opacity", rgba.alpha);
}

static void xhairs_length_add_marks (GtkScale *scale)
{
    GtkAdjustment *scale_model;
    GdkRectangle rect;
    GdkMonitor *monitor;
    GdkDisplay *display;
    gint length, quarter_length;

    /* Get maximum dimension of the monitor */
    display = gtk_widget_get_display (GTK_WIDGET (scale));
    monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (GTK_WIDGET (scale)));
    gdk_monitor_get_workarea (monitor, &rect);

    length = MAX (rect.width, rect.height);
    scale_model = gtk_range_get_adjustment (GTK_RANGE (scale));

    if (length < gtk_adjustment_get_upper (scale_model))
        gtk_adjustment_set_upper (scale_model, length);

    /* The crosshair is made up of four lines in pairs (top, bottom) and
       (left, right).  Stipulating: "quarter of the screen" means that the
       length of one hair is 25% of the screen. */
    quarter_length = length / 4;

    gtk_scale_add_mark (scale, 0, GTK_POS_BOTTOM, C_("Distance", "Short"));
    gtk_scale_add_mark (scale, quarter_length, GTK_POS_BOTTOM, C_("Distance", "¼ Screen"));
    gtk_scale_add_mark (scale, quarter_length * 2 , GTK_POS_BOTTOM, C_("Distance", "½ Screen"));
    gtk_scale_add_mark (scale, quarter_length * 3, GTK_POS_BOTTOM, C_("Distance", "¾ Screen"));
    gtk_scale_add_mark (scale, length, GTK_POS_BOTTOM, C_("Distance", "Long"));
}

static void
init_effects_slider (GtkRange *slider,
                     ZoomOptionsPrivate *priv,
                     gchar **keys,
                     GCallback notify_cb)
{
  gchar **key;
  gchar *signal;

  g_object_set_data (G_OBJECT (slider), "settings-keys", keys);
  effects_slider_set_value (slider, priv->settings);

  for (key = keys; *key; key++)
    {
      signal = g_strdup_printf ("changed::%s", *key);
      g_signal_connect (G_OBJECT (priv->settings), signal, notify_cb, priv);
      g_free (signal);
    }
  g_signal_connect (G_OBJECT (slider), "value-changed",
                    G_CALLBACK (effects_slider_changed),
                    priv);
  gtk_scale_add_mark (GTK_SCALE (slider), 0, GTK_POS_BOTTOM, NULL);
}

static void
effects_slider_set_value (GtkRange *slider, GSettings *settings)
{
    gchar **keys;
    gdouble red, green, blue;
    gdouble value;

    keys = g_object_get_data (G_OBJECT (slider), "settings-keys");

    red = g_settings_get_double (settings, keys[0]);
    green = g_settings_get_double (settings, keys[1]);
    blue = g_settings_get_double (settings, keys[2]);

    if (red == green && green == blue)
      value = red;
    else
      /* use NTSC conversion weights for reasonable average */
      value = 0.299 * red + 0.587 * green + 0.114 * blue;

    gtk_range_set_value (slider, value);
}

static void
brightness_slider_notify_cb (GSettings *settings,
                             const gchar *key,
                             ZoomOptionsPrivate *priv)
{
  GtkRange *slider = GTK_RANGE (priv->brightness_slider);

  g_signal_handlers_block_by_func (slider, effects_slider_changed, priv);
  effects_slider_set_value (slider, settings);
  g_signal_handlers_unblock_by_func (slider, effects_slider_changed, priv);
}

static void
contrast_slider_notify_cb (GSettings *settings,
                           const gchar *key,
                           ZoomOptionsPrivate *priv)
{
  GtkRange *slider = GTK_RANGE (priv->contrast_slider);

  g_signal_handlers_block_by_func (slider, effects_slider_changed, priv);
  effects_slider_set_value (slider, settings);
  g_signal_handlers_unblock_by_func (slider, effects_slider_changed, priv);
}

static void
effects_slider_changed (GtkRange *slider, ZoomOptionsPrivate *priv)
{
  gchar **keys, **key;
  gdouble value;

  keys = g_object_get_data (G_OBJECT (slider), "settings-keys");
  value = gtk_range_get_value (slider);

  for (key = keys; *key; key++)
    {
      g_settings_set_double (priv->settings, *key, value);
    }
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

  g_clear_object (&priv->application_settings);

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
  PangoAttrList *pango_attrs;
  PangoAttribute *attr;
  GError *err = NULL;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ZOOM_TYPE_OPTIONS, ZoomOptionsPrivate);

  priv->builder = gtk_builder_new ();
  gtk_builder_add_from_resource (priv->builder,
                                 "/org/gnome/control-center/universal-access/zoom-options.ui",
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
  priv->application_settings = g_settings_new ("org.gnome.desktop.a11y.applications");

  pango_attrs = pango_attr_list_new ();
  attr = pango_attr_scale_new (FONT_SCALE);
  pango_attr_list_insert (pango_attrs, attr);

  /* Zoom switch */
  g_settings_bind (priv->application_settings, "screen-magnifier-enabled",
                   WID ("seeing_zoom_switch"), "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Magnification factor */
  w = WID ("magFactorSpinButton");
  g_settings_bind (priv->settings, "mag-factor",
                   gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (w)),
                   "value", G_SETTINGS_BIND_DEFAULT);

  /* Screen position combo */
  w = WID ("screen_position_combo_box");
  screen_position_notify_cb (priv->settings, "screen-position", self);
  g_signal_connect (G_OBJECT (priv->settings), "changed::screen-position",
                    G_CALLBACK (screen_position_notify_cb), self);
  g_signal_connect (G_OBJECT (w), "changed",
                    G_CALLBACK (screen_position_combo_changed_cb), self);

  /* Screen part section */
  init_screen_part_section (priv, pango_attrs);

  /* Cross hairs: show/hide ... */
  w = WID ("xhairsEnabledSwitch");
  g_settings_bind (priv->settings, "show-cross-hairs", w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Cross hairs: color and opacity */
  w = WID ("xHairsPicker");
  init_xhairs_color_opacity (GTK_COLOR_BUTTON (w), priv->settings);
  g_signal_connect (G_OBJECT (priv->settings), "changed::cross-hairs-color",
                    G_CALLBACK (xhairs_color_notify_cb), w);
  g_signal_connect (G_OBJECT (priv->settings), "changed::cross-hairs-opacity",
                    G_CALLBACK (xhairs_opacity_notify_cb), w);
  g_signal_connect (G_OBJECT (w), "color-set",
                    G_CALLBACK (xhairs_color_opacity_changed),
                    priv);

  /* ... Cross hairs: thickness ... */
  w = WID ("xHairsThicknessSlider");
  g_settings_bind (priv->settings, "cross-hairs-thickness",
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Cross hairs: clip ... */
  w = WID ("xHairsClipCheckbox");
  scale_label (GTK_BIN(w), pango_attrs);
  g_settings_bind (priv->settings, "cross-hairs-clip", w, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* ... Cross hairs: length ... */
  w = WID ("xHairsLengthSlider");
  xhairs_length_add_marks (GTK_SCALE (w));
  g_settings_bind (priv->settings, "cross-hairs-length",
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Color effects ... */
  w = WID ("inverseEnabledSwitch");
  g_settings_bind (priv->settings, "invert-lightness", w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = WID ("brightnessSlider");
  priv->brightness_slider = w;
  init_effects_slider (GTK_RANGE(w), priv, brightness_keys,
                       G_CALLBACK (brightness_slider_notify_cb));

  w = WID ("contrastSlider");
  priv->contrast_slider = w;
  init_effects_slider (GTK_RANGE(w), priv, contrast_keys,
                       G_CALLBACK (contrast_slider_notify_cb));

  w = WID ("grayscale_slider");
  g_settings_bind (priv->settings, "color-saturation",
                   gtk_range_get_adjustment (GTK_RANGE (w)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_scale_add_mark (GTK_SCALE(w), 1.0, GTK_POS_BOTTOM, NULL);
  /* ... Window itself ... */
  priv->dialog = WID ("magPrefsDialog");

  g_signal_connect (G_OBJECT (priv->dialog), "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);

  pango_attr_list_unref (pango_attrs);
}

/**
 * zoom_options_set_parent:
 * @self: the #ZoomOptions object
 * @parent: the parent #GtkWindow
 *
 * Activate the dialog associated with this ZoomOptions.
 */
void
zoom_options_set_parent (ZoomOptions *self,
			 GtkWindow   *parent)
{
  g_return_if_fail (ZOOM_IS_OPTIONS (self));

  gtk_window_set_transient_for (GTK_WINDOW (self->priv->dialog), parent);
  gtk_window_set_modal (GTK_WINDOW (self->priv->dialog), TRUE);
  gtk_widget_show (self->priv->dialog);
}

ZoomOptions *
zoom_options_new (void)
{
  return g_object_new (ZOOM_TYPE_OPTIONS, NULL);
}
