#include <unistd.h>
#include <string.h>
#include <metacity-private/util.h>
#include <metacity-private/theme.h>
#include <metacity-private/theme-parser.h>
#include <metacity-private/preview-widget.h>
#include <signal.h>
#include <errno.h>

/* We have to #undef this as metacity #defines these. */
#undef _
#undef N_

#include <libgnomeui/gnome-icon-theme.h>
#include <config.h>

#include "theme-thumbnail.h"
#include "capplet-util.h"

static gint child_pid;
#define ICON_SIZE_WIDTH 150
#define ICON_SIZE_HEIGHT 150

typedef struct
{
  gboolean set;
  GByteArray *data;
  gchar *meta_theme_name;
  ThemeThumbnailFunc func;
  gpointer user_data;
  GDestroyNotify destroy;
  GIOChannel *channel;
  guint watch_id;
} ThemeThumbnailAsyncData;


GHashTable *theme_hash = NULL;
ThemeThumbnailAsyncData async_data;


/* Protocol */

/* Our protocol is pretty simple.  The parent process will write four strings
 * (separated by a '\000') They are the widget theme, the wm theme, the icon
 * theme, and the font string.  Then, it will wait for the child to write back
 * the data.  The parent expects ICON_SIZE_WIDTH * ICON_SIZE_HEIGHT * 4 bytes of
 * information.  After that, the child is ready for the next theme to render.
 */

enum
{
  READY_FOR_THEME,
  READING_CONTROL_THEME_NAME,
  READING_WM_THEME_NAME,
  READING_ICON_THEME_NAME,
  READING_APPLICATION_FONT,
  WRITING_PIXBUF_DATA
};

typedef struct
{
  gint status;
  GByteArray *control_theme_name;
  GByteArray *wm_theme_name;
  GByteArray *icon_theme_name;
  GByteArray *application_font;
} ThemeThumbnailData;

int pipe_to_factory_fd[2];
int pipe_from_factory_fd[2];

static void
fake_expose_widget (GtkWidget *widget,
		    GdkPixmap *pixmap)
{
  GdkWindow *tmp_window;
  GdkEventExpose event;

  event.type = GDK_EXPOSE;
  event.window = pixmap;
  event.send_event = FALSE;
  event.area = widget->allocation;
  event.region = NULL;
  event.count = 0;

  tmp_window = widget->window;
  widget->window = pixmap;
  gtk_widget_send_expose (widget, (GdkEvent *) &event);
  widget->window = tmp_window;
}



static void
hbox_foreach (GtkWidget *widget,
	      gpointer   data)
{
  gtk_widget_realize (widget);
  gtk_widget_map (widget);
  gtk_widget_ensure_style (widget);
  fake_expose_widget (widget, (GdkPixmap *) data);
}

static void
create_image (ThemeThumbnailData *theme_thumbnail_data,
	      GdkPixbuf          *pixbuf)
{
  GtkWidget *window;
  GtkWidget *preview;
  GtkWidget *align;
  GtkWidget *stock_button;

  GtkRequisition requisition;
  GtkAllocation allocation;
  GdkPixmap *pixmap;
  GdkVisual *visual;
  MetaFrameFlags flags;
  MetaTheme *theme = NULL;
  GtkSettings *settings;
  GtkIconTheme *icon_theme;
  GdkPixbuf *folder_icon;
  GtkIconInfo *folder_icon_info;
  const gchar *filename;

  settings = gtk_settings_get_default ();
  g_object_set (G_OBJECT (settings),
		"gtk-theme-name", (char *) theme_thumbnail_data->control_theme_name->data,
		"gtk-font-name", (char *) theme_thumbnail_data->application_font->data,
		NULL);
  theme = meta_theme_load ((char *) theme_thumbnail_data->wm_theme_name->data, NULL);

  flags = META_FRAME_ALLOWS_DELETE |
    META_FRAME_ALLOWS_MENU |
    META_FRAME_ALLOWS_MINIMIZE |
    META_FRAME_ALLOWS_MAXIMIZE |
    META_FRAME_ALLOWS_VERTICAL_RESIZE |
    META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
    META_FRAME_HAS_FOCUS |
    META_FRAME_ALLOWS_SHADE |
    META_FRAME_ALLOWS_MOVE;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  preview = meta_preview_new ();
  gtk_container_add (GTK_CONTAINER (window), preview);
  gtk_widget_realize (window);
  gtk_widget_realize (preview);
  align = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
  gtk_container_add (GTK_CONTAINER (preview), align);
  gtk_container_set_border_width (GTK_CONTAINER (align), 5);
  stock_button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
  gtk_container_add (GTK_CONTAINER (align), stock_button);

  gtk_widget_show_all (preview);
  gtk_widget_realize (align);
  gtk_widget_realize (stock_button);
  gtk_widget_realize (GTK_BIN (stock_button)->child);
  gtk_widget_map (stock_button);
  gtk_widget_map (GTK_BIN (stock_button)->child);

  meta_preview_set_frame_flags (META_PREVIEW (preview), flags);
  meta_preview_set_theme (META_PREVIEW (preview), theme);
  meta_preview_set_title (META_PREVIEW (preview), "");


  gtk_window_set_default_size (GTK_WINDOW (window), ICON_SIZE_WIDTH, ICON_SIZE_HEIGHT);

  gtk_widget_size_request (window, &requisition);
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = ICON_SIZE_WIDTH;
  allocation.height = ICON_SIZE_HEIGHT;
  gtk_widget_size_allocate (window, &allocation);
  gtk_widget_size_request (window, &requisition);

  /* Create a pixmap */
  visual = gtk_widget_get_visual (window);
  pixmap = gdk_pixmap_new (NULL, ICON_SIZE_WIDTH, ICON_SIZE_HEIGHT, visual->depth);
  gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), gtk_widget_get_colormap (window));

  /* Draw the window */
  gtk_widget_ensure_style (window);
  g_assert (window->style);
  g_assert (window->style->font_desc);

  fake_expose_widget (window, pixmap);
  fake_expose_widget (preview, pixmap);
  fake_expose_widget (stock_button, pixmap);
  gtk_container_foreach (GTK_CONTAINER (GTK_BIN (GTK_BIN (stock_button)->child)->child),
			 hbox_foreach,
			 pixmap);
  fake_expose_widget (GTK_BIN (stock_button)->child, pixmap);


  gdk_pixbuf_get_from_drawable (pixbuf, pixmap, NULL, 0, 0, 0, 0, ICON_SIZE_WIDTH, ICON_SIZE_HEIGHT);

  /* Handle the icon theme */
  icon_theme = gtk_icon_theme_new ();
  gtk_icon_theme_set_custom_theme (icon_theme, (char *) theme_thumbnail_data->icon_theme_name->data);

  /* Have to try both "folder" and "gnome-fs-directory" seems themes seem to use either name */
  folder_icon_info = gtk_icon_theme_lookup_icon (icon_theme, "folder", 48, GTK_ICON_LOOKUP_FORCE_SVG);
  if (folder_icon_info == NULL) {
    folder_icon_info = gtk_icon_theme_lookup_icon (icon_theme, "gnome-fs-directory", 48, GTK_ICON_LOOKUP_FORCE_SVG);
  }
 
  g_object_unref (icon_theme);

  filename = gtk_icon_info_get_filename (folder_icon_info);
 
  if (filename != NULL)
    {
      folder_icon = gdk_pixbuf_new_from_file (filename, NULL);
    }
  else
    {
      folder_icon = NULL;
    }

  gtk_icon_info_free (folder_icon_info);

  /* render the icon to the thumbnail */
  if (folder_icon)
    {
      gdk_pixbuf_composite (folder_icon,
			    pixbuf,
			    align->allocation.x + align->allocation.width - gdk_pixbuf_get_width (folder_icon) - 5,
			    align->allocation.y + align->allocation.height - gdk_pixbuf_get_height (folder_icon) - 5,
			    gdk_pixbuf_get_width (folder_icon),
			    gdk_pixbuf_get_height (folder_icon),
			    align->allocation.x + align->allocation.width - gdk_pixbuf_get_width (folder_icon) - 5,
			    align->allocation.y + align->allocation.height - gdk_pixbuf_get_height (folder_icon) - 5,
			    1.0, 1.0, GDK_INTERP_BILINEAR, 255);
      g_object_unref (folder_icon);
    }
}

static void
handle_bytes (const gchar        *buffer,
	      gint                bytes_read,
	      ThemeThumbnailData *theme_thumbnail_data)
{
  const gchar *ptr;
  ptr = buffer;

  while (bytes_read > 0)
    {
      char *nil;
      switch (theme_thumbnail_data->status)
	{
	case READY_FOR_THEME:
	case READING_CONTROL_THEME_NAME:
	  theme_thumbnail_data->status = READING_CONTROL_THEME_NAME;
	  nil = memchr (ptr, '\000', bytes_read);
	  if (nil == NULL)
	    {
	      g_byte_array_append (theme_thumbnail_data->control_theme_name, ptr, bytes_read);
	      bytes_read = 0;
	    }
	  else
	    {
	      g_byte_array_append (theme_thumbnail_data->control_theme_name, ptr, nil - ptr + 1);
	      bytes_read -= (nil - ptr + 1);
	      ptr = nil + 1;
	      theme_thumbnail_data->status = READING_WM_THEME_NAME;
	    }
	  break;
	case READING_WM_THEME_NAME:
	  nil = memchr (ptr, '\000', bytes_read);
	  if (nil == NULL)
	    {
	      g_byte_array_append (theme_thumbnail_data->wm_theme_name, ptr, bytes_read);
	      bytes_read = 0;
	    }
	  else
	    {
	      g_byte_array_append (theme_thumbnail_data->wm_theme_name, ptr, nil - ptr + 1);
	      bytes_read -= (nil - ptr + 1);
	      ptr = nil + 1;
	      theme_thumbnail_data->status = READING_ICON_THEME_NAME;
	    }
	  break;
	case READING_ICON_THEME_NAME:
	  nil = memchr (ptr, '\000', bytes_read);
	  if (nil == NULL)
	    {
	      g_byte_array_append (theme_thumbnail_data->icon_theme_name, ptr, bytes_read);
	      bytes_read = 0;
	    }
	  else
	    {
	      g_byte_array_append (theme_thumbnail_data->icon_theme_name, ptr, nil - ptr + 1);
	      bytes_read -= (nil - ptr + 1);
	      ptr = nil + 1;
	      theme_thumbnail_data->status = READING_APPLICATION_FONT;
	    }
	  break;
	case READING_APPLICATION_FONT:
	  nil = memchr (ptr, '\000', bytes_read);
	  if (nil == NULL)
	    {
	      g_byte_array_append (theme_thumbnail_data->application_font, ptr, bytes_read);
	      bytes_read = 0;
	    }
	  else
	    {
	      g_byte_array_append (theme_thumbnail_data->application_font, ptr, nil - ptr + 1);
	      bytes_read -= (nil - ptr + 1);
	      ptr = nil + 1;
	      theme_thumbnail_data->status = WRITING_PIXBUF_DATA;
	    }
	  break;
	default:
	  g_assert_not_reached ();
	}
    }
}

static gboolean
message_from_capplet (GIOChannel   *source,
		      GIOCondition  condition,
		      gpointer      data)
{
  gchar buffer[1024];
  GIOStatus status;
  gsize bytes_read;
  GdkPixbuf *pixbuf;
  gint i, rowstride;
  char *pixels;
  ThemeThumbnailData *theme_thumbnail_data;

  theme_thumbnail_data = (ThemeThumbnailData *)data;

  status = g_io_channel_read_chars (source,
                                    buffer,
                                    1024,
                                    &bytes_read,
                                    NULL);

  switch (status)
    {
    case G_IO_STATUS_NORMAL:
      handle_bytes (buffer, bytes_read, theme_thumbnail_data);

      if (theme_thumbnail_data->status == WRITING_PIXBUF_DATA)
	{
	  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, ICON_SIZE_WIDTH, ICON_SIZE_HEIGHT);
	  create_image (theme_thumbnail_data, pixbuf);
	  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	  pixels = gdk_pixbuf_get_pixels (pixbuf);
	  for (i = 0; i < ICON_SIZE_HEIGHT; i ++)
	    {
	      write (pipe_from_factory_fd[1], pixels + (rowstride)*i, ICON_SIZE_WIDTH * gdk_pixbuf_get_n_channels (pixbuf));
	    }
	  g_object_unref (pixbuf);
	  theme_thumbnail_data->status = READY_FOR_THEME;
	  g_byte_array_set_size (theme_thumbnail_data->control_theme_name, 0);
	  g_byte_array_set_size (theme_thumbnail_data->wm_theme_name, 0);
	  g_byte_array_set_size (theme_thumbnail_data->icon_theme_name, 0);
	  g_byte_array_set_size (theme_thumbnail_data->application_font, 0);
	}
      return TRUE;
    case G_IO_STATUS_AGAIN:
      return TRUE;
    case G_IO_STATUS_EOF:
    case G_IO_STATUS_ERROR:
      _exit (0);
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
message_from_child (GIOChannel   *source,
		    GIOCondition  condition,
		    gpointer      data)
{

  gchar buffer[1024];
  GIOStatus status;
  gsize bytes_read;

  if (async_data.set == FALSE)
    return TRUE;

  status = g_io_channel_read_chars (source,
                                    buffer,
                                    1024,
                                    &bytes_read,
                                    NULL);
  switch (status)
    {
    case G_IO_STATUS_NORMAL:
      g_byte_array_append (async_data.data, buffer, bytes_read);
      if (async_data.data->len == ICON_SIZE_WIDTH * ICON_SIZE_HEIGHT * 4)
	{
	  GdkPixbuf *pixbuf;
	  GdkPixbuf *scaled_pixbuf;
	  gchar *pixels;
	  gint i, rowstride;

	  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, ICON_SIZE_WIDTH, ICON_SIZE_HEIGHT);
	  pixels = gdk_pixbuf_get_pixels (pixbuf);
	  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	  for (i = 0; i < ICON_SIZE_HEIGHT; i++)
	    memcpy (pixels + rowstride * i, async_data.data->data + 4 * ICON_SIZE_WIDTH * i, ICON_SIZE_WIDTH * 4);

	  scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, ICON_SIZE_WIDTH/2, ICON_SIZE_HEIGHT/2, GDK_INTERP_BILINEAR);
	  g_hash_table_insert (theme_hash, g_strdup(async_data.meta_theme_name), scaled_pixbuf);
	  g_object_unref (pixbuf);

 	  (* async_data.func) (scaled_pixbuf, async_data.user_data);
	  if (async_data.destroy)
	    (* async_data.destroy) (async_data.user_data);

	  /* Clean up async_data */
	  g_free (async_data.meta_theme_name);
	  g_source_remove (async_data.watch_id);
	  g_io_channel_unref (async_data.channel);

	  /* reset async_data */
	  async_data.meta_theme_name = NULL;
	  async_data.channel = NULL;
	  async_data.func = NULL;
	  async_data.user_data = NULL;
	  async_data.destroy = NULL;
	  async_data.set = FALSE;
	  g_byte_array_set_size (async_data.data, 0);
	}
      return TRUE;
    case G_IO_STATUS_AGAIN:
      return TRUE;

    case G_IO_STATUS_EOF:
    case G_IO_STATUS_ERROR:
      return TRUE;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

void  
theme_thumbnail_invalidate_cache (GnomeThemeMetaInfo *meta_theme_info)
{
  gboolean success;

  success = g_hash_table_remove (theme_hash, meta_theme_info->name);
  printf ("Success is %d\n", success);
}

GdkPixbuf *
generate_theme_thumbnail (GnomeThemeMetaInfo *meta_theme_info,
			  gboolean            clear_cache)
{
  GdkPixbuf *retval = NULL;
  GdkPixbuf *pixbuf = NULL;
  gint i, rowstride;
  char *pixels;

  g_return_val_if_fail (async_data.set == FALSE, NULL);

  pixbuf = g_hash_table_lookup (theme_hash, meta_theme_info->name);
  if (pixbuf != NULL)
    {
      if (clear_cache)
	g_hash_table_remove (theme_hash, meta_theme_info->name);
      else
	return pixbuf;
    }

  if (!pipe_to_factory_fd[1] || !pipe_from_factory_fd[0])
    return NULL;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, ICON_SIZE_WIDTH, ICON_SIZE_HEIGHT);
  write (pipe_to_factory_fd[1], meta_theme_info->gtk_theme_name, strlen (meta_theme_info->gtk_theme_name) + 1);
  write (pipe_to_factory_fd[1], meta_theme_info->metacity_theme_name, strlen (meta_theme_info->metacity_theme_name) + 1);
  write (pipe_to_factory_fd[1], meta_theme_info->icon_theme_name, strlen (meta_theme_info->icon_theme_name) + 1);
  if (meta_theme_info->application_font == NULL)
    write (pipe_to_factory_fd[1], "Sans 10", strlen ("Sans 10") + 1);
  else
    write (pipe_to_factory_fd[1], meta_theme_info->application_font, strlen (meta_theme_info->application_font) + 1);

  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  for (i = 0; i < ICON_SIZE_HEIGHT; i++)
    {
      gint j = 0;
      gint bytes_read;

      do
	{
	  bytes_read = read (pipe_from_factory_fd[0], pixels + (rowstride)*i + j, ICON_SIZE_WIDTH * gdk_pixbuf_get_n_channels (pixbuf) - j);
	  if (bytes_read > 0)
	    j += bytes_read;
	  else if (bytes_read == 0)
	    {
	      g_warning ("Received EOF while reading thumbnail for gtk: '%s', metacity '%s', icon: '%s', font: '%s'\n",
			 meta_theme_info->gtk_theme_name,
			 meta_theme_info->metacity_theme_name,
			 meta_theme_info->icon_theme_name,
			 meta_theme_info->application_font ? meta_theme_info->application_font : "Sans 10");
	      g_object_unref (pixbuf);
	      close (pipe_to_factory_fd[1]);
	      pipe_to_factory_fd[1] = 0;
	      close (pipe_from_factory_fd[0]);
	      pipe_from_factory_fd[0] = 0;
	      return NULL;
	    }
	}
      while (j < ICON_SIZE_WIDTH * gdk_pixbuf_get_n_channels (pixbuf));
    }

  retval = gdk_pixbuf_scale_simple (pixbuf, ICON_SIZE_WIDTH/2, ICON_SIZE_HEIGHT/2, GDK_INTERP_BILINEAR);
  g_object_unref (pixbuf);
  
  g_hash_table_insert (theme_hash, g_strdup (meta_theme_info->name), retval);
  return retval;
}

void
generate_theme_thumbnail_async (GnomeThemeMetaInfo *meta_theme_info,
				ThemeThumbnailFunc  func,
				gpointer            user_data,
				GDestroyNotify      destroy)
{
  GdkPixbuf *pixbuf;

  g_return_if_fail (async_data.set == FALSE);

  pixbuf = g_hash_table_lookup (theme_hash, meta_theme_info->name);
  if (pixbuf != NULL)
    {
      (* func) (pixbuf, user_data);
      if (destroy)
	(* destroy) (user_data);
      return;
    }

  if (!pipe_to_factory_fd[1] || !pipe_from_factory_fd[0])
    {
      (* func) (NULL, user_data);
      if (destroy)
	(* destroy) (user_data);
      return;
    }

  if (async_data.channel == NULL)
    {
      async_data.channel = g_io_channel_unix_new (pipe_from_factory_fd[0]);
      g_io_channel_set_flags (async_data.channel, g_io_channel_get_flags (async_data.channel) |
			      G_IO_FLAG_NONBLOCK, NULL);
      g_io_channel_set_encoding (async_data.channel, NULL, NULL);
      async_data.watch_id = g_io_add_watch (async_data.channel, G_IO_IN | G_IO_HUP, message_from_child, NULL);
    }


  async_data.set = TRUE;
  async_data.meta_theme_name = g_strdup (meta_theme_info->name);
  async_data.func = func;
  async_data.user_data = user_data;
  async_data.destroy = destroy;

  write (pipe_to_factory_fd[1], meta_theme_info->gtk_theme_name, strlen (meta_theme_info->gtk_theme_name) + 1);
  write (pipe_to_factory_fd[1], meta_theme_info->metacity_theme_name, strlen (meta_theme_info->metacity_theme_name) + 1);
  write (pipe_to_factory_fd[1], meta_theme_info->icon_theme_name, strlen (meta_theme_info->icon_theme_name) + 1);
  if (meta_theme_info->application_font == NULL)
    write (pipe_to_factory_fd[1], "Sans 10", strlen ("Sans 10") + 1);
  else
    write (pipe_to_factory_fd[1], meta_theme_info->application_font, strlen (meta_theme_info->application_font) + 1);
}

void
theme_thumbnail_factory_init (int argc, char *argv[])
{
  pipe (pipe_to_factory_fd);
  pipe (pipe_from_factory_fd);

  child_pid = fork ();
  if (child_pid == 0)
    {
      ThemeThumbnailData data;
      GIOChannel *channel;

      /* Child */
      gtk_init (&argc, &argv);

      close (pipe_to_factory_fd[1]);
      pipe_to_factory_fd[1] = 0;
      close (pipe_from_factory_fd[0]);
      pipe_from_factory_fd[0] = 0;

      data.status = READY_FOR_THEME;
      data.control_theme_name = g_byte_array_new ();
      data.wm_theme_name = g_byte_array_new ();
      data.icon_theme_name = g_byte_array_new ();
      data.application_font = g_byte_array_new ();

      channel = g_io_channel_unix_new (pipe_to_factory_fd[0]);
      g_io_channel_set_flags (channel, g_io_channel_get_flags (channel) |
			      G_IO_FLAG_NONBLOCK, NULL);
      g_io_channel_set_encoding (channel, NULL, NULL);
      g_io_add_watch (channel, G_IO_IN | G_IO_HUP, message_from_capplet, &data);
      g_io_channel_unref (channel);

      gtk_main ();
      _exit (0);
    }

  g_assert (child_pid > 0);

  /* Parent */
  close (pipe_to_factory_fd[0]);
  close (pipe_from_factory_fd[1]);
  async_data.set = FALSE;
  async_data.meta_theme_name = NULL;
  async_data.data = g_byte_array_new ();

  theme_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}
