#include <unistd.h>
#include <string.h>
#include <metacity-private/util.h>
#include <metacity-private/theme.h>
#include <metacity-private/theme-parser.h>
#include <metacity-private/preview-widget.h>

/* We have to #undef this as metacity #defines these. */
#undef _
#undef N_

#include <libgnomeui/gnome-icon-theme.h>
#include <config.h>

#include "theme-thumbnail.h"
#include "capplet-util.h"


/* Protocol */

/* Our protocol is pretty simple.  The parent process will write three strings
 * (separated by a '\000') They are the widget theme, the wm theme, and the icon
 * theme.  Then, it will wait for the child to write back the data.  It expects
 * 100x100x3 bytes of information.  After that, the child is ready for the next
 * theme to render.
 */

enum
{
  READY_FOR_THEME,
  READING_CONTROL_THEME_NAME,
  READING_WM_THEME_NAME,
  READING_ICON_THEME_NAME,
  WRITING_PIXBUF_DATA
};

typedef struct
{
  gint status;
  GByteArray *control_theme_name;
  GByteArray *wm_theme_name;
  GByteArray *icon_theme_name;
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
  GnomeIconTheme *icon_theme;
  GdkPixbuf *folder_icon;
  char *folder_icon_name;

  settings = gtk_settings_get_default ();
  g_object_set (G_OBJECT (settings),
		"gtk-theme-name", (char *) theme_thumbnail_data->control_theme_name->data,
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


  gtk_window_set_default_size (GTK_WINDOW (window), 100, 100);

  gtk_widget_size_request (window, &requisition);
  allocation.x = 0;
  allocation.y = 0;
  allocation.width = 100;
  allocation.height = 100;
  gtk_widget_size_allocate (window, &allocation);
  gtk_widget_size_request (window, &requisition);

  /* Create a pixmap */
  visual = gtk_widget_get_visual (window);
  pixmap = gdk_pixmap_new (NULL, 100, 100, gdk_visual_get_best_depth());
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


  gdk_pixbuf_get_from_drawable (pixbuf, pixmap, NULL, 0, 0, 0, 0, 100, 100);

  /* Handle the icon theme */
  icon_theme = gnome_icon_theme_new ();
  gnome_icon_theme_set_custom_theme (icon_theme, (char *) theme_thumbnail_data->icon_theme_name->data);
  folder_icon_name = gnome_icon_theme_lookup_icon (icon_theme, "folder", 48, NULL, NULL);
  if (folder_icon_name != NULL)
    {
      folder_icon = gdk_pixbuf_new_from_file (folder_icon_name, NULL);
      g_free (folder_icon_name);
    }
  else
    {
      folder_icon = NULL;
    }
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
  gint bytes_read;
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
	  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 100, 100);
	  create_image (theme_thumbnail_data, pixbuf);
	  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	  pixels = gdk_pixbuf_get_pixels (pixbuf);

	  for (i = 0; i < 100; i ++)
	    {
	      write (pipe_from_factory_fd[1], pixels + (rowstride)*i, 100*4);
	    }
	  theme_thumbnail_data->status = READY_FOR_THEME;
	  g_byte_array_set_size (theme_thumbnail_data->control_theme_name, 0);
	  g_byte_array_set_size (theme_thumbnail_data->wm_theme_name, 0);
	  g_byte_array_set_size (theme_thumbnail_data->icon_theme_name, 0);
	}
      return TRUE;
    case G_IO_STATUS_AGAIN:
      return TRUE;
    case G_IO_STATUS_EOF:
    case G_IO_STATUS_ERROR:
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}


GdkPixbuf *
generate_theme_thumbnail (GnomeThemeMetaInfo *meta_theme_info)
{
  GdkPixbuf *retval = NULL;
  gint i, rowstride;
  char *pixels;

  retval = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 100, 100);
  write (pipe_to_factory_fd[1], meta_theme_info->gtk_theme_name, strlen (meta_theme_info->gtk_theme_name) + 1);
  write (pipe_to_factory_fd[1], meta_theme_info->metacity_theme_name, strlen (meta_theme_info->metacity_theme_name) + 1);
  write (pipe_to_factory_fd[1], meta_theme_info->icon_theme_name, strlen (meta_theme_info->icon_theme_name) + 1);

  rowstride = gdk_pixbuf_get_rowstride (retval);
  pixels = gdk_pixbuf_get_pixels (retval);

  for (i = 0; i < 100; i++)
    {
      read (pipe_from_factory_fd[0], pixels + (rowstride)*i, 100*4);
    }
  return retval;
}


void
setup_theme_thumbnail_factory (int argc, char *argv[])
{
  gint pid;

  pipe (pipe_to_factory_fd);
  pipe (pipe_from_factory_fd);

  pid = fork ();
  if (pid == 0)
    {
      GIOChannel *channel;
      ThemeThumbnailData data;

      /* Child */
      gtk_init (&argc, &argv);
      close (pipe_to_factory_fd[1]);
      close (pipe_from_factory_fd[0]);

      data.status = READY_FOR_THEME;
      data.control_theme_name = g_byte_array_new ();
      data.wm_theme_name = g_byte_array_new ();
      data.icon_theme_name = g_byte_array_new ();

      channel = g_io_channel_unix_new (pipe_to_factory_fd[0]);
      g_io_channel_set_flags (channel, g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK, NULL);
      g_io_channel_set_encoding (channel, NULL, NULL);
      g_io_add_watch (channel, G_IO_IN, message_from_capplet, &data);
      g_io_channel_unref (channel);

      gtk_main ();
      _exit (0);
    }

  /* Parent */
  close (pipe_to_factory_fd[0]);
  close (pipe_from_factory_fd[1]);
}
