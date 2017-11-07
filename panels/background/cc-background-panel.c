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
#include <gio/gdesktopappinfo.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "cc-background-item.h"
#include "cc-background-store.h"
#include "cc-background-grid-item.h"
#include "cc-background-resources.h"
#include "cc-background-xml.h"

#include "bg-pictures-source.h"

#define WP_PATH_ID "org.gnome.desktop.background"
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
on_panel_resize (GtkWidget *widget,
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
                   GSettings *settings)
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
copy_finished_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      pointer)
{
  g_autoptr(GError) err = NULL;
  g_autoptr(CcBackgroundPanel) panel = (CcBackgroundPanel *) pointer;
  CcBackgroundItem *item;
  CcBackgroundItem *current_background;
  GSettings *settings;

  if (!g_file_copy_finish (G_FILE (source_object), result, &err))
    {
      if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        return;
      }
      g_warning ("Failed to copy image to cache location: %s", err->message);
    }
  item = g_object_get_data (source_object, "item");
  settings = g_object_get_data (source_object, "settings");
  current_background = panel->current_background;

  g_settings_apply (settings);

  /* the panel may have been destroyed before the callback is run, so be sure
   * to check the widgets are not NULL */

  if (panel->spinner)
    {
      gtk_widget_destroy (GTK_WIDGET (panel->spinner));
      panel->spinner = NULL;
    }

  if (current_background)
    cc_background_item_load (current_background, NULL);

  if (panel->builder)
    {
      g_autofree gchar *filename = NULL;

      update_preview (panel, settings, item);
      current_background = panel->current_background;

      /* Save the source XML if there is one */
      filename = get_save_path ("last-edited.xml");
      if (create_save_dir ())
        cc_background_xml_save (current_background, filename);
    }
}

static void
set_background (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  GDesktopBackgroundStyle style;
  gboolean save_settings = TRUE;
  const char *uri;
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
      g_autoptr(GFile) source = NULL;
      g_autoptr(GFile) dest = NULL;
      g_autofree gchar *cache_path = NULL;
      g_autofree gchar *basename = NULL;
      g_autofree gchar *display_name = NULL;
      g_autofree gchar *dest_path = NULL;
      g_autofree gchar *dest_uri = NULL;
      g_autoptr(GdkPixbuf) pixbuf = NULL;

      cache_path = bg_pictures_source_get_cache_path ();
      if (g_mkdir_with_parents (cache_path, USER_DIR_MODE) < 0)
        {
          g_warning ("Failed to create directory '%s'", cache_path);
          return;
        }

      dest_path = bg_pictures_source_get_unique_path (cc_background_item_get_source_url (item));
      dest = g_file_new_for_path (dest_path);
      source = g_file_new_for_uri (cc_background_item_get_source_url (item));
      basename = g_file_get_basename (source);
      display_name = g_filename_display_name (basename);
      dest_path = g_file_get_path (dest);

      /* create a blank image to use until the source image is ready */
      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
      gdk_pixbuf_fill (pixbuf, 0x00000000);
      gdk_pixbuf_save (pixbuf, dest_path, "png", NULL, NULL);

      if (panel->copy_cancellable)
        {
          g_cancellable_cancel (panel->copy_cancellable);
          g_cancellable_reset (panel->copy_cancellable);
        }

      if (panel->spinner)
        {
          gtk_widget_destroy (GTK_WIDGET (panel->spinner));
          panel->spinner = NULL;
        }

      /* create a spinner while the file downloads */
      panel->spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (panel->spinner));
      gtk_box_pack_start (GTK_BOX (WID ("bottom-hbox")), panel->spinner, FALSE,
                          FALSE, 6);
      gtk_widget_show (panel->spinner);

      /* reference the panel in case it is removed before the copy is
       * finished */
      g_object_ref (panel);
      g_object_set_data_full (G_OBJECT (source), "item", g_object_ref (item), g_object_unref);
      g_object_set_data (G_OBJECT (source), "settings", settings);
      g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, panel->copy_cancellable,
                         NULL, NULL,
                         copy_finished_cb, panel);
      dest_uri = g_file_get_uri (dest);

      g_settings_set_string (settings, WP_URI_KEY, dest_uri);
      g_object_set (G_OBJECT (item),
                    "uri", dest_uri,
                    "needs-download", FALSE,
                    "name", display_name,
                    NULL);

      /* delay the updated drawing of the preview until the copy finishes */
      save_settings = FALSE;
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
  GtkWidget *selected = GTK_WIDGET (child);
  CcBackgroundPanel *panel = user_data;
  CcBackgroundItem *item;
  item = cc_background_grid_item_get_ref (selected);

  set_background (panel, panel->settings, item);
}

static void
on_open_gnome_photos (GtkWidget *widget,
                      gpointer  user_data)
{
  GAppLaunchContext *context;
  GDesktopAppInfo *appInfo;
  GError **error = NULL;

  context = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (gdk_display_get_default ()));
  appInfo = g_desktop_app_info_new("org.gnome.Photos.desktop");

  if (appInfo == NULL) {
    g_debug ("Gnome Photos is not installed.");
  }
  else {
    g_app_info_launch (G_APP_INFO (appInfo), NULL, context, error);
    g_prefix_error (error, "Problem opening Gnome Photos: ");

    g_object_unref (appInfo);
  }
  g_object_unref (context);
}

static void
on_open_picture_folder (GtkWidget *widget,
                        gpointer  user_data)
{
  GDBusProxy      *proxy;
  GVariant        *retval;
  GVariantBuilder *builder;
  const gchar     *uri;
  GError **error = NULL;
  const gchar     *path;

  path = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);

  uri = g_filename_to_uri (path, NULL, error);

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "org.freedesktop.FileManager1",
                                         "/org/freedesktop/FileManager1",
                                         "org.freedesktop.FileManager1",
                                         NULL, error);

  if (!proxy) {
    g_prefix_error (error,
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
                                     -1, NULL, error);

    g_variant_builder_unref (builder);
    g_object_unref (proxy);

    if (!retval)
      {
        g_prefix_error (error, ("Calling ShowFolders failed: "));
      }
    else
      g_variant_unref (retval);
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

  pixbuf = cc_background_item_get_frame_thumbnail (self,
                                                   panel->thumb_factory,
                                                   preview_width,
                                                   preview_height,
                                                   scale_factor,
                                                   -2, TRUE);
  flow = cc_background_grid_item_new(self, pixbuf);
  return flow;
}

static void
cc_background_panel_init (CcBackgroundPanel *panel)
{
  gchar *objects[] = {"background-panel", NULL };
  g_autoptr(GError) err = NULL;
  GtkStyleProvider *provider;
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
  g_object_unref (provider);

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
  gtk_flow_box_bind_model (GTK_FLOW_BOX (WID("background-gallery")),
                           G_LIST_MODEL (cc_background_store_get_liststore (panel->store)),
                           create_gallery_item,
                           panel,
                           NULL);

  /* Background settings */
  g_signal_connect (panel->settings, "changed", G_CALLBACK (on_settings_changed), panel);
}
