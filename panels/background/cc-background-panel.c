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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "cc-background-chooser.h"
#include "cc-background-item.h"
#include "cc-background-preview.h"
#include "cc-background-resources.h"
#include "cc-time-row.h"
#include "shell/cc-object-storage.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_LOCK_PATH_ID "org.gnome.desktop.screensaver"
#define WP_URI_KEY "picture-uri"
#define WP_URI_DARK_KEY "picture-uri-dark"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

#define INTERFACE_PATH_ID "org.gnome.desktop.interface"
#define LOCATION_PATH_ID "org.gnome.system.location"
#define COLOR_PATH_ID "org.gnome.settings-daemon.plugins.color"
#define INTERFACE_COLOR_SCHEME_KEY "color-scheme"
#define INTERFACE_ACCENT_COLOR_KEY "accent-color"

struct _CcBackgroundPanel
{
  CcPanel parent_instance;

  GDBusConnection *connection;

  GSettings *settings;
  GSettings *lock_settings;
  GSettings *interface_settings;
  GSettings *location_settings;
  GSettings *color_settings;

  GDBusProxy *proxy;
  GDBusProxy *color_proxy;

  CcBackgroundItem *current_background;

  GtkWidget *schedule_row;
  GtkWidget *beginning_row;
  GtkWidget *end_row;
  GtkWidget *accent_box;
  CcBackgroundChooser *background_chooser;
  CcBackgroundPreview *default_preview;
  CcBackgroundPreview *dark_preview;
  GtkToggleButton *default_toggle;
  GtkToggleButton *dark_toggle;

  GCancellable *proxy_cancellable;
};

CC_PANEL_REGISTER (CcBackgroundPanel, cc_background_panel)

static void on_settings_changed (CcBackgroundPanel *self);

static void
load_custom_css (CcBackgroundPanel *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/background/preview.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
transition_screen (CcBackgroundPanel *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->proxy)
    return;

  g_dbus_proxy_call_sync (self->proxy,
                          "ScreenTransition",
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          &error);

  if (error)
    g_warning ("Couldn't transition screen: %s", error->message);
}

static void
on_accent_color_toggled_cb (CcBackgroundPanel *self,
                            GtkToggleButton   *toggle)
{
  GDesktopAccentColor accent_color_from_key;
  GDesktopAccentColor accent_color = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (toggle), "accent-color"));

  if (!gtk_toggle_button_get_active (toggle))
    return;

  accent_color_from_key = g_settings_get_enum (self->interface_settings,
                                               INTERFACE_ACCENT_COLOR_KEY);

  /* Don't unnecessarily set the key again */
  if (accent_color == accent_color_from_key)
    return;

  transition_screen (self);

  g_settings_set_enum (self->interface_settings,
                       INTERFACE_ACCENT_COLOR_KEY,
                       accent_color);
}

/* Adapted from adw-inspector-page.c */
static const char *
get_color_tooltip (GDesktopAccentColor color)
{
  switch (color)
    {
    case G_DESKTOP_ACCENT_COLOR_BLUE:
      return _("Blue");
    case G_DESKTOP_ACCENT_COLOR_TEAL:
      return _("Teal");
    case G_DESKTOP_ACCENT_COLOR_GREEN:
      return _("Green");
    case G_DESKTOP_ACCENT_COLOR_YELLOW:
      return _("Yellow");
    case G_DESKTOP_ACCENT_COLOR_ORANGE:
      return _("Orange");
    case G_DESKTOP_ACCENT_COLOR_RED:
      return _("Red");
    case G_DESKTOP_ACCENT_COLOR_PINK:
      return _("Pink");
    case G_DESKTOP_ACCENT_COLOR_PURPLE:
      return _("Purple");
    case G_DESKTOP_ACCENT_COLOR_SLATE:
      return _("Slate");
    default:
      g_assert_not_reached ();
    }
}

static const char *
get_untranslated_color (GDesktopAccentColor color)
{
  switch (color)
    {
    case G_DESKTOP_ACCENT_COLOR_BLUE:
      return "blue";
    case G_DESKTOP_ACCENT_COLOR_TEAL:
      return "teal";
    case G_DESKTOP_ACCENT_COLOR_GREEN:
      return "green";
    case G_DESKTOP_ACCENT_COLOR_YELLOW:
      return "yellow";
    case G_DESKTOP_ACCENT_COLOR_ORANGE:
      return "orange";
    case G_DESKTOP_ACCENT_COLOR_RED:
      return "red";
    case G_DESKTOP_ACCENT_COLOR_PINK:
      return "pink";
    case G_DESKTOP_ACCENT_COLOR_PURPLE:
      return "purple";
    case G_DESKTOP_ACCENT_COLOR_SLATE:
      return "slate";
    default:
      g_assert_not_reached ();
    }
}

static void
setup_accent_color_toggles (CcBackgroundPanel *self)
{
  GDesktopAccentColor accent_color = g_settings_get_enum (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY);
  GDesktopAccentColor i;

  for (i = G_DESKTOP_ACCENT_COLOR_BLUE; i <= G_DESKTOP_ACCENT_COLOR_SLATE; i++)
    {
      GtkWidget *button = GTK_WIDGET (gtk_toggle_button_new ());
      GtkToggleButton *grouping_button = GTK_TOGGLE_BUTTON (gtk_widget_get_first_child (self->accent_box));

      gtk_widget_set_tooltip_text (button, get_color_tooltip (i));
      gtk_widget_add_css_class (button, "accent-button");
      gtk_widget_add_css_class (button, get_untranslated_color (i));
      g_object_set_data (G_OBJECT (button), "accent-color", GINT_TO_POINTER (i));
      g_signal_connect_object (button, "toggled",
                               G_CALLBACK (on_accent_color_toggled_cb),
                               self,
                               G_CONNECT_SWAPPED);

      if (grouping_button != NULL)
        gtk_toggle_button_set_group (GTK_TOGGLE_BUTTON (button), grouping_button);

      if (i == accent_color)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

      gtk_box_append (GTK_BOX (self->accent_box), button);
    }
}

static void
reload_accent_color_toggles (CcBackgroundPanel *self)
{
  GDesktopAccentColor accent_color = g_settings_get_enum (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY);
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (self->accent_box);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      GDesktopAccentColor child_color = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "accent-color"));

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child), child_color == accent_color);
    }
}

static void
reload_color_scheme (CcBackgroundPanel *self)
{
  GDesktopColorScheme scheme;

  scheme = g_settings_get_enum (self->interface_settings, INTERFACE_COLOR_SCHEME_KEY);

  if (scheme == G_DESKTOP_COLOR_SCHEME_DEFAULT)
    {
      gtk_toggle_button_set_active (self->default_toggle, TRUE);
    }
  else if (scheme == G_DESKTOP_COLOR_SCHEME_PREFER_DARK)
    {
      gtk_toggle_button_set_active (self->dark_toggle, TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (self->default_toggle, FALSE);
      gtk_toggle_button_set_active (self->dark_toggle, FALSE);
    }
}

static void
set_color_scheme (CcBackgroundPanel   *self,
                  GDesktopColorScheme  color_scheme)
{
  GDesktopColorScheme scheme;

  scheme = g_settings_get_enum (self->interface_settings,
                                INTERFACE_COLOR_SCHEME_KEY);

  /* We have to check the equality manually to avoid starting an unnecessary
   * screen transition */
  if (color_scheme == scheme)
    return;

  transition_screen (self);

  g_settings_set_enum (self->interface_settings,
                       INTERFACE_COLOR_SCHEME_KEY,
                       color_scheme);
}

/* Color schemes */

static void
on_color_scheme_toggle_active_cb (CcBackgroundPanel *self)
{
  if (gtk_toggle_button_get_active (self->default_toggle))
    set_color_scheme (self, G_DESKTOP_COLOR_SCHEME_DEFAULT);
  else if (gtk_toggle_button_get_active (self->dark_toggle))
    set_color_scheme (self, G_DESKTOP_COLOR_SCHEME_PREFER_DARK);
}

static void
got_transition_proxy_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      data)
{
  g_autoptr(GError) error = NULL;
  CcBackgroundPanel *self = data;

  self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (self->proxy == NULL)
    {
      g_warning ("Error creating proxy: %s", error->message);
      return;
    }
}

static void
schedule_update_state (CcBackgroundPanel *self)
{
  gboolean enabled, automatic;

  enabled = g_settings_get_boolean (self->color_settings, "color-scheme-enabled");
  automatic = g_settings_get_boolean (self->color_settings, "color-scheme-schedule-automatic");

  adw_combo_row_set_selected (ADW_COMBO_ROW (self->schedule_row), enabled ? automatic ? 1 : 2 : 0);

  if (automatic && self->color_proxy != NULL)
    {
      gdouble sunset_value, sunrise_value;

      g_autoptr (GVariant) sunset = NULL;
      g_autoptr (GVariant) sunrise = NULL;
      sunset = g_dbus_proxy_get_cached_property (self->color_proxy, "Sunset");
      sunrise = g_dbus_proxy_get_cached_property (self->color_proxy, "Sunrise");

      if (sunset != NULL)
        sunset_value = g_variant_get_double (sunset);
      else
        sunset_value = 16.0f;

      if (sunrise != NULL)
        sunrise_value = g_variant_get_double (sunrise);
      else
        sunrise_value = 8.0f;

      cc_time_row_set_time (CC_TIME_ROW (self->beginning_row), sunset_value);
      cc_time_row_set_time (CC_TIME_ROW (self->end_row), sunrise_value);
    }

  g_settings_bind (self->color_settings, "color-scheme-schedule-automatic",
                   self->beginning_row, "sensitive", G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_settings_bind (self->color_settings, "color-scheme-schedule-automatic",
                   self->end_row, "sensitive", G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static void
schedule_factory_setup_cb (CcBackgroundPanel *self,
                           GtkListItem       *list_item,
                           gpointer           user_data)
{
  GtkWidget *box, *label_box, *title, *subtitle, *checkmark;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  title = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (title), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (title), 20);
  gtk_widget_set_valign (title, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (label_box), title);

  subtitle = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (subtitle), 0.0);
  gtk_label_set_ellipsize (GTK_LABEL (subtitle), PANGO_ELLIPSIZE_END);
  gtk_widget_set_valign (subtitle, GTK_ALIGN_CENTER);
  gtk_widget_set_visible (subtitle, FALSE);
  gtk_widget_add_css_class (subtitle, "caption");
  gtk_box_append (GTK_BOX (label_box), subtitle);

  gtk_box_append (GTK_BOX (box), label_box);

  checkmark = g_object_new (GTK_TYPE_IMAGE,
                            "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                            "icon-name", "object-select-symbolic",
                            NULL);
  gtk_box_append (GTK_BOX (box), checkmark);

  g_object_set_data (G_OBJECT (list_item), "box", box);
  g_object_set_data (G_OBJECT (list_item), "title", title);
  g_object_set_data (G_OBJECT (list_item), "subtitle", subtitle);
  g_object_set_data (G_OBJECT (list_item), "checkmark", checkmark);

  gtk_list_item_set_child (list_item, box);
}

static void
schedule_factory_notify_selected_item_cb (AdwComboRow *self,
                                          GParamSpec  *pspec,
                                          GtkListItem *list_item)
{
  GtkWidget *checkmark = g_object_get_data (G_OBJECT (list_item), "checkmark");
  gboolean selected;

  selected = (adw_combo_row_get_selected_item (self) == gtk_list_item_get_item (list_item));
  gtk_widget_set_opacity (checkmark, selected ? 1.0 : 0.0);
}

static void
schedule_factory_location_changed_cb (GSettings   *self,
                                      GParamSpec  *pspec,
                                      GtkListItem *list_item)
{
  gboolean location_enabled;
  GtkWidget *box, *subtitle;

  box = g_object_get_data (G_OBJECT (list_item), "box");
  subtitle = g_object_get_data (G_OBJECT (list_item), "subtitle");

  location_enabled = g_settings_get_boolean (self, "enabled");

  gtk_widget_set_visible (subtitle, !location_enabled);

  gtk_list_item_set_selectable (list_item, location_enabled);
  gtk_list_item_set_activatable (list_item, location_enabled);
  gtk_widget_set_sensitive (box, location_enabled);
}

static void
schedule_factory_bind_cb (CcBackgroundPanel        *self,
                          GtkListItem              *list_item,
                          GtkSignalListItemFactory *factory)
{
  AdwComboRow *row = ADW_COMBO_ROW (self->schedule_row);
  GtkWidget *box, *title, *subtitle, *checkmark, *popup;
  GtkStringObject *string_item;

  string_item = GTK_STRING_OBJECT (gtk_list_item_get_item (list_item));

  box = g_object_get_data (G_OBJECT (list_item), "box");
  title = g_object_get_data (G_OBJECT (list_item), "title");
  subtitle = g_object_get_data (G_OBJECT (list_item), "subtitle");
  checkmark = g_object_get_data (G_OBJECT (list_item), "checkmark");

  gtk_label_set_label (GTK_LABEL (title), gtk_string_object_get_string (string_item));

  if (gtk_list_item_get_position (list_item) == 1)
    {
      gtk_label_set_label (GTK_LABEL (subtitle),
                           _("Unavailable: location services disabled"));

      g_signal_connect (self->location_settings, "changed::enabled",
                        G_CALLBACK (schedule_factory_location_changed_cb), list_item);
      schedule_factory_location_changed_cb (self->location_settings, NULL, list_item);
    }

  popup = gtk_widget_get_ancestor (title, GTK_TYPE_POPOVER);
  if (popup && gtk_widget_is_ancestor (popup, GTK_WIDGET (row)))
    {
      gtk_box_set_spacing (GTK_BOX (box), 0);
      gtk_widget_set_visible (checkmark, TRUE);
      g_signal_connect (row, "notify::selected",
                        G_CALLBACK (schedule_factory_notify_selected_item_cb), list_item);
      schedule_factory_notify_selected_item_cb (row, NULL, list_item);
    }
  else
    {
      gtk_box_set_spacing (GTK_BOX (box), 6);
      gtk_widget_set_visible (checkmark, FALSE);
    }
}

static void
schedule_factory_unbind_cb (CcBackgroundPanel        *self,
                            GtkListItem              *list_item,
                            GtkSignalListItemFactory *factory)
{
  g_signal_handlers_disconnect_by_func (ADW_COMBO_ROW (self->schedule_row), schedule_factory_notify_selected_item_cb, list_item);
  g_signal_handlers_disconnect_by_func (self->location_settings, schedule_factory_location_changed_cb, list_item);
}

static void
schedule_location_changed (CcBackgroundPanel *self)
{
  gboolean automatic, location_enabled;

  automatic = g_settings_get_boolean (self->color_settings, "color-scheme-schedule-automatic");
  location_enabled = g_settings_get_boolean (self->location_settings, "enabled");

  if (automatic && !location_enabled)
    g_settings_set_boolean (self->color_settings, "color-scheme-schedule-automatic", FALSE);
}

static void
on_schedule_row_selected_cb (CcBackgroundPanel *self)
{
  guint selected;
  gboolean enabled, automatic;

  selected = adw_combo_row_get_selected (ADW_COMBO_ROW (self->schedule_row));
  enabled = selected != 0;
  automatic = selected == 1;

  g_settings_set_boolean (self->color_settings, "color-scheme-enabled", enabled);
  g_settings_set_boolean (self->color_settings, "color-scheme-schedule-automatic", automatic);

  schedule_update_state (self);
}

static void
on_beginning_time_updated_cb (CcBackgroundPanel *self)
{
  gdouble value = cc_time_row_get_time (CC_TIME_ROW (self->beginning_row));

  g_settings_set_double (self->color_settings, "color-scheme-schedule-from", value);
}

static void
on_end_time_updated_cb (CcBackgroundPanel *self)
{
  gdouble value = cc_time_row_get_time (CC_TIME_ROW (self->end_row));

  g_settings_set_double (self->color_settings, "color-scheme-schedule-to", value);
}

static void
color_changed_cb (CcBackgroundPanel *self)
{
  schedule_update_state (self);
}

static void
color_got_proxy_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  CcBackgroundPanel *self = CC_BACKGROUND_PANEL (user_data);
  GDBusProxy *proxy;
  g_autoptr (GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to connect to g-s-d: %s", error->message);
      return;
    }

  self->color_proxy = proxy;

  g_signal_connect_object (G_OBJECT (self->color_proxy), "g-properties-changed",
                           G_CALLBACK (color_changed_cb),
                           self, G_CONNECT_SWAPPED);

  schedule_update_state (self);
}

/* Background */

static void
update_preview (CcBackgroundPanel *self)
{
  CcBackgroundItem *current_background;

  current_background = self->current_background;
  cc_background_preview_set_item (self->default_preview, current_background);
  cc_background_preview_set_item (self->dark_preview, current_background);
}

static void
reload_current_bg (CcBackgroundPanel *self)
{
  CcBackgroundItem *configured;
  GSettings *settings = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *dark_uri = NULL;
  g_autofree gchar *pcolor = NULL;
  g_autofree gchar *scolor = NULL;

  /* initalise the current background information from settings */
  settings = self->settings;
  uri = g_settings_get_string (settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    g_clear_pointer (&uri, g_free);


  configured = cc_background_item_new (uri);

  dark_uri = g_settings_get_string (settings, WP_URI_DARK_KEY);
  pcolor = g_settings_get_string (settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
                "name", _("Current background"),
                "uri-dark", dark_uri,
                "placement", g_settings_get_enum (settings, WP_OPTIONS_KEY),
                "shading", g_settings_get_enum (settings, WP_SHADING_KEY),
                "primary-color", pcolor,
                "secondary-color", scolor,
                NULL);

  g_clear_object (&self->current_background);
  self->current_background = configured;
  cc_background_item_load (configured, NULL);

  cc_background_chooser_set_active_item (self->background_chooser, configured);
}

static void
reset_settings_if_defaults (CcBackgroundPanel *self,
                            GSettings         *settings,
                            gboolean           check_dark)
{
  gsize i;
  const char *keys[] = {
    WP_URI_KEY,       /* this key needs to be first */
    WP_URI_DARK_KEY,
    WP_OPTIONS_KEY,
    WP_SHADING_KEY,
    WP_PCOLOR_KEY,
    WP_SCOLOR_KEY,
    NULL
  };

  for (i = 0; keys[i] != NULL; i++)
    {
      g_autoptr (GVariant) default_value = NULL;
      g_autoptr (GVariant) user_value = NULL;
      gboolean setting_is_default;

      if (!check_dark && g_str_equal (keys[i], WP_URI_DARK_KEY))
        continue;

      default_value = g_settings_get_default_value (settings, keys[i]);
      user_value = g_settings_get_value (settings, keys[i]);

      setting_is_default = g_variant_equal (default_value, user_value);

      /* As a courtesy to distros that are a little lackadaisical about making sure
       * schema defaults match the settings in the background item with the default
       * picture, we only look at the URI to determine if we shouldn't clean out dconf.
       *
       * In otherwords, we still clean out the picture-uri key from dconf when a user
       * selects the default background in control-center, even if after selecting it
       * e.g., primary-color still mismatches with schema defaults.
       */
      if (g_str_equal (keys[i], WP_URI_KEY) && !setting_is_default)
        return;

      if (setting_is_default)
        g_settings_reset (settings, keys[i]);
    }

  g_settings_apply (settings);
}

static void
set_background (CcBackgroundPanel *self,
                GSettings         *settings,
                CcBackgroundItem  *item,
                gboolean           set_dark)
{
  GDesktopBackgroundStyle style;
  CcBackgroundItemFlags flags;
  g_autofree gchar *filename = NULL;
  const char *uri;

  if (item == NULL)
    return;

  uri = cc_background_item_get_uri (item);
  flags = cc_background_item_get_flags (item);

  g_settings_set_string (settings, WP_URI_KEY, uri);

  if (set_dark)
    {
      const char *uri_dark;

      uri_dark = cc_background_item_get_uri_dark (item);

      if (uri_dark && uri_dark[0])
        g_settings_set_string (settings, WP_URI_DARK_KEY, uri_dark);
      else
        g_settings_set_string (settings, WP_URI_DARK_KEY, uri);
    }

  /* Also set the placement if we have a URI and the previous value was none */
  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }
  else if (uri != NULL)
    {
      style = g_settings_get_enum (settings, WP_OPTIONS_KEY);
      if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
        g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_SHADING)
    g_settings_set_enum (settings, WP_SHADING_KEY, cc_background_item_get_shading (item));

  g_settings_set_string (settings, WP_PCOLOR_KEY, cc_background_item_get_pcolor (item));
  g_settings_set_string (settings, WP_SCOLOR_KEY, cc_background_item_get_scolor (item));

  /* Apply all changes */
  g_settings_apply (settings);

  /* Clean out dconf if the user went back to distro defaults */
  reset_settings_if_defaults (self, settings, set_dark);
}

static void
on_chooser_background_chosen_cb (CcBackgroundPanel *self,
                                 CcBackgroundItem  *item)
{
  g_signal_handlers_block_by_func (self->settings, on_settings_changed, self);

  set_background (self, self->settings, item, TRUE);
  set_background (self, self->lock_settings, item, FALSE);

  on_settings_changed (self);

  g_signal_handlers_unblock_by_func (self->settings, on_settings_changed, self);
}

static void
on_add_picture_button_clicked_cb (CcBackgroundPanel *self)
{
  cc_background_chooser_select_file (self->background_chooser);
}

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static void
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanel *self = CC_BACKGROUND_PANEL (object);

  g_clear_object (&self->settings);
  g_clear_object (&self->lock_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->location_settings);
  g_clear_object (&self->color_settings);
  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanel *self = CC_BACKGROUND_PANEL (object);

  g_clear_object (&self->current_background);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_BACKGROUND_CHOOSER);
  g_type_ensure (CC_TYPE_BACKGROUND_PREVIEW);
  g_type_ensure (CC_TYPE_TIME_ROW);

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, schedule_row);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, beginning_row);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, end_row);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, accent_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, background_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, default_preview);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, dark_preview);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, default_toggle);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, dark_toggle);

  gtk_widget_class_bind_template_callback (widget_class, schedule_factory_setup_cb);
  gtk_widget_class_bind_template_callback (widget_class, schedule_factory_bind_cb);
  gtk_widget_class_bind_template_callback (widget_class, schedule_factory_unbind_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_beginning_time_updated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_end_time_updated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_schedule_row_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_color_scheme_toggle_active_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_chooser_background_chosen_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_add_picture_button_clicked_cb);
}

static void
on_settings_changed (CcBackgroundPanel *self)
{
  reload_current_bg (self);
  update_preview (self);
}

static void
cc_background_panel_init (CcBackgroundPanel *self)
{
  g_resources_register (cc_background_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->connection = g_application_get_dbus_connection (g_application_get_default ());

  self->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (self->settings);
 
  self->lock_settings = g_settings_new (WP_LOCK_PATH_ID);
  g_settings_delay (self->lock_settings);

  self->interface_settings = g_settings_new (INTERFACE_PATH_ID);
  self->location_settings = g_settings_new (LOCATION_PATH_ID);
  self->color_settings = g_settings_new (COLOR_PATH_ID);

  /* Load the background */
  reload_current_bg (self);
  update_preview (self);

  /* Background settings */
  g_signal_connect_object (self->settings, "changed", G_CALLBACK (on_settings_changed), self, G_CONNECT_SWAPPED);

  /* Interface settings */
  reload_color_scheme (self);
  setup_accent_color_toggles (self);

  g_signal_connect_object (self->interface_settings,
                           "changed::" INTERFACE_COLOR_SCHEME_KEY,
                           G_CALLBACK (reload_color_scheme),
                           self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_object (self->interface_settings,
                           "changed::" INTERFACE_ACCENT_COLOR_KEY,
                           G_CALLBACK (reload_accent_color_toggles),
                           self,
                           G_CONNECT_SWAPPED);

  /* Colour Scheme Settings */
  g_signal_connect_object (self->location_settings, "changed::enabled",
                           G_CALLBACK (schedule_location_changed),
                           self,
                           G_CONNECT_SWAPPED);
  schedule_location_changed (self);

  g_signal_connect_object (self->color_settings, "changed",
                           G_CALLBACK (color_changed_cb),
                           self, G_CONNECT_SWAPPED);

  g_settings_bind (self->color_settings, "color-scheme-enabled",
                   self->beginning_row, "visible", G_SETTINGS_BIND_GET);

  g_settings_bind (self->color_settings, "color-scheme-enabled",
                   self->end_row, "visible", G_SETTINGS_BIND_GET);

  g_settings_bind (self->color_settings, "color-scheme-schedule-from",
                   self->beginning_row, "time", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->color_settings, "color-scheme-schedule-to",
                   self->end_row, "time", G_SETTINGS_BIND_DEFAULT);

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Color",
                                       "/org/gnome/SettingsDaemon/Color",
                                       "org.gnome.SettingsDaemon.Color",
                                       self->proxy_cancellable,
                                       color_got_proxy_cb,
                                       self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.Shell",
                            "/org/gnome/Shell",
                            "org.gnome.Shell",
                            NULL,
                            got_transition_proxy_cb,
                            self);

  load_custom_css (self);
}
