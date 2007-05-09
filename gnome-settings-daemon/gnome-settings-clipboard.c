/*
 * Copyright (C) 2007 The GNOME Foundation
 *
 * Authors: Matthias Clasen
 *          Anders Carlsson
 *          Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "gnome-settings-module.h"
#include "xutils.h"
#include "list.h"

typedef struct {
	GnomeSettingsModule parent;

	Display *display;
	Window window;
	Time timestamp;

	List *contents;
	List *conversions;

	Window requestor;
	Atom property;
	Time time;
} GnomeSettingsModuleClipboard;

typedef struct {
	GnomeSettingsModuleClass parent_class;
} GnomeSettingsModuleClipboardClass;

typedef struct
{
	unsigned char *data;
	int length;
	Atom target;
	Atom type;
	int format;
	int refcount;
} TargetData;

typedef struct
{
	Atom target;
	TargetData *data;
	Atom property;
	Window requestor;
	int offset;
} IncrConversion;

static GnomeSettingsModuleRunlevel gnome_settings_module_clipboard_get_runlevel (GnomeSettingsModule *module);
static gboolean gnome_settings_module_clipboard_initialize (GnomeSettingsModule *module, GConfClient *client);
static gboolean gnome_settings_module_clipboard_start (GnomeSettingsModule *module);
static gboolean gnome_settings_module_clipboard_stop (GnomeSettingsModule *module);

static void clipboard_manager_watch_cb (Window window, Bool is_start, long mask, void *cb_data);
static Bool clipboard_manager_process_event (GnomeSettingsModuleClipboard *module_cp, XEvent *xev);
static GdkFilterReturn clipboard_manager_event_filter (GdkXEvent *xevent, GdkEvent  *event, gpointer data);

static GnomeSettingsModuleClipboard *module_clipboard_instance = NULL;

static void
gnome_settings_module_clipboard_class_init (GnomeSettingsModuleClipboardClass *klass)
{
	GnomeSettingsModuleClass *module_class;

	module_class = GNOME_SETTINGS_MODULE_CLASS (klass);
	module_class->get_runlevel = gnome_settings_module_clipboard_get_runlevel;
	module_class->initialize = gnome_settings_module_clipboard_initialize;
	module_class->start = gnome_settings_module_clipboard_start;
	module_class->stop = gnome_settings_module_clipboard_stop;
}

static void
gnome_settings_module_clipboard_init (GnomeSettingsModuleClipboard *module)
{
	module_clipboard_instance = module;
	module->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
}

/* We need to use reference counting for the target data, since we may 
 * need to keep the data around after loosing the CLIPBOARD ownership
 * to complete incremental transfers. 
 */
static TargetData *
target_data_ref (TargetData *data)
{
	data->refcount++;
	return data;
}

static void
target_data_unref (TargetData *data)
{
	data->refcount--;
	if (data->refcount == 0) {
		free (data->data);
		free (data);
	}
}

static void
conversion_free (IncrConversion *rdata)
{
	if (rdata->data)
		target_data_unref (rdata->data);
	free (rdata);
}

static void
send_selection_notify (GnomeSettingsModuleClipboard *module_cp, Bool success)
{
	XSelectionEvent notify;

	notify.type = SelectionNotify;
	notify.serial = 0;
	notify.send_event = True;
	notify.display = module_cp->display;
	notify.requestor = module_cp->requestor;
	notify.selection = XA_CLIPBOARD_MANAGER;
	notify.target = XA_SAVE_TARGETS;
	notify.property = success ? module_cp->property : None;
	notify.time = module_cp->time;

	gdk_error_trap_push ();

	XSendEvent (module_cp->display, module_cp->requestor,
		    False, NoEventMask, (XEvent *)&notify);
	XSync (module_cp->display, False);
  
	gdk_error_trap_pop ();
}

static void
finish_selection_request (GnomeSettingsModuleClipboard *module_cp, XEvent *xev, Bool success)
{
	XSelectionEvent notify;

	notify.type = SelectionNotify;
	notify.serial = 0;
	notify.send_event = True;
	notify.display = xev->xselectionrequest.display;
	notify.requestor = xev->xselectionrequest.requestor;
	notify.selection = xev->xselectionrequest.selection;
	notify.target = xev->xselectionrequest.target;
	notify.property = success ? xev->xselectionrequest.property : None;
	notify.time = xev->xselectionrequest.time;

	gdk_error_trap_push ();

	XSendEvent (xev->xselectionrequest.display, 
		    xev->xselectionrequest.requestor,
		    False, NoEventMask, (XEvent *) &notify);
	XSync (module_cp->display, False);

	gdk_error_trap_pop ();
}

static int 
clipboard_bytes_per_item (int format)
{
	switch (format) {
	case 8: return sizeof (char);
	case 16: return sizeof (short);
	case 32: return sizeof (long);
	default: ;
	}

	return 0;
}

static void
save_targets (GnomeSettingsModuleClipboard *module_cp, Atom *save_targets, int nitems)
{
	int nout, i;
	Atom *multiple;
	TargetData *tdata;

	multiple = (Atom *) malloc (2 * nitems * sizeof (Atom));

	nout = 0;
	for (i = 0; i < nitems; i++) {
		if (save_targets[i] != XA_TARGETS &&
		    save_targets[i] != XA_MULTIPLE &&
		    save_targets[i] != XA_DELETE &&
		    save_targets[i] != XA_INSERT_PROPERTY &&
		    save_targets[i] != XA_INSERT_SELECTION &&
		    save_targets[i] != XA_PIXMAP) {
			tdata = (TargetData *) malloc (sizeof (TargetData));
			tdata->data = NULL;
			tdata->length = 0;
			tdata->target = save_targets[i];
			tdata->type = None;
			tdata->format = 0;
			tdata->refcount = 1;
			module_cp->contents = list_prepend (module_cp->contents, tdata);
	  
			multiple[nout++] = save_targets[i];
			multiple[nout++] = save_targets[i];
		}
	}

	XFree (save_targets);
  
	XChangeProperty (module_cp->display, module_cp->window,
			 XA_MULTIPLE, XA_ATOM_PAIR,
			 32, PropModeReplace, (const unsigned char *) multiple, nout);
	free (multiple);

	XConvertSelection (module_cp->display, XA_CLIPBOARD,
			   XA_MULTIPLE, XA_MULTIPLE,
			   module_cp->window, module_cp->time);
}

static int
find_content_target (TargetData *tdata,
		     Atom        target)
{
	return tdata->target == target;
}

static int
find_content_type (TargetData *tdata,
		   Atom        type)
{
	return tdata->type == type;
}

static int
find_conversion_requestor (IncrConversion *rdata,
			   XEvent         *xev)
{
	return (rdata->requestor == xev->xproperty.window &&
		rdata->property == xev->xproperty.atom);
}

static void
get_property (TargetData *tdata, GnomeSettingsModuleClipboard *module_cp)
{
	Atom type;
	int format;
	unsigned long length;
	unsigned long remaining;
	unsigned char *data;
	
	XGetWindowProperty (module_cp->display, 
			    module_cp->window,
			    tdata->target,
			    0, 0x1FFFFFFF, True, AnyPropertyType,
			    &type, &format, &length, &remaining, 
			    &data);

	if (type == None) {
		module_cp->contents = list_remove (module_cp->contents, tdata);
		free (tdata);
	} else if (type == XA_INCR) {
		tdata->type = type;
		tdata->length = 0;
		XFree (data);
	} else {
		tdata->type = type;
		tdata->data = data;
		tdata->length = length * clipboard_bytes_per_item (format);
		tdata->format = format;
	}
}

static Bool
receive_incrementally (GnomeSettingsModuleClipboard *module_cp, XEvent *xev)
{
	List *list;
	TargetData *tdata;
	Atom type;
	int format;
	unsigned long length, nitems, remaining;
	unsigned char *data;

	if (xev->xproperty.window != module_cp->window)
		return False;

	list = list_find (module_cp->contents,
			  (ListFindFunc) find_content_target, (void *) xev->xproperty.atom);
  
	if (!list)
		return False;
  
	tdata = (TargetData *) list->data;

	if (tdata->type != XA_INCR)
		return False;

	XGetWindowProperty (xev->xproperty.display,
			    xev->xproperty.window,
			    xev->xproperty.atom,
			    0, 0x1FFFFFFF, True, AnyPropertyType,
			    &type, &format, &nitems, &remaining, &data);

	length = nitems * clipboard_bytes_per_item (format);
	if (length == 0) {
		tdata->type = type;
		tdata->format = format;
      
		if (!list_find (module_cp->contents, 
				(ListFindFunc) find_content_type, (void *)XA_INCR)) {
			/* all incremental transfers done */
			send_selection_notify (module_cp, True);
			module_cp->requestor = None;
		}

		XFree (data);
	} else {
		if (!tdata->data) {
			tdata->data = data;
			tdata->length = length;
		} else {
			tdata->data = realloc (tdata->data, tdata->length + length + 1);
			memcpy (tdata->data + tdata->length, data, length + 1);
			tdata->length += length;
			XFree (data);
		}
	}

	return True;
}

static Bool
send_incrementally (GnomeSettingsModuleClipboard *module_cp, XEvent *xev)
{
	List *list;
	IncrConversion *rdata;
	unsigned long length, items;
	unsigned char *data;
	
	list = list_find (module_cp->conversions, 
			  (ListFindFunc) find_conversion_requestor, xev);
	if (list == NULL) 
		return False;
  
	rdata = (IncrConversion *) list->data;
  
	data = rdata->data->data + rdata->offset; 
	length = rdata->data->length - rdata->offset;
	if (length > SELECTION_MAX_SIZE)
		length = SELECTION_MAX_SIZE;
  
	rdata->offset += length;
  
	items = length / clipboard_bytes_per_item (rdata->data->format);
	XChangeProperty (module_cp->display, rdata->requestor,
			 rdata->property, rdata->data->type,
			 rdata->data->format, PropModeAppend,
			 data, items);
  
	if (length == 0) {
		module_cp->conversions = list_remove (module_cp->conversions, rdata);
		conversion_free (rdata);
	}

	return True;
}

static void
convert_clipboard_manager (GnomeSettingsModuleClipboard *module_cp, XEvent *xev)
{
	Atom type = None;
	int format;
	unsigned long nitems, remaining;
	Atom *targets = NULL;

	if (xev->xselectionrequest.target == XA_SAVE_TARGETS) {
		if (module_cp->requestor != None || module_cp->contents != NULL) {
			/* We're in the middle of a conversion request, or own 
			 * the CLIPBOARD already 
			 */
			finish_selection_request (module_cp, xev, False);
		} else {
			gdk_error_trap_push ();
	  
			clipboard_manager_watch_cb (xev->xselectionrequest.requestor, True,
						    StructureNotifyMask, NULL);
			XSelectInput (module_cp->display, xev->xselectionrequest.requestor,
				      StructureNotifyMask);
			XSync (module_cp->display, False);
	  
			if (gdk_error_trap_pop () != Success)
				return;

			gdk_error_trap_push ();
	  
			if (xev->xselectionrequest.property != None) {
				XGetWindowProperty (module_cp->display, xev->xselectionrequest.requestor,
						    xev->xselectionrequest.property,
						    0, 0x1FFFFFFF, False, XA_ATOM,
						    &type, &format, &nitems, &remaining,
						    (unsigned char **) &targets);
	      
				if (gdk_error_trap_pop () != Success) {
					if (targets)
						XFree (targets);
		  
					return;
				}
			}
	  
			module_cp->requestor = xev->xselectionrequest.requestor;
			module_cp->property = xev->xselectionrequest.property;
			module_cp->time = xev->xselectionrequest.time;

			if (type == None)
				XConvertSelection (module_cp->display, XA_CLIPBOARD,
						   XA_TARGETS, XA_TARGETS,
						   module_cp->window, module_cp->time);
			else
				save_targets (module_cp, targets, nitems);
		}
	} else if (xev->xselectionrequest.target == XA_TIMESTAMP) {
		XChangeProperty (module_cp->display,
				 xev->xselectionrequest.requestor,
				 xev->xselectionrequest.property,
				 XA_INTEGER, 32, PropModeReplace,
				 (unsigned char *) &module_cp->timestamp, 1);
      
		finish_selection_request (module_cp, xev, True);
	} else if (xev->xselectionrequest.target == XA_TARGETS) {
		int n_targets = 0;
		Atom targets[3];
      
		targets[n_targets++] = XA_TARGETS;
		targets[n_targets++] = XA_TIMESTAMP;
		targets[n_targets++] = XA_SAVE_TARGETS;
      
		XChangeProperty (module_cp->display,
				 xev->xselectionrequest.requestor,
				 xev->xselectionrequest.property,
				 XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) targets, n_targets);
      
		finish_selection_request (module_cp, xev, True);
	} else
		finish_selection_request (module_cp, xev, False);
}

static void
convert_clipboard_target (IncrConversion *rdata, GnomeSettingsModuleClipboard *module_cp)
{
	TargetData *tdata;
	Atom *targets;
	int n_targets;
	List *list;
	unsigned long items;
	XWindowAttributes atts;				
	  
	if (rdata->target == XA_TARGETS) {
		n_targets = list_length (module_cp->contents) + 2;
		targets = (Atom *) malloc (n_targets * sizeof (Atom));
      
		n_targets = 0;
      
		targets[n_targets++] = XA_TARGETS;
		targets[n_targets++] = XA_MULTIPLE;
      
		for (list = module_cp->contents; list; list = list->next) {
			tdata = (TargetData *) list->data;
			targets[n_targets++] = tdata->target;
		}
      
		XChangeProperty (module_cp->display, rdata->requestor,
				 rdata->property,
				 XA_ATOM, 32, PropModeReplace,
				 (unsigned char *) targets, n_targets);
		free (targets);
	} else  {
		/* Convert from stored CLIPBOARD data */
		list = list_find (module_cp->contents, 
				  (ListFindFunc) find_content_target, (void *) rdata->target);

		/* We got a target that we don't support */
		if (!list)
			return;
      
		tdata = (TargetData *)list->data;
		if (tdata->type == XA_INCR) {
			/* we haven't completely received this target yet  */
			rdata->property = None;
			return;
		}

		rdata->data = target_data_ref (tdata);
		items = tdata->length / clipboard_bytes_per_item (tdata->format);
		if (tdata->length <= SELECTION_MAX_SIZE)
			XChangeProperty (module_cp->display, rdata->requestor,
					 rdata->property,
					 tdata->type, tdata->format, PropModeReplace,
					 tdata->data, items);
		else {
			/* start incremental transfer */
			rdata->offset = 0;

			gdk_error_trap_push ();
	  
			XGetWindowAttributes (module_cp->display, rdata->requestor, &atts);
			XSelectInput (module_cp->display, rdata->requestor, 
				      atts.your_event_mask | PropertyChangeMask);
	  
			XChangeProperty (module_cp->display, rdata->requestor,
					 rdata->property,
					 XA_INCR, 32, PropModeReplace,
					 (unsigned char *) &items, 1);

			XSync (module_cp->display, False);
  
			gdk_error_trap_pop ();
		}
	}
}

static void
collect_incremental (IncrConversion *rdata, GnomeSettingsModuleClipboard *module_cp)
{
	if (rdata->offset >= 0)
		module_cp->conversions = list_prepend (module_cp->conversions, rdata);
	else {
		if (rdata->data) {
			target_data_unref (rdata->data);
			rdata->data = NULL;
		}
		free (rdata);
	}
}

static void
convert_clipboard (GnomeSettingsModuleClipboard *module_cp, XEvent *xev)
{
	List *list, *conversions;
	IncrConversion *rdata;
	Atom type;
	int i, format;
	unsigned long nitems, remaining;
	Atom *multiple;
    
	conversions = NULL;
	type = None;
    
	if (xev->xselectionrequest.target == XA_MULTIPLE) {
		XGetWindowProperty (xev->xselectionrequest.display,
				    xev->xselectionrequest.requestor,
				    xev->xselectionrequest.property,
				    0, 0x1FFFFFFF, False, XA_ATOM_PAIR,
				    &type, &format, &nitems, &remaining,
				    (unsigned char **) &multiple);
	
		if (type != XA_ATOM_PAIR)
			return;
	
		for (i = 0; i < nitems; i += 2) {
			rdata = (IncrConversion *) malloc (sizeof (IncrConversion));
			rdata->requestor = xev->xselectionrequest.requestor;
			rdata->target = multiple[i];
			rdata->property = multiple[i+1];
			rdata->data = NULL;
			rdata->offset = -1;
			conversions = list_prepend (conversions, rdata);
		}
	} else {
		multiple = NULL;
	
		rdata = (IncrConversion *) malloc (sizeof (IncrConversion));
		rdata->requestor = xev->xselectionrequest.requestor;
		rdata->target = xev->xselectionrequest.target;
		rdata->property = xev->xselectionrequest.property;
		rdata->data = NULL;
		rdata->offset = -1;
		conversions = list_prepend (conversions, rdata);
	}

	list_foreach (conversions, (Callback) convert_clipboard_target, module_cp);
    
	if (conversions->next == NULL &&
	    ((IncrConversion *) conversions->data)->property == None) {
		finish_selection_request (module_cp, xev, False);
	} else {
		if (multiple) {
			i = 0;
			for (list = conversions; list; list = list->next) {
				rdata = (IncrConversion *)list->data;
				multiple[i++] = rdata->target;
				multiple[i++] = rdata->property;
			}
			XChangeProperty (xev->xselectionrequest.display,
					 xev->xselectionrequest.requestor,
					 xev->xselectionrequest.property,
					 XA_ATOM_PAIR, 32, PropModeReplace,
					 (unsigned char *) multiple, nitems);
		}
		finish_selection_request (module_cp, xev, True);
	}
    
	list_foreach (conversions, (Callback) collect_incremental, module_cp);
	list_free (conversions);
    
	if (multiple)
		free (multiple);
}

static Bool
clipboard_manager_process_event (GnomeSettingsModuleClipboard *module_cp, XEvent *xev)
{
	Atom type;
	int format;
	unsigned long nitems;
	unsigned long remaining;
	Atom *targets;
  
	targets = NULL;

	switch (xev->xany.type) {
	case DestroyNotify:
		if (xev->xdestroywindow.window == module_cp->requestor) {
			list_foreach (module_cp->contents, (Callback)target_data_unref, NULL);
			list_free (module_cp->contents);
			module_cp->contents = NULL;

			clipboard_manager_watch_cb (module_cp->requestor, False, 0, NULL);
			module_cp->requestor = None;
		}
		break;
	case PropertyNotify:
		if (xev->xproperty.state == PropertyNewValue)
			return receive_incrementally (module_cp, xev);
		else 
			return send_incrementally (module_cp, xev);
      
	case SelectionClear:
		if (xev->xany.window != module_cp->window)
			return False;

		if (xev->xselectionclear.selection == XA_CLIPBOARD_MANAGER) {
			/* We lost the manager selection */
			if (module_cp->contents) {
				list_foreach (module_cp->contents, (Callback)target_data_unref, NULL);
				list_free (module_cp->contents);
				module_cp->contents = NULL;
	      
				XSetSelectionOwner (module_cp->display, 
						    XA_CLIPBOARD,
						    None, module_cp->time);
			}

			return True;
		}
		if (xev->xselectionclear.selection == XA_CLIPBOARD) {
			/* We lost the clipboard selection */
			list_foreach (module_cp->contents, (Callback)target_data_unref, NULL);
			list_free (module_cp->contents);
			module_cp->contents = NULL;
			clipboard_manager_watch_cb (module_cp->requestor, False, 0, NULL);
			module_cp->requestor = None;
	  
			return True;
		}
		break;

	case SelectionNotify:
		if (xev->xany.window != module_cp->window)
			return False;

		if (xev->xselection.selection == XA_CLIPBOARD) {
			/* a CLIPBOARD conversion is done */
			if (xev->xselection.property == XA_TARGETS) {
				XGetWindowProperty (xev->xselection.display,
						    xev->xselection.requestor,
						    xev->xselection.property,
						    0, 0x1FFFFFFF, True, XA_ATOM,
						    &type, &format, &nitems, &remaining,
						    (unsigned char **) &targets);
	      
				save_targets (module_cp, targets, nitems);
			}
			else if (xev->xselection.property == XA_MULTIPLE) {
				List *tmp;

				tmp = list_copy (module_cp->contents);
				list_foreach (tmp, (Callback) get_property, module_cp);
				list_free (tmp);
	      
				module_cp->time = xev->xselection.time;
				XSetSelectionOwner (module_cp->display, XA_CLIPBOARD,
						    module_cp->window, module_cp->time);

				if (module_cp->property != None)
					XChangeProperty (module_cp->display, module_cp->requestor,
							 module_cp->property,
							 XA_ATOM, 32, PropModeReplace,
							 (unsigned char *)&XA_NULL, 1);

				if (!list_find (module_cp->contents, 
						(ListFindFunc)find_content_type, (void *)XA_INCR)) {
					/* all transfers done */
					send_selection_notify (module_cp, True);
					clipboard_manager_watch_cb (module_cp->requestor, False, 0, NULL);
					module_cp->requestor = None;
				}
			}
			else if (xev->xselection.property == None) {
				send_selection_notify (module_cp, False);
				clipboard_manager_watch_cb (module_cp->requestor, False, 0, NULL);
				module_cp->requestor = None;
			}

			return True;
		}
		break;

	case SelectionRequest:
		if (xev->xany.window != module_cp->window)
			return False;

		if (xev->xselectionrequest.selection == XA_CLIPBOARD_MANAGER) {
			convert_clipboard_manager (module_cp, xev);
			return True;
		}
		else if (xev->xselectionrequest.selection == XA_CLIPBOARD) {
			convert_clipboard (module_cp, xev);
			return True;
		}
		break;

	default: ;
	}
     
	return False;
}

static GdkFilterReturn 
clipboard_manager_event_filter (GdkXEvent *xevent,
				GdkEvent  *event,
				gpointer   data)
{
	if (clipboard_manager_process_event (module_clipboard_instance,
					     (XEvent *)xevent))
		return GDK_FILTER_REMOVE;
	else
		return GDK_FILTER_CONTINUE;
}

static void
clipboard_manager_watch_cb (Window  window,
			    Bool    is_start,
			    long    mask,
			    void   *cb_data)
{
	GdkWindow *gdkwin;
	GdkDisplay *display;

	display = gdk_display_get_default ();
	gdkwin = gdk_window_lookup_for_display (display, window);

	if (is_start) {
		if (!gdkwin)
			gdkwin = gdk_window_foreign_new_for_display (display, window);
		else
			g_object_ref (gdkwin);
      
		gdk_window_add_filter (gdkwin, clipboard_manager_event_filter, NULL);
	} else {
		if (!gdkwin)
			return;
		gdk_window_remove_filter (gdkwin, clipboard_manager_event_filter, NULL);
		g_object_unref (gdkwin);
	}
}

GType
gnome_settings_module_clipboard_get_type (void)
{
	static GType module_type = 0;
  
	if (!module_type) {
		static const GTypeInfo module_info = {
			sizeof (GnomeSettingsModuleClipboardClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) gnome_settings_module_clipboard_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GnomeSettingsModuleClipboard),
			0,		/* n_preallocs */
			(GInstanceInitFunc) gnome_settings_module_clipboard_init,
		};
      
		module_type = g_type_register_static (GNOME_SETTINGS_TYPE_MODULE,
						      "GnomeSettingsModuleClipboard",
						      &module_info, 0);
	}
  
	return module_type;
}

static GnomeSettingsModuleRunlevel
gnome_settings_module_clipboard_get_runlevel (GnomeSettingsModule *module)
{
	return GNOME_SETTINGS_MODULE_RUNLEVEL_CORE_SERVICES;
}

static gboolean
gnome_settings_module_clipboard_initialize (GnomeSettingsModule *module, GConfClient *config_client)
{
	GnomeSettingsModuleClipboard *module_cp = (GnomeSettingsModuleClipboard *) module;

	init_atoms (module_cp->display);

	/* check if there is a clipboard manager running */
	if (XGetSelectionOwner (module_cp->display, XA_CLIPBOARD_MANAGER))
		return FALSE;

	module_cp->contents = NULL;
	module_cp->conversions = NULL;
	module_cp->requestor = None;
	
	return TRUE;
}

static gboolean
gnome_settings_module_clipboard_start (GnomeSettingsModule *module)
{
	XClientMessageEvent xev;
	GnomeSettingsModuleClipboard *module_cp = (GnomeSettingsModuleClipboard *) module;

	module_cp->window = XCreateSimpleWindow (module_cp->display,
						 DefaultRootWindow (module_cp->display),
						 0, 0, 10, 10, 0,
						 WhitePixel (module_cp->display, DefaultScreen (module_cp->display)),
						 WhitePixel (module_cp->display, DefaultScreen (module_cp->display)));
	clipboard_manager_watch_cb (module_cp->window, True, PropertyChangeMask, NULL);
	XSelectInput (module_cp->display, module_cp->window, PropertyChangeMask);
	module_cp->timestamp = get_server_time (module_cp->display, module_cp->window);

	XSetSelectionOwner (module_cp->display, XA_CLIPBOARD_MANAGER, module_cp->window, module_cp->timestamp);

	/* Check to see if we managed to claim the selection. If not,
	 * we treat it as if we got it then immediately lost it
	 */
	if (XGetSelectionOwner (module_cp->display, XA_CLIPBOARD_MANAGER) == module_cp->window) {
		xev.type = ClientMessage;
		xev.window = DefaultRootWindow (module_cp->display);
		xev.message_type = XA_MANAGER;
		xev.format = 32;
		xev.data.l[0] = module_cp->timestamp;
		xev.data.l[1] = XA_CLIPBOARD_MANAGER;
		xev.data.l[2] = module_cp->window;
		xev.data.l[3] = 0;	/* manager specific data */
		xev.data.l[4] = 0;	/* manager specific data */

		XSendEvent (module_cp->display, DefaultRootWindow (module_cp->display),
			    False, StructureNotifyMask, (XEvent *)&xev);
	} else {
		clipboard_manager_watch_cb (module_cp->window, False, 0, NULL);
		/* FIXME: module_cp->terminate (module_cp->cb_data); */

		return FALSE;
	}

	return TRUE;
}

static gboolean
gnome_settings_module_clipboard_stop (GnomeSettingsModule *module)
{
	GnomeSettingsModuleClipboard *module_cp = (GnomeSettingsModuleClipboard *) module;

	clipboard_manager_watch_cb (module_cp->window, FALSE, 0, NULL);
	XDestroyWindow (module_cp->display, module_cp->window);

	list_foreach (module_cp->conversions, (Callback) conversion_free, NULL);
	list_free (module_cp->conversions);

	list_foreach (module_cp->contents, (Callback) target_data_unref, NULL);
	list_free (module_cp->contents);

	return TRUE;
}
