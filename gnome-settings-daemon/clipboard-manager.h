/*
 * Copyright © 2004 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Matthias Clasen, Red Hat, Inc.
 */
#ifndef CLIPBOARD_MANAGER_H
#define CLIPBOARD_MANAGER_H

#include <X11/Xlib.h>

typedef struct _ClipboardManager  ClipboardManager;
typedef void (*ClipboardTerminateFunc)  (void   *data);
typedef void (*ClipboardWatchFunc)      (Window  window,
				         Bool    is_start,
				         long    mask,
				         void   *cb_data);

typedef void (*ClipboardErrorTrapPushFunc)   (void);
typedef int  (*ClipboardErrorTrapPopFunc)    (void);

ClipboardManager *clipboard_manager_new (Display                      *display,
					 ClipboardErrorTrapPushFunc    error_trap_push_cb,
					 ClipboardErrorTrapPopFunc     error_trap_pop_cb,
					 ClipboardTerminateFunc        terminate_cb,
					 ClipboardWatchFunc            watch_cb,
					 void                         *cb_data);

void              clipboard_manager_destroy       (ClipboardManager       *manager);
Bool              clipboard_manager_process_event (ClipboardManager       *manager,
				                   XEvent                 *xev);
Bool              clipboard_manager_check_running (Display                *display);


#endif /* CLIPBOARD_MANAGER_H */
