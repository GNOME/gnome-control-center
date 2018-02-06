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
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Julian Sparber <julian@sparber.net>
 *
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gdesktopappinfo.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "cc-background-item.h"
#include "cc-background-store.h"
#include "cc-background-grid-item.h"
#include "cc-background-resources.h"
#include "cc-background-xml.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_LOCK_PATH_ID "org.gnome.desktop.screensaver"
#define WP_URI_KEY "picture-uri"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

struct _CcBackgroundPanel
{
  CcPanel parent_instance;

  GtkBuilder *builder;
  GDBusConnection *connection;
  GSettings *settings;
  GSettings *lock_settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcBackgroundItem *current_background;
  CcBackgroundStore *store;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;
};

CC_PANEL_REGISTER (CcBackgroundPanel, cc_background_panel)

#define WID(y) (GtkWidget *) gtk_builder_get_object (panel->builder, y)

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static void
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->builder);

  /* destroying the builder object will also destroy the spinner */
  panel->spinner = NULL;

  g_clear_object (&panel->settings);
  g_clear_object (&panel->lock_settings);

  if (panel->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (panel->copy_cancellable);

      g_clear_object (&panel->copy_cancellable);
    }

  g_clear_object (&panel->thumb_factory);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->current_background);
  g_clear_object (&panel->store);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;
}

static void
update_preview (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  gboolean changes_with_time;
  CcBackgroundItem *current_background;

  current_background = panel->current_background;

  if (item && current_background)
    {
      g_object_unref (current_background);
      current_background = cc_background_item_copy (item);
      panel->current_background = current_background;
      cc_background_item_load (current_background, NULL);
    }

  changes_with_time = FALSE;

  if (current_background)
    {
      changes_with_time = cc_background_item_changes_with_time (current_background);
    }

  gtk_revealer_set_reveal_child (GTK_REVEALER (WID ("wallpaper-info")),
                                 changes_with_time);

  gtk_widget_queue_draw (WID ("background-desktop-drawingarea"));
}

static gchar *
get_save_path (const char *filename)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           filename,
                           NULL);
}

static GdkPixbuf*
get_or_create_cached_pixbuf (CcBackgroundPanel *panel,
                             GtkWidget         *widget,
                             CcBackgroundItem  *background)
{
  gint scale_factor;
  GdkPixbuf *pixbuf;
  const gint preview_width = gtk_widget_get_allocated_width (widget);
  const gint preview_height = gtk_widget_get_allocated_height (widget);

  pixbuf = g_object_get_data (G_OBJECT (background), "pixbuf");
  if (pixbuf == NULL ||
      gdk_pixbuf_get_width (pixbuf) != preview_width ||
      gdk_pixbuf_get_height (pixbuf) != preview_height) {
    scale_factor = gtk_widget_get_scale_factor (widget);
    pixbuf = cc_background_item_get_frame_thumbnail (background,
                                                     panel->thumb_factory,
                                                     preview_width,
                                                     preview_height,
                                                     scale_factor,
                                                     -2, TRUE);
    g_object_set_data_full (G_OBJECT (background), "pixbuf", pixbuf, g_object_unref);
  }

  return pixbuf;
}

static gboolean
on_preview_draw (GtkWidget         *widget,
                 cairo_t           *cr,
                 CcBackgroundPanel *panel)
{
  GdkPixbuf *pixbuf;

  pixbuf = get_or_create_cached_pixbuf (panel,
                                        widget,
                                        panel->current_background);
  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               0, 0);
  cairo_paint (cr);
  return TRUE;
}

static void
on_panel_resize (GtkWidget    *widget,
                 GdkRectangle *allocation,
                 gpointer      user_data)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (user_data);
  GtkWidget *preview = WID ("background-preview");

  if (allocation->height > 700) {
    gtk_widget_set_size_request (preview, -1, 200);
  }
  else {
    gtk_widget_set_size_request (preview, -1, 150);
  }
}


static void
reload_current_bg (CcBackgroundPanel *panel,
                   GSettings         *settings)
{
  g_autoptr(CcBackgroundItem) saved = NULL;
  CcBackgroundItem *configured;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *pcolor = NULL;
  g_autofree gchar *scolor = NULL;

  /* Load the saved configuration */
  uri = get_save_path ("last-edited.xml");
  saved = cc_background_xml_get_item (uri);

  /* initalise the current background information from settings */
  uri = g_settings_get_string (settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    {
      g_clear_pointer (&uri, g_free);
    }
  else
    {
      g_autoptr(GFile) file = NULL;
      file = g_file_new_for_commandline_arg (uri);
    }
  configured = cc_background_item_new (uri);

  pcolor = g_settings_get_string (settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
                "name", _("Current background"),
                "placement", g_settings_get_enum (settings, WP_OPTIONS_KEY),
                "shading", g_settings_get_enum (settings, WP_SHADING_KEY),
                "primary-color", pcolor,
                "secondary-color", scolor,
                NULL);

  if (saved != NULL && cc_background_item_compare (saved, configured))
    {
      CcBackgroundItemFlags flags;
      flags = cc_background_item_get_flags (saved);
      /* Special case for colours */
      if (cc_background_item_get_placement (saved) == G_DESKTOP_BACKGROUND_STYLE_NONE)
        flags &=~ (CC_BACKGROUND_ITEM_HAS_PCOLOR | CC_BACKGROUND_ITEM_HAS_SCOLOR);
      g_object_set (G_OBJECT (configured),
                    "name", cc_background_item_get_name (saved),
                    "flags", flags,
                    "source-url", cc_background_item_get_source_url (saved),
                    "source-xml", cc_background_item_get_source_xml (saved),
                    NULL);
    }

  g_clear_object (&panel->current_background);
  panel->current_background = configured;

  cc_background_item_load (configured, NULL);
}

static gboolean
create_save_dir (void)
{
  g_autofree char *path = NULL;

  path = g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           NULL);
  if (g_mkdir_with_parents (path, USER_DIR_MODE) < 0)
    {
      g_warning ("Failed to create directory '%s'", path);
      return FALSE;
    }
  return TRUE;
}

static void
set_background (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  GDesktopBackgroundStyle style;
  gboolean save_settings = TRUE;
  const gchar *uri = NULL;
  CcBackgroundItemFlags flags;

  if (item == NULL)
    return;

  uri = cc_background_item_get_uri (item);
  flags = cc_background_item_get_flags (item);

  if ((flags & CC_BACKGROUND_ITEM_HAS_URI) && uri == NULL)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, G_DESKTOP_BACKGROUND_STYLE_NONE);
      g_settings_set_string (settings, WP_URI_KEY, "");
    }
  else if (cc_background_item_get_source_url (item) != NULL &&
           cc_background_item_get_needs_download (item))
    {
      //do not handle
    }
  else
    {
      g_settings_set_string (settings, WP_URI_KEY, uri);
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

  /* update the preview information */
  if (save_settings != FALSE)
    {
      g_autofree gchar *filename = NULL;

      /* Apply all changes */
      g_settings_apply (settings);

      /* Save the source XML if there is one */
      filename = get_save_path ("last-edited.xml");
      if (create_save_dir ())
        cc_background_xml_save (panel->current_background, filename);
    }

}

static void
on_settings_changed (GSettings         *settings,
                     gchar             *key,
                     CcBackgroundPanel *self)
{
  reload_current_bg (self, settings);
  update_preview (self, settings, NULL);
}

static void
on_background_select (GtkFlowBox      *box,
                      GtkFlowBoxChild *child,
                      gpointer         user_data)
{
  GtkWidget *widget = gtk_bin_get_child (GTK_BIN (child));
  CcBackgroundPanel *panel = user_data;
  CcBackgroundItem *item;
  item = cc_background_grid_item_get_item (widget);

  set_background (panel, panel->settings, item);
  set_background (panel, panel->lock_settings, item);
}

static void
on_open_gnome_photos (GtkWidget *widget,
                      gpointer   user_data)
{
  g_autoptr(GAppLaunchContext) context = NULL;
  g_autoptr(GDesktopAppInfo) appInfo = NULL;
  g_autoptr(GError) error = NULL;

  context = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (gdk_display_get_default ()));
  appInfo = g_desktop_app_info_new("org.gnome.Photos.desktop");

  if (appInfo == NULL) {
    g_debug ("Gnome Photos is not installed.");
  }
  else {
    g_app_info_launch (G_APP_INFO (appInfo), NULL, context, &error);
    g_prefix_error (&error, "Problem opening Gnome Photos: ");
  }
}

static void
on_open_picture_folder (GtkWidget *widget,
                        gpointer   user_data)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GVariant) retval = NULL;
  g_autoptr(GVariantBuilder) builder = NULL;
  const gchar *uri;
  g_autoptr(GError) error = NULL;
  const gchar *path;

  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);

  uri = g_filename_to_uri (path, NULL, &error);

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "org.freedesktop.FileManager1",
                                         "/org/freedesktop/FileManager1",
                                         "org.freedesktop.FileManager1",
                                         NULL, &error);

  if (!proxy) {
    g_prefix_error (&error,
                    ("Connecting to org.freedesktop.FileManager1 failed: "));
  }
  else {

    builder = g_variant_builder_new (G_VARIANT_TYPE ("as"));
    g_variant_builder_add (builder, "s", uri);

    retval = g_dbus_proxy_call_sync (proxy,
                                     "ShowFolders",
                                     g_variant_new ("(ass)",
                                                    builder,
                                                    ""),
                                     G_DBUS_CALL_FLAGS_NONE,
                                     -1, NULL, &error);

    if (!retval)
      {
        g_prefix_error (&error, ("Calling ShowFolders failed: "));
      }
  }
}

static gboolean
is_gnome_photos_installed ()
{
  if (g_desktop_app_info_new("org.gnome.Photos.desktop") == NULL) {
    return FALSE;
  }
  return TRUE;
}

static GtkWidget *
create_gallery_item (gpointer item,
                     gpointer user_data)
{
  CcBackgroundPanel *panel = user_data;
  CcBackgroundItem *self = item;
  GtkWidget *flow;
  GdkPixbuf *pixbuf;
  gint scale_factor;
  const gint preview_width = 400;
  const gint preview_height = 400 * 9 / 16;

  scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (panel));

  /* Create a small cached pixbuf which is bigger then a gallery item */
  pixbuf = cc_background_item_get_frame_thumbnail (self,
                                                   panel->thumb_factory,
                                                   preview_width,
                                                   preview_height,
                                                   scale_factor,
                                                   -2, TRUE);
  flow = GTK_WIDGET (cc_background_grid_item_new(self, pixbuf));
  return flow;
}

static void
cc_background_panel_init (CcBackgroundPanel *panel)
{
  gchar *objects[] = {"background-panel", NULL };
  g_autoptr(GError) err = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkWidget *widget;

  /* Create wallpapers store */
  panel->store = cc_background_store_new ();

  panel->connection = g_application_get_dbus_connection (g_application_get_default ());
  g_resources_register (cc_background_get_resource ());

  panel->builder = gtk_builder_new ();
  gtk_builder_add_objects_from_resource (panel->builder,
                                         "/org/gnome/control-center/background/background.ui",
                                         objects, &err);

  if (err)
    {
      g_warning ("Could not load ui: %s", err->message);
      return;
    }

  panel->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (panel->settings);

  panel->lock_settings = g_settings_new (WP_LOCK_PATH_ID);
  g_settings_delay (panel->lock_settings);

  /* add the top level widget */
  widget = WID ("background-panel");

  gtk_container_add (GTK_CONTAINER (panel), widget);
  gtk_widget_show_all (GTK_WIDGET (panel));

  /* add style */
  widget = WID ("background-preview-top");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "org/gnome/control-center/background/background.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default(),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* setup preview area */
  widget = WID ("background-desktop-drawingarea");
  g_signal_connect (widget, "draw", G_CALLBACK (on_preview_draw), panel);

  /* Add handler for resizing the preview */
  g_signal_connect (panel, "size-allocate", G_CALLBACK (on_panel_resize), panel);

  panel->copy_cancellable = g_cancellable_new ();

  panel->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

  /* add button handler */
  widget = WID ("open-gnome-photos");
  g_signal_connect (G_OBJECT (widget), "clicked",
                    G_CALLBACK (on_open_gnome_photos), panel);

  if (!is_gnome_photos_installed ()) {
    gtk_widget_hide (widget);
    gtk_label_set_text (GTK_LABEL (WID ("description-label")), _("The background can also be set from Files"));
  }
  else {
    gtk_label_set_text (GTK_LABEL (WID ("description-label")), _("The background can also be set from Photos or Files"));
  }
  widget = WID ("open-picture-folder");
  g_signal_connect (G_OBJECT (widget), "clicked",
                    G_CALLBACK (on_open_picture_folder), panel);

  /* add the gallery widget */
  widget = WID ("background-gallery");

  g_signal_connect (G_OBJECT (widget), "child-activated",
                    G_CALLBACK (on_background_select), panel);

  /* Load the backgrounds */
  reload_current_bg (panel, panel->settings);
  update_preview (panel, panel->settings, NULL);

  /* Bind liststore to flowbox */
  cc_background_store_bind_flow_box (panel->store,
                                     panel,
                                     WID("background-gallery"),
                                     create_gallery_item);


  /* Background settings */
  g_signal_connect (panel->settings, "changed", G_CALLBACK (on_settings_changed), panel);
}
