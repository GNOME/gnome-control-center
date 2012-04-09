/*
 * Copyright (C) 2007, 2008  Red Hat, Inc.
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
 * Author: Soren Sandmann <sandmann@redhat.com>
 *
 */

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "cc-display-panel.h"

#include <gtk/gtk.h>
#include "scrollarea.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>
#include <libgnome-desktop/gnome-rr-config.h>
#include <libgnome-desktop/gnome-rr-labeler.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <glib/gi18n.h>
#include <gdesktop-enums.h>

G_DEFINE_DYNAMIC_TYPE (CcDisplayPanel, cc_display_panel, CC_TYPE_PANEL)

#define DISPLAY_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DISPLAY_PANEL, CcDisplayPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define TOP_BAR_HEIGHT 10

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

/* The minimum supported size for the panel, see:
 * http://live.gnome.org/Design/SystemSettings */
#define MINIMUM_WIDTH 675
#define MINIMUM_HEIGHT 530

enum {
  TEXT_COL,
  WIDTH_COL,
  HEIGHT_COL,
  RATE_COL,
  SORT_COL,
  ROTATION_COL,
  NUM_COLS
};

struct _CcDisplayPanelPrivate
{
  GnomeRRScreen       *screen;
  GnomeRRConfig  *current_configuration;
  GnomeRRLabeler *labeler;
  GnomeRROutputInfo         *current_output;

  GSettings      *clock_settings;
  GtkBuilder     *builder;
  guint           focus_id;

  GtkWidget      *panel;
  GtkWidget      *current_monitor_event_box;
  GtkWidget      *current_monitor_label;
  GtkWidget      *monitor_switch;
  GtkListStore   *resolution_store;
  GtkWidget      *resolution_combo;
  GtkWidget      *rotation_combo;
  GtkWidget      *clone_checkbox;
  GtkWidget      *clone_label;
  GtkWidget      *show_icon_checkbox;

  /* We store the event timestamp when the Apply button is clicked */
  guint32         apply_button_clicked_timestamp;

  GtkWidget      *area;
  gboolean        ignore_gui_changes;
  gboolean        dragging_top_bar;

  /* These are used while we are waiting for the ApplyConfiguration method to be executed over D-bus */
  GDBusProxy *proxy;
};

typedef struct
{
  int grab_x;
  int grab_y;
  int output_x;
  int output_y;
} GrabInfo;

static void rebuild_gui (CcDisplayPanel *self);
static void on_clone_changed (GtkWidget *box, gpointer data);
static gboolean output_overlaps (GnomeRROutputInfo *output, GnomeRRConfig *config);
static void select_current_output_from_dialog_position (CcDisplayPanel *self);
static void monitor_switch_active_cb (GObject *object, GParamSpec *pspec, gpointer data);
static void get_geometry (GnomeRROutputInfo *output, int *w, int *h);
static void apply_configuration_returned_cb (GObject *proxy, GAsyncResult *res, gpointer data);
static gboolean get_clone_size (GnomeRRScreen *screen, int *width, int *height);
static gboolean output_info_supports_mode (CcDisplayPanel *self, GnomeRROutputInfo *info, int width, int height);
static char *make_resolution_string (int width, int height);
static GObject *cc_display_panel_constructor (GType                  gtype,
					      guint                  n_properties,
					      GObjectConstructParam *properties);
static void on_screen_changed (GnomeRRScreen *scr, gpointer data);

static void
cc_display_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_display_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_display_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_display_panel_parent_class)->dispose (object);
}

static void
cc_display_panel_finalize (GObject *object)
{
  CcDisplayPanel *self;
  CcShell *shell;
  GtkWidget *toplevel;

  self = CC_DISPLAY_PANEL (object);

  g_signal_handlers_disconnect_by_func (self->priv->screen, on_screen_changed, self);
  g_object_unref (self->priv->screen);
  g_object_unref (self->priv->builder);

  if (self->priv->clock_settings != NULL)
    g_object_unref (self->priv->clock_settings);

  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = cc_shell_get_toplevel (shell);
  g_signal_handler_disconnect (G_OBJECT (toplevel),
                               self->priv->focus_id);

  gnome_rr_labeler_hide (self->priv->labeler);
  g_object_unref (self->priv->labeler);

  G_OBJECT_CLASS (cc_display_panel_parent_class)->finalize (object);
}

static void
cc_display_panel_class_init (CcDisplayPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDisplayPanelPrivate));

  object_class->constructor = cc_display_panel_constructor;
  object_class->get_property = cc_display_panel_get_property;
  object_class->set_property = cc_display_panel_set_property;
  object_class->dispose = cc_display_panel_dispose;
  object_class->finalize = cc_display_panel_finalize;
}

static void
cc_display_panel_class_finalize (CcDisplayPanelClass *klass)
{
}

static void
error_message (CcDisplayPanel *self, const char *primary_text, const char *secondary_text)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  if (self && self->priv->panel)
    toplevel = gtk_widget_get_toplevel (self->priv->panel);
  else
    toplevel = NULL;

  dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", primary_text);

  if (secondary_text)
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_text);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static gboolean
should_show_resolution (gint output_width,
                        gint output_height,
                        gint width,
                        gint height)
{
  if (width >= MIN (output_width, MINIMUM_WIDTH) &&
      height >= MIN (output_height, MINIMUM_HEIGHT))
    {
      return TRUE;
    }
  return FALSE;
}

static void
on_screen_changed (GnomeRRScreen *scr,
                   gpointer data)
{
  GnomeRRConfig *current;
  CcDisplayPanel *self = data;

  current = gnome_rr_config_new_current (self->priv->screen, NULL);
  gnome_rr_config_ensure_primary (current);

  if (self->priv->current_configuration)
    g_object_unref (self->priv->current_configuration);

  self->priv->current_configuration = current;
  self->priv->current_output = NULL;

  if (self->priv->labeler) {
    gnome_rr_labeler_hide (self->priv->labeler);
    g_object_unref (self->priv->labeler);
  }

  self->priv->labeler = gnome_rr_labeler_new (self->priv->current_configuration);
  if (gtk_widget_has_focus (self->priv->panel))
     gnome_rr_labeler_show (self->priv->labeler);

  select_current_output_from_dialog_position (self);
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
layout_set_font (PangoLayout *layout, const char *font)
{
  PangoFontDescription *desc =
    pango_font_description_from_string (font);

  if (desc)
    {
      pango_layout_set_font_description (layout, desc);

      pango_font_description_free (desc);
    }
}

static void
clear_combo (GtkWidget *widget)
{
  GtkComboBox *box = GTK_COMBO_BOX (widget);
  GtkTreeModel *model = gtk_combo_box_get_model (box);
  GtkListStore *store = GTK_LIST_STORE (model);

  gtk_list_store_clear (store);
}

typedef struct
{
  const char *text;
  gboolean found;
  GtkTreeIter iter;
} ForeachInfo;

static gboolean
foreach (GtkTreeModel *model,
         GtkTreePath *path,
         GtkTreeIter *iter,
         gpointer data)
{
  ForeachInfo *info = data;
  char *text = NULL;

  gtk_tree_model_get (model, iter, TEXT_COL, &text, -1);

  g_assert (text != NULL);

  if (strcmp (info->text, text) == 0)
    {
      info->found = TRUE;
      info->iter = *iter;
      return TRUE;
    }

  return FALSE;
}

static void
add_key (GtkTreeModel *model,
         const char *text,
         gboolean preferred,
         int width, int height, int rate,
         GnomeRRRotation rotation)
{
  ForeachInfo info;

  info.text = text;
  info.found = FALSE;

  gtk_tree_model_foreach (model, foreach, &info);

  if (!info.found)
    {
      GtkTreeIter iter;
      g_debug ("adding %s with rate %d Hz", text, rate);
      gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, -1,
                                         TEXT_COL, text,
                                         WIDTH_COL, width,
                                         HEIGHT_COL, height,
                                         RATE_COL, rate,
                                         SORT_COL, width * 10000 + height,
                                         ROTATION_COL, rotation,
                                         -1);
      return;
    }

  /* Look, the preferred output, replace the old one */
  if (preferred)
    {
      g_debug ("replacing %s with rate %d Hz (preferred mode)", text, rate);
      gtk_list_store_set (GTK_LIST_STORE (model), &info.iter,
                          RATE_COL, rate,
                          -1);
      return;
    }

  {
    int old_rate;

    gtk_tree_model_get (model, &info.iter,
                        RATE_COL, &old_rate,
                        -1);

    /* Higher refresh rate */
    if (rate > old_rate)
    {
      g_debug ("replacing %s with rate %d Hz (old rate: %d)", text, rate, old_rate);
      gtk_list_store_set (GTK_LIST_STORE (model), &info.iter,
                          RATE_COL, rate,
                          -1);
      return;
    }
  }

  g_debug ("not adding %s with rate %d Hz (higher rate already there)", text, rate);
}

static void
add_mode (CcDisplayPanel *self,
	  GnomeRRMode *mode,
	  gint  output_width,
	  gint  output_height,
	  guint preferred_id)
{
  int width, height, rate;

  width = gnome_rr_mode_get_width (mode);
  height = gnome_rr_mode_get_height (mode);
  rate = gnome_rr_mode_get_freq (mode);

  if (should_show_resolution (output_width, output_height, width, height))
    {
      char *text;
      gboolean preferred;

      preferred = (gnome_rr_mode_get_id (mode) == preferred_id);
      text = make_resolution_string (width, height);
      add_key (gtk_combo_box_get_model (GTK_COMBO_BOX (self->priv->resolution_combo)),
               text, preferred, width, height, rate, -1);
      g_free (text);
    }
}



static gboolean
combo_select (GtkWidget *widget, const char *text)
{
  GtkComboBox *box = GTK_COMBO_BOX (widget);
  GtkTreeModel *model = gtk_combo_box_get_model (box);
  ForeachInfo info;

  info.text = text;
  info.found = FALSE;

  gtk_tree_model_foreach (model, foreach, &info);

  if (!info.found)
    return FALSE;

  gtk_combo_box_set_active_iter (box, &info.iter);
  return TRUE;
}

static GnomeRRMode **
get_current_modes (CcDisplayPanel *self)
{
  GnomeRROutput *output;

  if (gnome_rr_config_get_clone (self->priv->current_configuration))
    {
      return gnome_rr_screen_list_clone_modes (self->priv->screen);
    }
  else
    {
      if (!self->priv->current_output)
        return NULL;

      output = gnome_rr_screen_get_output_by_name (self->priv->screen,
                                                   gnome_rr_output_info_get_name (self->priv->current_output));

      if (!output)
        return NULL;

      return gnome_rr_output_list_modes (output);
    }
}

static void
rebuild_rotation_combo (CcDisplayPanel *self)
{
  typedef struct
  {
    GnomeRRRotation	rotation;
    const char *	name;
  } RotationInfo;
  static const RotationInfo rotations[] = {
    { GNOME_RR_ROTATION_0, NC_("display panel, rotation", "Normal") },
    { GNOME_RR_ROTATION_90, NC_("display panel, rotation", "Counterclockwise") },
    { GNOME_RR_ROTATION_270, NC_("display panel, rotation", "Clockwise") },
    { GNOME_RR_ROTATION_180, NC_("display panel, rotation", "180 Degrees") },
  };
  const char *selection;
  GnomeRRRotation current;
  int i;

  clear_combo (self->priv->rotation_combo);

  gtk_widget_set_sensitive (self->priv->rotation_combo,
                            self->priv->current_output && gnome_rr_output_info_is_active (self->priv->current_output));

  if (!self->priv->current_output)
    return;

  current = gnome_rr_output_info_get_rotation (self->priv->current_output);

  selection = NULL;
  for (i = 0; i < G_N_ELEMENTS (rotations); ++i)
    {
      const RotationInfo *info = &(rotations[i]);

      gnome_rr_output_info_set_rotation (self->priv->current_output, info->rotation);

      /* NULL-GError --- FIXME: we should say why this rotation is not available! */
      if (gnome_rr_config_applicable (self->priv->current_configuration, self->priv->screen, NULL))
        {
          add_key (gtk_combo_box_get_model (GTK_COMBO_BOX (self->priv->rotation_combo)), g_dpgettext2 (NULL, "display panel, rotation", info->name), FALSE, 0, 0, 0, info->rotation);

          if (info->rotation == current)
            selection = g_dpgettext2 (NULL, "display panel, rotation", info->name);
        }
    }

  gnome_rr_output_info_set_rotation (self->priv->current_output, current);

  if (!(selection && combo_select (self->priv->rotation_combo, selection)))
    gtk_combo_box_set_active (GTK_COMBO_BOX (self->priv->rotation_combo), 0);
}

static int
count_active_outputs (CcDisplayPanel *self)
{
  int i, count = 0;
  GnomeRROutputInfo **outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);

  for (i = 0; outputs[i] != NULL; ++i)
    {
      if (gnome_rr_output_info_is_active (outputs[i]))
        count++;
    }

  return count;
}

#if 0
static int
count_all_outputs (GnomeRRConfig *config)
{
  int i;
  GnomeRROutputInfo **outputs = gnome_rr_config_get_outputs (config);

  for (i = 0; outputs[i] != NULL; i++)
    ;

  return i;
}
#endif

/* Computes whether "Mirror displays" (clone mode) is supported based on these criteria:
 *
 * 1. There is an available size for cloning.
 *
 * 2. There are 2 or more connected outputs that support that size.
 */
static gboolean
mirror_screens_is_supported (CcDisplayPanel *self)
{
  int clone_width, clone_height;
  gboolean have_clone_size;
  gboolean mirror_is_supported;

  mirror_is_supported = FALSE;

  have_clone_size = get_clone_size (self->priv->screen, &clone_width, &clone_height);

  if (have_clone_size) {
    int i;
    int num_outputs_with_clone_size;
    GnomeRROutputInfo **outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);

    num_outputs_with_clone_size = 0;

    for (i = 0; outputs[i] != NULL; i++)
      {
         /* We count the connected outputs that support the clone size.  It
          * doesn't matter if those outputs aren't actually On currently; we
          * will turn them on in on_clone_changed().
          */
         if (gnome_rr_output_info_is_connected (outputs[i]) && output_info_supports_mode (self, outputs[i], clone_width, clone_height))
           num_outputs_with_clone_size++;
      }

    if (num_outputs_with_clone_size >= 2)
      mirror_is_supported = TRUE;
  }

  return mirror_is_supported;
}

static void
rebuild_mirror_screens (CcDisplayPanel *self)
{
  gboolean mirror_is_active;
  gboolean mirror_is_supported;

  g_signal_handlers_block_by_func (self->priv->clone_checkbox, G_CALLBACK (on_clone_changed), self);

  mirror_is_active = self->priv->current_configuration && gnome_rr_config_get_clone (self->priv->current_configuration);

  /* If mirror_is_active, then it *must* be possible to turn mirroring off */
  mirror_is_supported = mirror_is_active || mirror_screens_is_supported (self);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->clone_checkbox), mirror_is_active);
  gtk_widget_set_sensitive (self->priv->clone_checkbox, mirror_is_supported);
  gtk_widget_set_sensitive (self->priv->clone_label, mirror_is_supported);

  g_signal_handlers_unblock_by_func (self->priv->clone_checkbox, G_CALLBACK (on_clone_changed), self);
}

static char *
mirror_monitor_name (void)
{
  /* Translators:  this is the feature where what you see on your laptop's
   * screen is the same as your external monitor.  Here, "Mirror" is being
   * used as an adjective, not as a verb.  For example, the Spanish
   * translation could be "Pantallas en Espejo", *not* "Espejar Pantallas".
   */
  return g_strdup (_("Mirror Displays"));
}

static void
rebuild_current_monitor_label (CcDisplayPanel *self)
{
  char *str, *tmp;
  GdkColor color;
  gboolean use_color;

  if (self->priv->current_output)
    {
      if (gnome_rr_config_get_clone (self->priv->current_configuration))
        tmp = mirror_monitor_name ();
      else
        tmp = g_strdup (gnome_rr_output_info_get_display_name (self->priv->current_output));

      str = g_strdup_printf ("<b>%s</b>", tmp);
      gnome_rr_labeler_get_color_for_output (self->priv->labeler, self->priv->current_output, &color);
      use_color = TRUE;
      g_free (tmp);
    }
  else
    {
      str = g_strdup_printf ("<b>%s</b>", _("Monitor"));
      use_color = FALSE;
    }

  gtk_label_set_markup (GTK_LABEL (self->priv->current_monitor_label), str);
  g_free (str);

  if (use_color)
    {
      GdkRGBA black = { 0, 0, 0, 1.0 };
      GdkRGBA light;

      light.red = color.red / 65535.0;
      light.green = color.green / 65535.0;
      light.blue = color.blue / 65535.0;
      light.alpha = 1.0;
      gtk_widget_override_background_color (self->priv->current_monitor_event_box,
                                            gtk_widget_get_state_flags (self->priv->current_monitor_event_box),
                                            &light);

      /* Make the label explicitly black.  We don't want it to follow the
       * theme's colors, since the label is always shown against a light
       * pastel background.  See bgo#556050
       */
      gtk_widget_override_color (self->priv->current_monitor_label,
                                 gtk_widget_get_state_flags (self->priv->current_monitor_label),
                                 &black);
    }
  else
    {
      /* Remove any modifications we did on the label's color */
      gtk_widget_override_color (self->priv->current_monitor_label,
                                 gtk_widget_get_state_flags (self->priv->current_monitor_label),
                                 NULL);
    }

    gtk_event_box_set_visible_window (GTK_EVENT_BOX (self->priv->current_monitor_event_box), use_color);
}

static void
rebuild_on_off_radios (CcDisplayPanel *self)
{
  gboolean sensitive;
  gboolean on_active;

  g_signal_handlers_block_by_func (self->priv->monitor_switch, G_CALLBACK (monitor_switch_active_cb), self);

  sensitive = FALSE;
  on_active = FALSE;

  if (!gnome_rr_config_get_clone (self->priv->current_configuration) && self->priv->current_output)
    {
      if (count_active_outputs (self) > 1 || !gnome_rr_output_info_is_active (self->priv->current_output))
        sensitive = TRUE;
      else
        sensitive = FALSE;

      on_active = gnome_rr_output_info_is_active (self->priv->current_output);
    }

  gtk_widget_set_sensitive (self->priv->monitor_switch, sensitive);

  gtk_switch_set_active (GTK_SWITCH (self->priv->monitor_switch), on_active);

  g_signal_handlers_unblock_by_func (self->priv->monitor_switch, G_CALLBACK (monitor_switch_active_cb), self);
}

static char *
make_resolution_string (int width, int height)
{
  int ratio;
  const char *aspect = NULL;

  if (width && height) {
    if (width > height)
      ratio = width * 10 / height;
    else
      ratio = height * 10 / width;

    switch (ratio) {
    case 13:
      aspect = "4:3";
      break;
    case 16:
      aspect = "16:10";
      break;
    case 17:
      aspect = "16:9";
      break;
    case 12:
      aspect = "5:4";
      break;
      /* This catches 1.5625 as well (1600x1024) when maybe it shouldn't. */
    case 15:
      aspect = "3:2";
      break;
    case 18:
      aspect = "9:5";
      break;
    case 10:
      aspect = "1:1";
      break;
    }
  }

  if (aspect != NULL)
    return g_strdup_printf (_("%d x %d (%s)"), width, height, aspect);
  else
    return g_strdup_printf (_("%d x %d"), width, height);
}

static void
find_best_mode (GnomeRRMode **modes, int *out_width, int *out_height)
{
  int i;

  *out_width = 0;
  *out_height = 0;

  for (i = 0; modes[i] != NULL; i++)
    {
      int w, h;

      w = gnome_rr_mode_get_width (modes[i]);
      h = gnome_rr_mode_get_height (modes[i]);

      if (w * h > *out_width * *out_height)
        {
          *out_width = w;
          *out_height = h;
        }
    }
}

static void
rebuild_resolution_combo (CcDisplayPanel *self)
{
  int i;
  GnomeRRMode **modes;
  GnomeRRMode *mode;
  char *current;
  int output_width, output_height;
  guint32 preferred_id;
  GnomeRROutput *output;

  clear_combo (self->priv->resolution_combo);

  if (!(modes = get_current_modes (self))
      || !self->priv->current_output
      || !gnome_rr_output_info_is_active (self->priv->current_output))
    {
      gtk_widget_set_sensitive (self->priv->resolution_combo, FALSE);
      return;
    }

  g_assert (self->priv->current_output != NULL);

  gnome_rr_output_info_get_geometry (self->priv->current_output, NULL, NULL, &output_width, &output_height);
  g_assert (output_width != 0 && output_height != 0);

  gtk_widget_set_sensitive (self->priv->resolution_combo, TRUE);

  output = gnome_rr_screen_get_output_by_name (self->priv->screen,
                                               gnome_rr_output_info_get_name (self->priv->current_output));
  mode = gnome_rr_output_get_preferred_mode (output);
  preferred_id = gnome_rr_mode_get_id (mode);

  for (i = 0; modes[i] != NULL; ++i)
    add_mode (self, modes[i], output_width, output_height, preferred_id);

  /* And force the preferred mode in the drop-down */
  add_mode (self, mode, output_width, output_height, preferred_id);

  current = make_resolution_string (output_width, output_height);

  if (!combo_select (self->priv->resolution_combo, current))
    {
      int best_w, best_h;
      char *str;

      find_best_mode (modes, &best_w, &best_h);
      str = make_resolution_string (best_w, best_h);
      combo_select (self->priv->resolution_combo, str);
      g_free (str);
    }

  g_free (current);
}

static void
rebuild_gui (CcDisplayPanel *self)
{
  /* We would break spectacularly if we recursed, so
   * just assert if that happens
   */
  g_assert (self->priv->ignore_gui_changes == FALSE);

  self->priv->ignore_gui_changes = TRUE;

  rebuild_mirror_screens (self);
  rebuild_current_monitor_label (self);
  rebuild_on_off_radios (self);
  rebuild_resolution_combo (self);
  rebuild_rotation_combo (self);

  self->priv->ignore_gui_changes = FALSE;
}

static gboolean
get_mode (GtkWidget *widget, int *width, int *height, int *rate, GnomeRRRotation *rot)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkComboBox *box = GTK_COMBO_BOX (widget);
  int dummy;

  if (!gtk_combo_box_get_active_iter (box, &iter))
    return FALSE;

  if (!width)
    width = &dummy;

  if (!height)
    height = &dummy;

  if (!rate)
    rate = &dummy;

  if (!rot)
    rot = (GnomeRRRotation *)&dummy;

  model = gtk_combo_box_get_model (box);
  gtk_tree_model_get (model, &iter,
                      WIDTH_COL, width,
                      HEIGHT_COL, height,
                      RATE_COL, rate,
                      ROTATION_COL, rot,
                      -1);

  return TRUE;

}

static void
on_rotation_changed (GtkComboBox *box, gpointer data)
{
  CcDisplayPanel *self = data;
  GnomeRRRotation rotation;

  if (!self->priv->current_output)
    return;

  if (get_mode (self->priv->rotation_combo, NULL, NULL, NULL, &rotation))
    gnome_rr_output_info_set_rotation (self->priv->current_output, rotation);

  foo_scroll_area_invalidate (FOO_SCROLL_AREA (self->priv->area));
}

static void
select_resolution_for_current_output (CcDisplayPanel *self)
{
  GnomeRRMode **modes;
  int width, height;
  int x,y;
  gnome_rr_output_info_get_geometry (self->priv->current_output, &x, &y, NULL, NULL);

  width = gnome_rr_output_info_get_preferred_width (self->priv->current_output);
  height = gnome_rr_output_info_get_preferred_height (self->priv->current_output);

  if (width != 0 && height != 0)
    {
      gnome_rr_output_info_set_geometry (self->priv->current_output, x, y, width, height);
      return;
    }

  modes = get_current_modes (self);
  if (!modes)
    return;

  find_best_mode (modes, &width, &height);

  gnome_rr_output_info_set_geometry (self->priv->current_output, x, y, width, height);
}

static void
monitor_switch_active_cb (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    data)
{
  CcDisplayPanel *self = data;
  gboolean value;

  if (!self->priv->current_output)
    return;

  value = gtk_switch_get_active (GTK_SWITCH (object));

  if (value)
    {
      gnome_rr_output_info_set_active (self->priv->current_output, TRUE);
      select_resolution_for_current_output (self);
    }
  else
    {
      gnome_rr_output_info_set_active (self->priv->current_output, FALSE);
      gnome_rr_config_ensure_primary (self->priv->current_configuration);
    }

  rebuild_gui (self);
  foo_scroll_area_invalidate (FOO_SCROLL_AREA (self->priv->area));
}

static void
realign_outputs_after_resolution_change (CcDisplayPanel *self, GnomeRROutputInfo *output_that_changed, int old_width, int old_height)
{
  /* We find the outputs that were below or to the right of the output that
   * changed, and realign them; we also do that for outputs that shared the
   * right/bottom edges with the output that changed.  The outputs that are
   * above or to the left of that output don't need to change.
   */

  int i;
  int old_right_edge, old_bottom_edge;
  int dx, dy;
  int x, y, width, height;
  GnomeRROutputInfo **outputs;

  g_assert (self->priv->current_configuration != NULL);

  gnome_rr_output_info_get_geometry (output_that_changed, &x, &y, &width, &height); 

  if (width == old_width && height == old_height)
    return;

  old_right_edge = x + old_width;
  old_bottom_edge = y + old_height;

  dx = width - old_width;
  dy = height - old_height;

  outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);

  for (i = 0; outputs[i] != NULL; i++)
    {
      int output_x, output_y;
      int output_width, output_height;

      if (outputs[i] == output_that_changed || !gnome_rr_output_info_is_connected (outputs[i]))
        continue;

      gnome_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);

      if (output_x >= old_right_edge)
         output_x += dx;
      else if (output_x + output_width == old_right_edge)
         output_x = x + width - output_width;

      if (output_y >= old_bottom_edge)
         output_y += dy;
      else if (output_y + output_height == old_bottom_edge)
         output_y = y + height - output_height;

      gnome_rr_output_info_set_geometry (outputs[i], output_x, output_y, output_width, output_height);
    }
}

static void
on_resolution_changed (GtkComboBox *box, gpointer data)
{
  CcDisplayPanel *self = data;
  int old_width, old_height;
  int x,y;
  int width;
  int height;

  if (!self->priv->current_output)
    return;

  gnome_rr_output_info_get_geometry (self->priv->current_output, &x, &y, &old_width, &old_height);

  if (get_mode (self->priv->resolution_combo, &width, &height, NULL, NULL))
    {
      gnome_rr_output_info_set_geometry (self->priv->current_output, x, y, width, height);

      if (width == 0 || height == 0)
        gnome_rr_output_info_set_active (self->priv->current_output, FALSE);
      else
        gnome_rr_output_info_set_active (self->priv->current_output, TRUE);
    }

  realign_outputs_after_resolution_change (self, self->priv->current_output, old_width, old_height);

  rebuild_rotation_combo (self);

  foo_scroll_area_invalidate (FOO_SCROLL_AREA (self->priv->area));
}

static void
lay_out_outputs_horizontally (CcDisplayPanel *self)
{
  int i;
  int x;
  GnomeRROutputInfo **outputs;

  /* Lay out all the monitors horizontally when "mirror screens" is turned
   * off, to avoid having all of them overlapped initially.  We put the
   * outputs turned off on the right-hand side.
   */

  x = 0;

  /* First pass, all "on" outputs */
  outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);

  for (i = 0; outputs[i]; ++i)
    {
      int width, height;
      if (gnome_rr_output_info_is_connected (outputs[i]) && gnome_rr_output_info_is_active (outputs[i]))
        {
          gnome_rr_output_info_get_geometry (outputs[i], NULL, NULL, &width, &height);
          gnome_rr_output_info_set_geometry (outputs[i], x, 0, width, height);
          x += width;
        }
    }

  /* Second pass, all the black screens */

    for (i = 0; outputs[i]; ++i)
    {
      int width, height;
      if (!(gnome_rr_output_info_is_connected (outputs[i]) && gnome_rr_output_info_is_active (outputs[i])))
        {
          gnome_rr_output_info_get_geometry (outputs[i], NULL, NULL, &width, &height);
          gnome_rr_output_info_set_geometry (outputs[i], x, 0, width, height);
          x += width;
        }
    }

}

/* FIXME: this function is copied from gnome-settings-daemon/plugins/xrandr/gsd-xrandr-manager.c.
 * Do we need to put this function in gnome-desktop for public use?
 */
static gboolean
get_clone_size (GnomeRRScreen *screen, int *width, int *height)
{
  GnomeRRMode **modes = gnome_rr_screen_list_clone_modes (screen);
  int best_w, best_h;
  int i;

  best_w = 0;
  best_h = 0;

  for (i = 0; modes[i] != NULL; ++i) {
    GnomeRRMode *mode = modes[i];
    int w, h;

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
output_info_supports_mode (CcDisplayPanel *self, GnomeRROutputInfo *info, int width, int height)
{
  GnomeRROutput *output;
  GnomeRRMode **modes;
  int i;

  if (!gnome_rr_output_info_is_connected (info))
    return FALSE;

  output = gnome_rr_screen_get_output_by_name (self->priv->screen, gnome_rr_output_info_get_name (info));
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
on_clone_changed (GtkWidget *box, gpointer data)
{
  CcDisplayPanel *self = data;

  gnome_rr_config_set_clone (self->priv->current_configuration, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->clone_checkbox)));

  if (gnome_rr_config_get_clone (self->priv->current_configuration))
    {
      int i;
      int width, height;
      GnomeRROutputInfo **outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);

      for (i = 0; outputs[i]; ++i)
	{
	  if (gnome_rr_output_info_is_connected (outputs[i]))
	    {
	      self->priv->current_output = outputs[i];
	      break;
	    }
	}

      /* Turn on all the connected screens that support the best clone mode.
       * The user may hit "Mirror displays", but he shouldn't have to turn on
       * all the required outputs as well.
       */

      get_clone_size (self->priv->screen, &width, &height);

      for (i = 0; outputs[i]; i++) {
	int x, y;
	if (output_info_supports_mode (self, outputs[i], width, height)) {
	  gnome_rr_output_info_set_active (outputs[i], TRUE);
	  gnome_rr_output_info_get_geometry (outputs[i], &x, &y, NULL, NULL);
	  gnome_rr_output_info_set_geometry (outputs[i], x, y, width, height);
	}
      }
    }
  else
    {
      if (output_overlaps (self->priv->current_output, self->priv->current_configuration))
        lay_out_outputs_horizontally (self);
    }

  rebuild_gui (self);
}

static void
apply_rotation_to_geometry (GnomeRROutputInfo *output, int *w, int *h)
{
  GnomeRRRotation rotation;

  rotation = gnome_rr_output_info_get_rotation (output);
  if ((rotation & GNOME_RR_ROTATION_90) || (rotation & GNOME_RR_ROTATION_270))
    {
      int tmp;
      tmp = *h;
      *h = *w;
      *w = tmp;
    }
}

static void
get_geometry (GnomeRROutputInfo *output, int *w, int *h)
{
  if (gnome_rr_output_info_is_active (output))
    {
      gnome_rr_output_info_get_geometry (output, NULL, NULL, w, h);
    }
  else
    {
      *h = gnome_rr_output_info_get_preferred_height (output);
      *w = gnome_rr_output_info_get_preferred_width (output);
    }

  apply_rotation_to_geometry (output, w, h);
}

#define SPACE 15
#define MARGIN  15

static GList *
list_connected_outputs (CcDisplayPanel *self, int *total_w, int *total_h)
{
  int i, dummy;
  GList *result = NULL;
  GnomeRROutputInfo **outputs;

  if (!total_w)
    total_w = &dummy;
  if (!total_h)
    total_h = &dummy;

  *total_w = 0;
  *total_h = 0;

  outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);
  for (i = 0; outputs[i] != NULL; ++i)
    {
      if (gnome_rr_output_info_is_connected (outputs[i]))
	{
	  int w, h;

	  result = g_list_prepend (result, outputs[i]);

	  get_geometry (outputs[i], &w, &h);

          *total_w += w;
          *total_h += h;
        }
    }

  return g_list_reverse (result);
}

static int
get_n_connected (CcDisplayPanel *self)
{
  GList *connected_outputs = list_connected_outputs (self, NULL, NULL);
  int n = g_list_length (connected_outputs);

  g_list_free (connected_outputs);

  return n;
}

static double
compute_scale (CcDisplayPanel *self)
{
  int available_w, available_h;
  int total_w, total_h;
  int n_monitors;
  GdkRectangle viewport;
  GList *connected_outputs;

  foo_scroll_area_get_viewport (FOO_SCROLL_AREA (self->priv->area), &viewport);

  connected_outputs = list_connected_outputs (self, &total_w, &total_h);

  n_monitors = g_list_length (connected_outputs);

  g_list_free (connected_outputs);

  available_w = viewport.width - 2 * MARGIN - (n_monitors - 1) * SPACE;
  available_h = viewport.height - 2 * MARGIN - (n_monitors - 1) * SPACE;

  return MIN ((double)available_w / total_w, (double)available_h / total_h);
}

typedef struct Edge
{
  GnomeRROutputInfo *output;
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
add_edge (GnomeRROutputInfo *output, int x1, int y1, int x2, int y2, GArray *edges)
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
list_edges_for_output (GnomeRROutputInfo *output, GArray *edges)
{
  int x, y, w, h;

  gnome_rr_output_info_get_geometry (output, &x, &y, &w, &h);

  apply_rotation_to_geometry (output, &w, &h);

  /* Top, Bottom, Left, Right */
  add_edge (output, x, y, x + w, y, edges);
  add_edge (output, x, y + h, x + w, y + h, edges);
  add_edge (output, x, y, x, y + h, edges);
  add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (GnomeRRConfig *config, GArray *edges)
{
  int i;
  GnomeRROutputInfo **outputs = gnome_rr_config_get_outputs (config);

  for (i = 0; outputs[i]; ++i)
    {
      if (gnome_rr_output_info_is_connected (outputs[i]))
	list_edges_for_output (outputs[i], edges);
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
list_snaps (GnomeRROutputInfo *output, GArray *edges, GArray *snaps)
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
output_is_aligned (GnomeRROutputInfo *output, GArray *edges)
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
get_output_rect (GnomeRROutputInfo *output, GdkRectangle *rect)
{
  gnome_rr_output_info_get_geometry (output, &rect->x, &rect->y, &rect->width, &rect->height);

  apply_rotation_to_geometry (output, &rect->width, &rect->height);
}

static gboolean
output_overlaps (GnomeRROutputInfo *output, GnomeRRConfig *config)
{
  int i;
  GdkRectangle output_rect;
  GnomeRROutputInfo **outputs;

  g_assert (output != NULL);

  get_output_rect (output, &output_rect);

  outputs = gnome_rr_config_get_outputs (config);
  for (i = 0; outputs[i]; ++i)
    {
      if (outputs[i] != output && gnome_rr_output_info_is_connected (outputs[i]))
	{
	  GdkRectangle other_rect;

	  get_output_rect (outputs[i], &other_rect);
	  if (gdk_rectangle_intersect (&output_rect, &other_rect, NULL))
	    return TRUE;
	}
    }

  return FALSE;
}

static gboolean
gnome_rr_config_is_aligned (GnomeRRConfig *config, GArray *edges)
{
  int i;
  gboolean result = TRUE;
  GnomeRROutputInfo **outputs;

  outputs = gnome_rr_config_get_outputs (config);
  for (i = 0; outputs[i]; ++i)
    {
      if (gnome_rr_output_info_is_connected (outputs[i]))
	{
	  if (!output_is_aligned (outputs[i], edges))
	    return FALSE;

	  if (output_overlaps (outputs[i], config))
	    return FALSE;
	}
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
set_top_bar_tooltip (CcDisplayPanel *self, gboolean is_dragging)
{
  const char *text;

  if (is_dragging)
    text = NULL;
  else
    text = _("Drag to change primary display.");

  gtk_widget_set_tooltip_text (self->priv->area, text);
}

static void
on_top_bar_event (FooScrollArea *area,
                  FooScrollAreaEvent *event,
                  CcDisplayPanel *self)
{
  /* Ignore drops */
  if (event->type == FOO_DROP)
    return;

  /* If the mouse is inside the top bar, set the cursor to "you can move me".  See
   * on_canvas_event() for where we reset the cursor to the default if it
   * exits the outputs' area.
   */
  if (!gnome_rr_config_get_clone (self->priv->current_configuration) && get_n_connected (self) > 1)
    set_cursor (GTK_WIDGET (area), GDK_HAND1);

  if (event->type == FOO_BUTTON_PRESS)
    {
      rebuild_gui (self);
      set_top_bar_tooltip (self, TRUE);

      if (!gnome_rr_config_get_clone (self->priv->current_configuration) && get_n_connected (self) > 1)
        {
          self->priv->dragging_top_bar = TRUE;
          foo_scroll_area_begin_grab (area, (FooScrollAreaEventFunc) on_top_bar_event, self);
        }

      foo_scroll_area_invalidate (area);
    }
  else
    {
      if (foo_scroll_area_is_grabbed (area))
        {
          if (event->type == FOO_BUTTON_RELEASE)
            {
              foo_scroll_area_end_grab (area, event);
              self->priv->dragging_top_bar = FALSE;
              set_top_bar_tooltip (self, FALSE);
            }

          foo_scroll_area_invalidate (area);
        }
    }
}

static void
set_monitors_tooltip (CcDisplayPanel *self, gboolean is_dragging)
{
  const char *text;

  if (is_dragging)
    text = NULL;
  else
    text = _("Select a monitor to change its properties; drag it to rearrange its placement.");

  gtk_widget_set_tooltip_text (self->priv->area, text);
}

static void
set_primary_output (CcDisplayPanel *self,
                    GnomeRROutputInfo *output)
{
  int i;
  GnomeRROutputInfo **outputs;

  outputs = gnome_rr_config_get_outputs (self->priv->current_configuration);
  for (i = 0; outputs[i] != NULL; ++i)
    gnome_rr_output_info_set_primary (outputs[i], outputs[i] == output);
}

static void
on_output_event (FooScrollArea *area,
                 FooScrollAreaEvent *event,
                 gpointer data)
{
  GnomeRROutputInfo *output = data;
  CcDisplayPanel *self = g_object_get_data (G_OBJECT (area), "panel");

  if (event->type == FOO_DRAG_HOVER)
    {
      if (gnome_rr_output_info_is_active (output) && self->priv->dragging_top_bar)
        set_primary_output (self, output);
      return;
    }
  if (event->type == FOO_DROP)
    {
      /* Activate new primary? */
      return;
    }

  /* If the mouse is inside the outputs, set the cursor to "you can move me".  See
   * on_canvas_event() for where we reset the cursor to the default if it
   * exits the outputs' area.
   */
  if (!gnome_rr_config_get_clone (self->priv->current_configuration) && get_n_connected (self) > 1)
    set_cursor (GTK_WIDGET (area), GDK_FLEUR);

  if (event->type == FOO_BUTTON_PRESS)
    {
      GrabInfo *info;

      self->priv->current_output = output;

      rebuild_gui (self);
      set_monitors_tooltip (self, TRUE);

      if (!gnome_rr_config_get_clone (self->priv->current_configuration) && get_n_connected (self) > 1)
	{
	  int output_x, output_y;
	  gnome_rr_output_info_get_geometry (output, &output_x, &output_y, NULL, NULL);

	  foo_scroll_area_begin_grab (area, on_output_event, data);

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
	  double scale = compute_scale (self);
	  int old_x, old_y;
	  int width, height;
	  int new_x, new_y;
	  int i;
	  GArray *edges, *snaps, *new_edges;

	  gnome_rr_output_info_get_geometry (output, &old_x, &old_y, &width, &height);
	  new_x = info->output_x + (event->x - info->grab_x) / scale;
	  new_y = info->output_y + (event->y - info->grab_y) / scale;

	  gnome_rr_output_info_set_geometry (output, new_x, new_y, width, height);

	  edges = g_array_new (TRUE, TRUE, sizeof (Edge));
	  snaps = g_array_new (TRUE, TRUE, sizeof (Snap));
	  new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

	  list_edges (self->priv->current_configuration, edges);
	  list_snaps (output, edges, snaps);

	  g_array_sort (snaps, compare_snaps);

	  gnome_rr_output_info_set_geometry (output, new_x, new_y, width, height);

	  for (i = 0; i < snaps->len; ++i)
	    {
	      Snap *snap = &(g_array_index (snaps, Snap, i));
	      GArray *new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

	      gnome_rr_output_info_set_geometry (output, new_x + snap->dx, new_y + snap->dy, width, height);

	      g_array_set_size (new_edges, 0);
	      list_edges (self->priv->current_configuration, new_edges);

	      if (gnome_rr_config_is_aligned (self->priv->current_configuration, new_edges))
		{
		  g_array_free (new_edges, TRUE);
		  break;
		}
	      else
		{
		  gnome_rr_output_info_set_geometry (output, info->output_x, info->output_y, width, height);
		}
	    }

	  g_array_free (new_edges, TRUE);
	  g_array_free (snaps, TRUE);
	  g_array_free (edges, TRUE);

	  if (event->type == FOO_BUTTON_RELEASE)
	    {
	      foo_scroll_area_end_grab (area, event);
	      set_monitors_tooltip (self, FALSE);

	      g_free (g_object_get_data (G_OBJECT (output), "grab-info"));
	      g_object_set_data (G_OBJECT (output), "grab-info", NULL);

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

static PangoLayout *
get_display_name (CcDisplayPanel *self,
		  GnomeRROutputInfo *output)
{
  PangoLayout *layout;
  char *text;

  if (gnome_rr_config_get_clone (self->priv->current_configuration))
    text = mirror_monitor_name ();
  else
    text = g_strdup (gnome_rr_output_info_get_display_name (output));

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->priv->area), text);
  g_free (text);
  pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

  return layout;
}

static void
paint_background (FooScrollArea *area,
                  cairo_t       *cr)
{
  GdkRectangle viewport;
  GtkWidget *widget;
  GtkStyleContext *context;
  GdkRGBA fg, bg;

  widget = GTK_WIDGET (area);

  foo_scroll_area_get_viewport (area, &viewport);
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &fg);
  gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg);

  cairo_set_source_rgba (cr,
                         (fg.red + bg.red) / 2,
                         (fg.green + bg.green) / 2,
                         (fg.blue + bg.blue) / 2,
                         (fg.alpha + bg.alpha) / 2);

  cairo_rectangle (cr,
                   viewport.x, viewport.y,
                   viewport.width, viewport.height);

  cairo_fill_preserve (cr);

  foo_scroll_area_add_input_from_fill (area, cr, on_canvas_event, NULL);

  cairo_set_source_rgba (cr,
                         0.7 * bg.red,
                         0.7 * bg.green,
                         0.7 * bg.blue,
                         0.7 * bg.alpha);

  cairo_stroke (cr);
}

static void
color_shade (double *r,
             double *g,
             double *b,
             double  k)
{
  double h, s, v;

  gtk_rgb_to_hsv (*r, *g, *b, &h, &s, &v);

  s *= k;
  if (s > 1.0)
    s = 1.0;
  else if (s < 0.0)
    s = 0.0;

  v *= k;
  if (v > 1.0)
    v = 1.0;
  else if (v < 0.0)
    v = 0.0;

  gtk_hsv_to_rgb (h, s, v, r, g, b);
}

static void
paint_output (CcDisplayPanel *self, cairo_t *cr, int i)
{
  int w, h;
  double scale = compute_scale (self);
  double x, y;
  int output_x, output_y;
  GnomeRRRotation rotation;
  int total_w, total_h;
  GList *connected_outputs = list_connected_outputs (self, &total_w, &total_h);
  GnomeRROutputInfo *output = g_list_nth (connected_outputs, i)->data;
  PangoLayout *layout = get_display_name (self, output);
  PangoRectangle ink_extent, log_extent;
  GdkRectangle viewport;
  GdkColor output_color;
  double r, g, b;
  double available_w;
  double factor;

  cairo_save (cr);

  foo_scroll_area_get_viewport (FOO_SCROLL_AREA (self->priv->area), &viewport);
  get_geometry (output, &w, &h);

#if 0
  g_debug ("%s (%p) geometry %d %d %d primary=%d", output->name, output->name,
           w, h, output->rate, output->primary);
#endif

  viewport.height -= 2 * MARGIN;
  viewport.width -= 2 * MARGIN;

  gnome_rr_output_info_get_geometry (output, &output_x, &output_y, NULL, NULL);
  x = output_x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0;
  y = output_y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0;

#if 0
  g_debug ("scaled: %f %f", x, y);

  g_debug ("scale: %f", scale);

  g_debug ("%f %f %f %f", x, y, w * scale + 0.5, h * scale + 0.5);
#endif

  cairo_translate (cr,
                   x + (w * scale + 0.5) / 2,
                   y + (h * scale + 0.5) / 2);

  /* rotation is already applied in get_geometry */

  rotation = gnome_rr_output_info_get_rotation (output);
  if (rotation & GNOME_RR_REFLECT_X)
    cairo_scale (cr, -1, 1);

  if (rotation & GNOME_RR_REFLECT_Y)
    cairo_scale (cr, 1, -1);

  cairo_translate (cr,
                   - x - (w * scale + 0.5) / 2,
                   - y - (h * scale + 0.5) / 2);

  if (output == self->priv->current_output)
    {
      GtkStyleContext *context;
      GdkRGBA color;

      context = gtk_widget_get_style_context (self->priv->area);
      gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED, &color);

      cairo_rectangle (cr, x - 2, y - 2, w * scale + 0.5 + 4, h * scale + 0.5 + 4);

      cairo_set_line_width (cr, 4);
      cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.5);
      cairo_stroke (cr);
    }

  cairo_rectangle (cr, x, y, w * scale + 0.5, h * scale + 0.5);
  cairo_clip_preserve (cr);

  gnome_rr_labeler_get_color_for_output (self->priv->labeler, output, &output_color);
  r = output_color.red / 65535.0;
  g = output_color.green / 65535.0;
  b = output_color.blue / 65535.0;

  if (!gnome_rr_output_info_is_active (output))
    {
      /* If the output is turned off, just darken the selected color */
      color_shade (&r, &g, &b, 0.4);
    }

  cairo_set_source_rgba (cr, r, g, b, 1.0);

  foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (self->priv->area),
                                       cr, on_output_event, output);
  cairo_fill (cr);

  cairo_rectangle (cr, x + 0.5, y + 0.5, w * scale + 0.5 - 1, h * scale + 0.5 - 1);

  cairo_set_line_width (cr, 1);
  cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

  cairo_stroke (cr);
  cairo_set_line_width (cr, 2);

  cairo_save (cr);

  layout_set_font (layout, "Sans 10");
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
  if (gnome_rr_output_info_is_active (output))
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
  else
    cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

  pango_cairo_show_layout (cr, layout);
  g_object_unref (layout);
  cairo_restore (cr);

  if (gnome_rr_output_info_get_primary (output))
    {
      const char *clock_format;
      char *text;
      gboolean use_24;
      GDateTime *dt;
      GDesktopClockFormat value;

      /* top bar */
      cairo_rectangle (cr, x, y, w * scale + 0.5, TOP_BAR_HEIGHT);
      cairo_set_source_rgb (cr, 0, 0, 0);
      foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (self->priv->area),
                                           cr,
                                           (FooScrollAreaEventFunc) on_top_bar_event,
                                           self);

      cairo_fill (cr);

      /* clock */
      value = g_settings_get_enum (self->priv->clock_settings, CLOCK_FORMAT_KEY);
      use_24 = value == G_DESKTOP_CLOCK_FORMAT_24H;
      if (use_24)
        clock_format = _("%a %R");
      else
        clock_format = _("%a %l:%M %p");

      dt = g_date_time_new_now_local ();
      text = g_date_time_format (dt, clock_format);
      g_date_time_unref (dt);

      layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->priv->area), text);
      g_free (text);
      pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

      layout_set_font (layout, "Sans 4");
      pango_layout_get_pixel_extents (layout, &ink_extent, &log_extent);

      if (available_w < ink_extent.width)
        factor = available_w / ink_extent.width;
      else
        factor = 1.0;

      cairo_move_to (cr,
                     x + ((w * scale + 0.5) - factor * log_extent.width) / 2,
                     y + (TOP_BAR_HEIGHT - factor * log_extent.height) / 2);

      cairo_scale (cr, factor, factor);

      cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

      pango_cairo_show_layout (cr, layout);
      g_object_unref (layout);
    }

  cairo_restore (cr);
}

static void
on_area_paint (FooScrollArea  *area,
               cairo_t        *cr,
               gpointer        data)
{
  CcDisplayPanel *self = data;
  GList *connected_outputs = NULL;
  GList *list;

  paint_background (area, cr);

  if (!self->priv->current_configuration)
    return;

  connected_outputs = list_connected_outputs (self, NULL, NULL);

  for (list = connected_outputs; list != NULL; list = list->next)
    {
      paint_output (self, cr, g_list_position (connected_outputs, list));

      if (gnome_rr_config_get_clone (self->priv->current_configuration))
	break;
    }
}

static void
make_text_combo (GtkWidget *widget, int sort_column)
{
  GtkComboBox *box = GTK_COMBO_BOX (widget);
  GtkListStore *store = gtk_list_store_new (
                                            NUM_COLS,
                                            G_TYPE_STRING,          /* Text */
                                            G_TYPE_INT,             /* Width */
                                            G_TYPE_INT,             /* Height */
                                            G_TYPE_INT,             /* Frequency */
                                            G_TYPE_INT,             /* Width * Height */
                                            G_TYPE_INT);            /* Rotation */

  GtkCellRenderer *cell;

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (widget));

  gtk_combo_box_set_model (box, GTK_TREE_MODEL (store));

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (box), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (box), cell,
                                  "text", 0,
                                  NULL);

  if (sort_column != -1)
    {
      gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                            sort_column,
                                            GTK_SORT_DESCENDING);
    }
}

static void
compute_virtual_size_for_configuration (GnomeRRConfig *config, int *ret_width, int *ret_height)
{
  int i;
  int width, height;
  int output_x, output_y, output_width, output_height;
  GnomeRROutputInfo **outputs;

  width = height = 0;

  outputs = gnome_rr_config_get_outputs (config);
  for (i = 0; outputs[i] != NULL; i++)
    {
      if (gnome_rr_output_info_is_active (outputs[i]))
	{
	  gnome_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);
	  width = MAX (width, output_x + output_width);
	  height = MAX (height, output_y + output_height);
	}
    }

  *ret_width = width;
  *ret_height = height;
}

static void
check_required_virtual_size (CcDisplayPanel *self)
{
  int req_width, req_height;
  int min_width, max_width;
  int min_height, max_height;

  compute_virtual_size_for_configuration (self->priv->current_configuration, &req_width, &req_height);

  gnome_rr_screen_get_ranges (self->priv->screen, &min_width, &max_width, &min_height, &max_height);

#if 0
  g_debug ("X Server supports:");
  g_debug ("min_width = %d, max_width = %d", min_width, max_width);
  g_debug ("min_height = %d, max_height = %d", min_height, max_height);

  g_debug ("Requesting size of %dx%d", req_width, req_height);
#endif

  if (!(min_width <= req_width && req_width <= max_width
        && min_height <= req_height && req_height <= max_height))
    {
      /* FIXME: present a useful dialog, maybe even before the user tries to Apply */
#if 0
      g_debug ("Your X server needs a larger Virtual size!");
#endif
    }
}

static void
begin_version2_apply_configuration (CcDisplayPanel *self, GdkWindow *parent_window, guint32 timestamp)
{
  XID parent_window_xid;
  GError *error = NULL;

  parent_window_xid = GDK_WINDOW_XID (parent_window);

  self->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     NULL,
                                                     "org.gnome.SettingsDaemon",
                                                     "/org/gnome/SettingsDaemon/XRANDR",
                                                     "org.gnome.SettingsDaemon.XRANDR_2",
                                                     NULL,
                                                     &error);
  if (self->priv->proxy == NULL) {
    error_message (self, _("Failed to apply configuration: %s"), error->message);
    g_error_free (error);
    return;
  }

  g_dbus_proxy_call (self->priv->proxy,
                     "ApplyConfiguration",
                     g_variant_new ("(xx)", (gint64) parent_window_xid, (gint64) timestamp),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     apply_configuration_returned_cb,
                     self);
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

  rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL); /* NULL-GError */
  if (!rr_screen)
    return;

  rr_config = gnome_rr_config_new_current (rr_screen, NULL);
  gnome_rr_config_ensure_primary (rr_config);
  gnome_rr_config_save (rr_config, NULL); /* NULL-GError */

  g_object_unref (rr_config);
  g_object_unref (rr_screen);
}

static void
apply_configuration_returned_cb (GObject          *proxy,
                                 GAsyncResult     *res,
                                 gpointer          data)
{
  CcDisplayPanel *self = data;
  GVariant *result;
  GError *error = NULL;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &error);
  if (error)
    error_message (self, _("Failed to apply configuration: %s"), error->message);
  g_clear_error (&error);  
  if (result)
    g_variant_unref (result);

  g_object_unref (self->priv->proxy);
  self->priv->proxy = NULL;

  gtk_widget_set_sensitive (self->priv->panel, TRUE);
}

static gboolean
sanitize_and_save_configuration (CcDisplayPanel *self)
{
  GError *error;

  gnome_rr_config_sanitize (self->priv->current_configuration);
  gnome_rr_config_ensure_primary (self->priv->current_configuration);

  check_required_virtual_size (self);

  foo_scroll_area_invalidate (FOO_SCROLL_AREA (self->priv->area));

  ensure_current_configuration_is_saved ();

  error = NULL;
  if (!gnome_rr_config_save (self->priv->current_configuration, &error))
    {
      error_message (self, _("Could not save the monitor configuration"), error->message);
      g_error_free (error);
      return FALSE;
    }

  return TRUE;
}

static void
apply (CcDisplayPanel *self)
{
  GdkWindow *window;

  self->priv->apply_button_clicked_timestamp = gtk_get_current_event_time ();

  if (!sanitize_and_save_configuration (self))
    return;

  g_assert (self->priv->proxy == NULL);

  gtk_widget_set_sensitive (self->priv->panel, FALSE);

  window = gtk_widget_get_window (gtk_widget_get_toplevel (self->priv->panel));

  begin_version2_apply_configuration (self, window,
                                      self->priv->apply_button_clicked_timestamp);
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

  return (count_all_outputs (config) == 1 && strcmp (gnome_rr_output_info_get_name (gnome_rr_config_get_outputs (config)[0]), "default") == 0);
}
#endif

static void
on_detect_displays (GtkWidget *widget, gpointer data)
{
  CcDisplayPanel *self = data;
  GError *error;

  error = NULL;
  if (!gnome_rr_screen_refresh (self->priv->screen, &error)) {
    if (error) {
      error_message (self, _("Could not detect displays"), error->message);
      g_error_free (error);
    }
  }
}

static GnomeRROutputInfo *
get_nearest_output (GnomeRRConfig *configuration, int x, int y)
{
  int i;
  int nearest_index;
  int nearest_dist;
  GnomeRROutputInfo **outputs;

  nearest_index = -1;
  nearest_dist = G_MAXINT;

  outputs = gnome_rr_config_get_outputs (configuration);
  for (i = 0; outputs[i] != NULL; i++)
    {
      int dist_x, dist_y;
      int output_x, output_y, output_width, output_height;

      if (!(gnome_rr_output_info_is_connected (outputs[i]) && gnome_rr_output_info_is_active (outputs[i])))
	continue;

      gnome_rr_output_info_get_geometry (outputs[i], &output_x, &output_y, &output_width, &output_height);

      if (x < output_x)
	dist_x = output_x - x;
      else if (x >= output_x + output_width)
	dist_x = x - (output_x + output_width) + 1;
      else
	dist_x = 0;

      if (y < output_y)
	dist_y = output_y - y;
      else if (y >= output_y + output_height)
	dist_y = y - (output_y + output_height) + 1;
      else
	dist_y = 0;

      if (MIN (dist_x, dist_y) < nearest_dist)
	{
	  nearest_dist = MIN (dist_x, dist_y);
	  nearest_index = i;
	}
    }

  if (nearest_index != -1)
    return outputs[nearest_index];
  else
    return NULL;
}

/* Gets the output that contains the largest intersection with the window.
 * Logic stolen from gdk_screen_get_monitor_at_window().
 */
static GnomeRROutputInfo *
get_output_for_window (GnomeRRConfig *configuration, GdkWindow *window)
{
  GdkRectangle win_rect;
  int i;
  int largest_area;
  int largest_index;
  GnomeRROutputInfo **outputs;

  gdk_window_get_geometry (window, &win_rect.x, &win_rect.y, &win_rect.width, &win_rect.height);
  gdk_window_get_origin (window, &win_rect.x, &win_rect.y);

  largest_area = 0;
  largest_index = -1;

  outputs = gnome_rr_config_get_outputs (configuration);
  for (i = 0; outputs[i] != NULL; i++)
    {
      GdkRectangle output_rect, intersection;

      gnome_rr_output_info_get_geometry (outputs[i], &output_rect.x, &output_rect.y, &output_rect.width, &output_rect.height);

      if (gnome_rr_output_info_is_connected (outputs[i]) && gdk_rectangle_intersect (&win_rect, &output_rect, &intersection))
	{
	  int area;

	  area = intersection.width * intersection.height;
	  if (area > largest_area)
	    {
	      largest_area = area;
	      largest_index = i;
	    }
	}
    }

  if (largest_index != -1)
    return outputs[largest_index];
  else
    return get_nearest_output (configuration,
			       win_rect.x + win_rect.width / 2,
			       win_rect.y + win_rect.height / 2);
}

static void
dialog_toplevel_focus_changed (GtkWindow      *window,
			       GParamSpec     *pspec,
			       CcDisplayPanel *self)
{
  if (self->priv->labeler == NULL)
    return;
  if (gtk_window_has_toplevel_focus (window))
    gnome_rr_labeler_show (self->priv->labeler);
  else
    gnome_rr_labeler_hide (self->priv->labeler);
}

static void
on_toplevel_realized (GtkWidget     *widget,
                      CcDisplayPanel *self)
{
  self->priv->current_output = get_output_for_window (self->priv->current_configuration,
                                               gtk_widget_get_window (widget));
  rebuild_gui (self);
}

/* We select the current output, i.e. select the one being edited, based on
 * which output is showing the configuration dialog.
 */
static void
select_current_output_from_dialog_position (CcDisplayPanel *self)
{
  GtkWidget *toplevel;

  toplevel = gtk_widget_get_toplevel (self->priv->panel);

  if (gtk_widget_get_realized (toplevel)) {
    self->priv->current_output = get_output_for_window (self->priv->current_configuration,
                                                 gtk_widget_get_window (toplevel));
    rebuild_gui (self);
  } else {
    g_signal_connect (toplevel, "realize", G_CALLBACK (on_toplevel_realized), self);
    self->priv->current_output = NULL;
  }
}

/* This is a GtkWidget::map-event handler.  We wait for the display-properties
 * dialog to be mapped, and then we select the output which corresponds to the
 * monitor on which the dialog is being shown.
 */
static gboolean
dialog_map_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
  CcDisplayPanel *self = data;

  select_current_output_from_dialog_position (self);
  return FALSE;
}

static void
cc_display_panel_init (CcDisplayPanel *self)
{
}

static GObject *
cc_display_panel_constructor (GType                  gtype,
                              guint                  n_properties,
                              GObjectConstructParam *properties)
{
  GtkBuilder *builder;
  GtkWidget *align;
  GError *error;
  GObject *obj;
  CcDisplayPanel *self;
  CcShell *shell;
  GtkWidget *toplevel;
  gchar *objects[] = {"display-panel", NULL};

  obj = G_OBJECT_CLASS (cc_display_panel_parent_class)->constructor (gtype, n_properties, properties);
  self = CC_DISPLAY_PANEL (obj);
  self->priv = DISPLAY_PANEL_PRIVATE (self);

  error = NULL;
  self->priv->builder = builder = gtk_builder_new ();

  if (!gtk_builder_add_objects_from_file (builder, UIDIR "/display-capplet.ui", objects, &error))
    {
      g_warning ("Could not parse UI definition: %s", error->message);
      g_error_free (error);
      g_object_unref (builder);
      return obj;
    }

  self->priv->screen = gnome_rr_screen_new (gdk_screen_get_default (), &error);
  g_signal_connect (self->priv->screen, "changed", G_CALLBACK (on_screen_changed), self);
  if (!self->priv->screen)
    {
      error_message (NULL, _("Could not get screen information"), error->message);
      g_error_free (error);
      g_object_unref (builder);
      return obj;
    }

  self->priv->clock_settings = g_settings_new (CLOCK_SCHEMA);

  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = cc_shell_get_toplevel (shell);
  self->priv->focus_id = g_signal_connect (toplevel, "notify::has-toplevel-focus",
                                           G_CALLBACK (dialog_toplevel_focus_changed), self);

  self->priv->panel = WID ("display-panel");
  g_signal_connect_after (self->priv->panel, "show",
                          G_CALLBACK (dialog_map_event_cb), self);

  self->priv->current_monitor_event_box = WID ("current_monitor_event_box");
  self->priv->current_monitor_label = WID ("current_monitor_label");

  self->priv->monitor_switch = WID ("monitor_switch");
  g_signal_connect (self->priv->monitor_switch, "notify::active",
                    G_CALLBACK (monitor_switch_active_cb), self);

  self->priv->resolution_combo = WID ("resolution_combo");
  g_signal_connect (self->priv->resolution_combo, "changed",
                    G_CALLBACK (on_resolution_changed), self);

  self->priv->rotation_combo = WID ("rotation_combo");
  g_signal_connect (self->priv->rotation_combo, "changed",
                    G_CALLBACK (on_rotation_changed), self);

  self->priv->clone_checkbox = WID ("clone_checkbox");
  g_signal_connect (self->priv->clone_checkbox, "toggled",
                    G_CALLBACK (on_clone_changed), self);

  self->priv->clone_label    = WID ("clone_resolution_warning_label");

  g_signal_connect (WID ("detect_displays_button"),
                    "clicked", G_CALLBACK (on_detect_displays), self);

  make_text_combo (self->priv->resolution_combo, 4);
  make_text_combo (self->priv->rotation_combo, -1);

  /* Scroll Area */
  self->priv->area = (GtkWidget *)foo_scroll_area_new ();

  g_object_set_data (G_OBJECT (self->priv->area), "panel", self);

  set_monitors_tooltip (self, FALSE);

  /* FIXME: this should be computed dynamically */
  foo_scroll_area_set_min_size (FOO_SCROLL_AREA (self->priv->area), 0, 200);
  gtk_widget_show (self->priv->area);
  g_signal_connect (self->priv->area, "paint",
                    G_CALLBACK (on_area_paint), self);
  g_signal_connect (self->priv->area, "viewport_changed",
                    G_CALLBACK (on_viewport_changed), self);

  align = WID ("align");

  gtk_container_add (GTK_CONTAINER (align), self->priv->area);

  on_screen_changed (self->priv->screen, self);

  g_signal_connect_swapped (WID ("apply_button"),
                            "clicked", G_CALLBACK (apply), self);

  gtk_widget_show (self->priv->panel);
  gtk_container_add (GTK_CONTAINER (self), self->priv->panel);

  return obj;
}

void
cc_display_panel_register (GIOModule *module)
{
  cc_display_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_DISPLAY_PANEL,
                                  "display", 0);
}

