/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Author: Ray Strode
 * Based on work by Colin Walters
 */

#include "config.h"

#include "um-alarm.h"

#ifdef HAVE_TIMERFD
#include <sys/timerfd.h>
#endif

#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>

typedef struct {
        GSource      *source;
        GInputStream *stream;
} Timer;

typedef struct {
        GSource *source;
} Timeout;

#define MAX_TIMEOUT_INTERVAL (10 * 1000)

typedef enum {
    UM_ALARM_TYPE_UNSCHEDULED,
    UM_ALARM_TYPE_TIMER,
    UM_ALARM_TYPE_TIMEOUT,
} UmAlarmType;

struct _UmAlarmPrivate
{
        GCancellable *cancellable;
        GDateTime    *time;
        GDateTime    *previous_wakeup_time;
        GMainContext *context;
        GSource      *immediate_wakeup_source;

        UmAlarmType type;
        union {
                Timer   timer;
                Timeout timeout;
        };
};

enum {
        FIRED,
        REARMED,
        NUMBER_OF_SIGNALS,
};

static void schedule_wakeups (UmAlarm *self);
static void schedule_wakeups_with_timeout_source (UmAlarm *self);
static guint signals[NUMBER_OF_SIGNALS] = { 0 };

G_DEFINE_TYPE (UmAlarm, um_alarm, G_TYPE_OBJECT);

static void
clear_scheduled_immediate_wakeup (UmAlarm *self)
{
        if (self->priv->immediate_wakeup_source != NULL) {
                g_source_destroy (self->priv->immediate_wakeup_source);
                self->priv->immediate_wakeup_source = NULL;
        }
}

static void
clear_scheduled_timer_wakeups (UmAlarm *self)
{
#ifdef HAVE_TIMERFD
        GError *error;
        gboolean is_closed;

        if (self->priv->timer.stream == NULL) {
                return;
        }

        g_source_destroy (self->priv->timer.source);
        self->priv->timer.source = NULL;

        error = NULL;
        is_closed = g_input_stream_close (self->priv->timer.stream,
                                          NULL,
                                          &error);

        if (!is_closed) {
                g_warning ("UmAlarm: could not close timer stream: %s",
                           error->message);
                g_error_free (error);
        }

        g_object_unref (self->priv->timer.stream);
        self->priv->timer.stream = NULL;

#endif
}

static void
clear_scheduled_timeout_wakeups (UmAlarm *self)
{
        g_source_destroy (self->priv->timeout.source);
        self->priv->timeout.source = NULL;
}

static void
clear_scheduled_wakeups (UmAlarm *self)
{
        clear_scheduled_immediate_wakeup (self);

        switch (self->priv->type) {
                case UM_ALARM_TYPE_TIMER:
                        clear_scheduled_timer_wakeups (self);
                        break;

                case UM_ALARM_TYPE_TIMEOUT:
                        clear_scheduled_timeout_wakeups (self);
                        break;

                default:
                        break;
        }

        if (self->priv->cancellable != NULL) {
                g_object_unref (self->priv->cancellable);
                self->priv->cancellable = NULL;
        }

        if (self->priv->context != NULL) {
                g_main_context_unref (self->priv->context);
                self->priv->context = NULL;
        }

        if (self->priv->previous_wakeup_time != NULL) {
                g_date_time_unref (self->priv->previous_wakeup_time);
                self->priv->previous_wakeup_time = NULL;
        }

        self->priv->type = UM_ALARM_TYPE_UNSCHEDULED;
}

static void
um_alarm_finalize (GObject *object)
{
        UmAlarm *self = UM_ALARM (object);

        if (self->priv->cancellable != NULL &&
            !g_cancellable_is_cancelled (self->priv->cancellable)) {
                g_cancellable_cancel (self->priv->cancellable);
        }

        clear_scheduled_wakeups (self);

        if (self->priv->time != NULL) {
                g_date_time_unref (self->priv->time);
        }

        if (self->priv->previous_wakeup_time != NULL) {
                g_date_time_unref (self->priv->previous_wakeup_time);
        }

        G_OBJECT_CLASS (um_alarm_parent_class)->finalize (object);
}

static void
um_alarm_class_init (UmAlarmClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = um_alarm_finalize;

        g_type_class_add_private (klass, sizeof (UmAlarmPrivate));

        signals[FIRED] = g_signal_new ("fired",
                                       G_TYPE_FROM_CLASS (klass),
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL, NULL, NULL,
                                       G_TYPE_NONE, 0);

        signals[REARMED] = g_signal_new ("rearmed",
                                         G_TYPE_FROM_CLASS (klass),
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL, NULL, NULL,
                                         G_TYPE_NONE, 0);
}

static void
um_alarm_init (UmAlarm *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                  UM_TYPE_ALARM,
                                                  UmAlarmPrivate);
        self->priv->type = UM_ALARM_TYPE_UNSCHEDULED;
}

static void
on_cancelled (GCancellable *cancellable,
              gpointer      user_data)
{
        UmAlarm *self = UM_ALARM (user_data);

        clear_scheduled_wakeups (self);
}

static void
fire_alarm (UmAlarm *self)
{
        g_signal_emit (G_OBJECT (self), signals[FIRED], 0);
}

static void
rearm_alarm (UmAlarm *self)
{
        g_signal_emit (G_OBJECT (self), signals[REARMED], 0);
}

static void
fire_or_rearm_alarm (UmAlarm *self)
{
        GTimeSpan  time_until_fire;
        GTimeSpan  previous_time_until_fire;
        GDateTime *now;

        now = g_date_time_new_now_local ();
        time_until_fire = g_date_time_difference (self->priv->time, now);

        if (self->priv->previous_wakeup_time == NULL) {
                self->priv->previous_wakeup_time = now;

                /* If, according to the time, we're past when we should have fired,
                 * then fire the alarm.
                 */
                if (time_until_fire <= 0) {
                        fire_alarm (self);
                }
        } else {
                previous_time_until_fire = g_date_time_difference (self->priv->time,
                                                                   self->priv->previous_wakeup_time);

                g_date_time_unref (self->priv->previous_wakeup_time);
                self->priv->previous_wakeup_time = now;

                /* If, according to the time, we're past when we should have fired,
                 * and this is the first wakeup where that's been true then fire
                 * the alarm. The first check makes sure we don't fire prematurely,
                 * and the second check makes sure we don't fire more than once
                 */
                if (time_until_fire <= 0 && previous_time_until_fire > 0) {
                        fire_alarm (self);

                /* If, according to the time, we're before when we should fire,
                 * and we previously fired the alarm, then we've jumped back in
                 * time and need to rearm the alarm.
                 */
                } else if (time_until_fire > 0 && previous_time_until_fire <= 0) {
                        rearm_alarm (self);
                }
        }
}

static gboolean
on_immediate_wakeup_source_ready (gpointer user_data)
{
        UmAlarm *self = UM_ALARM (user_data);

        fire_or_rearm_alarm (self);

        return FALSE;
}

#ifdef HAVE_TIMERFD
static gboolean
on_timer_source_ready (GObject  *stream,
                       gpointer  user_data)
{
        UmAlarm *self = UM_ALARM (user_data);
        gint64 number_of_fires;
        gssize bytes_read;

        bytes_read = g_pollable_input_stream_read_nonblocking (G_POLLABLE_INPUT_STREAM (stream),
                                                               &number_of_fires,
                                                               sizeof (gint64),
                                                               NULL,
                                                               NULL);

        if (bytes_read == sizeof (gint64)) {
                if (number_of_fires < 0 || number_of_fires > 1) {
                        g_warning ("UmAlarm: expected timerfd to report firing once,"
                                   "but it reported firing %ld times\n",
                                   (long) number_of_fires);
                }
        }

        fire_or_rearm_alarm (self);
        return TRUE;
}
#endif

static gboolean
schedule_wakeups_with_timerfd (UmAlarm *self)
{
#ifdef HAVE_TIMERFD
        struct itimerspec timer_spec;
        int fd;
        int result;

        g_debug ("UmAlarm: trying to use kernel timer");

        fd = timerfd_create (CLOCK_REALTIME, TFD_CLOEXEC | TFD_NONBLOCK);

        if (fd < 0) {
                g_debug ("UmAlarm: could not create timer fd: %m");
                return FALSE;
        }

        memset (&timer_spec, 0, sizeof (timer_spec));
        timer_spec.it_value.tv_sec = g_date_time_to_unix (self->priv->time) + 1;

        result = timerfd_settime (fd,
                                  TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET,
                                  &timer_spec,
                                  NULL);

        if (result < 0) {
                g_debug ("UmAlarm: could not set timer: %m");
                return FALSE;
        }

        self->priv->type = UM_ALARM_TYPE_TIMER;
        self->priv->timer.stream = g_unix_input_stream_new (fd, TRUE);

        self->priv->timer.source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (self->priv->timer.stream),
                                                                          self->priv->cancellable);
        g_source_set_callback (self->priv->timer.source,
                               (GSourceFunc)
                               on_timer_source_ready,
                               self,
                               NULL);
        g_source_attach (self->priv->timer.source,
                         self->priv->context);

        return TRUE;

#endif /* HAVE_TIMERFD */

    return FALSE;
}

static gboolean
on_timeout_source_ready (gpointer user_data)
{
        UmAlarm *self = UM_ALARM (user_data);

        fire_or_rearm_alarm (self);

        schedule_wakeups_with_timeout_source (self);

        return FALSE;
}

static void
schedule_wakeups_with_timeout_source (UmAlarm *self)
{
        GDateTime *now;
        GTimeSpan time_span;
        guint interval;
        self->priv->type = UM_ALARM_TYPE_TIMEOUT;

        now = g_date_time_new_now_local ();
        time_span = g_date_time_difference (self->priv->time, now);
        g_date_time_unref (now);

        time_span = CLAMP (time_span, 1000 * G_TIME_SPAN_MILLISECOND, G_MAXUINT * G_TIME_SPAN_MILLISECOND);
        interval = time_span / G_TIME_SPAN_MILLISECOND;

        /* We poll every 10 seconds or so because we want to catch time skew
         */
        interval = MIN (interval, MAX_TIMEOUT_INTERVAL);

        self->priv->timeout.source = g_timeout_source_new (interval);
        g_source_set_callback (self->priv->timeout.source,
                               (GSourceFunc)
                               on_timeout_source_ready,
                               self,
                               NULL);

        g_source_attach (self->priv->timeout.source,
                         self->priv->context);
}

static void
schedule_wakeups (UmAlarm *self)
{
        gboolean wakeup_scheduled;

        wakeup_scheduled = schedule_wakeups_with_timerfd (self);

        if (!wakeup_scheduled) {
                g_debug ("UmAlarm: falling back to polling timeout\n");
                schedule_wakeups_with_timeout_source (self);
        }
}

static void
schedule_immediate_wakeup (UmAlarm *self)
{
        self->priv->immediate_wakeup_source = g_idle_source_new ();

        g_source_set_callback (self->priv->immediate_wakeup_source,
                               (GSourceFunc)
                               on_immediate_wakeup_source_ready,
                               self,
                               NULL);

        g_source_attach (self->priv->immediate_wakeup_source,
                         self->priv->context);
}

void
um_alarm_set (UmAlarm      *self,
              GDateTime    *time,
              GCancellable *cancellable)
{
        if (self->priv->cancellable != NULL) {
                if (!g_cancellable_is_cancelled (self->priv->cancellable)) {
                        g_cancellable_cancel (cancellable);
                }

                if (self->priv->cancellable != NULL) {
                    g_object_unref (self->priv->cancellable);
                    self->priv->cancellable = NULL;
                }
        }

        if (cancellable == NULL) {
                self->priv->cancellable = g_cancellable_new ();
        } else {
                self->priv->cancellable = g_object_ref (cancellable);
        }

        g_cancellable_connect (self->priv->cancellable,
                               G_CALLBACK (on_cancelled),
                               self,
                               NULL);
        self->priv->time = g_date_time_ref (time);
        self->priv->context = g_main_context_ref (g_main_context_default ());

        schedule_wakeups (self);

        /* Wake up right away, in case it's already expired leaving the gate */
        schedule_immediate_wakeup (self);
}

UmAlarm *
um_alarm_new (void)
{
        UmAlarm *self;

        self = UM_ALARM (g_object_new (UM_TYPE_ALARM, NULL));

        return UM_ALARM (self);
}
