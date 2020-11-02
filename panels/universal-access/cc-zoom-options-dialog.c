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

#include "cc-zoom-options-dialog.h"
#include <gdk/gdk.h>
#include <glib/gi18n.h>
#include <string.h>

#define POSITION_MODEL_VALUE_COLUMN     2
#define FONT_SCALE                      1.25

struct _CcZoomOptionsDialog
{
  GtkDialog parent;

  GSettings *settings;
  GSettings *application_settings;

  GtkWidget *screen_position_combobox;
  GtkWidget *follow_mouse_radio;
  GtkWidget *screen_part_radio;
  GtkWidget *centered_radio;
  GtkWidget *push_radio;
  GtkWidget *proportional_radio;
  GtkWidget *extend_beyond_checkbox;
  GtkWidget *brightness_slider;
  GtkWidget *contrast_slider;
  GtkWidget *crosshair_picker_color_button;
  GtkWidget *magnifier_factor_spin;
  GtkWidget *seeing_zoom_switch;
  GtkWidget *crosshair_thickness_scale;
  GtkWidget *grayscale_slider;
  GtkWidget *crosshair_clip_checkbox;
  GtkWidget *crosshair_length_slider;
  GtkWidget *crosshair_enabled_switcher;
  GtkWidget *inverse_enabled_switch;
};

G_DEFINE_TYPE (CcZoomOptionsDialog, cc_zoom_options_dialog, GTK_TYPE_DIALOG);

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

static void set_enable_screen_part_ui (CcZoomOptionsDialog *self);
static void scale_label (GtkBin *toggle, PangoAttrList *attrs);
static void xhairs_length_add_marks (CcZoomOptionsDialog *self, GtkScale *scale);
static void effects_slider_set_value (GtkRange *slider, GSettings *settings);
static void brightness_slider_notify_cb (CcZoomOptionsDialog *self, const gchar *key);
static void contrast_slider_notify_cb (CcZoomOptionsDialog *self, const gchar *key);
static void effects_slider_changed (CcZoomOptionsDialog *self, GtkRange *slider);

static void
mouse_tracking_radio_toggled_cb (CcZoomOptionsDialog *self, GtkWidget *widget)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  if (widget == self->centered_radio)
      g_settings_set_string (self->settings, "mouse-tracking", "centered");
  else if (widget == self->proportional_radio)
      g_settings_set_string (self->settings, "mouse-tracking", "proportional");
  else if (widget == self->push_radio)
      g_settings_set_string (self->settings, "mouse-tracking", "push");
}

static void
mouse_tracking_notify_cb (CcZoomOptionsDialog *self)
{
    g_autofree gchar *tracking = NULL;

    tracking = g_settings_get_string (self->settings, "mouse-tracking");
    if (g_strcmp0 (tracking, "centered") == 0)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->centered_radio), TRUE);
    else if (g_strcmp0 (tracking, "proportional") == 0)
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->proportional_radio), TRUE);
    else
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->push_radio), TRUE);
}

static void
init_screen_part_section (CcZoomOptionsDialog *self, PangoAttrList *pango_attrs)
{
  gboolean lens_mode;

  /* Scale the labels of the toggles */
  scale_label (GTK_BIN (self->follow_mouse_radio), pango_attrs);
  scale_label (GTK_BIN (self->screen_part_radio), pango_attrs);
  scale_label (GTK_BIN (self->centered_radio), pango_attrs);
  scale_label (GTK_BIN (self->push_radio), pango_attrs);
  scale_label (GTK_BIN (self->proportional_radio), pango_attrs);
  scale_label (GTK_BIN (self->extend_beyond_checkbox), pango_attrs);

  lens_mode = g_settings_get_boolean (self->settings, "lens-mode");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->follow_mouse_radio), lens_mode);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->screen_part_radio), !lens_mode);

  set_enable_screen_part_ui (self);

  g_settings_bind (self->settings, "lens-mode",
                   self->follow_mouse_radio, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings, "scroll-at-edges",
                   self->extend_beyond_checkbox, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (G_OBJECT (self->screen_part_radio), "toggled",
                           G_CALLBACK (set_enable_screen_part_ui), self, G_CONNECT_SWAPPED);

  mouse_tracking_notify_cb (self);
  g_signal_connect_object (G_OBJECT (self->settings), "changed::mouse-tracking",
                           G_CALLBACK (mouse_tracking_notify_cb), self, G_CONNECT_SWAPPED);
}

static void
set_enable_screen_part_ui (CcZoomOptionsDialog *self)
{
    gboolean screen_part;

    /* If the "screen part" radio is not checked, then the "follow mouse" radio
     * is checked (== lens mode). Set mouse tracking back to the default.
     */
    screen_part = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->screen_part_radio));
    if (!screen_part)
      {
        g_settings_set_string (self->settings,
                               "mouse-tracking", "proportional");
      }

    gtk_widget_set_sensitive (self->centered_radio, screen_part);
    gtk_widget_set_sensitive (self->push_radio, screen_part);
    gtk_widget_set_sensitive (self->proportional_radio, screen_part);
    gtk_widget_set_sensitive (self->extend_beyond_checkbox, screen_part);
}

static void
scale_label (GtkBin *toggle, PangoAttrList *attrs)
{
  GtkWidget *label;

  label = gtk_bin_get_child (toggle);
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
}

static void
screen_position_combo_changed_cb (CcZoomOptionsDialog *self)
{
  g_autofree gchar *combo_value = NULL;
  GtkTreeIter iter;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->screen_position_combobox), &iter);

  gtk_tree_model_get (gtk_combo_box_get_model (GTK_COMBO_BOX (self->screen_position_combobox)), &iter,
                      POSITION_MODEL_VALUE_COLUMN, &combo_value,
                      -1);

  if (g_strcmp0 (combo_value, ""))
    {
      g_settings_set_string (self->settings, "screen-position", combo_value);
    }
}

static void
screen_position_notify_cb (CcZoomOptionsDialog *self,
                           const gchar *key)
{
  g_autofree gchar *position = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkComboBox *combobox;
  gboolean valid;

  position = g_settings_get_string (self->settings, key);
  combobox = GTK_COMBO_BOX (self->screen_position_combobox);
  model = gtk_combo_box_get_model (combobox);

  /* Find the matching screen position value in the combobox model.  If nothing
   * matches, leave the combobox as is.
   */
  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
        g_autofree gchar *combo_value = NULL;

        gtk_tree_model_get (model, &iter,
                            POSITION_MODEL_VALUE_COLUMN, &combo_value,
                            -1);
        if (!g_strcmp0 (combo_value, position))
          {
            g_signal_handlers_block_by_func (combobox, screen_position_combo_changed_cb, self);
            gtk_combo_box_set_active_iter (combobox, &iter);
            g_signal_handlers_unblock_by_func (combobox, screen_position_combo_changed_cb, self);
            break;
          }

        valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
init_xhairs_color_opacity (GtkColorButton *color_button, GSettings *settings)
{
    g_autofree gchar *color_setting = NULL;
    GdkRGBA rgba;

    color_setting = g_settings_get_string (settings, "cross-hairs-color");
    gdk_rgba_parse (&rgba, color_setting);

    rgba.alpha = g_settings_get_double (settings, "cross-hairs-opacity");
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (color_button), &rgba);
}

static void
xhairs_color_notify_cb (CcZoomOptionsDialog *self)
{
    init_xhairs_color_opacity (GTK_COLOR_BUTTON (self->crosshair_picker_color_button), self->settings);
}

static void
xhairs_opacity_notify_cb (CcZoomOptionsDialog *self, gchar *key)
{
    GdkRGBA rgba;
    gdouble opacity;

    opacity = g_settings_get_double (self->settings, key);
    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->crosshair_picker_color_button), &rgba);
    rgba.alpha = opacity * 65535;
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->crosshair_picker_color_button), &rgba);
}

#define TO_HEX(x) (int) ((gdouble) x * 255.0)
static void
xhairs_color_opacity_changed (CcZoomOptionsDialog *self)
{
    GdkRGBA rgba;
    g_autofree gchar *color_string = NULL;

    gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (self->crosshair_picker_color_button), &rgba);
    color_string = g_strdup_printf ("#%02x%02x%02x",
                                    TO_HEX(rgba.red),
                                    TO_HEX(rgba.green),
                                    TO_HEX(rgba.blue));

    g_settings_set_string (self->settings, "cross-hairs-color", color_string);

    g_settings_set_double (self->settings, "cross-hairs-opacity", rgba.alpha);
}

static void xhairs_length_add_marks (CcZoomOptionsDialog *self, GtkScale *scale)
{
    GtkAdjustment *scale_model;
    GdkRectangle rect;
    GdkMonitor *monitor;
    GdkDisplay *display;
    GtkWindow *transient_for;
    gint length, quarter_length;

    /* Get maximum dimension of the monitor */
    transient_for = gtk_window_get_transient_for (GTK_WINDOW (self));
    display = gtk_widget_get_display (GTK_WIDGET (transient_for));
    monitor = gdk_display_get_monitor_at_window (display, gtk_widget_get_window (GTK_WIDGET (transient_for)));
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
                     CcZoomOptionsDialog *self,
                     gchar **keys,
                     GCallback notify_cb)
{
  gchar **key;

  g_object_set_data (G_OBJECT (slider), "settings-keys", keys);
  effects_slider_set_value (slider, self->settings);

  for (key = keys; *key; key++)
    {
      g_autofree gchar *signal = g_strdup_printf ("changed::%s", *key);
      g_signal_connect_object (G_OBJECT (self->settings), signal, notify_cb, self, G_CONNECT_SWAPPED);
    }
  g_signal_connect_object (G_OBJECT (slider), "value-changed",
                           G_CALLBACK (effects_slider_changed),
                           self, G_CONNECT_SWAPPED);
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
brightness_slider_notify_cb (CcZoomOptionsDialog *self,
                             const gchar *key)
{
  GtkRange *slider = GTK_RANGE (self->brightness_slider);

  g_signal_handlers_block_by_func (slider, effects_slider_changed, self);
  effects_slider_set_value (slider, self->settings);
  g_signal_handlers_unblock_by_func (slider, effects_slider_changed, self);
}

static void
contrast_slider_notify_cb (CcZoomOptionsDialog *self,
                           const gchar *key)
{
  GtkRange *slider = GTK_RANGE (self->contrast_slider);

  g_signal_handlers_block_by_func (slider, effects_slider_changed, self);
  effects_slider_set_value (slider, self->settings);
  g_signal_handlers_unblock_by_func (slider, effects_slider_changed, self);
}

static void
effects_slider_changed (CcZoomOptionsDialog *self, GtkRange *slider)
{
  gchar **keys, **key;
  gdouble value;

  keys = g_object_get_data (G_OBJECT (slider), "settings-keys");
  value = gtk_range_get_value (slider);

  for (key = keys; *key; key++)
    {
      g_settings_set_double (self->settings, *key, value);
    }
}

static void
cc_zoom_options_dialog_constructed (GObject *object)
{
  PangoAttribute *attr;
  PangoAttrList *pango_attrs;
  CcZoomOptionsDialog *self;

  self = CC_ZOOM_OPTIONS_DIALOG (object);

  G_OBJECT_CLASS (cc_zoom_options_dialog_parent_class)->constructed (object);

  pango_attrs = pango_attr_list_new ();
  attr = pango_attr_scale_new (FONT_SCALE);
  pango_attr_list_insert (pango_attrs, attr);

  /* Zoom switch */
  g_settings_bind (self->application_settings, "screen-magnifier-enabled",
                   self->seeing_zoom_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Magnification factor */
  g_settings_bind (self->settings, "mag-factor",
                   gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (self->magnifier_factor_spin)),
                   "value", G_SETTINGS_BIND_DEFAULT);

  /* Screen position combo */
  screen_position_notify_cb (self, "screen-position");
  g_signal_connect_object (self->settings, "changed::screen-position",
                           G_CALLBACK (screen_position_notify_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->screen_position_combobox, "changed",
                           G_CALLBACK (screen_position_combo_changed_cb), self, G_CONNECT_SWAPPED);

  /* Screen part section */
  init_screen_part_section (self, pango_attrs);

  /* Cross hairs: show/hide ... */
  g_settings_bind (self->settings, "show-cross-hairs",
                   self->crosshair_enabled_switcher, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Cross hairs: color and opacity */
  init_xhairs_color_opacity (GTK_COLOR_BUTTON (self->crosshair_picker_color_button), self->settings);
  g_signal_connect_object (self->settings, "changed::cross-hairs-color",
                           G_CALLBACK (xhairs_color_notify_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->settings, "changed::cross-hairs-opacity",
                           G_CALLBACK (xhairs_opacity_notify_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->crosshair_picker_color_button, "color-set",
                           G_CALLBACK (xhairs_color_opacity_changed),
                           self, G_CONNECT_SWAPPED);

  /* ... Cross hairs: thickness ... */
  g_settings_bind (self->settings, "cross-hairs-thickness",
                   gtk_range_get_adjustment (GTK_RANGE (self->crosshair_thickness_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Cross hairs: clip ... */
  scale_label (GTK_BIN (self->crosshair_clip_checkbox), pango_attrs);
  g_settings_bind (self->settings, "cross-hairs-clip",
                   self->crosshair_clip_checkbox, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* ... Cross hairs: length ... */
  g_settings_bind (self->settings, "cross-hairs-length",
                   gtk_range_get_adjustment (GTK_RANGE (self->crosshair_length_slider)), "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* ... Color effects ... */
  g_settings_bind (self->settings, "invert-lightness", self->inverse_enabled_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  init_effects_slider (GTK_RANGE (self->brightness_slider),
                       self,
                       brightness_keys,
                       G_CALLBACK (brightness_slider_notify_cb));

  init_effects_slider (GTK_RANGE (self->contrast_slider),
                       self,
                       contrast_keys,
                       G_CALLBACK (contrast_slider_notify_cb));

  g_settings_bind (self->settings, "color-saturation",
                   gtk_range_get_adjustment (GTK_RANGE (self->grayscale_slider)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_scale_add_mark (GTK_SCALE(self->grayscale_slider), 1.0, GTK_POS_BOTTOM, NULL);

  pango_attr_list_unref (pango_attrs);
}

static void
cc_zoom_options_dialog_realize (GtkWidget *widget)
{
  CcZoomOptionsDialog *self = CC_ZOOM_OPTIONS_DIALOG (widget);

  GTK_WIDGET_CLASS (cc_zoom_options_dialog_parent_class)->realize (widget);

  xhairs_length_add_marks (self, GTK_SCALE (self->crosshair_length_slider));
}

static void
cc_zoom_options_dialog_finalize (GObject *object)
{
  CcZoomOptionsDialog *self = CC_ZOOM_OPTIONS_DIALOG (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->application_settings);

  G_OBJECT_CLASS (cc_zoom_options_dialog_parent_class)->finalize (object);
}

static void
cc_zoom_options_dialog_class_init (CcZoomOptionsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_zoom_options_dialog_finalize;
  object_class->constructed = cc_zoom_options_dialog_constructed;

  widget_class->realize =  cc_zoom_options_dialog_realize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-zoom-options-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, brightness_slider);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, centered_radio);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, contrast_slider);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, crosshair_clip_checkbox);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, crosshair_enabled_switcher);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, crosshair_length_slider);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, crosshair_picker_color_button);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, crosshair_thickness_scale);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, extend_beyond_checkbox);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, follow_mouse_radio);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, grayscale_slider);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, inverse_enabled_switch);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, magnifier_factor_spin);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, proportional_radio);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, push_radio);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, screen_part_radio);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, screen_position_combobox);
  gtk_widget_class_bind_template_child (widget_class, CcZoomOptionsDialog, seeing_zoom_switch);

  gtk_widget_class_bind_template_callback (widget_class, mouse_tracking_radio_toggled_cb);
}

static void
cc_zoom_options_dialog_init (CcZoomOptionsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->settings = g_settings_new ("org.gnome.desktop.a11y.magnifier");
  self->application_settings = g_settings_new ("org.gnome.desktop.a11y.applications");
}

CcZoomOptionsDialog *
cc_zoom_options_dialog_new (void)
{
  return g_object_new (cc_zoom_options_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
