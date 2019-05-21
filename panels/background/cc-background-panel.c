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

#include "cc-background-chooser-dialog.h"
#include "cc-background-item.h"
#include "cc-background-resources.h"
#include "cc-background-xml.h"

#include "bg-pictures-source.h"

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

  GDBusConnection *connection;

  GSettings *settings;
  GSettings *lock_settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcBackgroundItem *current_background;
  CcBackgroundItem *current_lock_background;

  GCancellable *copy_cancellable;

  GtkWidget *bottom_hbox;
  GtkWidget *desktop_drawing_area;
  GtkWidget *desktop_slide_image;
  GtkWidget *lock_drawing_area;
  GtkWidget *lock_slide_image;

  GtkWidget *spinner;
  GtkWidget *chooser;
};

CC_PANEL_REGISTER (CcBackgroundPanel, cc_background_panel)

static GdkPixbuf*
get_or_create_cached_pixbuf (CcBackgroundPanel *panel,
                             GtkWidget         *widget,
                             CcBackgroundItem  *background)
{
  GtkAllocation allocation;
  const gint preview_width = 309;
  const gint preview_height = 168;
  gint scale_factor;
  GdkPixbuf *pixbuf;

  pixbuf = g_object_get_data (G_OBJECT (background), "pixbuf");
  if (pixbuf == NULL)
    {
      gtk_widget_get_allocation (widget, &allocation);
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

static void
update_display_preview (CcBackgroundPanel *panel,
                        GtkWidget         *widget,
                        CcBackgroundItem  *background)
{
  GdkPixbuf *pixbuf;
  cairo_t *cr;

  pixbuf = get_or_create_cached_pixbuf (panel, widget, background);

  cr = gdk_cairo_create (gtk_widget_get_window (widget));
  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static CcBackgroundItem *
get_current_background (CcBackgroundPanel *panel,
                        GSettings         *settings)
{
  if (settings == panel->settings)
    return panel->current_background;
  else
    return panel->current_lock_background;
}

static void
update_preview (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item)
{
  gboolean changes_with_time;
  CcBackgroundItem *current_background;

  current_background = get_current_background (panel, settings);

  if (item && current_background)
    {
      g_object_unref (current_background);
      current_background = cc_background_item_copy (item);
      if (settings == panel->settings)
        panel->current_background = current_background;
      else
        panel->current_lock_background = current_background;
      cc_background_item_load (current_background, NULL);
    }

  changes_with_time = FALSE;

  if (current_background)
    {
      changes_with_time = cc_background_item_changes_with_time (current_background);
    }

  if (settings == panel->settings)
    {
      gtk_widget_set_visible (panel->desktop_slide_image, changes_with_time);
      gtk_widget_queue_draw (panel->desktop_drawing_area);
    }
  else
    {
      gtk_widget_set_visible (panel->lock_slide_image, changes_with_time);
      gtk_widget_queue_draw (panel->lock_drawing_area);
    }
}

static gchar *
get_save_path (CcBackgroundPanel *panel, GSettings *settings)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           settings == panel->settings ? "last-edited.xml" : "last-edited-lock.xml",
                           NULL);
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
  uri = get_save_path (panel, settings);
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

  if (settings == panel->settings)
    {
      g_clear_object (&panel->current_background);
      panel->current_background = configured;
    }
  else
    {
      g_clear_object (&panel->current_lock_background);
      panel->current_lock_background = configured;
    }
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
  g_autoptr(CcBackgroundPanel) panel = (CcBackgroundPanel *) pointer;
  g_autoptr(GError) err = NULL;
  g_autofree gchar *filename = NULL;
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
  current_background = get_current_background (panel, settings);

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

  update_preview (panel, settings, item);
  current_background = get_current_background (panel, settings);

  /* Save the source XML if there is one */
  filename = get_save_path (panel, settings);
  if (create_save_dir ())
    cc_background_xml_save (current_background, filename);
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
      gtk_box_pack_start (GTK_BOX (panel->bottom_hbox), panel->spinner, FALSE, FALSE, 6);
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
      filename = get_save_path (panel, settings);
      if (create_save_dir ())
        cc_background_xml_save (get_current_background (panel, settings), filename);
    }
}

static void
on_chooser_dialog_response (GtkDialog         *dialog,
                            int                response_id,
                            CcBackgroundPanel *panel)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      g_autoptr(CcBackgroundItem) item = NULL;

      item = cc_background_chooser_dialog_get_item (CC_BACKGROUND_CHOOSER_DIALOG (dialog));
      if (item != NULL)
          set_background (panel, g_object_get_data (G_OBJECT (dialog), "settings"), item);
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
launch_chooser (CcBackgroundPanel *panel,
                GSettings         *settings)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (panel));
  dialog = cc_background_chooser_dialog_new (GTK_WINDOW (toplevel));
  g_object_set_data (G_OBJECT (dialog), "settings", settings);
  gtk_widget_show (dialog);
  g_signal_connect (dialog, "response", G_CALLBACK (on_chooser_dialog_response), panel);
  panel->chooser = dialog;
  g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &panel->chooser);
}

static void
on_background_button_clicked_cb (GtkButton         *button,
                                 CcBackgroundPanel *panel)
{
  launch_chooser (panel, panel->settings);
}

static void
on_lock_button_clicked_cb (GtkButton         *button,
                           CcBackgroundPanel *panel)
{
  launch_chooser (panel, panel->lock_settings);
}

static gboolean
on_preview_draw_cb (GtkWidget         *widget,
                    cairo_t           *cr,
                    CcBackgroundPanel *panel)
{
  update_display_preview (panel, widget, panel->current_background);
  return TRUE;
}

static gboolean
on_lock_preview_draw_cb (GtkWidget         *widget,
                         cairo_t           *cr,
                         CcBackgroundPanel *panel)
{
  update_display_preview (panel, widget, panel->current_lock_background);
  return TRUE;
}

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static void
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  /* cancel any copy operation */
  g_cancellable_cancel (panel->copy_cancellable);

  g_clear_object (&panel->settings);
  g_clear_object (&panel->lock_settings);
  g_clear_object (&panel->copy_cancellable);
  g_clear_object (&panel->thumb_factory);

  g_clear_pointer (&panel->chooser, gtk_widget_destroy);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->current_background);
  g_clear_object (&panel->current_lock_background);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, bottom_hbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_drawing_area);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, desktop_slide_image);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_drawing_area);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, lock_slide_image);

  gtk_widget_class_bind_template_callback (widget_class, on_background_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_lock_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_lock_preview_draw_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_preview_draw_cb);
}

static void
on_settings_changed (GSettings         *settings,
                     gchar             *key,
                     CcBackgroundPanel *panel)
{
  reload_current_bg (panel, settings);
  update_preview (panel, settings, NULL);
}

static void
cc_background_panel_init (CcBackgroundPanel *panel)
{
  g_resources_register (cc_background_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  panel->connection = g_application_get_dbus_connection (g_application_get_default ());

  panel->copy_cancellable = g_cancellable_new ();
  panel->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

  panel->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (panel->settings);

  panel->lock_settings = g_settings_new (WP_LOCK_PATH_ID);
  g_settings_delay (panel->lock_settings);

  /* Load the backgrounds */
  reload_current_bg (panel, panel->settings);
  update_preview (panel, panel->settings, NULL);
  reload_current_bg (panel, panel->lock_settings);
  update_preview (panel, panel->lock_settings, NULL);

  /* Background settings */
  g_signal_connect (panel->settings, "changed", G_CALLBACK (on_settings_changed), panel);
  g_signal_connect (panel->lock_settings, "changed", G_CALLBACK (on_settings_changed), panel);
}
