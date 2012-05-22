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

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "cc-background-item.h"
#include "cc-background-xml.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_URI_KEY "picture-uri"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

enum {
  COL_SOURCE_NAME,
  COL_SOURCE_TYPE,
  COL_SOURCE,
  NUM_COLS
};

G_DEFINE_DYNAMIC_TYPE (CcBackgroundPanel, cc_background_panel, CC_TYPE_PANEL)

#define BACKGROUND_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BACKGROUND_PANEL, CcBackgroundPanelPrivate))

struct _CcBackgroundPanelPrivate
{
  GtkBuilder *builder;
  GDBusConnection *connection;

  GSettings *settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcBackgroundItem *current_background;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;

  GdkPixbuf *display_overlay;
  GdkPixbuf *display_screenshot;
  char *screenshot_path;
};

enum
{
  SOURCE_WALLPAPERS,
  SOURCE_PICTURES,
  SOURCE_COLORS,
#ifdef HAVE_LIBSOCIALWEB
  SOURCE_FLICKR
#endif
};


#define WID(y) (GtkWidget *) gtk_builder_get_object (priv->builder, y)

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static void
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanelPrivate *priv = CC_BACKGROUND_PANEL (object)->priv;

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;

      /* destroying the builder object will also destroy the spinner */
      priv->spinner = NULL;
    }

  if (priv->settings)
    {
      g_object_unref (priv->settings);
      priv->settings = NULL;
    }

  if (priv->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (priv->copy_cancellable);

      g_object_unref (priv->copy_cancellable);
      priv->copy_cancellable = NULL;
    }

  if (priv->thumb_factory)
    {
      g_object_unref (priv->thumb_factory);
      priv->thumb_factory = NULL;
    }

  if (priv->display_overlay)
    {
      g_object_unref (priv->display_overlay);
      priv->display_overlay = NULL;
    }

  if (priv->display_screenshot)
    {
      g_object_unref (priv->display_screenshot);
      priv->display_screenshot = NULL;
    }

  g_free (priv->screenshot_path);
  g_clear_object (&priv->connection);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanelPrivate *priv = CC_BACKGROUND_PANEL (object)->priv;

  if (priv->current_background)
    {
      g_object_unref (priv->current_background);
      priv->current_background = NULL;
    }

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcBackgroundPanelPrivate));

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;
}

static void
cc_background_panel_class_finalize (CcBackgroundPanelClass *klass)
{
}

static void
update_preview (CcBackgroundPanelPrivate *priv,
                CcBackgroundItem         *item)
{
  gboolean changes_with_time;

  if (item && priv->current_background)
    {
      g_object_unref (priv->current_background);
      priv->current_background = cc_background_item_copy (item);
      cc_background_item_load (priv->current_background, NULL);
    }

  changes_with_time = FALSE;

  if (priv->current_background)
    {
      changes_with_time = cc_background_item_changes_with_time (priv->current_background);
    }

  gtk_widget_set_visible (WID ("slide_image"), changes_with_time);
  gtk_widget_set_visible (WID ("slide-label"), changes_with_time);

  gtk_widget_queue_draw (WID ("background-desktop-drawingarea"));
}

static char *
get_save_path (void)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           "last-edited.xml",
                           NULL);
}

static void
update_display_preview (CcBackgroundPanel *panel)
{
  CcBackgroundPanelPrivate *priv = panel->priv;
  GtkWidget *widget;
  GtkAllocation allocation;
  const gint preview_width = 416;
  const gint preview_height = 248;
  GdkPixbuf *pixbuf;
  GIcon *icon;
  cairo_t *cr;

  widget = WID ("background-desktop-drawingarea");
  gtk_widget_get_allocation (widget, &allocation);

  if (!priv->current_background)
    return;

  icon = cc_background_item_get_frame_thumbnail (priv->current_background,
                                                 priv->thumb_factory,
                                                 preview_width,
                                                 preview_height,
                                                 -2, TRUE);
  pixbuf = GDK_PIXBUF (icon);

  cr = gdk_cairo_create (gtk_widget_get_window (widget));
  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               0, 0);
  cairo_paint (cr);
  g_object_unref (pixbuf);

  pixbuf = NULL;
  if (panel->priv->display_screenshot != NULL)
    pixbuf = gdk_pixbuf_scale_simple (panel->priv->display_screenshot,
                                      preview_width,
                                      preview_height,
                                      GDK_INTERP_BILINEAR);

  if (pixbuf)
    {
      gdk_cairo_set_source_pixbuf (cr,
                                   pixbuf,
                                   0, 0);
      cairo_paint (cr);
    }

  if (priv->display_overlay)
    {
      gdk_cairo_set_source_pixbuf (cr,
                                   priv->display_overlay,
                                   0, 0);
      cairo_paint (cr);
    }

  cairo_destroy (cr);
}

static void
on_screenshot_finished (GObject *source,
                        GAsyncResult *res,
                        gpointer user_data)
{
  CcBackgroundPanel *panel = user_data;
  CcBackgroundPanelPrivate *priv = panel->priv;
  GError *error;
  GdkRectangle rect;
  GtkWidget *widget;
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width;
  int height;

  error = NULL;
  g_dbus_connection_call_finish (panel->priv->connection,
                                 res,
                                 &error);

  if (error != NULL) {
    g_debug ("Unable to get screenshot: %s",
             error->message);
    g_error_free (error);
    /* fallback? */
    goto out;
  }

  pixbuf = gdk_pixbuf_new_from_file (panel->priv->screenshot_path, &error);
  if (error != NULL)
    {
      g_debug ("Unable to use GNOME Shell's builtin screenshot interface: %s",
               error->message);
      g_error_free (error);
      goto out;
    }

  /* remove the temporary file created by the shell */
  g_unlink (panel->priv->screenshot_path);

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width, height);
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  g_object_unref (pixbuf);

  /* clear the workarea */
   widget = WID ("background-desktop-drawingarea");
  gdk_screen_get_monitor_workarea (gtk_widget_get_screen (widget), 0, &rect);

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
  cairo_fill (cr);
  cairo_restore (cr);

  g_clear_object (&panel->priv->display_screenshot);
  panel->priv->display_screenshot = gdk_pixbuf_get_from_surface (surface,
                                                                 0, 0,
                                                                 width,
                                                                 height);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

 out:
  update_display_preview (panel);
}

static void
get_screenshot_async (CcBackgroundPanel *panel,
                      GdkRectangle      *rectangle)
{
  gchar *path, *tmpname;
  const gchar *method_name;
  GVariant *method_params;

  path = g_build_filename (g_get_user_cache_dir (), "gnome-control-center", NULL);
  g_mkdir_with_parents (path, 0700);

  tmpname = g_strdup_printf ("scr-%d.png", g_random_int ());
  g_free (panel->priv->screenshot_path);
  panel->priv->screenshot_path = g_build_filename (path, tmpname, NULL);
  g_free (path);
  g_free (tmpname);

  method_name = "ScreenshotArea";
  method_params = g_variant_new ("(iiiibs)",
                                 rectangle->x, rectangle->y,
                                 rectangle->width, rectangle->height,
                                 FALSE, /* flash */
                                 panel->priv->screenshot_path);

  g_dbus_connection_call (panel->priv->connection,
                          "org.gnome.Shell",
                          "/org/gnome/Shell",
                          "org.gnome.Shell",
                          method_name,
                          method_params,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          on_screenshot_finished,
                          panel);
}

static gboolean
on_preview_draw (GtkWidget         *widget,
                 cairo_t           *cr,
                 CcBackgroundPanel *panel)
{
  if (!panel->priv->display_screenshot)
    {
      GdkRectangle rect;

      gdk_screen_get_monitor_geometry (gtk_widget_get_screen (widget), 0, &rect);
      get_screenshot_async (panel, &rect);
    }
  else
    update_display_preview (panel);

  return TRUE;
}

static void
load_current_bg (CcBackgroundPanel *self)
{
  CcBackgroundPanelPrivate *priv;
  CcBackgroundItem *saved, *configured;
  gchar *uri, *pcolor, *scolor;

  priv = self->priv;

  /* Load the saved configuration */
  uri = get_save_path ();
  saved = cc_background_xml_get_item (uri);
  g_free (uri);

  /* initalise the current background information from settings */
  uri = g_settings_get_string (priv->settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    {
      g_free (uri);
      uri = NULL;
    }
  else
    {
      GFile *file;

      file = g_file_new_for_commandline_arg (uri);
      g_object_unref (file);
    }
  configured = cc_background_item_new (uri);
  g_free (uri);

  pcolor = g_settings_get_string (priv->settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (priv->settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
                "name", _("Current background"),
                "placement", g_settings_get_enum (priv->settings, WP_OPTIONS_KEY),
                "shading", g_settings_get_enum (priv->settings, WP_SHADING_KEY),
                "primary-color", pcolor,
                "secondary-color", scolor,
                NULL);
  g_free (pcolor);
  g_free (scolor);

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
  if (saved != NULL)
    g_object_unref (saved);

  priv->current_background = configured;
  cc_background_item_load (priv->current_background, NULL);
}

static void
cc_background_panel_init (CcBackgroundPanel *self)
{
  CcBackgroundPanelPrivate *priv;
  gchar *objects[] = {"background-panel", NULL };
  GError *err = NULL;
  GtkWidget *widget;

  priv = self->priv = BACKGROUND_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();
  priv->connection = g_application_get_dbus_connection (g_application_get_default ());

  gtk_builder_add_objects_from_file (priv->builder,
                                     DATADIR"/background.ui",
                                     objects, &err);

  if (err)
    {
      g_warning ("Could not load ui: %s", err->message);
      g_error_free (err);
      return;
    }

  priv->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (priv->settings);

  /* add the top level widget */
  widget = WID ("background-panel");

  gtk_container_add (GTK_CONTAINER (self), widget);
  gtk_widget_show_all (GTK_WIDGET (self));

  /* setup preview area */
  widget = WID ("background-desktop-drawingarea");
  g_signal_connect (widget, "draw", G_CALLBACK (on_preview_draw),
                    self);

  priv->copy_cancellable = g_cancellable_new ();

  priv->display_overlay = gdk_pixbuf_new_from_file (DATADIR
                                                    "/display-overlay.png",
                                                    NULL);

  priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  load_current_bg (self);

  update_preview (priv, NULL);
}

void
cc_background_panel_register (GIOModule *module)
{
  cc_background_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_BACKGROUND_PANEL,
                                  "background", 0);
}

