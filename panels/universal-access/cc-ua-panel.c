/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-ua-panel.h"

#include <gconf/gconf-client.h>

#include "gconf-property-editor.h"


#define CONFIG_ROOT "/desktop/gnome/accessibility"

#define KEY_CONFIG_ROOT CONFIG_ROOT "/keyboard"
#define MOUSE_CONFIG_ROOT CONFIG_ROOT "/mouse"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)


G_DEFINE_DYNAMIC_TYPE (CcUaPanel, cc_ua_panel, CC_TYPE_PANEL)

#define UA_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_UA_PANEL, CcUaPanelPrivate))

struct _CcUaPanelPrivate
{
  GtkBuilder *builder;
  GConfClient *client;
  GSettings *interface_settings;

  GSList *notify_list;
};


static void
cc_ua_panel_get_property (GObject    *object,
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
cc_ua_panel_set_property (GObject      *object,
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
cc_ua_panel_dispose (GObject *object)
{
  CcUaPanelPrivate *priv = CC_UA_PANEL (object)->priv;
  GSList *l;

  /* remove the notify callbacks, since they rely on builder/client being
   * available */
  if (priv->notify_list)
    {
      for (l = priv->notify_list; l; l = g_slist_next (l))
        {
          gconf_client_notify_remove (priv->client,
                                      GPOINTER_TO_INT (l->data));
        }
      g_slist_free (priv->notify_list);
      priv->notify_list = NULL;
    }


  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  if (priv->client)
    {
      g_object_unref (priv->client);
      priv->client = NULL;
    }

  if (priv->interface_settings)
    {
      g_object_unref (priv->interface_settings);
      priv->interface_settings = NULL;
    }

  G_OBJECT_CLASS (cc_ua_panel_parent_class)->dispose (object);
}

static void
cc_ua_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_ua_panel_parent_class)->finalize (object);
}

static void
cc_ua_panel_class_init (CcUaPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcUaPanelPrivate));

  object_class->get_property = cc_ua_panel_get_property;
  object_class->set_property = cc_ua_panel_set_property;
  object_class->dispose = cc_ua_panel_dispose;
  object_class->finalize = cc_ua_panel_finalize;
}

static void
cc_ua_panel_class_finalize (CcUaPanelClass *klass)
{
}

static gchar *sticky_keys_section[] = {
    "typing_sticky_keys_disable_two_keys_checkbutton",
    "typing_sticky_keys_beep_modifier_checkbutton",
    NULL
};

static gchar *slow_keys_section[]= {
    "typing_slowkeys_delay_box",
    "typing_slow_keys_beeb_box",
    NULL
};

static gchar *bounce_keys_section[] = {
    "typing_bouncekeys_delay_box",
    "typing_bounce_keys_beep_rejected_checkbutton",
    NULL
};

static gchar *secondary_click_section[] = {
    "pointing_secondary_click_scale_box",
    NULL
};

static gchar *dwell_click_section[] = {
    "pointing_hover_click_delay_scale_box",
    "pointing_hover_click_threshold_scale_box",
    NULL
};

static gchar *visual_alerts_section[] = {
    "hearing_test_flash_button",
    "hearing_flash_window_title_button",
    "hearing_flash_screen_button",
    NULL
};

static void
cc_ua_panel_section_toggled (GtkToggleButton *button,
                             GtkBuilder      *builder)
{
  GtkWidget *w;
  gboolean enabled;
  gchar **widgets, **s;

  widgets = g_object_get_data (G_OBJECT (button), "section-widgets");

  enabled = gtk_toggle_button_get_active (button);

  for (s = widgets; *s; s++)
    {
      w = WID (builder, *s);
      gtk_widget_set_sensitive (w, enabled);
    }
}

static GConfValue*
cc_ua_panel_toggle_radios (GConfPropertyEditor *peditor,
                           const GConfValue    *value)
{
  GtkWidget *radio;
  gboolean enabled;

  enabled = gconf_value_get_bool (value);
  radio = (GtkWidget*) gconf_property_editor_get_ui_control (peditor);


  if (!enabled)
    {
      GSList *list, *l;

      list = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio));

      if (list)
        {
          /* activate the "off" button */
          for (l = list; l; l = l->next)
            {
              if (l->data == radio)
                continue;

              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (l->data),
                                            TRUE);
            }
        }
    }

  return gconf_value_copy (value);
}

static void
gconf_on_off_peditor_new (CcUaPanelPrivate  *priv,
                          const gchar       *key,
                          GtkWidget         *widget,
                          gchar            **section)
{
  GObject *peditor;

  /* set data to enable/disable the section this on/off switch controls */
  if (section)
    {
      g_object_set_data (G_OBJECT (widget), "section-widgets", section);
      g_signal_connect (widget, "toggled",
                        G_CALLBACK (cc_ua_panel_section_toggled),
                        priv->builder);
    }

  /* set up the boolean editor */
  peditor = gconf_peditor_new_boolean (NULL, key, widget, NULL);
  g_object_set (peditor, "conv-to-widget-cb", cc_ua_panel_toggle_radios, NULL);

  /* emit the notify on the key, so that the conv-to-widget-cb callback is run
   */
  gconf_client_notify (priv->client, key);
}

/* seeing section */
#define GTK_THEME_KEY "gtk-theme"
#define ICON_THEME_KEY "icon-theme"
#define CONTRAST_MODEL_THEME_COLUMN 3
#define DPI_MODEL_FACTOR_COLUMN 2

#define DPI_KEY "/desktop/gnome/font_rendering/dpi"

/* The following two functions taken from gsd-a11y-preferences-dialog.c
 *
 * Copyright (C)  2008 William Jon McCann <jmccann@redhat.com>
 *
 * Licensed under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
/* X servers sometimes lie about the screen's physical dimensions, so we cannot
 * compute an accurate DPI value.  When this happens, the user gets fonts that
 * are too huge or too tiny.  So, we see what the server returns:  if it reports
 * something outside of the range [DPI_LOW_REASONABLE_VALUE,
 * DPI_HIGH_REASONABLE_VALUE], then we assume that it is lying and we use
 * DPI_FALLBACK instead.
 *
 * See get_dpi_from_gconf_or_server() below, and also
 * https://bugzilla.novell.com/show_bug.cgi?id=217790
 */
#define DPI_LOW_REASONABLE_VALUE 50
#define DPI_HIGH_REASONABLE_VALUE 500
#define DPI_DEFAULT        96

static gdouble
dpi_from_pixels_and_mm (gint pixels,
                        gint mm)
{
  gdouble dpi;

  if (mm >= 1)
    return pixels / (mm / 25.4);
  else
    return dpi = 0;
}

static gdouble
get_dpi_from_x_server ()
{
  GdkScreen *screen;
  gdouble dpi;

  screen = gdk_screen_get_default ();

  if (screen)
    {
      gdouble width_dpi, height_dpi;

      width_dpi = dpi_from_pixels_and_mm (gdk_screen_get_width (screen),
                                          gdk_screen_get_width_mm (screen));
      height_dpi = dpi_from_pixels_and_mm (gdk_screen_get_height (screen),
                                           gdk_screen_get_height_mm (screen));

      if (width_dpi < DPI_LOW_REASONABLE_VALUE
          || width_dpi > DPI_HIGH_REASONABLE_VALUE
          || height_dpi < DPI_LOW_REASONABLE_VALUE
          || height_dpi > DPI_HIGH_REASONABLE_VALUE)
        {
          dpi = DPI_DEFAULT;
        }
      else
        {
          dpi = (width_dpi + height_dpi) / 2.0;
        }
    }
  else
    dpi = DPI_DEFAULT;

  return dpi;
}

static void
dpi_notify_cb (GConfClient *client,
               guint        cnxn_id,
               GConfEntry  *entry,
               CcUaPanel   *panel)
{
  CcUaPanelPrivate *priv = panel->priv;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *combo;
  gboolean valid;
  gdouble gconf_value;
  gdouble x_dpi;

  if (!entry->value)
    return;

  gconf_value = gconf_value_get_float (entry->value);

  combo = WID (priv->builder, "seeing_text_size_combobox");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  /* get current value from screen */
  x_dpi = get_dpi_from_x_server ();

  /* see if the calculated value matches in the combobox model */
  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      gfloat factor;

      gtk_tree_model_get (model, &iter,
                          DPI_MODEL_FACTOR_COLUMN, &factor,
                          -1);

      if (gconf_value == (float) (factor * x_dpi))
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
          break;
        }

      valid = gtk_tree_model_iter_next (model, &iter);
    }

  /* if a matching value was not found in the combobox, set to "normal" */
  if (!valid)
    {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    }
}

static void
dpi_combo_box_changed (GtkComboBox *box,
                       CcUaPanel *panel)
{
  CcUaPanelPrivate *priv = panel->priv;
  GtkTreeIter iter;
  gfloat factor;

  gtk_combo_box_get_active_iter (box, &iter);

  gtk_tree_model_get (gtk_combo_box_get_model (box), &iter,
                      DPI_MODEL_FACTOR_COLUMN, &factor,
                      -1);

  if (factor == 1.0)
    gconf_client_unset (priv->client, DPI_KEY, NULL);
  else
    {
      gdouble x_dpi, u_dpi;

      x_dpi = get_dpi_from_x_server ();
      u_dpi = (gdouble) factor * x_dpi;

      gconf_client_set_float (priv->client, DPI_KEY, u_dpi, NULL);
    }
}


static void
interface_settings_changed_cb (GSettings   *settings,
                               const gchar *key,
                               CcUaPanel   *panel)
{
  CcUaPanelPrivate *priv = panel->priv;

  if (g_str_equal (key, "gtk-theme")) {
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkWidget *combo;
    gboolean valid;
    gchar *theme_value;

    theme_value = g_settings_get_string (settings, GTK_THEME_KEY);

    combo = WID (priv->builder, "seeing_contrast_combobox");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

    /* see if there is a matching theme name in the combobox model */
    valid = gtk_tree_model_get_iter_first (model, &iter);
    while (valid)
      {
        gchar *value;

        gtk_tree_model_get (model, &iter,
                            CONTRAST_MODEL_THEME_COLUMN, &value,
                            -1);

        if (!g_strcmp0 (value, theme_value))
          {
            gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
            g_free (value);
            break;
          }

        g_free (value);
        valid = gtk_tree_model_iter_next (model, &iter);
      }

    /* if a value for the current theme was not found in the combobox, set to the
     * "normal" option */
    if (!valid)
      {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
      }
  }
}

static void
contrast_combobox_changed_cb (GtkComboBox *box,
                              CcUaPanel   *panel)
{
  CcUaPanelPrivate *priv = panel->priv;
  gchar *theme_name = NULL;
  GtkTreeIter iter;

  gtk_combo_box_get_active_iter (box, &iter);

  gtk_tree_model_get (gtk_combo_box_get_model (box), &iter,
                      CONTRAST_MODEL_THEME_COLUMN, &theme_name,
                      -1);

  if (g_strcmp0 (theme_name, ""))
    {
      g_settings_set_string (priv->interface_settings, GTK_THEME_KEY, theme_name);
      g_settings_set_string (priv->interface_settings, ICON_THEME_KEY, theme_name);
    }
  else
    {
      g_settings_reset (priv->interface_settings, GTK_THEME_KEY);
      g_settings_reset (priv->interface_settings, ICON_THEME_KEY);
    }

  g_free (theme_name);
}

static void
cc_ua_panel_init_seeing (CcUaPanel *self)
{
  CcUaPanelPrivate *priv = self->priv;
  guint id;

  gconf_client_add_dir (priv->client, "/desktop/gnome/font_rendering",
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  id = gconf_client_notify_add (priv->client, DPI_KEY,
                                (GConfClientNotifyFunc) dpi_notify_cb,
                                self, NULL, NULL);
  priv->notify_list = g_slist_prepend (priv->notify_list, GINT_TO_POINTER (id));

  g_signal_connect (WID (priv->builder, "seeing_contrast_combobox"), "changed",
                    G_CALLBACK (contrast_combobox_changed_cb), self);

  g_signal_connect (WID (priv->builder, "seeing_text_size_combobox"), "changed",
                    G_CALLBACK (dpi_combo_box_changed), self);
  gconf_client_notify (priv->client, DPI_KEY);

  gconf_peditor_new_boolean (NULL,
                             "/desktop/gnome/accessibility/keyboard/togglekeys_enable",
                             WID (priv->builder,
                                  "seeing_enable_toggle_keys_checkbutton"),
                             NULL);
}


/* hearing/sound section */
static void
visual_bell_type_notify_cb (GConfClient *client,
                            guint        cnxn_id,
                            GConfEntry  *entry,
                            CcUaPanel   *panel)
{
  GtkWidget *widget;
  const gchar *value = gconf_value_get_string (entry->value);

  if (!strcmp ("frame_flash", value))
    widget = WID (panel->priv->builder, "hearing_flash_window_title_button");
  else
    widget = WID (panel->priv->builder, "hearing_flash_screen_button");

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
visual_bell_type_toggle_cb (GtkWidget *button,
                            CcUaPanel *panel)
{
  const gchar *key = "/apps/metacity/general/visual_bell_type";
  gboolean window_title;

  window_title = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  if (window_title)
    gconf_client_set_string (panel->priv->client, key, "frame_flash", NULL);
  else
    gconf_client_set_string (panel->priv->client, key, "fullscreen", NULL);
}

static void
cc_ua_panel_init_hearing (CcUaPanel *self)
{
  CcUaPanelPrivate *priv = self->priv;
  GtkWidget *w;
  GConfEntry *entry;
  guint id;

  gconf_client_add_dir (priv->client, "/apps/metacity/general",
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  w = WID (priv->builder, "hearing_visual_alerts_on_radiobutton");
  gconf_on_off_peditor_new (priv, "/apps/metacity/general/visual_bell",
                            w, visual_alerts_section);

  /* visual bell type */
  id = gconf_client_notify_add (priv->client,
                                "/apps/metacity/general/visual_bell_type",
                                (GConfClientNotifyFunc)
                                visual_bell_type_notify_cb,
                                self, NULL, NULL);
  priv->notify_list = g_slist_prepend (priv->notify_list, GINT_TO_POINTER (id));

  /* set the initial value */
  entry = gconf_client_get_entry (priv->client,
                                  "/apps/metacity/general/visual_bell_type",
                                  NULL, TRUE, NULL);
  visual_bell_type_notify_cb (priv->client, 0, entry, self);

  g_signal_connect (WID (priv->builder, "hearing_flash_window_title_button"),
                    "toggled", G_CALLBACK (visual_bell_type_toggle_cb), self);

  /* test flash */
  g_signal_connect (WID (priv->builder, "hearing_test_flash_button"),
                    "clicked", G_CALLBACK (gdk_beep), NULL);

}

/* typing/keyboard section */
static void
typing_keyboard_preferences_clicked (GtkButton *button,
                                     CcUaPanel *panel)
{
  CcShell *shell;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  cc_shell_set_active_panel_from_id (shell, "keyboard", NULL);
}

static void
cc_ua_panel_init_keyboard (CcUaPanel *self)
{
  CcUaPanelPrivate *priv = self->priv;
  GConfChangeSet *changeset = NULL;
  GtkWidget *w;


  /* enable shortcuts */
  w = WID (priv->builder, "typing_keyboard_toggle_checkbox");
  gconf_peditor_new_boolean (changeset, KEY_CONFIG_ROOT "/enable", w, NULL);

  /* sticky keys */
  w = WID (priv->builder, "typing_sticky_keys_on_radiobutton");
  gconf_on_off_peditor_new (priv, KEY_CONFIG_ROOT "/stickykeys_enable",
                            w, sticky_keys_section);

  w = WID (priv->builder, "typing_sticky_keys_disable_two_keys_checkbutton");
  gconf_peditor_new_boolean (changeset,
                             KEY_CONFIG_ROOT "/stickykeys_two_key_off", w,
                             NULL);

  w = WID (priv->builder, "typing_sticky_keys_beep_modifier_checkbutton");
  gconf_peditor_new_boolean (changeset,
                             KEY_CONFIG_ROOT "/stickykeys_modifier_beep", w,
                             NULL);

  /* slow keys */
  w = WID (priv->builder, "typing_slow_keys_on_radiobutton");
  gconf_on_off_peditor_new (priv, KEY_CONFIG_ROOT "/slowkeys_enable",
                            w, slow_keys_section);

  w = WID (priv->builder, "typing_slowkeys_delay_scale");
  gconf_peditor_new_numeric_range (changeset, KEY_CONFIG_ROOT "/slowkeys_delay",
                                   w, NULL);

  w = WID (priv->builder, "typing_slow_keys_beep_pressed_checkbutton");
  gconf_peditor_new_boolean (changeset, KEY_CONFIG_ROOT "/slowkeys_beep_press",
                             w, NULL);

  w = WID (priv->builder, "typing_slow_keys_beep_accepted_checkbutton");
  gconf_peditor_new_boolean (changeset, KEY_CONFIG_ROOT "/slowkeys_beep_accept",
                             w, NULL);

  w = WID (priv->builder, "typing_slow_keys_beep_rejected_checkbutton");
  gconf_peditor_new_boolean (changeset, KEY_CONFIG_ROOT "/slowkeys_beep_reject",
                             w, NULL);

  /* bounce keys */
  w = WID (priv->builder, "typing_bounce_keys_on_radiobutton");
  gconf_on_off_peditor_new (priv, KEY_CONFIG_ROOT "/bouncekeys_enable", w,
                            bounce_keys_section);

  w = WID (priv->builder, "typing_bouncekeys_delay_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   KEY_CONFIG_ROOT "/bouncekeys_delay", w,
                                   NULL);

  w = WID (priv->builder, "typing_bounce_keys_beep_rejected_checkbutton");
  gconf_peditor_new_boolean (changeset,
                             KEY_CONFIG_ROOT "/bouncekeys_beep_reject", w,
                             NULL);

  g_signal_connect (WID (priv->builder, "typing_keyboard_preferences_button"),
                    "clicked",
                    G_CALLBACK (typing_keyboard_preferences_clicked), self);
}

/* mouse/pointing & clicking section */
static void
pointing_mouse_preferences_clicked_cb (GtkButton *button,
                                       CcUaPanel *panel)
{
  CcShell *shell;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  cc_shell_set_active_panel_from_id (shell, "mouse", NULL);
}

static void
cc_ua_panel_init_mouse (CcUaPanel *self)
{
  CcUaPanelPrivate *priv = self->priv;
  GConfChangeSet *changeset = NULL;
  GtkWidget *w;

  /* mouse keys */
  w = WID (priv->builder, "pointing_mouse_keys_on_radiobutton");
  gconf_on_off_peditor_new (priv, KEY_CONFIG_ROOT "/mousekeys_enable", w, NULL);

  /* simulated secondary click */
  w = WID (priv->builder, "pointing_second_click_on_radiobutton");
  gconf_on_off_peditor_new (priv, MOUSE_CONFIG_ROOT "/delay_enable", w,
                            secondary_click_section);

  w = WID (priv->builder, "pointing_secondary_click_delay_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   MOUSE_CONFIG_ROOT "/delay_time", w,
                                   NULL);


  /* dwell click */
  w = WID (priv->builder, "pointing_hover_click_on_radiobutton");
  gconf_on_off_peditor_new (priv,MOUSE_CONFIG_ROOT "/dwell_enable", w,
                            dwell_click_section);

  w = WID (priv->builder, "pointing_dwell_delay_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   MOUSE_CONFIG_ROOT "/dwell_time", w,
                                   NULL);

  w = WID (priv->builder, "pointing_dwell_threshold_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   MOUSE_CONFIG_ROOT "/threshold", w,
                                   NULL);

  /* mouse preferences button */
  g_signal_connect (WID (priv->builder, "pointing_mouse_preferences_button"),
                    "clicked",
                    G_CALLBACK (pointing_mouse_preferences_clicked_cb), self);
}

static void
cc_ua_panel_init (CcUaPanel *self)
{
  CcUaPanelPrivate *priv;
  GtkWidget *widget;
  GError *err = NULL;
  gchar *objects[] = { "universal_access_box", "contrast_model",
                       "text_size_model", "slowkeys_delay_adjustment",
                       "bouncekeys_delay_adjustment", "click_delay_adjustment",
                       "dwell_time_adjustment", "dwell_threshold_adjustment",
                       "seeing_sizegroup", "typing_sizegroup",
                       "pointing_sizegroup", "pointing_sizegroup2",
                       "pointing_scale_sizegroup", "sizegroup1",
                       "hearing_sizegroup",
                       "NULL" };

  priv = self->priv = UA_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  gtk_builder_add_objects_from_file (priv->builder,
                                     GNOMECC_DATA_DIR "/ui/uap.ui",
                                     objects,
                                     &err);

  if (err)
    {
      g_warning ("Could not load interface file: %s", err->message);
      g_error_free (err);

      g_object_unref (priv->builder);
      priv->builder = NULL;

      return;
    }

  priv->client = gconf_client_get_default ();

  gconf_client_add_dir (priv->client, CONFIG_ROOT,
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  priv->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_signal_connect (priv->interface_settings, "changed",
                    G_CALLBACK (interface_settings_changed_cb), self);

  cc_ua_panel_init_keyboard (self);
  cc_ua_panel_init_mouse (self);
  cc_ua_panel_init_hearing (self);
  cc_ua_panel_init_seeing (self);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "universal_access_box");

  gtk_container_add (GTK_CONTAINER (self), widget);
}

void
cc_ua_panel_register (GIOModule *module)
{
  cc_ua_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_UA_PANEL,
                                  "universal-access", 0);
}

