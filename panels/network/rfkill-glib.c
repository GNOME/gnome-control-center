/*
 *
 *  gnome-bluetooth - Bluetooth integration for GNOME
 *
 *  Copyright (C) 2012  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include <glib.h>

#include "rfkill-glib.h"

enum {
	CHANGED,
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

#define CC_RFKILL_GLIB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
				CC_RFKILL_TYPE_GLIB, CcRfkillGlibPrivate))

struct CcRfkillGlibPrivate {
	int fd;
	GIOChannel *channel;
	guint watch_id;
};

G_DEFINE_TYPE(CcRfkillGlib, cc_rfkill_glib, G_TYPE_OBJECT)

int
cc_rfkill_glib_send_event (CcRfkillGlib *rfkill, struct rfkill_event *event)
{
	g_return_val_if_fail (RFKILL_IS_GLIB (rfkill), -1);
	g_return_val_if_fail (rfkill->priv->fd > 0, -1);

	return write (rfkill->priv->fd, event, sizeof(struct rfkill_event));
}

static const char *
type_to_string (unsigned int type)
{
	switch (type) {
	case RFKILL_TYPE_ALL:
		return "ALL";
	case RFKILL_TYPE_WLAN:
		return "WLAN";
	case RFKILL_TYPE_BLUETOOTH:
		return "RFKILL";
	case RFKILL_TYPE_UWB:
		return "UWB";
	case RFKILL_TYPE_WIMAX:
		return "WIMAX";
	case RFKILL_TYPE_WWAN:
		return "WWAN";
	default:
		g_assert_not_reached ();
	}
}

static const char *
op_to_string (unsigned int op)
{
	switch (op) {
	case RFKILL_OP_ADD:
		return "ADD";
	case RFKILL_OP_DEL:
		return "DEL";
	case RFKILL_OP_CHANGE:
		return "CHANGE";
	case RFKILL_OP_CHANGE_ALL:
		return "CHANGE_ALL";
	default:
		g_assert_not_reached ();
	}
}

static void
print_event (struct rfkill_event *event)
{
	g_debug ("RFKILL event: idx %u type %u (%s) op %u (%s) soft %u hard %u",
		 event->idx,
		 event->type, type_to_string (event->type),
		 event->op, op_to_string (event->op),
		 event->soft, event->hard);
}

static void
emit_changed_signal_and_free (CcRfkillGlib *rfkill,
			      GList      *events)
{
	if (events == NULL)
		return;

	g_signal_emit (G_OBJECT (rfkill),
		       signals[CHANGED],
		       0, events);
	g_list_free_full (events, g_free);
}

static gboolean
event_cb (GIOChannel   *source,
	  GIOCondition  condition,
	  CcRfkillGlib   *rfkill)
{
	GList *events;

	events = NULL;

	if (condition & G_IO_IN) {
		GIOStatus status;
		struct rfkill_event event;
		gsize read;

		status = g_io_channel_read_chars (source,
						  (char *) &event,
						  sizeof(event),
						  &read,
						  NULL);

		while (status == G_IO_STATUS_NORMAL && read == sizeof(event)) {
			struct rfkill_event *event_ptr;

			print_event (&event);

			event_ptr = g_memdup (&event, sizeof(event));
			events = g_list_prepend (events, event_ptr);

			status = g_io_channel_read_chars (source,
							  (char *) &event,
							  sizeof(event),
							  &read,
							  NULL);
		}
		events = g_list_reverse (events);
	} else {
		g_debug ("something else happened");
		return FALSE;
	}

	emit_changed_signal_and_free (rfkill, events);

	return TRUE;
}

static void
cc_rfkill_glib_init (CcRfkillGlib *rfkill)
{
	CcRfkillGlibPrivate *priv;

	priv = CC_RFKILL_GLIB_GET_PRIVATE (rfkill);
	rfkill->priv = priv;
	rfkill->priv->fd = -1;
}

int
cc_rfkill_glib_open (CcRfkillGlib *rfkill)
{
	CcRfkillGlibPrivate *priv;
	int fd;
	int ret;
	GList *events;

	g_return_val_if_fail (RFKILL_IS_GLIB (rfkill), -1);
	g_return_val_if_fail (rfkill->priv->fd == -1, -1);

	priv = rfkill->priv;

	fd = open("/dev/rfkill", O_RDWR);
	if (fd < 0) {
		if (errno == EACCES)
			g_warning ("Could not open RFKILL control device, please verify your installation");
		return fd;
	}

	ret = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (ret < 0) {
		g_debug ("Can't set RFKILL control device to non-blocking");
		close(fd);
		return ret;
	}

	events = NULL;

	while (1) {
		struct rfkill_event event;
		struct rfkill_event *event_ptr;
		ssize_t len;

		len = read(fd, &event, sizeof(event));
		if (len < 0) {
			if (errno == EAGAIN)
				break;
			g_debug ("Reading of RFKILL events failed");
			break;
		}

		if (len != RFKILL_EVENT_SIZE_V1) {
			g_warning ("Wrong size of RFKILL event\n");
			continue;
		}

		if (event.op != RFKILL_OP_ADD)
			continue;

		g_debug ("Read killswitch of type '%s' (idx=%d): soft %d hard %d",
			 type_to_string (event.type),
			 event.idx, event.soft, event.hard);

		event_ptr = g_memdup (&event, sizeof(event));
		events = g_list_prepend (events, event_ptr);
	}

	/* Setup monitoring */
	priv->fd = fd;
	priv->channel = g_io_channel_unix_new (priv->fd);
	priv->watch_id = g_io_add_watch (priv->channel,
					 G_IO_IN | G_IO_HUP | G_IO_ERR,
					 (GIOFunc) event_cb,
					 rfkill);

	events = g_list_reverse (events);
	emit_changed_signal_and_free (rfkill, events);

	return fd;
}

static void
cc_rfkill_glib_finalize (GObject *object)
{
	CcRfkillGlib *rfkill;
	CcRfkillGlibPrivate *priv;

	rfkill = CC_RFKILL_GLIB (object);
	priv = rfkill->priv;

	/* cleanup monitoring */
	if (priv->watch_id > 0) {
		g_source_remove (priv->watch_id);
		priv->watch_id = 0;
		g_io_channel_shutdown (priv->channel, FALSE, NULL);
		g_io_channel_unref (priv->channel);
	}
	close(priv->fd);
	priv->fd = -1;

	G_OBJECT_CLASS(cc_rfkill_glib_parent_class)->finalize(object);
}

static void
cc_rfkill_glib_class_init(CcRfkillGlibClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	g_type_class_add_private(klass, sizeof(CcRfkillGlibPrivate));
	object_class->finalize = cc_rfkill_glib_finalize;

	signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CcRfkillGlibClass, changed),
			      NULL, NULL,
			      NULL,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

}

CcRfkillGlib *
cc_rfkill_glib_new (void)
{
	return CC_RFKILL_GLIB (g_object_new (CC_RFKILL_TYPE_GLIB, NULL));
}
