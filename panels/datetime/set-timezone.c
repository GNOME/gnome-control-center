/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "set-timezone.h"


static DBusGConnection *
get_system_bus (GError **err)
{
        GError          *error;
        static DBusGConnection *bus = NULL;

	if (bus == NULL) {
        	error = NULL;
        	bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        	if (bus == NULL) {
			g_propagate_error (err, error);
		}
        }

        return bus;
}

#define CACHE_VALIDITY_SEC 2

typedef  void (*CanDoFunc) (gint value);

static void
notify_can_do (DBusGProxy     *proxy,
	       DBusGProxyCall *call,
	       void           *user_data)
{
	CanDoFunc callback = user_data;
	GError *error = NULL;
	gint value;

	if (dbus_g_proxy_end_call (proxy, call,
				   &error,
				   G_TYPE_INT, &value,
				   G_TYPE_INVALID)) {
		callback (value);
	}
}

static void
refresh_can_do (const gchar *action, CanDoFunc callback)
{
        DBusGConnection *bus;
        DBusGProxy      *proxy;

        bus = get_system_bus (NULL);
        if (bus == NULL)
                return;

	proxy = dbus_g_proxy_new_for_name (bus,
					   "org.gnome.SettingsDaemon.DateTimeMechanism",
					   "/",
					   "org.gnome.SettingsDaemon.DateTimeMechanism");

	dbus_g_proxy_begin_call_with_timeout (proxy,
					      action,
					      notify_can_do,
					      callback, NULL,
					      INT_MAX,
					      G_TYPE_INVALID);
}

static gint   settimezone_cache = 0;
static time_t settimezone_stamp = 0;

static void
update_can_settimezone (gint res)
{
	settimezone_cache = res;
	time (&settimezone_stamp);
}

gint
can_set_system_timezone (void)
{
	time_t          now;

	time (&now);
	if (ABS (now - settimezone_stamp) > CACHE_VALIDITY_SEC) {
		refresh_can_do ("CanSetTimezone", update_can_settimezone);
		settimezone_stamp = now;
	}

	return settimezone_cache;
}

static gint   settime_cache = 0;
static time_t settime_stamp = 0;

static void
update_can_settime (gint res)
{
	settime_cache = res;
	time (&settime_stamp);
}

gint
can_set_system_time (void)
{
	time_t now;

	time (&now);
	if (ABS (now - settime_stamp) > CACHE_VALIDITY_SEC) {
		refresh_can_do ("CanSetTime", update_can_settime);
		settime_stamp = now;
	}

	return settime_cache;
}

static gint   usingntp_cache = 0;
static time_t usingntp_stamp = 0;

static void
update_can_usingntp (gint res)
{
	usingntp_cache = res;
	time (&usingntp_stamp);
}

gint
can_set_using_ntp (void)
{
	time_t now;

	time (&now);
	if (ABS (now - usingntp_stamp) > CACHE_VALIDITY_SEC) {
		refresh_can_do ("CanSetUsingNtp", update_can_usingntp);
		settime_stamp = now;
	}

	return usingntp_cache;
}

typedef struct {
	gint ref_count;
        gchar *call;
	gint64 time;
	gchar *tz;
	gboolean using_ntp;
	GFunc callback;
	gpointer data;
	GDestroyNotify notify;
} SetTimeCallbackData;

static void
free_data (gpointer d)
{
	SetTimeCallbackData *data = d;

	data->ref_count--;
	if (data->ref_count == 0) {
		if (data->notify)
			data->notify (data->data);
		g_free (data->tz);
		g_free (data);
	}
}

static void
set_time_notify (DBusGProxy     *proxy,
		 DBusGProxyCall *call,
		 void           *user_data)
{
	SetTimeCallbackData *data = user_data;
	GError *error = NULL;

	if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID)) {
		if (data->callback)
			data->callback (data->data, NULL);
	}
	else {
		if (error->domain == DBUS_GERROR &&
		    error->code == DBUS_GERROR_NO_REPLY) {
			/* these errors happen because dbus doesn't
			 * use monotonic clocks
			 */	
			g_warning ("ignoring no-reply error when setting time");
			g_error_free (error);
			if (data->callback)
				data->callback (data->data, NULL);
		}
		else {
			if (data->callback)
				data->callback (data->data, error);
			else
				g_error_free (error);
		}		
	}
}

static void
set_time_async (SetTimeCallbackData *data)
{
        DBusGConnection *bus;
        DBusGProxy      *proxy;
	GError          *err = NULL;

        bus = get_system_bus (&err);
        if (bus == NULL) {
		if (err) {
			if (data->callback)
				data->callback (data->data, err);
			g_clear_error (&err);
		}
		return;
	}

	proxy = dbus_g_proxy_new_for_name (bus,
					   "org.gnome.SettingsDaemon.DateTimeMechanism",
					   "/",
					   "org.gnome.SettingsDaemon.DateTimeMechanism");

	data->ref_count++;
	if (strcmp (data->call, "SetTime") == 0)
		dbus_g_proxy_begin_call_with_timeout (proxy,
						      "SetTime",
						      set_time_notify,
						      data, free_data,
						      INT_MAX,
						      /* parameters: */
						      G_TYPE_INT64, data->time,
						      G_TYPE_INVALID,
						      /* return values: */
						      G_TYPE_INVALID);
	else if (strcmp (data->call, "SetTimezone") == 0)
		dbus_g_proxy_begin_call_with_timeout (proxy,
						      "SetTimezone",
						      set_time_notify,
						      data, free_data,
						      INT_MAX,
						      /* parameters: */
						      G_TYPE_STRING, data->tz,
						      G_TYPE_INVALID,
						      /* return values: */
						      G_TYPE_INVALID);
	else if (strcmp (data->call, "SetUsingNtp") == 0)
		dbus_g_proxy_begin_call_with_timeout (proxy,
						      "SetUsingNtp",
						      set_time_notify,
						      data, free_data,
						      INT_MAX,
						      /* parameters: */
						      G_TYPE_BOOLEAN, data->using_ntp,
						      G_TYPE_INVALID,
						      /* return values: */
						      G_TYPE_INVALID);
}

void
set_system_time_async (gint64         time,
		       GFunc          callback,
		       gpointer       d,
		       GDestroyNotify notify)
{
	SetTimeCallbackData *data;

	if (time == -1)
		return;

	data = g_new0 (SetTimeCallbackData, 1);
	data->ref_count = 1;
	data->call = "SetTime";
	data->time = time;
	data->tz = NULL;
	data->callback = callback;
	data->data = d;
	data->notify = notify;

	set_time_async (data);
	free_data (data);
}

void
set_system_timezone_async (const gchar    *tz,
			   GFunc           callback,
			   gpointer        d,
			   GDestroyNotify  notify)
{
	SetTimeCallbackData *data;

	g_return_if_fail (tz != NULL);

	data = g_new0 (SetTimeCallbackData, 1);
	data->ref_count = 1;
	data->call = "SetTimezone";
	data->time = -1;
	data->tz = g_strdup (tz);
	data->callback = callback;
	data->data = d;
	data->notify = notify;

	set_time_async (data);
	free_data (data);
}

/* get timezone */

typedef struct
{
  GetTimezoneFunc callback;
  GDestroyNotify notify;

  gpointer data;

} GetTimezoneData;

static void
get_timezone_destroy_notify (GetTimezoneData *data)
{
	if (data->notify && data->data)
		data->notify (data);

	g_free (data);
}

static void
get_timezone_notify (DBusGProxy     *proxy,
		     DBusGProxyCall *call,
		     void           *user_data)
{
	GError *error = NULL;
	gboolean retval;
	gchar *string = NULL;
	GetTimezoneData *data = user_data;

	retval = dbus_g_proxy_end_call (proxy, call, &error,
					G_TYPE_STRING, &string,
					G_TYPE_INVALID);

	if (data->callback) {
		if (!retval) {
			data->callback (data->data, NULL, error);
			g_error_free (error);
		}
		else {
			data->callback (data->data, string, NULL);
			g_free (string);
		}
	}
}

void
get_system_timezone_async (GetTimezoneFunc callback,
			   gpointer        user_data,
			   GDestroyNotify  notify)
{
	DBusGConnection *bus;
	DBusGProxy      *proxy;
	GetTimezoneData *data;
	GError          *error = NULL;

	bus = get_system_bus (&error);
	if (bus == NULL) {
		if (error) {
			if (callback)
				callback (user_data, NULL, error);
			g_clear_error (&error);
		}
		return;

        }

	data = g_new0 (GetTimezoneData, 1);
	data->data = user_data;
	data->notify = notify;
	data->callback = callback;

	proxy = dbus_g_proxy_new_for_name (bus,
					   "org.gnome.SettingsDaemon.DateTimeMechanism",
					   "/",
					   "org.gnome.SettingsDaemon.DateTimeMechanism");

	dbus_g_proxy_begin_call (proxy,
				 "GetTimezone",
				 get_timezone_notify,
				 data,
				 (GDestroyNotify) get_timezone_destroy_notify,
				 /* parameters: */
				 G_TYPE_INVALID,
				 /* return values: */
				 G_TYPE_STRING,
				 G_TYPE_INVALID);

}

gboolean
get_using_ntp (void)
{
	static gboolean can_use_cache = FALSE;
	static gboolean is_using_cache = FALSE;
	static time_t   last_refreshed = 0;
	time_t          now;
        DBusGConnection *bus;
        DBusGProxy      *proxy;

	time (&now);
	if (ABS (now - last_refreshed) > CACHE_VALIDITY_SEC) {
		gboolean cu, iu;
		bus = get_system_bus (NULL);
		if (bus == NULL)
			return FALSE;

		proxy = dbus_g_proxy_new_for_name (bus,
						   "org.gnome.SettingsDaemon.DateTimeMechanism",
						   "/",
						   "org.gnome.SettingsDaemon.DateTimeMechanism");


		if (dbus_g_proxy_call (proxy,
				       "GetUsingNtp",
				       NULL,
				       G_TYPE_INVALID,
				       G_TYPE_BOOLEAN, &cu,
				       G_TYPE_BOOLEAN, &iu,
				       G_TYPE_INVALID)) {
			can_use_cache = cu;
			is_using_cache = iu;
			last_refreshed = now;
		}
	}

	return is_using_cache;
}

void
set_using_ntp_async (gboolean        using_ntp,
	             GFunc           callback,
	             gpointer        d,
	             GDestroyNotify  notify)
{
	SetTimeCallbackData *data;

	data = g_new0 (SetTimeCallbackData, 1);
	data->ref_count = 1;
	data->call = "SetUsingNtp";
	data->time = -1;
	data->using_ntp = using_ntp;
	data->callback = callback;
	data->data = d;
	data->notify = notify;

	set_time_async (data);
	free_data (data);
}
