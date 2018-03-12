/*
 * Copyright (C) 2007, 2008  Red Hat, Inc.
 * Copyright (C) 2013 Intel, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "cc-display-panel.h"

#include <gtk/gtk.h>
#include "scrollarea.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-bg.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <gdesktop-enums.h>
#include <math.h>

#include "shell/list-box-helper.h"
#include <libupower-glib/upower.h>

#include "cc-display-config-manager-dbus.h"
#include "cc-display-config.h"
#include "cc-night-light-dialog.h"
#include "cc-display-resources.h"

CC_PANEL_REGISTER (CcDisplayPanel, cc_display_panel)

#define DISPLAY_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PANEL, CcDisplayPanelPrivate))

#define TOP_BAR_HEIGHT 5

#define PANEL_PADDING   32
#define SECTION_PADDING 32
#define HEADING_PADDING 12

enum
{
  DISPLAY_MODE_PRIMARY,
  DISPLAY_MODE_SECONDARY,
  /* DISPLAY_MODE_PRESENTATION, */
  DISPLAY_MODE_MIRROR,
  DISPLAY_MODE_OFF
};

struct _CcDisplayPanelPrivate
{
  CcDisplayConfigManager *manager;
  CcDisplayConfig *current_config;
  CcDisplayMonitor *current_output;

  GnomeBG *background;
  GnomeDesktopThumbnailFactory *thumbnail_factory;

  guint           focus_id;

  GtkSizeGroup *main_size_group;
  GtkSizeGroup *rows_size_group;
  GtkWidget *stack;

  CcNightLightDialog *night_light_dialog;
  GSettings *settings_color;

  UpClient *up_client;
  gboolean lid_is_closed;

  GDBusProxy *shell_proxy;
  GCancellable *shell_cancellable;

  guint       sensor_watch_id;
  GDBusProxy *iio_sensor_proxy;
  gboolean    has_accelerometer;

  GtkWidget *main_titlebar;
  GtkWidget *apply_titlebar;
};

typedef struct
{
  int grab_x;
  int grab_y;
  int output_x;
  int output_y;
} GrabInfo;

enum
{
  CURRENT_OUTPUT,
  LAST_PANEL_SIGNAL
};
static guint panel_signals[LAST_PANEL_SIGNAL] = { 0 };

static const gchar *
get_resolution_string (CcDisplayMode *mode);
static const gchar *
get_frequency_string (CcDisplayMode *mode);
static GtkWidget *
make_night_light_widget (CcDisplayPanel *panel);
static gboolean
should_show_rotation (CcDisplayPanel *panel,
                      CcDisplayMonitor  *output);
static void
update_apply_button (CcDisplayPanel *panel);
static void
apply_current_configuration (CcDisplayPanel *self);
static void
on_area_paint (FooScrollArea  *area,
               cairo_t        *cr,
               gpointer        data);
static char *
make_display_size_string (int width_mm,
                          int height_mm);
static void
reset_current_config (CcDisplayPanel *panel);

static char *
make_output_ui_name (CcDisplayMonitor *output)
{
  int width_mm, height_mm;
  char *size, *name;

  cc_display_monitor_get_physical_size (output, &width_mm, &height_mm);
  size = make_display_size_string (width_mm, height_mm);
  if (size)
    name = g_strdup_printf ("%s (%s)", cc_display_monitor_get_display_name (output), size);
  else
    name = g_strdup_printf ("%s", cc_display_monitor_get_display_name (output));

  g_free (size);
  return name;
}

static void
ensure_output_numbers (CcDisplayPanel *self)
{
  GList *outputs, *l;
  GList *sorted = NULL;
  gint n = 0;

  outputs = cc_display_config_get_monitors (self->priv->current_config);

  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      if (cc_display_monitor_is_builtin (output))
        sorted = g_list_prepend (sorted, output);
      else
        sorted = g_list_append (sorted, output);
    }

  for (l = sorted; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      gchar *ui_name = make_output_ui_name (output);
      gboolean lid_is_closed = (cc_display_monitor_is_builtin (output) &&
                                self->priv->lid_is_closed);

      g_object_set_data (G_OBJECT (output), "ui-number", GINT_TO_POINTER (++n));
      g_object_set_data_full (G_OBJECT (output), "ui-number-name",
                              g_strdup_printf ("%d\u2003%s", n, ui_name),
                              g_free);
      g_object_set_data_full (G_OBJECT (output), "ui-name", ui_name, g_free);

      g_object_set_data (G_OBJECT (output), "lid-is-closed", GINT_TO_POINTER (lid_is_closed));
    }

  g_object_set_data_full (G_OBJECT (self->priv->current_config), "ui-sorted-outputs",
                          sorted, (GDestroyNotify) g_list_free);
}

static void
monitor_labeler_hide (CcDisplayPanel *self)
{
  CcDisplayPanelPrivate *priv = self->priv;

  if (!priv->shell_proxy)
    return;

  g_dbus_proxy_call (priv->shell_proxy,
                     "HideMonitorLabels",
                     NULL, G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
monitor_labeler_show (CcDisplayPanel *self)
{
  CcDisplayPanelPrivate *priv = self->priv;
  GList *outputs, *l;
  GVariantBuilder builder;
  gint number = 0;

  if (!priv->shell_proxy || !priv->current_config)
    return;

  outputs = g_object_get_data (G_OBJECT (priv->current_config), "ui-sorted-outputs");
  if (!outputs)
    return;

  if (cc_display_config_is_cloning (priv->current_config))
    return monitor_labeler_hide (self);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  g_variant_builder_open (&builder, G_VARIANT_TYPE_ARRAY);

  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      number = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (output), "ui-number"));
      if (number == 0)
        continue;

      g_variant_builder_add (&builder, "{sv}",
                             cc_display_monitor_get_connector_name (output),
                             g_variant_new_int32 (number));
    }

  g_variant_builder_close (&builder);

  if (number < 2)
    return monitor_labeler_hide (self);

  g_dbus_proxy_call (priv->shell_proxy,
                     "ShowMonitorLabels2",
                     g_variant_builder_end (&builder),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, NULL, NULL);
}

static void
ensure_monitor_labels (CcDisplayPanel *self)
{
  GList *windows, *w;

  windows = gtk_window_list_toplevels ();

  for (w = windows; w; w = w->next)
    {
      if (gtk_window_has_toplevel_focus (GTK_WINDOW (w->data)))
        {
          monitor_labeler_show (self);
          break;
        }
    }

  if (!w)
    monitor_labeler_hide (self);

  g_list_free (windows);
}

static void
dialog_toplevel_focus_changed (CcDisplayPanel *self)
{
  ensure_monitor_labels (self);
}

static void
reset_titlebar (CcDisplayPanel *self)
{
  CcDisplayPanelPrivate *priv = self->priv;
  GtkWidget *toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  if (priv->main_titlebar)
    {
      gtk_window_set_titlebar (GTK_WINDOW (toplevel), priv->main_titlebar);
      g_clear_object (&priv->main_titlebar);
    }

  g_clear_object (&priv->apply_titlebar);
}

static void
active_panel_changed (CcShell    *shell,
                      GParamSpec *pspec,
                      CcPanel    *self)
{
  CcPanel *panel = NULL;

  g_object_get (shell, "active-panel", &panel, NULL);
  if (panel != self)
    reset_titlebar (CC_DISPLAY_PANEL (self));

  g_object_unref (panel);
}

static void
cc_display_panel_dispose (GObject *object)
{
  CcDisplayPanelPrivate *priv = CC_DISPLAY_PANEL (object)->priv;
  CcShell *shell;
  GtkWidget *toplevel;

  reset_titlebar (CC_DISPLAY_PANEL (object));

  if (priv->sensor_watch_id > 0)
    {
      g_bus_unwatch_name (priv->sensor_watch_id);
      priv->sensor_watch_id = 0;
    }

  g_clear_object (&priv->iio_sensor_proxy);

  if (priv->focus_id)
    {
      shell = cc_panel_get_shell (CC_PANEL (object));
      toplevel = cc_shell_get_toplevel (shell);
      if (toplevel != NULL)
        g_signal_handler_disconnect (G_OBJECT (toplevel),
                                     priv->focus_id);
      priv->focus_id = 0;
      monitor_labeler_hide (CC_DISPLAY_PANEL (object));
    }

  g_clear_object (&priv->manager);
  g_clear_object (&priv->current_config);
  g_clear_object (&priv->up_client);
  g_clear_object (&priv->background);
  g_clear_object (&priv->thumbnail_factory);
  g_clear_object (&priv->settings_color);
  g_clear_object (&priv->night_light_dialog);
  g_clear_object (&priv->main_size_group);

  g_cancellable_cancel (priv->shell_cancellable);
  g_clear_object (&priv->shell_cancellable);
  g_clear_object (&priv->shell_proxy);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->dispose (object);
}

static void
cc_display_panel_constructed (GObject *object)
{
  g_signal_connect_object (cc_panel_get_shell (CC_PANEL (object)), "notify::active-panel",
                           G_CALLBACK (active_panel_changed), object, 0);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->constructed (object);
}

static const char *
cc_display_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/prefs-display";
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDisplayPanelPrivate));

  panel_class->get_help_uri = cc_display_panel_get_help_uri;

  object_class->constructed = cc_display_panel_constructed;
  object_class->dispose = cc_display_panel_dispose;

  panel_signals[CURRENT_OUTPUT] =
    g_signal_new ("current-output",
                  CC_TYPE_DISPLAY_PANEL,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

static void
set_current_output (CcDisplayPanel   *panel,
                    CcDisplayMonitor *output)
{
  panel->priv->current_output = output;
  g_signal_emit (panel, panel_signals[CURRENT_OUTPUT], 0);
}

static void
apply_rotation_to_geometry (CcDisplayMonitor *output, int *w, int *h)
{
  CcDisplayRotation rotation;

  rotation = cc_display_monitor_get_rotation (output);
  if ((rotation == CC_DISPLAY_ROTATION_90) || (rotation == CC_DISPLAY_ROTATION_270))
    {
      int tmp;
      tmp = *h;
      *h = *w;
      *w = tmp;
    }
}

static void
get_geometry (CcDisplayMonitor *output, int *x, int *y, int *w, int *h)
{
  if (cc_display_monitor_is_active (output))
    {
      cc_display_monitor_get_geometry (output, x, y, w, h);
    }
  else
    {
      cc_display_monitor_get_geometry (output, x, y, NULL, NULL);
      cc_display_mode_get_resolution (cc_display_monitor_get_preferred_mode (output),
                                      w, h);
    }

  apply_rotation_to_geometry (output, w, h);
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
paint_output (CcDisplayPanel    *panel,
              cairo_t           *cr,
              CcDisplayConfig   *configuration,
              CcDisplayMonitor  *output,
              gint               num,
              gint               allocated_width,
              gint               allocated_height)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GdkPixbuf *pixbuf;
  gint x, y, width, height;

  get_geometry (output, NULL, NULL, &width, &height);

  x = y = 0;

  /* scale to fit allocation */
  if (width / (double) height < allocated_width / (double) allocated_height)
    {
      width = allocated_height * (width / (double) height);
      height = allocated_height;
    }
  else
    {
      height = allocated_width * (height / (double) width);
      width = allocated_width;
    }


  x = (allocated_width / 2.0) - (width / 2.0);
  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_rectangle (cr, x, y, width, height);
  cairo_fill (cr);

  pixbuf = gnome_bg_create_thumbnail (priv->background,
                                      priv->thumbnail_factory,
                                      gdk_screen_get_default (), width, height);

  if (cc_display_monitor_is_primary (output)
      || cc_display_config_is_cloning (configuration))
    {
      y += TOP_BAR_HEIGHT;
      height -= TOP_BAR_HEIGHT;
    }

  if (pixbuf)
    gdk_cairo_set_source_pixbuf (cr, pixbuf, x + 1, y + 1);
  else
    cairo_set_source_rgb (cr, 0.3, 0.3, 0.3);
  cairo_rectangle (cr, x + 1, y + 1, width - 2, height - 2);
  cairo_fill (cr);

  g_clear_object (&pixbuf);

  if (num > 0)
    {
      PangoLayout *layout;
      gchar *number_str;
      gdouble r = 3, r2 = r / 2.0, x1, y1, x2, y2;
      PangoRectangle extents;
      gdouble max_extent;

      number_str = g_strdup_printf ("<small>%d</small>", num);
      layout = gtk_widget_create_pango_layout (GTK_WIDGET (panel), "");
      pango_layout_set_markup (layout, number_str, -1);
      pango_layout_get_extents (layout, NULL, &extents);
      g_free (number_str);

      cairo_set_source_rgba (cr, 0, 0, 0, 0.75);
      max_extent = MAX ((extents.width - extents.x)/ PANGO_SCALE,
                        (extents.height - extents.y) / PANGO_SCALE);

      x += 5;
      y += 5;
      x1 = x;
      x2 = x1 + max_extent + 1;
      y1 = y;
      y2 = y1 + max_extent + 1;
      cairo_move_to    (cr, x1 + r, y1);
      cairo_line_to    (cr, x2 - r, y1);
      cairo_curve_to   (cr, x2 - r2, y1, x2, y1 + r2, x2, y1 + r);
      cairo_line_to    (cr, x2, y2 - r);
      cairo_curve_to   (cr, x2, y2 - r2, x2 - r2, y2, x2 - r, y2);
      cairo_line_to    (cr, x1 + r, y2);
      cairo_curve_to   (cr, x1 + r2, y2, x1, y2 - r2, x1, y2 - r);
      cairo_line_to    (cr, x1, y1 + r);
      cairo_curve_to   (cr, x1, y1 + r2, x1 + r2, y1, x1 + r, y1);
      cairo_fill (cr);

      cairo_set_source_rgb (cr, 1, 1, 1);
      cairo_move_to (cr,
                     x + (max_extent / 2.0) - ((extents.width / PANGO_SCALE) / 2.0),
                     y + (max_extent / 2.0) - ((extents.height / PANGO_SCALE) / 2.0));
      pango_cairo_show_layout (cr, layout);
      cairo_fill (cr);
      g_object_unref (layout);
    }
}

static GtkWidget *
make_bin (void)
{
  return g_object_new (GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL);
}

static GtkWidget *
wrap_in_boxes (GtkWidget *widget)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, PANEL_PADDING);
  gtk_box_pack_start (GTK_BOX (box), make_bin(), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), make_bin(), TRUE, TRUE, 0);
  return box;
}

static GtkWidget *
make_scrollable (GtkWidget *widget)
{
  GtkWidget *sw;
  sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                     "hscrollbar-policy", GTK_POLICY_NEVER,
                     "min-content-height", 450,
                     "propagate-natural-height", TRUE,
                     NULL);
  gtk_container_add (GTK_CONTAINER (sw), wrap_in_boxes (widget));
  return sw;
}

static GtkWidget *
make_bold_label (const gchar *text)
{
  GtkCssProvider *provider;
  GtkWidget *label = gtk_label_new (text);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "label { font-weight: bold; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (label),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  return label;
}

static GtkWidget *
make_main_vbox (GtkSizeGroup *size_group)
{
  GtkWidget *vbox;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_margin_top (vbox, PANEL_PADDING);
  gtk_widget_set_margin_bottom (vbox, PANEL_PADDING);

  if (size_group)
    gtk_size_group_add_widget (size_group, vbox);

  return vbox;
}

static GtkWidget *
make_row (GtkSizeGroup *size_group,
          GtkWidget    *start_widget,
          GtkWidget    *end_widget)
{
  GtkWidget *row, *box;

  row = g_object_new (CC_TYPE_LIST_BOX_ROW, NULL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_margin_top (box, 20);
  gtk_widget_set_margin_bottom (box, 20);

  if (start_widget)
    {
      gtk_widget_set_halign (start_widget, GTK_ALIGN_START);
      gtk_box_pack_start (GTK_BOX (box), start_widget, FALSE, FALSE, 0);
    }
  if (end_widget)
    {
      gtk_widget_set_halign (end_widget, GTK_ALIGN_END);
      gtk_box_pack_end (GTK_BOX (box), end_widget, FALSE, FALSE, 0);
    }

  gtk_container_add (GTK_CONTAINER (row), box);

  if (size_group)
    gtk_size_group_add_widget (size_group, row);

  return row;
}

static GtkWidget *
make_frame (const gchar *title, const gchar *subtitle)
{
  GtkWidget *frame;

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);

  if (title)
    {
      GtkWidget *vbox, *label;

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, HEADING_PADDING/2);
      gtk_widget_set_margin_bottom (vbox, HEADING_PADDING);

      label = make_bold_label (title);
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_container_add (GTK_CONTAINER (vbox), label);

      if (subtitle)
        {
          label = gtk_label_new (subtitle);
          gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
          gtk_label_set_xalign (GTK_LABEL (label), 0.0);
          gtk_widget_set_halign (label, GTK_ALIGN_START);
          gtk_container_add (GTK_CONTAINER (vbox), label);
          gtk_style_context_add_class (gtk_widget_get_style_context (label),
                                       GTK_STYLE_CLASS_DIM_LABEL);
        }

      gtk_frame_set_label_widget (GTK_FRAME (frame), vbox);
      gtk_frame_set_label_align (GTK_FRAME (frame), 0.0, 1.0);
    }

  return frame;
}

static GtkWidget *
make_list_box (void)
{
  GtkWidget *listbox;

  listbox = g_object_new (CC_TYPE_LIST_BOX, NULL);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (listbox), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (listbox),
                                cc_list_box_update_header_func,
                                NULL, NULL);
  return listbox;
}

static GtkWidget *
make_list_transparent (GtkWidget *listbox)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   "list { border-style: none; background-color: transparent; }", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (listbox),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  return listbox;
}

static GtkWidget *
make_list_popover (GtkWidget *listbox)
{
  GtkWidget *popover = g_object_new (GTK_TYPE_POPOVER,
                                     "position", GTK_POS_BOTTOM,
                                     NULL);
  GtkWidget *sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                                "hscrollbar-policy", GTK_POLICY_NEVER,
                                "max-content-height", 300,
                                "propagate-natural-height", TRUE,
                                NULL);
  gtk_container_add (GTK_CONTAINER (sw), make_list_transparent (listbox));
  gtk_widget_show_all (sw);

  gtk_container_add (GTK_CONTAINER (popover), sw);
  g_signal_connect_object (listbox, "row-activated", G_CALLBACK (gtk_widget_hide),
                           popover, G_CONNECT_SWAPPED);
  return popover;
}

static GtkWidget *
make_popover_label (const gchar *text)
{
  return g_object_new (GTK_TYPE_LABEL,
                       "label", text,
                       "margin", 4,
                       "halign", GTK_ALIGN_START,
                       NULL);
}

static const gchar *
string_for_rotation (CcDisplayRotation rotation)
{
  switch (rotation)
    {
    case CC_DISPLAY_ROTATION_NONE:
    case CC_DISPLAY_ROTATION_180_FLIPPED:
      return C_("Display rotation", "Landscape");
    case CC_DISPLAY_ROTATION_90:
    case CC_DISPLAY_ROTATION_270_FLIPPED:
      return C_("Display rotation", "Portrait Right");
    case CC_DISPLAY_ROTATION_270:
    case CC_DISPLAY_ROTATION_90_FLIPPED:
      return C_("Display rotation", "Portrait Left");
    case CC_DISPLAY_ROTATION_180:
    case CC_DISPLAY_ROTATION_FLIPPED:
      return C_("Display rotation", "Landscape (flipped)");
    }
  return "";
}

static void
orientation_row_activated (CcDisplayPanel *panel,
                           GtkListBoxRow  *row)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  CcDisplayRotation rotation = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (row), "rotation"));

  cc_display_monitor_set_rotation (priv->current_output, rotation);
  update_apply_button (panel);
}

static GtkWidget *
make_orientation_popover (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox;
  CcDisplayRotation rotations[] = { CC_DISPLAY_ROTATION_NONE,
                                    CC_DISPLAY_ROTATION_90,
                                    CC_DISPLAY_ROTATION_270,
                                    CC_DISPLAY_ROTATION_180 };
  guint i = 0;

  listbox = make_list_box ();

  for (i = 0; i < G_N_ELEMENTS (rotations); ++i)
    {
      CcDisplayRotation rotation = rotations[i];
      if (cc_display_monitor_supports_rotation (priv->current_output, rotation))
        {
          GtkWidget *row;

          row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                              "child", make_popover_label (string_for_rotation (rotation)),
                              NULL);
          g_object_set_data (G_OBJECT (row), "rotation", GUINT_TO_POINTER (rotation));

          g_signal_connect_object (row, "activated", G_CALLBACK (orientation_row_activated),
                                   panel, G_CONNECT_SWAPPED);
          gtk_container_add (GTK_CONTAINER (listbox), row);
        }
    }

  return make_list_popover (listbox);
}

static void
orientation_row_sync (GtkPopover       *popover,
                      CcDisplayMonitor *output)
{
  gtk_label_set_text (GTK_LABEL (gtk_popover_get_relative_to (popover)),
                      string_for_rotation (cc_display_monitor_get_rotation (output)));
}

static GtkWidget *
make_orientation_row (CcDisplayPanel *panel, CcDisplayMonitor *output)
{
  GtkWidget *row, *label, *popover;

  label = gtk_label_new (string_for_rotation (cc_display_monitor_get_rotation (output)));
  popover = make_orientation_popover (panel);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), label);

  row = make_row (panel->priv->rows_size_group, gtk_label_new (_("Orientation")), label);
  g_signal_connect_object (row, "activated", G_CALLBACK (gtk_popover_popup),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (output, "rotation", G_CALLBACK (orientation_row_sync),
                           popover, G_CONNECT_SWAPPED);
  return row;
}

static void
resolution_row_activated (CcDisplayPanel *panel,
                          GtkListBoxRow  *row)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  CcDisplayMode *mode = g_object_get_data (G_OBJECT (row), "mode");

  cc_display_monitor_set_mode (priv->current_output, mode);
  update_apply_button (panel);
}

static GtkWidget *
make_resolution_popover (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox;
  GList *resolutions, *l;

  resolutions = g_object_get_data (G_OBJECT (priv->current_output), "res-list");

  listbox = make_list_box ();

  for (l = resolutions; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      GtkWidget *row;

      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", make_popover_label (get_resolution_string (mode)),
                          NULL);
      g_object_set_data (G_OBJECT (row), "mode", mode);

      g_signal_connect_object (row, "activated", G_CALLBACK (resolution_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  return make_list_popover (listbox);
}

static void
resolution_row_sync (GtkPopover       *popover,
                     CcDisplayMonitor *output)
{
  gtk_label_set_text (GTK_LABEL (gtk_popover_get_relative_to (popover)),
                      get_resolution_string (cc_display_monitor_get_mode (output)));
}

static GtkWidget *
make_resolution_row (CcDisplayPanel *panel, CcDisplayMonitor *output)
{
  GtkWidget *row, *label, *popover;

  label = gtk_label_new (get_resolution_string (cc_display_monitor_get_mode (output)));
  popover = make_resolution_popover (panel);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), label);

  row = make_row (panel->priv->rows_size_group, gtk_label_new (_("Resolution")), label);
  g_signal_connect_object (row, "activated", G_CALLBACK (gtk_popover_popup),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (output, "mode", G_CALLBACK (resolution_row_sync),
                           popover, G_CONNECT_SWAPPED);
  return row;
}

static void
refresh_rate_row_activated (CcDisplayPanel *panel,
                            GtkListBoxRow  *row)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  CcDisplayMode *mode = g_object_get_data (G_OBJECT (row), "mode");

  cc_display_monitor_set_mode (priv->current_output, mode);
  update_apply_button (panel);
}

static GtkWidget *
make_refresh_rate_popover (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox;
  GHashTable *res_freqs;
  GList *freqs, *l;

  res_freqs = g_object_get_data (G_OBJECT (priv->current_output), "res-freqs");
  freqs = g_hash_table_lookup (res_freqs,
                               get_resolution_string (cc_display_monitor_get_mode (priv->current_output)));

  listbox = make_list_box ();

  for (l = freqs; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      GtkWidget *row;

      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", make_popover_label (get_frequency_string (mode)),
                          NULL);
      g_object_set_data (G_OBJECT (row), "mode", mode);

      g_signal_connect_object (row, "activated", G_CALLBACK (refresh_rate_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  return make_list_popover (listbox);
}

static void
refresh_rate_row_sync (GtkPopover       *popover,
                       CcDisplayMonitor *output)
{
  gtk_label_set_text (GTK_LABEL (gtk_popover_get_relative_to (popover)),
                      get_frequency_string (cc_display_monitor_get_mode (output)));
}

static gboolean
should_show_refresh_rate (CcDisplayMonitor *output)
{
  GHashTable *res_freqs = g_object_get_data (G_OBJECT (output), "res-freqs");
  const gchar *resolution = get_resolution_string (cc_display_monitor_get_mode (output));
  GList *freqs = g_hash_table_lookup (res_freqs, resolution);

  return g_list_length (freqs) > 1;
}

static void
refresh_rate_row_sync_visibility (GtkWidget        *row,
                                  CcDisplayMonitor *output)
{
  if (!should_show_refresh_rate (output))
    gtk_widget_hide (row);
  else
    gtk_widget_show (row);
}

static GtkWidget *
make_refresh_rate_row (CcDisplayPanel *panel, CcDisplayMonitor *output)
{
  GtkWidget *row, *label, *popover;

  label = gtk_label_new (get_frequency_string (cc_display_monitor_get_mode (output)));
  popover = make_refresh_rate_popover (panel);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), label);

  row = make_row (panel->priv->rows_size_group, gtk_label_new (_("Refresh Rate")), label);
  g_signal_connect_object (row, "activated", G_CALLBACK (gtk_popover_popup),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (output, "mode", G_CALLBACK (refresh_rate_row_sync),
                           popover, G_CONNECT_SWAPPED);

  gtk_widget_show_all (row);
  gtk_widget_set_no_show_all (row, TRUE);

  g_signal_connect_object (output, "mode", G_CALLBACK (refresh_rate_row_sync_visibility),
                           row, G_CONNECT_SWAPPED);
  refresh_rate_row_sync_visibility (row, output);

  return row;
}

static guint
n_supported_scales (CcDisplayMode *mode)
{
  const double *scales = cc_display_mode_get_supported_scales (mode);
  guint n = 0;

  while (scales[n] != 0.0)
    n++;

  return n;
}

static gboolean
should_show_scale_row (CcDisplayMonitor *output)
{
  CcDisplayMode *mode = cc_display_monitor_get_mode (output);
  return mode ? n_supported_scales (mode) > 1 : FALSE;
}

static void
scale_row_sync_visibility (GtkWidget        *row,
                           CcDisplayMonitor *output)
{
  if (!should_show_scale_row (output))
    gtk_widget_hide (row);
  else
    gtk_widget_show (row);
}

static void
scale_buttons_active (CcDisplayPanel *panel,
                      GParamSpec     *pspec,
                      GtkWidget      *button)
{
  double scale = *(double*) g_object_get_data (G_OBJECT (button), "scale");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      cc_display_monitor_set_scale (panel->priv->current_output, scale);
      update_apply_button (panel);
    }
}

static double
round_scale_for_ui (double scale)
{
  /* Keep in sync with mutter */
  return round (scale*4)/4;
}

static GtkWidget *
make_label_for_scale (double scale)
{
  gchar *text = g_strdup_printf (" %d %% ", (int) (round_scale_for_ui (scale)*100));
  GtkWidget *label = gtk_label_new (text);
  g_free (text);
  return label;
}

#define MAX_N_SCALES 5
static void
setup_scale_buttons (GtkWidget        *bbox,
                     CcDisplayMonitor *output)
{
  CcDisplayPanel *panel;
  GtkRadioButton *group;
  CcDisplayMode *mode;
  const double *scales, *scale;
  guint i;

  panel = g_object_get_data (G_OBJECT (bbox), "panel");

  gtk_container_foreach (GTK_CONTAINER (bbox), (GtkCallback) gtk_widget_destroy, NULL);

  mode = cc_display_monitor_get_mode (output);
  if (!mode)
    return;

  scales = cc_display_mode_get_supported_scales (mode);
  group = NULL;
  for (scale = scales, i = 0; *scale != 0.0 && i < MAX_N_SCALES; scale++, i++)
    {
      GtkWidget *button = gtk_radio_button_new_from_widget (group);

      gtk_button_set_image (GTK_BUTTON (button), make_label_for_scale (*scale));
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

      g_object_set_data_full (G_OBJECT (button), "scale", g_memdup (scale, sizeof (double)), g_free);

      g_signal_connect_object (button, "notify::active", G_CALLBACK (scale_buttons_active),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (bbox), button);
      group = GTK_RADIO_BUTTON (button);
    }

  gtk_widget_show_all (bbox);
}
#undef MAX_N_SCALES

static void
scale_buttons_sync (GtkWidget        *bbox,
                    CcDisplayMonitor *output)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (bbox));
  for (l = children; l; l = l->next)
    {
      GtkWidget *button = l->data;
      double scale = *(double*) g_object_get_data (G_OBJECT (button), "scale");
      if (scale == cc_display_monitor_get_scale (output))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }
  g_list_free (children);
}

static GtkWidget *
make_scale_row (CcDisplayPanel *panel, CcDisplayMonitor *output)
{
  GtkWidget *row, *bbox, *label;

  label = gtk_label_new (_("Scale"));

  bbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_valign (bbox, GTK_ALIGN_CENTER);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_EXPAND);

  row = make_row (panel->priv->rows_size_group, label, bbox);
  gtk_widget_set_margin_top (gtk_bin_get_child (GTK_BIN (row)), 0);
  gtk_widget_set_margin_bottom (gtk_bin_get_child (GTK_BIN (row)), 0);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  g_object_set_data (G_OBJECT (bbox), "panel", panel);
  g_signal_connect_object (output, "mode", G_CALLBACK (setup_scale_buttons),
                           bbox, G_CONNECT_SWAPPED);
  setup_scale_buttons (bbox, output);

  g_signal_connect_object (output, "scale", G_CALLBACK (scale_buttons_sync),
                           bbox, G_CONNECT_SWAPPED);
  scale_buttons_sync (bbox, output);

  gtk_widget_show_all (row);
  gtk_widget_set_no_show_all (row, TRUE);

  g_signal_connect_object (output, "mode", G_CALLBACK (scale_row_sync_visibility),
                           row, G_CONNECT_SWAPPED);
  scale_row_sync_visibility (row, output);

  return row;
}

static void
underscanning_switch_active (CcDisplayPanel *panel,
                             GParamSpec     *pspec,
                             GtkWidget      *button)
{
  cc_display_monitor_set_underscanning (panel->priv->current_output,
                                        gtk_switch_get_active (GTK_SWITCH (button)));
  update_apply_button (panel);
}

static GtkWidget *
make_underscanning_row (CcDisplayPanel   *panel,
                        CcDisplayMonitor *output)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *row, *button;

  button = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (button),
                         cc_display_monitor_get_underscanning (output));
  g_signal_connect_object (button, "notify::active", G_CALLBACK (underscanning_switch_active),
                           panel, G_CONNECT_SWAPPED);

  row = make_row (priv->rows_size_group, gtk_label_new (_("Adjust for TV")), button);
  return row;
}

static gint
sort_modes_by_area_desc (CcDisplayMode *a, CcDisplayMode *b)
{
  int wa, ha, wb, hb;

  cc_display_mode_get_resolution (a, &wa, &ha);
  cc_display_mode_get_resolution (b, &wb, &hb);

  return wb*hb - wa*ha;
}

static gint
sort_modes_by_freq_desc (CcDisplayMode *a, CcDisplayMode *b)
{
  double delta = (cc_display_mode_get_freq_f (b) - cc_display_mode_get_freq_f (a))*1000.;
  return delta;
}

static void
ensure_res_freqs (CcDisplayMonitor *output)
{
  GHashTable *res_freqs;
  GHashTableIter iter;
  GList *resolutions, *modes, *m;

  if (g_object_get_data (G_OBJECT (output), "res-freqs"))
    return;

  res_freqs = g_hash_table_new_full (g_str_hash, g_str_equal,
                                     NULL, (GDestroyNotify) g_list_free);
  resolutions = NULL;

  modes = cc_display_monitor_get_modes (output);
  for (m = modes; m; m = m->next)
    {
      CcDisplayMode *mode = m->data;
      const gchar *resolution = get_resolution_string (mode);
      GList *l, *exist;

      exist = l = g_hash_table_lookup (res_freqs, resolution);
      l = g_list_append (l, mode);
      if (!exist)
        g_hash_table_insert (res_freqs, (gpointer) resolution, l);
    }

  g_hash_table_iter_init (&iter, res_freqs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &modes))
    {
      modes = g_list_copy (modes);
      modes = g_list_sort (modes, (GCompareFunc) sort_modes_by_freq_desc);
      g_hash_table_iter_replace (&iter, modes);

      resolutions = g_list_prepend (resolutions, g_list_nth_data (modes, 0));
    }

  resolutions = g_list_sort (resolutions, (GCompareFunc) sort_modes_by_area_desc);

  g_object_set_data_full (G_OBJECT (output), "res-freqs",
                          res_freqs, (GDestroyNotify) g_hash_table_destroy);
  g_object_set_data_full (G_OBJECT (output), "res-list",
                          resolutions, (GDestroyNotify) g_list_free);
}

static GtkWidget *
make_output_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox;

  ensure_res_freqs (priv->current_output);

  listbox = make_list_box ();

  if (should_show_rotation (panel, priv->current_output))
      gtk_container_add (GTK_CONTAINER (listbox),
                         make_orientation_row (panel, priv->current_output));

  gtk_container_add (GTK_CONTAINER (listbox),
                     make_resolution_row (panel, priv->current_output));

  gtk_container_add (GTK_CONTAINER (listbox),
                     make_scale_row (panel, priv->current_output));

  gtk_container_add (GTK_CONTAINER (listbox),
                     make_refresh_rate_row (panel, priv->current_output));

  if (cc_display_monitor_supports_underscanning (priv->current_output))
    gtk_container_add (GTK_CONTAINER (listbox),
                       make_underscanning_row (panel, priv->current_output));

  return listbox;
}

static GtkWidget *
make_single_output_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *vbox, *frame;

  priv->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = make_main_vbox (priv->main_size_group);

  frame = make_frame (g_object_get_data (G_OBJECT (priv->current_output), "ui-name"), NULL);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  gtk_container_add (GTK_CONTAINER (frame), make_output_ui (panel));

  gtk_container_add (GTK_CONTAINER (vbox), make_night_light_widget (panel));

  g_clear_object (&priv->rows_size_group);
  return make_scrollable (vbox);
}

static void
monitor_output_changes (GtkWidget      *area,
                        CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  const gchar *signals[] = { "rotation", "mode", "primary", "active", "scale" };
  GList *outputs, *l;
  guint i;

  outputs = cc_display_config_get_monitors (priv->current_config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      for (i = 0; i < G_N_ELEMENTS (signals); ++i)
        {
          g_signal_connect_object (output, signals[i], G_CALLBACK (gtk_widget_queue_draw),
                                   area, G_CONNECT_SWAPPED);
        }
    }
}

static GtkWidget *
make_arrangement_row (CcDisplayPanel *panel)
{
  GtkWidget *row, *area;

  area = (GtkWidget *) foo_scroll_area_new ();
  g_object_set_data (G_OBJECT (area), "panel", panel);
  foo_scroll_area_set_min_size (FOO_SCROLL_AREA (area), 400, 150);
  g_signal_connect (area, "paint",
                    G_CALLBACK (on_area_paint), panel);
  g_signal_connect (area, "viewport_changed",
                    G_CALLBACK (on_viewport_changed), panel);

  monitor_output_changes (area, panel);

  row = g_object_new (CC_TYPE_LIST_BOX_ROW, NULL);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  gtk_container_add (GTK_CONTAINER (row), area);

  return row;
}

static gboolean
is_output_useful (CcDisplayMonitor *output)
{
  return (cc_display_monitor_is_active (output) &&
          !g_object_get_data (G_OBJECT (output), "lid-is-closed"));
}

static guint
count_useful_outputs (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GList *outputs, *l;
  guint active = 0;

  outputs = cc_display_config_get_monitors (priv->current_config);
  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      if (!is_output_useful (output))
        continue;
      else
        active++;
    }
  return active;
}

static void
primary_chooser_sync (GtkPopover      *popover,
                      CcDisplayConfig *config)
{
  GtkWidget *label;
  GList *outputs, *l;

  label = gtk_popover_get_relative_to (popover);
  outputs = cc_display_config_get_monitors (config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      if (cc_display_monitor_is_primary (output))
        {
          gchar *text = g_object_get_data (G_OBJECT (output), "ui-number-name");
          gtk_label_set_text (GTK_LABEL (label), text);
          return;
        }
    }
}

static void
primary_chooser_row_activated (CcDisplayPanel *panel,
                               GtkListBoxRow  *row)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (row), "output");

  cc_display_monitor_set_primary (output, TRUE);
  update_apply_button (panel);
}

static GtkWidget *
make_primary_chooser_popover (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox;
  GList *outputs, *l;

  outputs = g_object_get_data (G_OBJECT (priv->current_config), "ui-sorted-outputs");

  listbox = make_list_box ();

  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkWidget *row;
      gchar *text;

      if (!is_output_useful (output))
        continue;

      text = g_object_get_data (G_OBJECT (output), "ui-number-name");
      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", make_popover_label (text),
                          NULL);
      g_object_set_data (G_OBJECT (row), "output", output);

      g_signal_connect_object (row, "activated", G_CALLBACK (primary_chooser_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  return make_list_popover (listbox);
}

static GtkWidget *
make_primary_chooser_row (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *row, *label, *popover;

  label = gtk_label_new (NULL);
  popover = make_primary_chooser_popover (panel);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), label);

  row = make_row (priv->rows_size_group, gtk_label_new (_("Primary Display")), label);
  g_signal_connect_object (row, "activated", G_CALLBACK (gtk_popover_popup),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->current_config, "primary", G_CALLBACK (primary_chooser_sync),
                           popover, G_CONNECT_SWAPPED);
  primary_chooser_sync (GTK_POPOVER (popover), priv->current_config);

  return row;
}

static void
replace_current_output_ui (GtkWidget      *frame,
                           CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  priv->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (frame)));
  gtk_container_add (GTK_CONTAINER (frame), make_output_ui (panel));
  gtk_widget_show_all (frame);

  g_clear_object (&priv->rows_size_group);
}

static GtkWidget *
make_arrangement_ui (CcDisplayPanel *panel)
{
  GtkWidget *frame, *listbox;

  frame = make_frame (_("Display Arrangement"),
                      _("Drag displays to match your setup. The top bar is placed on the primary display."));
  listbox = make_list_box ();
  gtk_container_add (GTK_CONTAINER (frame), listbox);

  gtk_container_add (GTK_CONTAINER (listbox), make_arrangement_row (panel));

  gtk_container_add (GTK_CONTAINER (listbox), make_primary_chooser_row (panel));

  return frame;
}

static void
two_output_chooser_active (CcDisplayPanel *panel,
                           GParamSpec     *pspec,
                           GtkWidget      *button)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (button), "output");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    set_current_output (panel, output);
}

static void
two_output_chooser_sync (GtkWidget      *box,
                         CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  for (l = children; l; l = l->next)
    {
      GtkWidget *button = l->data;
      CcDisplayMonitor *output = g_object_get_data (G_OBJECT (button), "output");
      if (priv->current_output == output)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
    }
  g_list_free (children);
}

static GtkWidget *
make_two_output_chooser (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *box;
  GtkRadioButton *group;
  GList *outputs, *l;

  outputs = g_object_get_data (G_OBJECT (priv->current_config), "ui-sorted-outputs");

  box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_EXPAND);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);

  group = NULL;
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkWidget *button = gtk_radio_button_new_from_widget (group);

      gtk_button_set_label (GTK_BUTTON (button), g_object_get_data (G_OBJECT (output), "ui-name"));
      gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);

      g_object_set_data (G_OBJECT (button), "output", output);
      g_signal_connect_object (button, "notify::active", G_CALLBACK (two_output_chooser_active),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (box), button);
      group = GTK_RADIO_BUTTON (button);
    }

  g_signal_connect_object (panel, "current-output", G_CALLBACK (two_output_chooser_sync),
                           box, G_CONNECT_SWAPPED);
  two_output_chooser_sync (box, panel);

  return box;
}

static GtkWidget *
make_two_join_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *vbox, *frame, *box;

  priv->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  gtk_container_add (GTK_CONTAINER (vbox), make_arrangement_ui (panel));

  box = make_two_output_chooser (panel);
  gtk_widget_set_margin_top (box, SECTION_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), box);

  frame = make_frame (NULL, NULL);
  gtk_widget_set_margin_top (frame, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  gtk_container_add (GTK_CONTAINER (frame), make_output_ui (panel));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (replace_current_output_ui),
                           frame, G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (vbox), make_night_light_widget (panel));

  g_clear_object (&priv->rows_size_group);
  return vbox;
}

static void
two_output_chooser_activate_output (CcDisplayPanel *panel,
                                    GParamSpec     *pspec,
                                    GtkWidget      *button)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (button), "output");

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      GList *outputs, *l;

      cc_display_monitor_set_active (output, TRUE);

      outputs = cc_display_config_get_monitors (priv->current_config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *other = l->data;
          if (other != output)
            cc_display_monitor_set_active (other, FALSE);
        }

      update_apply_button (panel);
    }
}

static void
connect_activate_output (GtkWidget *button,
                         gpointer   panel)
{
  g_signal_connect_object (button, "notify::active", G_CALLBACK (two_output_chooser_activate_output),
                           panel, G_CONNECT_SWAPPED);
}

static GtkWidget *
make_two_single_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *vbox, *frame, *box;

  priv->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  box = make_two_output_chooser (panel);
  gtk_container_foreach (GTK_CONTAINER (box), connect_activate_output, panel);
  gtk_container_add (GTK_CONTAINER (vbox), box);

  frame = make_frame (NULL, NULL);
  gtk_widget_set_margin_top (frame, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  gtk_container_add (GTK_CONTAINER (frame), make_output_ui (panel));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (replace_current_output_ui),
                           frame, G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (vbox), make_night_light_widget (panel));

  g_clear_object (&priv->rows_size_group);
  return vbox;
}

static void
set_mode_on_all_outputs (CcDisplayConfig *config,
                         CcDisplayMode   *mode)
{
  GList *outputs, *l;
  outputs = cc_display_config_get_monitors (config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      cc_display_monitor_set_mode (output, mode);
      cc_display_monitor_set_position (output, 0, 0);
    }
}

static void
mirror_resolution_row_activated (CcDisplayPanel *panel,
                                 GtkListBoxRow  *row)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  CcDisplayMode *mode = g_object_get_data (G_OBJECT (row), "mode");

  set_mode_on_all_outputs (priv->current_config, mode);
  update_apply_button (panel);
}

static GtkWidget *
make_mirror_resolution_popover (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox;
  GList *resolutions, *l;

  resolutions = g_object_get_data (G_OBJECT (priv->current_config), "mirror-res-list");

  listbox = make_list_box ();

  for (l = resolutions; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      GtkWidget *row;

      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", make_popover_label (get_resolution_string (mode)),
                          NULL);
      g_object_set_data (G_OBJECT (row), "mode", mode);

      g_signal_connect_object (row, "activated", G_CALLBACK (mirror_resolution_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  return make_list_popover (listbox);
}

static GtkWidget *
make_mirror_resolution_row (CcDisplayPanel   *panel,
                            CcDisplayMonitor *output)
{
  GtkWidget *row, *label, *popover;

  label = gtk_label_new (get_resolution_string (cc_display_monitor_get_mode (output)));
  popover = make_mirror_resolution_popover (panel);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), label);

  row = make_row (panel->priv->rows_size_group, gtk_label_new (_("Resolution")), label);
  g_signal_connect_object (row, "activated", G_CALLBACK (gtk_popover_popup),
                           popover, G_CONNECT_SWAPPED);
  g_signal_connect_object (output, "mode", G_CALLBACK (resolution_row_sync),
                           popover, G_CONNECT_SWAPPED);
  return row;
}

static void
ensure_mirror_res_list (CcDisplayConfig *config)
{
  GHashTable *res_set;
  GList *resolutions, *l;

  if (g_object_get_data (G_OBJECT (config), "mirror-res-list"))
    return;

  res_set = g_hash_table_new (g_str_hash, g_str_equal);

  resolutions = cc_display_config_get_cloning_modes (config);
  for (l = resolutions; l; l = l->next)
    {
      CcDisplayMode *mode = l->data;
      const gchar *resolution = get_resolution_string (mode);
      if (!g_hash_table_contains (res_set, resolution))
        g_hash_table_insert (res_set, (gpointer) resolution, mode);
    }

  resolutions = g_hash_table_get_values (res_set);
  g_hash_table_destroy (res_set);

  resolutions = g_list_sort (resolutions, (GCompareFunc) sort_modes_by_area_desc);

  g_object_set_data_full (G_OBJECT (config), "mirror-res-list",
                          resolutions, (GDestroyNotify) g_list_free);
}

static GtkWidget *
make_two_mirror_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *vbox, *listbox, *frame;

  ensure_mirror_res_list (priv->current_config);
  if (!cc_display_config_is_cloning (priv->current_config))
    {
      GList *modes;
      cc_display_config_set_cloning (priv->current_config, TRUE);
      modes = g_object_get_data (G_OBJECT (priv->current_config), "mirror-res-list");
      set_mode_on_all_outputs (priv->current_config,
                               CC_DISPLAY_MODE (g_list_nth_data (modes, 0)));
    }

  priv->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  frame = make_frame (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (vbox), frame);
  listbox = make_list_box ();
  gtk_container_add (GTK_CONTAINER (frame), listbox);

  if (should_show_rotation (panel, priv->current_output))
    gtk_container_add (GTK_CONTAINER (listbox),
                       make_orientation_row (panel, priv->current_output));

  gtk_container_add (GTK_CONTAINER (listbox),
                     make_mirror_resolution_row (panel, priv->current_output));

  gtk_container_add (GTK_CONTAINER (vbox), make_night_light_widget (panel));

  g_clear_object (&priv->rows_size_group);
  return vbox;
}

static void
two_output_visible_child_changed (CcDisplayPanel *panel,
                                  GParamSpec     *pspec,
                                  GtkWidget      *stack)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *bin;
  GList *children, *l;

  reset_current_config (panel);

  children = gtk_container_get_children (GTK_CONTAINER (stack));
  for (l = children; l; l = l->next)
    {
      GtkWidget *ui = gtk_bin_get_child (GTK_BIN (l->data));
      if (ui)
        gtk_widget_destroy (ui);
    }
  g_list_free (children);

  bin = gtk_stack_get_visible_child (GTK_STACK (stack));

  if (g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (stack)), "mirror"))
    {
      gtk_container_add (GTK_CONTAINER (bin), make_two_mirror_ui (panel));
    }
  else
    {
      gboolean single;
      GList *outputs, *l;

      if (cc_display_config_is_cloning (priv->current_config))
        {
          cc_display_config_set_cloning (priv->current_config, FALSE);
        }
      single = g_str_equal (gtk_stack_get_visible_child_name (GTK_STACK (stack)), "single");
      outputs = cc_display_config_get_monitors (priv->current_config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;
          cc_display_monitor_set_active (output, (!single || output == priv->current_output));
        }

      if (single)
        gtk_container_add (GTK_CONTAINER (bin), make_two_single_ui (panel));
      else
        gtk_container_add (GTK_CONTAINER (bin), make_two_join_ui (panel));
    }

  gtk_widget_show_all (stack);

  ensure_monitor_labels (panel);
  update_apply_button (panel);
}

static gboolean
transform_stack_to_button (GBinding     *binding,
                           const GValue *from_value,
                           GValue       *to_value,
                           gpointer      user_data)
{
  GtkWidget *visible_child = g_value_get_object (from_value);
  GtkWidget *button_child = user_data;

  g_value_set_boolean (to_value, visible_child == button_child);
  return TRUE;
}

static gboolean
transform_button_to_stack (GBinding     *binding,
                           const GValue *from_value,
                           GValue       *to_value,
                           gpointer      user_data)
{
  GtkWidget *button_child = user_data;

  if (g_value_get_boolean (from_value))
    g_value_set_object (to_value, button_child);
  return TRUE;
}

static void
add_two_output_page (GtkWidget   *switcher,
                     GtkWidget   *stack,
                     const gchar *name,
                     const gchar *title,
                     const gchar *icon)
{
  GtkWidget *button, *bin, *image;

  bin = make_bin ();
  gtk_stack_add_named (GTK_STACK (stack), bin, name);
  image = gtk_image_new_from_icon_name (icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
  g_object_set (G_OBJECT (image), "margin", HEADING_PADDING, NULL);
  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "image", image,
                         "image-position", GTK_POS_LEFT,
                         "always-show-image", TRUE,
                         "label", title,
                         NULL);
  gtk_container_add (GTK_CONTAINER (switcher), button);
  g_object_bind_property_full (stack, "visible-child", button, "active", G_BINDING_BIDIRECTIONAL,
                               transform_stack_to_button, transform_button_to_stack, bin, NULL);
}

static GtkWidget *
make_two_output_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *vbox, *switcher, *stack, *label;
  gboolean show_mirror;

  show_mirror = g_list_length (cc_display_config_get_cloning_modes (priv->current_config)) > 0;

  vbox = make_main_vbox (priv->main_size_group);

  label = make_bold_label (_("Display Mode"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (label, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), label);

  switcher = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_set_margin_bottom (switcher, SECTION_PADDING);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (switcher), GTK_BUTTONBOX_EXPAND);
  gtk_container_add (GTK_CONTAINER (vbox), switcher);

  stack = gtk_stack_new ();
  gtk_container_add (GTK_CONTAINER (vbox), stack);
  /* Add a dummy first stack page so that setting the visible child
   * below triggers a visible-child-name notification. */
  gtk_stack_add_named (GTK_STACK (stack), make_bin (), "dummy");

  add_two_output_page (switcher, stack, "join", _("Join Displays"),
                       "video-joined-displays-symbolic");
  if (show_mirror)
    add_two_output_page (switcher, stack, "mirror", _("Mirror"),
                         "view-mirror-symbolic");

  add_two_output_page (switcher, stack, "single", _("Single Display"),
                       "video-single-display-symbolic");

  gtk_widget_show_all (stack);

  g_signal_connect_object (stack, "notify::visible-child-name",
                           G_CALLBACK (two_output_visible_child_changed),
                           panel, G_CONNECT_SWAPPED);

  if (cc_display_config_is_cloning (priv->current_config) && show_mirror)
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "mirror");
  else if (count_useful_outputs (panel) > 1)
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "join");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (stack), "single");

  return make_scrollable (vbox);
}

static void
output_switch_active (CcDisplayPanel *panel,
                      GParamSpec     *pspec,
                      GtkWidget      *button)
{
  cc_display_monitor_set_active (panel->priv->current_output,
                                 gtk_switch_get_active (GTK_SWITCH (button)));
  update_apply_button (panel);
}

static void
output_switch_sync (GtkWidget        *button,
                    CcDisplayMonitor *output)
{
  gtk_switch_set_active (GTK_SWITCH (button), cc_display_monitor_is_active (output));
}

static GtkWidget *
make_output_switch (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *button = gtk_switch_new ();

  g_signal_connect_object (button, "notify::active", G_CALLBACK (output_switch_active),
                           panel, G_CONNECT_SWAPPED);
  g_signal_connect_object (priv->current_output, "active", G_CALLBACK (output_switch_sync),
                           button, G_CONNECT_SWAPPED);
  output_switch_sync (button, priv->current_output);

  if ((count_useful_outputs (panel) < 2 && cc_display_monitor_is_active (priv->current_output)) ||
      (cc_display_monitor_is_builtin (priv->current_output) && priv->lid_is_closed))
    gtk_widget_set_sensitive (button, FALSE);

  return button;
}

static void
replace_output_switch (GtkWidget      *frame,
                       CcDisplayPanel *panel)
{
  gtk_widget_destroy (gtk_bin_get_child (GTK_BIN (frame)));
  gtk_container_add (GTK_CONTAINER (frame), make_output_switch (panel));
  gtk_widget_show_all (frame);
}

static void
output_chooser_row_activated (CcDisplayPanel *panel,
                              GtkWidget      *row)
{
  CcDisplayMonitor *output = g_object_get_data (G_OBJECT (row), "output");
  set_current_output (panel, output);
}

static void
output_chooser_sync (GtkWidget      *button,
                     CcDisplayPanel *panel)
{
  gchar *text = g_object_get_data (G_OBJECT (panel->priv->current_output), "ui-number-name");
  GtkWidget *label = gtk_bin_get_child (GTK_BIN (button));

  gtk_label_set_text (GTK_LABEL (label), text);
}

static GtkWidget *
make_output_chooser_button (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *listbox, *button, *popover;
  GList *outputs, *l;

  outputs = g_object_get_data (G_OBJECT (priv->current_config), "ui-sorted-outputs");

  listbox = make_list_box ();

  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkWidget *row;
      gchar *text;

      text = g_object_get_data (G_OBJECT (output), "ui-number-name");
      row = g_object_new (CC_TYPE_LIST_BOX_ROW,
                          "child", make_popover_label (text),
                          NULL);
      g_object_set_data (G_OBJECT (row), "output", output);

      g_signal_connect_object (row, "activated", G_CALLBACK (output_chooser_row_activated),
                               panel, G_CONNECT_SWAPPED);
      gtk_container_add (GTK_CONTAINER (listbox), row);
    }

  popover = make_list_popover (listbox);
  button = gtk_menu_button_new ();
  gtk_container_add (GTK_CONTAINER (button), make_bold_label (NULL));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
  g_signal_connect_object (panel, "current-output", G_CALLBACK (output_chooser_sync),
                           button, G_CONNECT_SWAPPED);
  output_chooser_sync (button, panel);

  return button;
}

static GtkWidget *
make_multi_output_ui (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *vbox, *frame, *hbox;

  priv->rows_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  vbox = make_main_vbox (priv->main_size_group);

  gtk_container_add (GTK_CONTAINER (vbox), make_arrangement_ui (panel));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_margin_top (hbox, SECTION_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);

  gtk_box_pack_start (GTK_BOX (hbox), make_output_chooser_button (panel), FALSE, FALSE, 0);

  frame = make_bin ();
  gtk_box_pack_end (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (frame), make_output_switch (panel));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (replace_output_switch),
                           frame, G_CONNECT_SWAPPED);

  frame = make_frame (NULL, NULL);
  gtk_widget_set_margin_top (frame, HEADING_PADDING);
  gtk_container_add (GTK_CONTAINER (vbox), frame);

  gtk_container_add (GTK_CONTAINER (frame), make_output_ui (panel));
  g_signal_connect_object (panel, "current-output", G_CALLBACK (replace_current_output_ui),
                           frame, G_CONNECT_SWAPPED);

  gtk_container_add (GTK_CONTAINER (vbox), make_night_light_widget (panel));

  g_clear_object (&priv->rows_size_group);
  return make_scrollable (vbox);
}

static void
reset_current_config (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GList *outputs, *l;
  CcDisplayConfig *current;

  g_clear_object (&priv->current_config);
  priv->current_output = NULL;

  current = cc_display_config_manager_get_current (priv->manager);
  if (!current)
    return;

  priv->current_config = current;

  ensure_output_numbers (panel);

  outputs = g_object_get_data (G_OBJECT (current), "ui-sorted-outputs");
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (!is_output_useful (output))
        continue;

      priv->current_output = output;
      break;
    }
}

static void
on_screen_changed (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *main_widget;
  GList *outputs;
  guint n_outputs;

  if (!priv->manager)
    return;

  reset_titlebar (panel);

  main_widget = gtk_stack_get_child_by_name (GTK_STACK (priv->stack), "main");
  if (main_widget)
    gtk_widget_destroy (main_widget);

  reset_current_config (panel);

  if (!priv->current_config)
    goto show_error;

  ensure_monitor_labels (panel);

  if (!priv->current_output)
    goto show_error;

  outputs = g_object_get_data (G_OBJECT (priv->current_config), "ui-sorted-outputs");
  n_outputs = g_list_length (outputs);
  if (priv->lid_is_closed)
    {
      if (n_outputs <= 2)
        main_widget = make_single_output_ui (panel);
      else
        main_widget = make_multi_output_ui (panel);
    }
  else
    {
      if (n_outputs == 1)
        main_widget = make_single_output_ui (panel);
      else if (n_outputs == 2)
        main_widget = make_two_output_ui (panel);
      else
        main_widget = make_multi_output_ui (panel);
    }

  gtk_widget_show_all (main_widget);
  gtk_stack_add_named (GTK_STACK (priv->stack), main_widget, "main");
  gtk_stack_set_visible_child (GTK_STACK (priv->stack), main_widget);
  return;

 show_error:
  gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), "error");
}

#define SPACE 15
#define MARGIN  15

static void
get_total_size (CcDisplayPanel *self, int *total_w, int *total_h)
{
  GList *outputs, *l;

  *total_w = 0;
  *total_h = 0;

  outputs = cc_display_config_get_monitors (self->priv->current_config);
  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      int w, h;

      if (!is_output_useful (output))
        continue;

      get_geometry (output, NULL, NULL, &w, &h);

      if (cc_display_config_is_layout_logical (self->priv->current_config))
        {
          double scale = cc_display_monitor_get_scale (output);
          w /= scale;
          h /= scale;
        }

      *total_w += w;
      *total_h += h;
    }
}

static double
compute_scale (CcDisplayPanel *self, FooScrollArea *area)
{
  int available_w, available_h;
  int total_w, total_h;
  int n_monitors;
  GdkRectangle viewport;

  foo_scroll_area_get_viewport (area, &viewport);

  get_total_size (self, &total_w, &total_h);

  n_monitors = count_useful_outputs (self);

  available_w = viewport.width - 2 * MARGIN - (n_monitors - 1) * SPACE;
  available_h = viewport.height - 2 * MARGIN - (n_monitors - 1) * SPACE;

  return MIN ((double)available_w / total_w, (double)available_h / total_h);
}

typedef struct Edge
{
  CcDisplayMonitor *output;
  int x1, y1;
  int x2, y2;
} Edge;

typedef struct Snap
{
  Edge *snapper;              /* Edge that should be snapped */
  Edge *snappee;
  int dy, dx;
} Snap;

static void
add_edge (CcDisplayMonitor *output, int x1, int y1, int x2, int y2, GArray *edges)
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
list_edges_for_output (CcDisplayMonitor *output, GArray *edges, gboolean should_scale)
{
  int x, y, w, h;

  get_geometry (output, &x, &y, &w, &h);

  if (should_scale)
    {
      double scale = cc_display_monitor_get_scale (output);
      w /= scale;
      h /= scale;
    }

  /* Top, Bottom, Left, Right */
  add_edge (output, x, y, x + w, y, edges);
  add_edge (output, x, y + h, x + w, y + h, edges);
  add_edge (output, x, y, x, y + h, edges);
  add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (CcDisplayPanel *panel, GArray *edges)
{
  GList *outputs, *l;
  gboolean should_scale;

  should_scale = cc_display_config_is_layout_logical (panel->priv->current_config);
  outputs = cc_display_config_get_monitors (panel->priv->current_config);

  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (!is_output_useful (output))
        continue;

      list_edges_for_output (output, edges, should_scale);
    }
}

static gboolean
overlap (int s1, int e1, int s2, int e2)
{
  return (!(e1 < s2 || s1 >= e2));
}

static gboolean
horizontal_overlap (Edge *snapper, Edge *snappee)
{
  if (snapper->y1 != snapper->y2 || snappee->y1 != snappee->y2)
    return FALSE;

  return overlap (snapper->x1, snapper->x2, snappee->x1, snappee->x2);
}

static gboolean
vertical_overlap (Edge *snapper, Edge *snappee)
{
  if (snapper->x1 != snapper->x2 || snappee->x1 != snappee->x2)
    return FALSE;

  return overlap (snapper->y1, snapper->y2, snappee->y1, snappee->y2);
}

static void
add_snap (GArray *snaps, Snap snap)
{
  if (ABS (snap.dx) <= 200 || ABS (snap.dy) <= 200)
    g_array_append_val (snaps, snap);
}

static void
add_edge_snaps (Edge *snapper, Edge *snappee, GArray *snaps)
{
  Snap snap;

  snap.snapper = snapper;
  snap.snappee = snappee;

  if (horizontal_overlap (snapper, snappee))
    {
      snap.dx = 0;
      snap.dy = snappee->y1 - snapper->y1;

      add_snap (snaps, snap);
    }
  else if (vertical_overlap (snapper, snappee))
    {
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
list_snaps (CcDisplayMonitor *output, GArray *edges, GArray *snaps)
{
  int i;

  for (i = 0; i < edges->len; ++i)
    {
      Edge *output_edge = &(g_array_index (edges, Edge, i));

      if (output_edge->output == output)
        {
          int j;

          for (j = 0; j < edges->len; ++j)
            {
              Edge *edge = &(g_array_index (edges, Edge, j));

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
corner_on_edge (int x, int y, Edge *e)
{
  if (x == e->x1 && x == e->x2 && y >= e->y1 && y <= e->y2)
    return TRUE;

  if (y == e->y1 && y == e->y2 && x >= e->x1 && x <= e->x2)
    return TRUE;

  return FALSE;
}

static gboolean
edges_align (Edge *e1, Edge *e2)
{
  if (corner_on_edge (e1->x1, e1->y1, e2))
    return TRUE;

  if (corner_on_edge (e2->x1, e2->y1, e1))
    return TRUE;

  return FALSE;
}

static gboolean
output_is_aligned (CcDisplayMonitor *output, GArray *edges)
{
  gboolean result = FALSE;
  int i;

  for (i = 0; i < edges->len; ++i)
    {
      Edge *output_edge = &(g_array_index (edges, Edge, i));

      if (output_edge->output == output)
        {
          int j;

          for (j = 0; j < edges->len; ++j)
            {
              Edge *edge = &(g_array_index (edges, Edge, j));

              /* We are aligned if an output edge matches
               * an edge of another output
               */
              if (edge->output != output_edge->output)
                {
                  if (edges_align (output_edge, edge))
                    {
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
get_output_rect (CcDisplayMonitor *output, GdkRectangle *rect, gboolean should_scale)
{
  get_geometry (output, &rect->x, &rect->y, &rect->width, &rect->height);
  if (should_scale)
    {
      double scale = cc_display_monitor_get_scale (output);
      rect->height /= scale;
      rect->width /= scale;
    }
}

static gboolean
output_overlaps (CcDisplayMonitor *output, CcDisplayPanel *panel)
{
  GdkRectangle output_rect;
  GList *outputs, *l;
  gboolean should_scale;

  g_assert (output != NULL);

  should_scale = cc_display_config_is_layout_logical (panel->priv->current_config);
  get_output_rect (output, &output_rect, should_scale);

  outputs = cc_display_config_get_monitors (panel->priv->current_config);
  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *o = l->data;

      if (!is_output_useful (o))
        continue;

      if (o != output)
	{
	  GdkRectangle other_rect;
	  get_output_rect (o, &other_rect, should_scale);
	  if (gdk_rectangle_intersect (&output_rect, &other_rect, NULL))
	    return TRUE;
	}
    }

  return FALSE;
}

static gboolean
config_is_aligned (CcDisplayPanel *panel, GArray *edges)
{
  gboolean result = TRUE;
  GList *outputs, *l;

  outputs = cc_display_config_get_monitors (panel->priv->current_config);
  for (l = outputs; l != NULL; l = l->next)
    {
      CcDisplayMonitor *output = l->data;

      if (!is_output_useful (output))
        continue;

      if (!output_is_aligned (output, edges))
        return FALSE;

      if (output_overlaps (output, panel))
        return FALSE;
    }

  return result;
}

static gboolean
is_corner_snap (const Snap *s)
{
  return s->dx != 0 && s->dy != 0;
}

static int
compare_snaps (gconstpointer v1, gconstpointer v2)
{
  const Snap *s1 = v1;
  const Snap *s2 = v2;
  int sv1 = MAX (ABS (s1->dx), ABS (s1->dy));
  int sv2 = MAX (ABS (s2->dx), ABS (s2->dy));
  int d;

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
  if (d == 0)
    {
      if (is_corner_snap (s1) && !is_corner_snap (s2))
        return -1;
      else if (is_corner_snap (s2) && !is_corner_snap (s1))
        return 1;
      else
        return 0;
    }
  else
    {
      return d;
    }
}

/* Sets a mouse cursor for a widget's window.  As a hack, you can pass
 * GDK_BLANK_CURSOR to mean "set the cursor to NULL" (i.e. reset the widget's
 * window's cursor to its default).
 */
static void
set_cursor (GtkWidget *widget, GdkCursorType type)
{
  GdkCursor *cursor;
  GdkWindow *window;

  if (type == GDK_BLANK_CURSOR)
    cursor = NULL;
  else
    cursor = gdk_cursor_new_for_display (gtk_widget_get_display (widget), type);

  window = gtk_widget_get_window (widget);

  if (window)
    gdk_window_set_cursor (window, cursor);

  if (cursor)
    g_object_unref (cursor);
}

static void
grab_weak_ref_notify (gpointer  area,
                      GObject  *object)
{
  foo_scroll_area_end_grab (area, NULL);
}

static gboolean
on_toplevel_key_press (GtkWidget   *button,
                       GdkEventKey *event)
{
  if (event->keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  g_signal_emit_by_name (button, "activate");
  return GDK_EVENT_STOP;
}

static void
show_apply_titlebar (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWidget *header, *button, *toplevel;
  GtkSizeGroup *size_group;

  if (priv->apply_titlebar)
    return;

  priv->apply_titlebar = header = gtk_header_bar_new ();
  gtk_header_bar_set_title (GTK_HEADER_BAR (header), _("Apply Changes?"));

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  button = gtk_button_new_with_mnemonic (_("_Cancel"));
  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);
  gtk_size_group_add_widget (size_group, button);
  g_signal_connect_object (button, "clicked", G_CALLBACK (on_screen_changed),
                           panel, G_CONNECT_SWAPPED);

  toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel)));
  g_signal_connect_object (toplevel, "key-press-event", G_CALLBACK (on_toplevel_key_press),
                           button, G_CONNECT_SWAPPED);

  button = gtk_button_new_with_mnemonic (_("_Apply"));
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);
  gtk_size_group_add_widget (size_group, button);
  g_signal_connect_object (button, "clicked", G_CALLBACK (apply_current_configuration),
                           panel, G_CONNECT_SWAPPED);
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               GTK_STYLE_CLASS_SUGGESTED_ACTION);

  gtk_widget_show_all (header);
  g_object_unref (size_group);

  header = gtk_window_get_titlebar (GTK_WINDOW (toplevel));
  if (header)
    priv->main_titlebar = g_object_ref (header);

  gtk_window_set_titlebar (GTK_WINDOW (toplevel), priv->apply_titlebar);
  g_object_ref (priv->apply_titlebar);
}

static void
update_apply_button (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  gboolean config_equal;
  CcDisplayConfig *applied_config;

  if (!cc_display_config_is_applicable (priv->current_config))
    {
      reset_titlebar (panel);
      return;
    }

  applied_config = cc_display_config_manager_get_current (priv->manager);

  config_equal = cc_display_config_equal (priv->current_config,
                                          applied_config);
  g_object_unref (applied_config);

  if (config_equal)
    reset_titlebar (panel);
  else
    show_apply_titlebar (panel);
}

static void
on_output_event (FooScrollArea *area,
                 FooScrollAreaEvent *event,
                 gpointer data)
{
  CcDisplayMonitor *output = data;
  CcDisplayPanel *self = g_object_get_data (G_OBJECT (area), "panel");
  int n_monitors;

  if (event->type == FOO_DRAG_HOVER)
    {
      return;
    }
  if (event->type == FOO_DROP)
    {
      /* Activate new primary? */
      return;
    }

  n_monitors = count_useful_outputs (self);

  /* If the mouse is inside the outputs, set the cursor to "you can move me".  See
   * on_canvas_event() for where we reset the cursor to the default if it
   * exits the outputs' area.
   */
  if (!cc_display_config_is_cloning (self->priv->current_config) &&
      n_monitors > 1)
    set_cursor (GTK_WIDGET (area), GDK_FLEUR);

  if (event->type == FOO_BUTTON_PRESS)
    {
      GrabInfo *info;

      set_current_output (self, output);

      if (!cc_display_config_is_cloning (self->priv->current_config) &&
          n_monitors > 1)
	{
	  int output_x, output_y;
	  cc_display_monitor_get_geometry (output, &output_x, &output_y, NULL, NULL);

	  foo_scroll_area_begin_grab (area, on_output_event, data);
	  g_object_weak_ref (data, grab_weak_ref_notify, area);

	  info = g_new0 (GrabInfo, 1);
	  info->grab_x = event->x;
	  info->grab_y = event->y;
	  info->output_x = output_x;
	  info->output_y = output_y;

	  g_object_set_data (G_OBJECT (output), "grab-info", info);
	}
      foo_scroll_area_invalidate (area);
    }
  else
    {
      if (foo_scroll_area_is_grabbed (area))
	{
	  GrabInfo *info = g_object_get_data (G_OBJECT (output), "grab-info");
	  double scale = compute_scale (self, area);
	  int old_x, old_y;
	  int new_x, new_y;
	  int i;
	  GArray *edges, *snaps, *new_edges;

	  cc_display_monitor_get_geometry (output, &old_x, &old_y, NULL, NULL);
	  new_x = info->output_x + (event->x - info->grab_x) / scale;
	  new_y = info->output_y + (event->y - info->grab_y) / scale;

	  cc_display_monitor_set_position (output, new_x, new_y);

	  edges = g_array_new (TRUE, TRUE, sizeof (Edge));
	  snaps = g_array_new (TRUE, TRUE, sizeof (Snap));
	  new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

	  list_edges (self, edges);
	  list_snaps (output, edges, snaps);

	  g_array_sort (snaps, compare_snaps);

	  cc_display_monitor_set_position (output, old_x, old_y);

	  for (i = 0; i < snaps->len; ++i)
	    {
	      Snap *snap = &(g_array_index (snaps, Snap, i));
	      GArray *new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

	      cc_display_monitor_set_position (output, new_x + snap->dx, new_y + snap->dy);

	      g_array_set_size (new_edges, 0);
	      list_edges (self, new_edges);

	      if (config_is_aligned (self, new_edges))
		{
		  g_array_free (new_edges, TRUE);
		  break;
		}
	      else
		{
		  cc_display_monitor_set_position (output, info->output_x, info->output_y);
		}
	    }

	  g_array_free (new_edges, TRUE);
	  g_array_free (snaps, TRUE);
	  g_array_free (edges, TRUE);

	  if (event->type == FOO_BUTTON_RELEASE)
	    {
	      foo_scroll_area_end_grab (area, event);

	      g_free (g_object_get_data (G_OBJECT (output), "grab-info"));
	      g_object_set_data (G_OBJECT (output), "grab-info", NULL);
	      g_object_weak_unref (data, grab_weak_ref_notify, area);
              update_apply_button (self);

#if 0
              g_debug ("new position: %d %d %d %d", output->x, output->y, output->width, output->height);
#endif
            }

          foo_scroll_area_invalidate (area);
        }
    }
}

static void
on_canvas_event (FooScrollArea *area,
                 FooScrollAreaEvent *event,
                 gpointer data)
{
  /* If the mouse exits the outputs, reset the cursor to the default.  See
   * on_output_event() for where we set the cursor to the movement cursor if
   * it is over one of the outputs.
   */
  set_cursor (GTK_WIDGET (area), GDK_BLANK_CURSOR);
}

static void
paint_background (FooScrollArea *area,
                  cairo_t       *cr)
{
  GdkRectangle viewport;

  foo_scroll_area_get_viewport (area, &viewport);

  cairo_set_source_rgba (cr, 0, 0, 0, 0.0);
  cairo_rectangle (cr,
                   viewport.x, viewport.y,
                   viewport.width, viewport.height);
  foo_scroll_area_add_input_from_fill (area, cr, on_canvas_event, NULL);
  cairo_fill (cr);
}

static void
on_area_paint (FooScrollArea  *area,
               cairo_t        *cr,
               gpointer        data)
{
  CcDisplayPanel *self = data;
  GList *connected_outputs = NULL;
  GList *list;
  int total_w, total_h;

  paint_background (area, cr);

  if (!self->priv->current_config)
    return;

  get_total_size (self, &total_w, &total_h);

  connected_outputs = cc_display_config_get_monitors (self->priv->current_config);
  for (list = connected_outputs; list != NULL; list = list->next)
    {
      int w, h;
      double scale = compute_scale (self, area);
      gint x, y;
      int output_x, output_y;
      CcDisplayMonitor *output = list->data;
      GdkRectangle viewport;

      if (!is_output_useful (output))
        continue;

      cairo_save (cr);

      foo_scroll_area_get_viewport (area, &viewport);
      get_geometry (output, &output_x, &output_y, &w, &h);
      if (cc_display_config_is_layout_logical (self->priv->current_config))
        {
          double scale = cc_display_monitor_get_scale (output);
          w /= scale;
          h /= scale;
        }

      viewport.height -= 2 * MARGIN;
      viewport.width -= 2 * MARGIN;

      x = output_x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0;
      y = output_y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0;


      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_rectangle (cr, x, y, w * scale + 0.5, h * scale + 0.5);
      foo_scroll_area_add_input_from_fill (area, cr, on_output_event, output);
      cairo_fill (cr);

      cairo_translate (cr, x, y);
      paint_output (self, cr, self->priv->current_config, output,
                    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (output), "ui-number")),
                    w * scale, h * scale);

      cairo_restore (cr);

      if (cc_display_config_is_cloning (self->priv->current_config))
        break;
    }
}

static void
apply_current_configuration (CcDisplayPanel *self)
{
  GError *error = NULL;

  cc_display_config_apply (self->priv->current_config, &error);

  /* re-read the configuration */
  on_screen_changed (self);

  if (error)
    {
      g_warning ("Error applying configuration: %s", error->message);
      g_clear_error (&error);
    }
}

static const gchar *
make_aspect_string (gint width,
                    gint height)
{
  int ratio;
  const gchar *aspect = NULL;

    /* We use a number of Unicode characters below:
     * ∶ is U+2236 RATIO
     *   is U+2009 THIN SPACE,
     * × is U+00D7 MULTIPLICATION SIGN
     */
  if (width && height) {
    if (width > height)
      ratio = width * 10 / height;
    else
      ratio = height * 10 / width;

    switch (ratio) {
    case 13:
      aspect = "4∶3";
      break;
    case 16:
      aspect = "16∶10";
      break;
    case 17:
      aspect = "16∶9";
      break;
    case 23:
      aspect = "21∶9";
      break;
    case 12:
      aspect = "5∶4";
      break;
      /* This catches 1.5625 as well (1600x1024) when maybe it shouldn't. */
    case 15:
      aspect = "3∶2";
      break;
    case 18:
      aspect = "9∶5";
      break;
    case 10:
      aspect = "1∶1";
      break;
    }
  }

  return aspect;
}

static char *
make_resolution_string (CcDisplayMode *mode)
{
  const char *interlaced = cc_display_mode_is_interlaced (mode) ? "i" : "";
  const char *aspect;
  int width, height;

  cc_display_mode_get_resolution (mode, &width, &height);
  aspect = make_aspect_string (width, height);

  if (aspect != NULL)
    return g_strdup_printf ("%d × %d%s (%s)", width, height, interlaced, aspect);
  else
    return g_strdup_printf ("%d × %d%s", width, height, interlaced);
}

static const gchar *
get_resolution_string (CcDisplayMode *mode)
{
  char *resolution;

  if (!mode)
    return "";

  resolution = g_object_get_data (G_OBJECT (mode), "resolution");
  if (resolution)
    return resolution;

  resolution = make_resolution_string (mode);
  g_object_set_data_full (G_OBJECT (mode), "resolution", resolution, g_free);
  return resolution;
}

static const gchar *
get_frequency_string (CcDisplayMode *mode)
{
  char *frequency;

  if (!mode)
    return "";

  frequency = g_object_get_data (G_OBJECT (mode), "frequency");
  if (frequency)
    return frequency;

  frequency = g_strdup_printf (_("%.2lf Hz"), cc_display_mode_get_freq_f (mode));

  g_object_set_data_full (G_OBJECT (mode), "frequency", frequency, g_free);
  return frequency;
}

static const double known_diagonals[] = {
    12.1,
    13.3,
    15.6
};

static char *
diagonal_to_str (double d)
{
    int i;

    for (i = 0; i < G_N_ELEMENTS (known_diagonals); i++)
    {
        double delta;

        delta = fabs(known_diagonals[i] - d);
        if (delta < 0.1)
            return g_strdup_printf ("%0.1lf\"", known_diagonals[i]);
    }

    return g_strdup_printf ("%d\"", (int) (d + 0.5));
}

static char *
make_display_size_string (int width_mm,
                          int height_mm)
{
  char *inches = NULL;

  if (width_mm > 0 && height_mm > 0)
    {
      double d = sqrt (width_mm * width_mm + height_mm * height_mm);

      inches = diagonal_to_str (d / 25.4);
    }

  return inches;
}

static gboolean
should_show_rotation (CcDisplayPanel *panel,
                      CcDisplayMonitor  *output)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  gboolean supports_rotation;

  supports_rotation = cc_display_monitor_supports_rotation (output,
                                                            CC_DISPLAY_ROTATION_90 |
                                                            CC_DISPLAY_ROTATION_180 |
                                                            CC_DISPLAY_ROTATION_270);

  /* Doesn't support rotation at all */
  if (!supports_rotation)
    return FALSE;

  /* We can always rotate displays that aren't builtin */
  if (!cc_display_monitor_is_builtin (output))
    return TRUE;

  /* Only offer rotation if there's no accelerometer */
  return !priv->has_accelerometer;
}

static void
cc_display_panel_night_light_activated (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  GtkWindow *toplevel;
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (panel))));
  cc_night_light_dialog_present (priv->night_light_dialog, toplevel);
}

static void
mapped_cb (CcDisplayPanel *panel)
{
  CcDisplayPanelPrivate *priv = panel->priv;
  CcShell *shell;
  GtkWidget *toplevel;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  toplevel = cc_shell_get_toplevel (shell);
  if (toplevel && !priv->focus_id)
    priv->focus_id = g_signal_connect_swapped (toplevel, "notify::has-toplevel-focus",
                                               G_CALLBACK (dialog_toplevel_focus_changed), panel);
}

static void
cc_display_panel_up_client_changed (UpClient       *client,
                                    GParamSpec     *pspec,
                                    CcDisplayPanel *self)
{
  CcDisplayPanelPrivate *priv = self->priv;
  gboolean lid_is_closed;

  lid_is_closed = up_client_get_lid_is_closed (client);

  if (lid_is_closed != priv->lid_is_closed)
    {
      priv->lid_is_closed = lid_is_closed;

      on_screen_changed (self);
    }
}

static void
shell_proxy_ready (GObject        *source,
                   GAsyncResult   *res,
                   CcDisplayPanel *self)
{
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (!proxy)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to contact gnome-shell: %s", error->message);
      g_error_free (error);
      return;
    }

  self->priv->shell_proxy = proxy;

  ensure_monitor_labels (self);
}

static void
update_has_accel (CcDisplayPanel *self)
{
  GVariant *v;

  if (self->priv->iio_sensor_proxy == NULL)
    {
      g_debug ("Has no accelerometer");
      self->priv->has_accelerometer = FALSE;
      return;
    }

  v = g_dbus_proxy_get_cached_property (self->priv->iio_sensor_proxy, "HasAccelerometer");
  if (v)
    {
      self->priv->has_accelerometer = g_variant_get_boolean (v);
      g_variant_unref (v);
    }
  else
    {
      self->priv->has_accelerometer = FALSE;
    }

  g_debug ("Has %saccelerometer", self->priv->has_accelerometer ? "" : "no ");
}

static void
sensor_proxy_properties_changed_cb (GDBusProxy     *proxy,
                                    GVariant       *changed_properties,
                                    GStrv           invalidated_properties,
                                    CcDisplayPanel *self)
{
  GVariantDict dict;

  g_variant_dict_init (&dict, changed_properties);

  if (g_variant_dict_contains (&dict, "HasAccelerometer"))
    update_has_accel (self);
}

static void
sensor_proxy_appeared_cb (GDBusConnection *connection,
                          const gchar     *name,
                          const gchar     *name_owner,
                          gpointer         user_data)
{
  CcDisplayPanel *self = user_data;

  g_debug ("SensorProxy appeared");

  self->priv->iio_sensor_proxy = g_dbus_proxy_new_sync (connection,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        NULL,
                                                        "net.hadess.SensorProxy",
                                                        "/net/hadess/SensorProxy",
                                                        "net.hadess.SensorProxy",
                                                        NULL,
                                                        NULL);
  g_return_if_fail (self->priv->iio_sensor_proxy);

  g_signal_connect (self->priv->iio_sensor_proxy, "g-properties-changed",
                    G_CALLBACK (sensor_proxy_properties_changed_cb), self);
  update_has_accel (self);
}

static void
sensor_proxy_vanished_cb (GDBusConnection *connection,
                          const gchar     *name,
                          gpointer         user_data)
{
  CcDisplayPanel *self = user_data;

  g_debug ("SensorProxy vanished");

  g_clear_object (&self->priv->iio_sensor_proxy);
  update_has_accel (self);
}

static void
night_light_sync_label (GtkWidget *label, GSettings *settings)
{
  gboolean ret = g_settings_get_boolean (settings, "night-light-enabled");
  gtk_label_set_label (GTK_LABEL (label),
                       /* TRANSLATORS: the state of the night light setting */
                       ret ? _("On") : _("Off"));
}

static void
settings_color_changed_cb (GSettings *settings, gchar *key, GtkWidget *label)
{
  if (g_strcmp0 (key, "night-light-enabled") == 0)
    night_light_sync_label (label, settings);
}

static GtkWidget *
make_night_light_widget (CcDisplayPanel *self)
{
  CcDisplayPanelPrivate *priv = DISPLAY_PANEL_PRIVATE (self);
  GtkWidget *frame, *row, *label, *state_label;
  GtkWidget *night_light_listbox;

  frame = make_frame (NULL, NULL);
  night_light_listbox = make_list_box ();
  gtk_container_add (GTK_CONTAINER (frame), night_light_listbox);

  label = gtk_label_new (_("_Night Light"));
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);

  state_label = gtk_label_new ("");
  g_signal_connect_object (priv->settings_color, "changed",
                           G_CALLBACK (settings_color_changed_cb), state_label, 0);
  night_light_sync_label (state_label, priv->settings_color);

  row = make_row (priv->rows_size_group, label, state_label);
  gtk_container_add (GTK_CONTAINER (night_light_listbox), row);
  g_signal_connect_object (row, "activated",
                           G_CALLBACK (cc_display_panel_night_light_activated),
                           self, G_CONNECT_SWAPPED);

  gtk_widget_set_margin_top (frame, SECTION_PADDING);
  return frame;
}

static void
session_bus_ready (GObject        *source,
                   GAsyncResult   *res,
                   CcDisplayPanel *self)
{
  GDBusConnection *bus;
  GError *error = NULL;

  bus = g_bus_get_finish (res, &error);
  if (!bus)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to get session bus: %s", error->message);
          gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "error");
        }
      g_error_free (error);
      return;
    }

  self->priv->manager = cc_display_config_manager_dbus_new ();
  g_signal_connect_object (self->priv->manager, "changed",
                           G_CALLBACK (on_screen_changed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
cc_display_panel_init (CcDisplayPanel *self)
{
  CcDisplayPanelPrivate *priv;
  GSettings *settings;
  GtkWidget *bin;

  g_resources_register (cc_display_get_resource ());

  priv = self->priv = DISPLAY_PANEL_PRIVATE (self);

  priv->stack = gtk_stack_new ();

  bin = make_bin ();
  gtk_widget_set_size_request (bin, 500, -1);
  gtk_stack_add_named (GTK_STACK (priv->stack), bin, "main-size-group");
  priv->main_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (priv->main_size_group, bin);

  gtk_stack_add_named (GTK_STACK (priv->stack),
                       gtk_label_new (_("Could not get screen information")),
                       "error");

  gtk_container_add (GTK_CONTAINER (self), priv->stack);
  gtk_widget_show_all (priv->stack);

  settings = g_settings_new ("org.gnome.desktop.background");
  priv->background = gnome_bg_new ();
  gnome_bg_load_from_preferences (priv->background, settings);
  g_object_unref (settings);

  priv->thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  priv->night_light_dialog = cc_night_light_dialog_new ();
  priv->settings_color = g_settings_new ("org.gnome.settings-daemon.plugins.color");

  self->priv->up_client = up_client_new ();
  if (up_client_get_lid_is_present (self->priv->up_client))
    {
      /* Connect to the "changed" signal to track changes to "lid-is-closed"
       * property. Connecting to "notify::lid-is-closed" would be preferable,
       * but currently doesn't work as expected:
       * https://bugs.freedesktop.org/show_bug.cgi?id=43001
       */

      g_signal_connect (self->priv->up_client, "notify::lid-is-closed",
                        G_CALLBACK (cc_display_panel_up_client_changed), self);
      cc_display_panel_up_client_changed (self->priv->up_client, NULL, self);
    }
  else
    g_clear_object (&self->priv->up_client);

  g_signal_connect (self, "map", G_CALLBACK (mapped_cb), NULL);

  self->priv->shell_cancellable = g_cancellable_new ();
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                            G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS |
                            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                            NULL,
                            "org.gnome.Shell",
                            "/org/gnome/Shell",
                            "org.gnome.Shell",
                            self->priv->shell_cancellable,
                            (GAsyncReadyCallback) shell_proxy_ready,
                            self);

  g_bus_get (G_BUS_TYPE_SESSION,
             self->priv->shell_cancellable,
             (GAsyncReadyCallback) session_bus_ready,
             self);

  priv->sensor_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                            "net.hadess.SensorProxy",
                                            G_BUS_NAME_WATCHER_FLAGS_NONE,
                                            sensor_proxy_appeared_cb,
                                            sensor_proxy_vanished_cb,
                                            self,
                                            NULL);
}
