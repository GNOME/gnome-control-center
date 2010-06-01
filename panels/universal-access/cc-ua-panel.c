/*
 * Copyright (C) 2010 Intel, Inc
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
  GConfChangeSet *changeset = NULL;
  CcUaPanelPrivate *priv = self->priv;
  GtkWidget *w;
  GConfEntry *entry;

  gconf_client_add_dir (priv->client, "/apps/metacity/general",
                        GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  w = WID (priv->builder, "hearing_visual_alerts_on_radiobutton");
  g_object_set_data (G_OBJECT (w), "section-widgets", visual_alerts_section);
  g_signal_connect (w, "toggled", G_CALLBACK (cc_ua_panel_section_toggled),
                    priv->builder);
  gconf_peditor_new_boolean (changeset, "/apps/metacity/general/visual_bell", w,
                             NULL);

  /* visual bell type */
  gconf_client_notify_add (priv->client,
                           "/apps/metacity/general/visual_bell_type",
                           (GConfClientNotifyFunc) visual_bell_type_notify_cb,
                           self, NULL, NULL);
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

static void
cc_ua_panel_init_keyboard (CcUaPanel *self)
{
  CcUaPanelPrivate *priv = self->priv;
  GConfChangeSet *changeset = NULL;
  GtkWidget *w;
  GObject *peditor;


  /* enable shortcuts */
  w = WID (priv->builder, "typing_keyboard_toggle_checkbox");
  gconf_peditor_new_boolean (changeset, KEY_CONFIG_ROOT "/enable", w, NULL);

  /* sticky keys */
  w = WID (priv->builder, "typing_sticky_keys_on_radiobutton");
  g_object_set_data (G_OBJECT (w), "section-widgets", sticky_keys_section);
  g_signal_connect (w, "toggled", G_CALLBACK (cc_ua_panel_section_toggled),
                    priv->builder);

  peditor = gconf_peditor_new_boolean (changeset,
                                       KEY_CONFIG_ROOT "/stickykeys_enable", w,
                                       NULL);
  g_object_set (peditor, "conv-to-widget-cb", cc_ua_panel_toggle_radios, NULL);

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
  g_object_set_data (G_OBJECT (w), "section-widgets", slow_keys_section);
  g_signal_connect (w, "toggled", G_CALLBACK (cc_ua_panel_section_toggled),
                    priv->builder);

  peditor = gconf_peditor_new_boolean (changeset,
                                       KEY_CONFIG_ROOT "/slowkeys_enable", w,
                                       NULL);
  g_object_set (peditor, "conv-to-widget-cb", cc_ua_panel_toggle_radios, NULL);

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
  g_object_set_data (G_OBJECT (w), "section-widgets", bounce_keys_section);
  g_signal_connect (w, "toggled", G_CALLBACK (cc_ua_panel_section_toggled),
                    priv->builder);

  peditor = gconf_peditor_new_boolean (changeset,
                                       KEY_CONFIG_ROOT "/bouncekeys_enable", w,
                                       NULL);
  g_object_set (peditor, "conv-to-widget-cb", cc_ua_panel_toggle_radios, NULL);

  w = WID (priv->builder, "typing_bouncekeys_delay_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   KEY_CONFIG_ROOT "/bouncekeys_delay", w,
                                   NULL);

  w = WID (priv->builder, "typing_bounce_keys_beep_rejected_checkbutton");
  gconf_peditor_new_boolean (changeset,
                             KEY_CONFIG_ROOT "/bouncekeys_beep_reject", w,
                             NULL);
}

static void
cc_ua_panel_init_mouse (CcUaPanel *self)
{
  CcUaPanelPrivate *priv = self->priv;
  GConfChangeSet *changeset = NULL;
  GObject *peditor;
  GtkWidget *w;

  /* mouse keys */
  w = WID (priv->builder, "pointing_mouse_keys_on_radiobutton");
  gconf_peditor_new_boolean (changeset,
                             KEY_CONFIG_ROOT "/mousekeys_enable", w,
                             NULL);

  /* simulated secondary click */
  w = WID (priv->builder, "pointing_second_click_on_radiobutton");
  g_object_set_data (G_OBJECT (w), "section-widgets", secondary_click_section);
  g_signal_connect (w, "toggled", G_CALLBACK (cc_ua_panel_section_toggled),
                    priv->builder);
  peditor = gconf_peditor_new_boolean (changeset,
                                       MOUSE_CONFIG_ROOT "/delay_enable", w,
                                       NULL);
  g_object_set (peditor, "conv-to-widget-cb", cc_ua_panel_toggle_radios, NULL);

  w = WID (priv->builder, "pointing_secondary_click_delay_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   MOUSE_CONFIG_ROOT "/delay_time", w,
                                   NULL);


  /* dwell click */
  w = WID (priv->builder, "pointing_hover_click_on_radiobutton");
  g_object_set_data (G_OBJECT (w), "section-widgets", dwell_click_section);
  g_signal_connect (w, "toggled", G_CALLBACK (cc_ua_panel_section_toggled),
                    priv->builder);
  peditor = gconf_peditor_new_boolean (changeset,
                                       MOUSE_CONFIG_ROOT "/dwell_enable", w,
                                       NULL);
  g_object_set (peditor, "conv-to-widget-cb", cc_ua_panel_toggle_radios, NULL);

  w = WID (priv->builder, "pointing_dwell_delay_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   MOUSE_CONFIG_ROOT "/dwell_time", w,
                                   NULL);

  w = WID (priv->builder, "pointing_dwell_threshold_scale");
  gconf_peditor_new_numeric_range (changeset,
                                   MOUSE_CONFIG_ROOT "/threshold", w,
                                   NULL);
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

  cc_ua_panel_init_keyboard (self);
  cc_ua_panel_init_mouse (self);
  cc_ua_panel_init_hearing (self);

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
                                  "gnome-universal-access.desktop", 0);
}

