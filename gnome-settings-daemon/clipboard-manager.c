/*
 * Copyright © 2004 Red Hat, Inc.
 * Copyright © 2004 Nokia Corporation
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
 * Authors:  Matthias Clasen, Red Hat, Inc.
 *           Anders Carlsson, Imendio AB
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include "clipboard-manager.h"
#include "xutils.h"
#include "list.h"


struct _ClipboardManager
{
  Display *display;
  Window window;
  Time timestamp;

  ClipboardTerminateFunc terminate;
  ClipboardWatchFunc watch;
  void *cb_data;

  ClipboardErrorTrapPushFunc error_trap_push;
  ClipboardErrorTrapPopFunc  error_trap_pop;
  
  List *contents;
  List *conversions;

  Window requestor;
  Atom property;
  Time time;
};

typedef struct
{
  unsigned char *data;
  int            length;
  Atom           target;
  Atom           type;
  int            format;
  int            refcount;
} TargetData;

typedef struct
{
  Atom target;
  TargetData *data;
  Atom property;
  Window requestor;
  int  offset;
} IncrConversion;


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
  if (data->refcount == 0)
    {
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
send_selection_notify (ClipboardManager *manager,
		       Bool              success)
{
  XSelectionEvent notify;

  notify.type = SelectionNotify;
  notify.serial = 0;
  notify.send_event = True;
  notify.display = manager->display;
  notify.requestor = manager->requestor;
  notify.selection = XA_CLIPBOARD_MANAGER;
  notify.target = XA_SAVE_TARGETS;
  notify.property = success ? manager->property : None;
  notify.time = manager->time;

  manager->error_trap_push ();

  XSendEvent (manager->display, manager->requestor,
	      False, NoEventMask, (XEvent *)&notify);
  XSync (manager->display, False);
  
  manager->error_trap_pop ();
}

static void
finish_selection_request (ClipboardManager *manager,
			  XEvent           *xev,
			  Bool              success)
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

  manager->error_trap_push ();

  XSendEvent (xev->xselectionrequest.display, 
  	      xev->xselectionrequest.requestor,
	      False, NoEventMask, (XEvent *)&notify);
  XSync (manager->display, False);

  manager->error_trap_pop ();
}

static int 
clipboard_bytes_per_item (int format)
{
  switch (format)
    {
    case 8:
      return sizeof (char);
      break;
    case 16:
      return sizeof (short);
      break;
    case 32:
      return sizeof (long);
      break;
    default: ;
    }
  return 0;
}

static void
save_targets (ClipboardManager *manager, 
	      Atom             *save_targets,
	      int               nitems)
{
  int nout, i;
  Atom *multiple;
  TargetData *tdata;

  multiple = (Atom *) malloc (2 * nitems * sizeof (Atom));

  nout = 0;
  for (i = 0; i < nitems; i++)
    {
      if (save_targets[i] != XA_TARGETS &&
	  save_targets[i] != XA_MULTIPLE &&
	  save_targets[i] != XA_DELETE &&
	  save_targets[i] != XA_INSERT_PROPERTY &&
	  save_targets[i] != XA_INSERT_SELECTION &&
	  save_targets[i] != XA_PIXMAP)
	{
	  
	  tdata = (TargetData *) malloc (sizeof (TargetData));
	  tdata->data = NULL;
	  tdata->length = 0;
	  tdata->target = save_targets[i];
	  tdata->type = None;
	  tdata->format = 0;
	  tdata->refcount = 1;
	  manager->contents = list_prepend (manager->contents, tdata);
	  
	  multiple[nout++] = save_targets[i];
	  multiple[nout++] = save_targets[i];
	}
    }

  XFree (save_targets);
  
  XChangeProperty (manager->display, manager->window,
		   XA_MULTIPLE, XA_ATOM_PAIR, 
		   32, PropModeReplace, (char *)multiple, nout);
  free (multiple);

  XConvertSelection (manager->display, XA_CLIPBOARD,
		     XA_MULTIPLE, XA_MULTIPLE,
		     manager->window, manager->time);
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
get_property (TargetData       *tdata,
	      ClipboardManager *manager)
{
  Atom type;
  int format;
  unsigned long length;
  unsigned long remaining;
  unsigned char *data;
	
  XGetWindowProperty (manager->display, 
		      manager->window,
		      tdata->target,
		      0, 0x1FFFFFFF, True, AnyPropertyType,
		      &type, &format, &length, &remaining, 
		      &data);

  if (type == None)
    {
      manager->contents = list_remove (manager->contents, tdata);
      free (tdata);
    }
  else if (type == XA_INCR)
    {
      tdata->type = type;
      tdata->length = 0;
      XFree (data);
    }
  else
    {
      tdata->type = type;
      tdata->data = data;
      tdata->length = length * clipboard_bytes_per_item (format);
      tdata->format = format;
    }
}

static Bool
receive_incrementally (ClipboardManager *manager,
		       XEvent           *xev)
{
  List *list;
  TargetData *tdata;
  Atom type;
  int format;
  unsigned long length, nitems, remaining;
  unsigned char *data;

  if (xev->xproperty.window != manager->window)
    return False;

  list = list_find (manager->contents, 
		    (ListFindFunc)find_content_target, (void *)xev->xproperty.atom);
  
  if (!list) 
    return False;
  
  tdata = (TargetData *)list->data;

  if (tdata->type != XA_INCR)
    return False;

  XGetWindowProperty (xev->xproperty.display,
		      xev->xproperty.window,
		      xev->xproperty.atom,
		      0, 0x1FFFFFFF, True, AnyPropertyType,
		      &type, &format, &nitems, &remaining, &data);

  length = nitems * clipboard_bytes_per_item (format);
  
  if (length == 0)
    {
      tdata->type = type;
      tdata->format = format;
      
      if (!list_find (manager->contents, 
		      (ListFindFunc)find_content_type, (void *)XA_INCR)) 
	{
	  /* all incremental transfers done */
	  send_selection_notify (manager, True);
	  manager->requestor = None;
	}

      XFree (data);
    }
  else
    {
      if (!tdata->data)
	{
	  tdata->data = data;
	  tdata->length = length;
	}
      else
	{
	  tdata->data = realloc (tdata->data, tdata->length + length + 1);
	  memcpy (tdata->data + tdata->length, data, length + 1);
	  tdata->length += length;
	  XFree (data);
	}
    }

  return True;
}

static Bool
send_incrementally (ClipboardManager *manager,
		    XEvent           *xev)
{
  List *list;
  IncrConversion *rdata;
  unsigned long length, items;
  unsigned char *data;
	
  list = list_find (manager->conversions, 
		    (ListFindFunc)find_conversion_requestor, xev);
  
  if (list == NULL) 
    return False;
  
  rdata = (IncrConversion *)list->data;
  
  data = rdata->data->data + rdata->offset; 
  length = rdata->data->length - rdata->offset;
  if (length > SELECTION_MAX_SIZE)
    length = SELECTION_MAX_SIZE;
  
  rdata->offset += length;
  
  items = length / clipboard_bytes_per_item (rdata->data->format);
  XChangeProperty (manager->display, rdata->requestor,
		   rdata->property, rdata->data->type,
		   rdata->data->format, PropModeAppend,
		   data, items);
  
  if (length == 0)
    {
      manager->conversions = list_remove (manager->conversions, rdata);
      conversion_free (rdata);
    }

  return True;
}

static void
convert_clipboard_manager (ClipboardManager *manager,
			   XEvent           *xev)
{
  Atom type = None;
  int format;
  unsigned long nitems, remaining;
  Atom *targets = NULL;

  if (xev->xselectionrequest.target == XA_SAVE_TARGETS)
    {
      if (manager->requestor != None || manager->contents != NULL)
	{
	  /* We're in the middle of a conversion request, or own 
	   * the CLIPBOARD already 
	   */
	  finish_selection_request (manager, xev, False);
	}
      else
	{
	  manager->error_trap_push ();
	  
	  manager->watch (xev->xselectionrequest.requestor, True, StructureNotifyMask, manager->cb_data);
	  XSelectInput (manager->display, xev->xselectionrequest.requestor,
 			StructureNotifyMask);
	  XSync (manager->display, False);
	  
	  if (manager->error_trap_pop () != Success)
	    return;

	  manager->error_trap_push ();
	  
	  if (xev->xselectionrequest.property != None)
	    {
	      XGetWindowProperty (manager->display, xev->xselectionrequest.requestor,
				  xev->xselectionrequest.property,
				  0, 0x1FFFFFFF, False, XA_ATOM,
				  &type, &format, &nitems, &remaining,
				  (unsigned char **)&targets);
	      
	      if (manager->error_trap_pop () != Success)
		{
		  if (targets)
		    XFree (targets);
		  
		  return;
		}
	    }
	  
	  manager->requestor = xev->xselectionrequest.requestor;
	  manager->property = xev->xselectionrequest.property;
	  manager->time = xev->xselectionrequest.time;

	  if (type == None)
	    XConvertSelection (manager->display, XA_CLIPBOARD,
			       XA_TARGETS, XA_TARGETS,
			       manager->window, manager->time);
	  else
	    save_targets (manager, targets, nitems);
	}
    }
  else if (xev->xselectionrequest.target == XA_TIMESTAMP)
    {
      XChangeProperty (manager->display,
		       xev->xselectionrequest.requestor,
		       xev->xselectionrequest.property,
		       XA_INTEGER, 32, PropModeReplace,
		       (unsigned char *)&manager->timestamp, 1);
      
      finish_selection_request (manager, xev, True);
    }
  else if (xev->xselectionrequest.target == XA_TARGETS)
    {
      int n_targets = 0;
      Atom targets[3];
      
      targets[n_targets++] = XA_TARGETS;
      targets[n_targets++] = XA_TIMESTAMP;
      targets[n_targets++] = XA_SAVE_TARGETS;
      
      XChangeProperty (manager->display,
		       xev->xselectionrequest.requestor,
		       xev->xselectionrequest.property,
		       XA_ATOM, 32, PropModeReplace,
		       (unsigned char *)targets, n_targets);
      
      finish_selection_request (manager, xev, True);
    }
  else
    {
      finish_selection_request (manager, xev, False);
    }
}

static void
convert_clipboard_target (IncrConversion   *rdata,
			  ClipboardManager *manager)
{
  TargetData *tdata;
  Atom *targets;
  int n_targets;
  List *list;		  
  unsigned long items;
  XWindowAttributes atts;				
	  
  if (rdata->target == XA_TARGETS)
    {
      n_targets = list_length (manager->contents) + 2;
      targets = (Atom *) malloc (n_targets * sizeof (Atom));
      
      n_targets = 0;
      
      targets[n_targets++] = XA_TARGETS;
      targets[n_targets++] = XA_MULTIPLE;
      
      for (list = manager->contents; list; list = list->next)
	{
	  tdata = (TargetData *)list->data;
	  targets[n_targets++] = tdata->target;
	}
      
      XChangeProperty (manager->display, rdata->requestor,
		       rdata->property,
		       XA_ATOM, 32, PropModeReplace,
		       (unsigned char *)targets, n_targets);
      free (targets);
    }
  else 
    {
      /* Convert from stored CLIPBOARD data */
      list = list_find (manager->contents, 
			(ListFindFunc)find_content_target, (void *)rdata->target);

      /* We got a target that we don't support */
      if (!list)
	return;
      
      tdata = (TargetData *)list->data;

      if (tdata->type == XA_INCR)
	{
	  /* we haven't completely received this target yet 
           */
	  rdata->property = None;
	  return;
	}

      rdata->data = target_data_ref (tdata);
      items = tdata->length / clipboard_bytes_per_item (tdata->format);
      if (tdata->length <= SELECTION_MAX_SIZE)
	XChangeProperty (manager->display, rdata->requestor,
			 rdata->property,
			 tdata->type, tdata->format, PropModeReplace,
			 tdata->data, items);
      else
	{
	  /* start incremental transfer
	   */
	  rdata->offset = 0;

	  manager->error_trap_push ();
	  
	  XGetWindowAttributes (manager->display, rdata->requestor, &atts);
	  XSelectInput (manager->display, rdata->requestor, 
			atts.your_event_mask | PropertyChangeMask);
	  
	  XChangeProperty (manager->display, rdata->requestor,
			   rdata->property,
			   XA_INCR, 32, PropModeReplace,
			   (unsigned char *)&items, 1);

	  XSync (manager->display, False);
  
	  manager->error_trap_pop ();
	}
    }
}

static void
collect_incremental (IncrConversion   *rdata, 
		     ClipboardManager *manager)
{
  if (rdata->offset >= 0)
    manager->conversions = list_prepend (manager->conversions, rdata);
  else
    {
      if (rdata->data)
	{
	  target_data_unref (rdata->data);
	  rdata->data = NULL;
	}
      free (rdata);
    }
}

static void
convert_clipboard (ClipboardManager *manager,
		   XEvent           *xev)
{
    List *list, *conversions;
    IncrConversion *rdata;
    Atom type;
    int i, format;
    unsigned long nitems, remaining;
    Atom *multiple;
    
    conversions = NULL;
    type = None;
    
    if (xev->xselectionrequest.target == XA_MULTIPLE)
      {
	
	XGetWindowProperty (xev->xselectionrequest.display,
			    xev->xselectionrequest.requestor,
			    xev->xselectionrequest.property,
			    0, 0x1FFFFFFF, False, XA_ATOM_PAIR,
			    &type, &format, &nitems, &remaining,
			    (unsigned char **)&multiple);


	
	if (type != XA_ATOM_PAIR)
	  return;
	
	for (i = 0; i < nitems; i += 2)
	  {
	    rdata = (IncrConversion *) malloc (sizeof (IncrConversion));
	    rdata->requestor = xev->xselectionrequest.requestor;
	    rdata->target = multiple[i];
	    rdata->property = multiple[i+1];
	    rdata->data = NULL;
	    rdata->offset = -1;
	    conversions = list_prepend (conversions, rdata);
	  }
      }
    else
      {
	multiple = NULL;
	
	rdata = (IncrConversion *) malloc (sizeof (IncrConversion));
	rdata->requestor = xev->xselectionrequest.requestor;
	rdata->target = xev->xselectionrequest.target;
	rdata->property = xev->xselectionrequest.property;
	rdata->data = NULL;
	rdata->offset = -1;
	conversions = list_prepend (conversions, rdata);
      }

    list_foreach (conversions, (Callback)convert_clipboard_target, manager);
    
    if (conversions->next == NULL &&
	((IncrConversion *)conversions->data)->property == None)
      {
	finish_selection_request (manager, xev, False);
      }
    else
      {
	if (multiple)
	  {
	    i = 0;
	    for (list = conversions; list; list = list->next)
	      {
		rdata = (IncrConversion *)list->data;
		multiple[i++] = rdata->target;
		multiple[i++] = rdata->property;
	      }
	    XChangeProperty (xev->xselectionrequest.display,
			     xev->xselectionrequest.requestor,
			     xev->xselectionrequest.property,
			     XA_ATOM_PAIR, 32, PropModeReplace,
			     (unsigned char *)multiple, nitems);
	  }
	finish_selection_request (manager, xev, True);
      }
    
    list_foreach (conversions, (Callback)collect_incremental, manager);
    list_free (conversions);
    
    if (multiple)
      free (multiple);
}

Bool
clipboard_manager_process_event (ClipboardManager *manager,
				 XEvent           *xev)
{
  Atom type;
  int format;
  unsigned long nitems;
  unsigned long remaining;
  Atom *targets;
  
  targets = NULL;

  switch (xev->xany.type)
    {
    case DestroyNotify:
      if (xev->xdestroywindow.window == manager->requestor)
	{
	  list_foreach (manager->contents, (Callback)target_data_unref, NULL);
	  list_free (manager->contents);
	  manager->contents = NULL;

	  manager->watch (manager->requestor, False, 0, manager->cb_data);
	  manager->requestor = None;
	}
      break;
    case PropertyNotify:
      
      if (xev->xproperty.state == PropertyNewValue)
 	return receive_incrementally (manager, xev);
      else 
	return send_incrementally (manager, xev);
      break;
      
    case SelectionClear:
      if (xev->xany.window != manager->window)
	return False;

      if (xev->xselectionclear.selection == XA_CLIPBOARD_MANAGER)
	{
 	  /* We lost the manager selection */
	  if (manager->contents)
	    {
	      list_foreach (manager->contents, (Callback)target_data_unref, NULL);
	      list_free (manager->contents);
	      manager->contents = NULL;
	      
	      XSetSelectionOwner (manager->display, 
				  XA_CLIPBOARD,
				  None, manager->time);
	    }
	  manager->terminate (manager->cb_data);

	  return True;
	}
      if (xev->xselectionclear.selection == XA_CLIPBOARD)
	{
	  /* We lost the clipboard selection */
	  list_foreach (manager->contents, (Callback)target_data_unref, NULL);
	  list_free (manager->contents);
	  manager->contents = NULL;
	  manager->watch (manager->requestor, False, 0, manager->cb_data);
	  manager->requestor = None;
	  
	  return True;
	}
      break;

    case SelectionNotify:
      if (xev->xany.window != manager->window)
	return False;

      if (xev->xselection.selection == XA_CLIPBOARD)
	{
	  /* a CLIPBOARD conversion is done */
	  if (xev->xselection.property == XA_TARGETS)
	    {
	      XGetWindowProperty (xev->xselection.display,
				  xev->xselection.requestor,
				  xev->xselection.property,
				  0, 0x1FFFFFFF, True, XA_ATOM,
				  &type, &format, &nitems, &remaining,
				  (unsigned char **)&targets);
	      
	      save_targets (manager, targets, nitems);
	    }
	  else if (xev->xselection.property == XA_MULTIPLE)
	    {
	      List *tmp;

	      tmp = list_copy (manager->contents);
	      list_foreach (tmp, (Callback)get_property, manager);
	      list_free (tmp);
	      
	      manager->time = xev->xselection.time;
	      XSetSelectionOwner (manager->display, XA_CLIPBOARD,
				  manager->window, manager->time);

	      if (manager->property != None)
		XChangeProperty (manager->display, manager->requestor,
				 manager->property,
				 XA_ATOM, 32, PropModeReplace,
				 (unsigned char *)&XA_NULL, 1);

	      if (!list_find (manager->contents, 
			      (ListFindFunc)find_content_type, (void *)XA_INCR)) 
		{
		  /* all transfers done */
		  send_selection_notify (manager, True);
		  manager->watch (manager->requestor, False, 0, manager->cb_data);
		  manager->requestor = None;
		}		  
	    }
	  else if (xev->xselection.property == None)
	    {
	      send_selection_notify (manager, False);
	      manager->watch (manager->requestor, False, 0, manager->cb_data);
	      manager->requestor = None;
	    }

	  return True;
	}
      break;

      case SelectionRequest:
	if (xev->xany.window != manager->window)
	  return False;

	if (xev->xselectionrequest.selection == XA_CLIPBOARD_MANAGER)
	  {
	    convert_clipboard_manager (manager, xev);
	    return True;
	  }
	else if (xev->xselectionrequest.selection == XA_CLIPBOARD)
	  {
	    convert_clipboard (manager, xev);
	    return True;
	  }
	break;

    default: ;
    }
     
  return False;
}

Bool
clipboard_manager_check_running (Display *display)
{
  init_atoms (display);

  if (XGetSelectionOwner (display, XA_CLIPBOARD_MANAGER))
    return True;
  else
    return False;
}

ClipboardManager *
clipboard_manager_new (Display                   *display,
		       ClipboardErrorTrapPushFunc error_trap_push_cb,
		       ClipboardErrorTrapPopFunc  error_trap_pop_cb,
		       ClipboardTerminateFunc     terminate,
		       ClipboardWatchFunc         watch,
		       void                      *cb_data)
{
  ClipboardManager *manager;
  XClientMessageEvent xev;

  init_atoms (display);

  manager = malloc (sizeof *manager);
  if (!manager)
    return NULL;

  manager->display = display;

  manager->error_trap_push = error_trap_push_cb;
  manager->error_trap_pop = error_trap_pop_cb;
  
  manager->terminate = terminate;
  manager->watch = watch;
  manager->cb_data = cb_data;

  manager->contents = NULL;
  manager->conversions = NULL;

  manager->requestor = None;

  manager->window = XCreateSimpleWindow (display,
					 DefaultRootWindow (display),
					 0, 0, 10, 10, 0,
					 WhitePixel (display, DefaultScreen (display)),
					 WhitePixel (display, DefaultScreen (display)));

  manager->watch (manager->window, True, PropertyChangeMask, manager->cb_data);
  XSelectInput (display, manager->window, PropertyChangeMask);
  manager->timestamp = get_server_time (display, manager->window);

  XSetSelectionOwner (display, XA_CLIPBOARD_MANAGER,
		      manager->window, manager->timestamp);

  /* Check to see if we managed to claim the selection. If not,
   * we treat it as if we got it then immediately lost it
   */

  if (XGetSelectionOwner (display, XA_CLIPBOARD_MANAGER) == manager->window)
    {
      xev.type = ClientMessage;
      xev.window = DefaultRootWindow (display);
      xev.message_type = XA_MANAGER;
      xev.format = 32;
      xev.data.l[0] = manager->timestamp;
      xev.data.l[1] = XA_CLIPBOARD_MANAGER;
      xev.data.l[2] = manager->window;
      xev.data.l[3] = 0;	/* manager specific data */
      xev.data.l[4] = 0;	/* manager specific data */

      XSendEvent (display, DefaultRootWindow (display),
		  False, StructureNotifyMask, (XEvent *)&xev);
    }
  else
    {
      manager->watch (manager->window, False, 0, manager->cb_data);
      manager->terminate (manager->cb_data);
      free (manager);
      manager = NULL;
    }
  
  return manager;
}

void
clipboard_manager_destroy (ClipboardManager *manager)
{
  if (manager)
    {
      manager->watch (manager->window, False, 0, manager->cb_data);
      XDestroyWindow (manager->display, manager->window);

      list_foreach (manager->conversions, (Callback)conversion_free, NULL);
      list_free (manager->conversions);

      list_foreach (manager->contents, (Callback)target_data_unref, NULL);
      list_free (manager->contents);

      free (manager);
      manager = NULL;
    }
}

