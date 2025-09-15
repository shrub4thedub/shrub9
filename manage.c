/*
 * Copyright multiple authors, see README for licence details
 */
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"
#include "workspace.h"
#include "config.h"
#include "plumb.h"


static void check_terminal_launch(Client *c);
static Client* find_candidate_terminal(Client *new_client);
void restore_terminal_from_child(Client *child);

int
manage(Client * c, int mapped)
{
	int fixsize, dohide, doreshape, state;
	long msize;
	XClassHint class;
	XWMHints *hints;

	fprintf(stderr, "manage: ENTRY - managing client %p (window=0x%lx) mapped=%d\n", 
		(void*)c, c->window, mapped);
	fprintf(stderr, "manage: client workspace at entry = %d\n", c->workspace);

	trace("manage", c, 0);
	XSelectInput(dpy, c->window, ColormapChangeMask | EnterWindowMask | PropertyChangeMask | FocusChangeMask);

	/*
	 * Get loads of hints 
	 */

	if (XGetClassHint(dpy, c->window, &class) != 0) {	/* ``Success'' */
		c->instance = class.res_name;
		c->class = class.res_class;
		
		/* Check if this is a terminal */
		if (c->class && is_terminal_class(c->class)) {
			c->is_terminal = 1;
		}
	} else {
		c->instance = 0;
		c->class = 0;
	}
	c->iconname = getprop(c->window, XA_WM_ICON_NAME);
	c->name = getprop(c->window, XA_WM_NAME);
	setlabel(c);

	hints = XGetWMHints(dpy, c->window);
	if (XGetWMNormalHints(dpy, c->window, &c->size, &msize) == 0 || c->size.flags == 0)
		c->size.flags = PSize;	/* not specified - punt */

	getcmaps(c);
	getproto(c);
	gettrans(c);

	/*
	 * Figure out what to do with the window from hints 
	 */

	if (!getwstate(c->window, &state))
		state = hints ? hints->initial_state : NormalState;
	dohide = (state == IconicState);

	fixsize = 0;
	if ((c->size.flags & (USSize | PSize)))
		fixsize = 1;
	if ((c->size.flags & (PMinSize | PMaxSize)) == (PMinSize | PMaxSize) && c->size.min_width == c->size.max_width
	    && c->size.min_height == c->size.max_height)
		fixsize = 1;
	doreshape = !mapped;
	if (fixsize) {
		if (c->size.flags & USPosition)
			doreshape = 0;
		if (dohide && (c->size.flags & PPosition))
			doreshape = 0;
		if (c->trans != None)
			doreshape = 0;
	}
	if (c->size.flags & PBaseSize) {
		c->min_dx = c->size.base_width;
		c->min_dy = c->size.base_height;
	} else if (c->size.flags & PMinSize) {
		c->min_dx = c->size.min_width;
		c->min_dy = c->size.min_height;
	} else
		c->min_dx = c->min_dy = 0;

	if (hints)
		XFree(hints);

	/*
	 * Now do it!!! 
	 */

	if (doreshape) {
		int xmax = DisplayWidth(dpy, c->screen->num);
		int ymax = DisplayHeight(dpy, c->screen->num);
		int x, y;

		getmouse(&x, &y, c->screen);

		c->x = x - (c->dx / 2);
		c->y = y - (c->dy / 2);

		if (c->x + c->dx > xmax) {
			c->x = xmax - c->dx;
		}
		if (c->x < 0) {
			c->x = 0;
		}

		if (c->y + c->dy > ymax) {
			c->y = ymax - c->dy;
		}
		if (c->y < 0) {
			c->y = 0;
		}
	}
	gravitate(c, 0);

	{
		int titlebar_offset = config.show_titlebars ? config.titlebar_height : 0;
		int parent_height = c->dy + 2 * (BORDER - 1) + titlebar_offset;
		
		c->parent = XCreateSimpleWindow(dpy, c->screen->root,
						c->x - BORDER, c->y - BORDER,
						c->dx + 2 * (BORDER - 1), parent_height, 
						config.window_frame_width, c->screen->frame_color, c->screen->white);
	}
	XSelectInput(dpy, c->parent, SubstructureRedirectMask | SubstructureNotifyMask);
	if (mapped)
		c->reparenting = 1;
	if (doreshape && !fixsize)
		XResizeWindow(dpy, c->window, c->dx, c->dy);
	XSetWindowBorderWidth(dpy, c->window, 0);
	
	{
		int window_y = config.show_titlebars ? BORDER - 1 + config.titlebar_height : BORDER - 1;
		XReparentWindow(dpy, c->window, c->parent, BORDER - 1, window_y);
	}
	
	
#ifdef	SHAPE
	if (shape) {
		XShapeSelectInput(dpy, c->window, ShapeNotifyMask);
		ignore_badwindow = 1;	/* magic */
		setshape(c);
		ignore_badwindow = 0;
	}
#endif
	XAddToSaveSet(dpy, c->window);
	
	/* Create titlebar if enabled */
	if (config.show_titlebars) {
		create_titlebar(c);
	}
	
	/* Add client to current workspace BEFORE visibility operations */
	fprintf(stderr, "manage: adding new client %p (window=0x%lx) to current workspace %d\n", 
		(void*)c, c->window, workspace_get_current());
	workspace_add_client(c, workspace_get_current());
	
	if (dohide)
		hide(c);
	else if (auto_reshape_next) {
		/* Don't map the window yet - we'll map it after reshape */
		setwstate(c, NormalState);
	} else {
		XMapWindow(dpy, c->window);
		XMapWindow(dpy, c->parent);
		if (doreshape)
			active(c);
		else if (c->trans != None && current && current->window == c->trans)
			active(c);
		else
			setactive(c, 0);
		setwstate(c, NormalState);
		
		/* Check if window was added to non-current workspace and hide it */
		if (c->workspace != workspace_get_current()) {
			fprintf(stderr, "manage: unmapping window that was added to non-current workspace %d (current=%d)\n", 
				c->workspace, workspace_get_current());
			XUnmapWindow(dpy, c->parent);
			XUnmapWindow(dpy, c->window);
		} else {
			fprintf(stderr, "manage: window remains mapped in current workspace\n");
		}
	}
	if (current && (current != c))
		cmapfocus(current);
	c->init = 1;
	
	fprintf(stderr, "manage: FINAL - client %p has workspace=%d\n", (void*)c, c->workspace);
	
	/* Handle terminal-launcher relationships */
	if (config.terminal_launcher_mode && !c->is_terminal && !plumb_is_image_viewer(c)) {
		/* This is a non-terminal window - check if it was launched from a terminal */
		check_terminal_launch(c);
	}
	
	/* Check if we should auto-reshape this window (for interactive terminal spawn) */
	if (auto_reshape_next && !dohide) {
		auto_reshape_next = 0;  /* Reset the flag */
		/* Give the window a moment to be ready */
		XFlush(dpy);
		/* Trigger reshape operation without activating */
		if (reshape_ex(c, 0)) {
			/* Reshape succeeded - now map the window */
			XMapWindow(dpy, c->window);
			XMapRaised(dpy, c->parent);
			active(c);
			top(c);
		} else {
			/* Reshape was cancelled - map at original size */
			XMapWindow(dpy, c->window);
			XMapWindow(dpy, c->parent);
			setactive(c, 0);
		}
	}
	
	
	return 1;
}

void
scanwins(ScreenInfo * s)
{
	unsigned int i, nwins;
	Client *c;
	Window dw1, dw2, *wins;
	XWindowAttributes attr;

	XQueryTree(dpy, s->root, &dw1, &dw2, &wins, &nwins);
	for (i = 0; i < nwins; i++) {
		XGetWindowAttributes(dpy, wins[i], &attr);
		if (attr.override_redirect || wins[i] == s->menuwin)
			continue;
		c = getclient(wins[i], 1);
		if (c != 0 && c->window == wins[i] && !c->init) {
			c->x = attr.x;
			c->y = attr.y;
			c->dx = attr.width;
			c->dy = attr.height;
			c->border = attr.border_width;
			c->screen = s;
			c->parent = s->root;
			if (attr.map_state == IsViewable)
				manage(c, 1);
		}
	}
	XFree((void *) wins);	/* cast is to shut stoopid compiler up */
}

void
gettrans(Client * c)
{
	Window trans;

	trans = None;
	if (XGetTransientForHint(dpy, c->window, &trans) != 0)
		c->trans = trans;
	else
		c->trans = None;
}

void
withdraw(Client * c)
{
	/* Check if we're in the middle of workspace switching */
	if (workspace_switching && pending_workspace_unmaps > 0) {
		fprintf(stderr, "withdraw: SKIPPING entire withdrawal - workspace switching in progress for client %p (pending=%d)\n", (void*)c, pending_workspace_unmaps);
		/* Decrement pending counter as we're processing a workspace switch unmap */
		pending_workspace_unmaps--;
		fprintf(stderr, "withdraw: decremented pending unmaps to %d\n", pending_workspace_unmaps);
		
		/* If this was the last pending unmap, clear the workspace switching flag */
		if (pending_workspace_unmaps == 0) {
			workspace_switching = 0;
			fprintf(stderr, "withdraw: all workspace switch unmaps processed, clearing workspace_switching flag\n");
		}
		/* During workspace switching, windows are already unmapped by workspace_hide_all_clients
		   Don't do any withdrawal operations - just return */
		return;
	}
	
	fprintf(stderr, "withdraw: removing client %p from workspace (actual withdrawal)\n", (void*)c);
	workspace_remove_client(c);
	
	XUnmapWindow(dpy, c->parent);
	gravitate(c, 1);
	XReparentWindow(dpy, c->window, c->screen->root, c->x, c->y);
	gravitate(c, 0);
	XRemoveFromSaveSet(dpy, c->window);
	setwstate(c, WithdrawnState);

	/*
	 * flush any errors 
	 */
	ignore_badwindow = 1;
	XSync(dpy, False);
	ignore_badwindow = 0;
}

void
gravitate(Client * c, int invert)
{
	int gravity, dx, dy, delta;

	gravity = NorthWestGravity;
	if (c->size.flags & PWinGravity)
		gravity = c->size.win_gravity;

	delta = c->border - BORDER;
	switch (gravity) {
	case NorthWestGravity:
		dx = 0;
		dy = 0;
		break;
	case NorthGravity:
		dx = delta;
		dy = 0;
		break;
	case NorthEastGravity:
		dx = 2 * delta;
		dy = 0;
		break;
	case WestGravity:
		dx = 0;
		dy = delta;
		break;
	case CenterGravity:
	case StaticGravity:
		dx = delta;
		dy = delta;
		break;
	case EastGravity:
		dx = 2 * delta;
		dy = delta;
		break;
	case SouthWestGravity:
		dx = 0;
		dy = 2 * delta;
		break;
	case SouthGravity:
		dx = delta;
		dy = 2 * delta;
		break;
	case SouthEastGravity:
		dx = 2 * delta;
		dy = 2 * delta;
		break;
	default:
		fprintf(stderr, "9wm: bad window gravity %d for 0x%x\n", gravity, (int) c->window);
		return;
	}
	dx += BORDER;
	dy += BORDER;
	if (invert) {
		dx = -dx;
		dy = -dy;
	}
	c->x += dx;
	c->y += dy;
}

static void
installcmap(ScreenInfo * s, Colormap cmap)
{
	if (cmap == None)
		XInstallColormap(dpy, s->def_cmap);
	else
		XInstallColormap(dpy, cmap);
}

void
cmapfocus(Client * c)
{
	int i, found;
	Client *cc;

	if (c == 0)
		return;
	else if (c->ncmapwins != 0) {
		found = 0;
		for (i = c->ncmapwins - 1; i >= 0; i--) {
			installcmap(c->screen, c->wmcmaps[i]);
			if (c->cmapwins[i] == c->window)
				found++;
		}
		if (!found)
			installcmap(c->screen, c->cmap);
	} else if (c->trans != None && (cc = getclient(c->trans, 0)) != 0 && cc->ncmapwins != 0)
		cmapfocus(cc);
	else
		installcmap(c->screen, c->cmap);
}

void
cmapnofocus(ScreenInfo * s)
{
	installcmap(s, None);
}

void
getcmaps(Client * c)
{
	int n, i;
	Window *cw;
	XWindowAttributes attr;

	if (!c->init) {
		XGetWindowAttributes(dpy, c->window, &attr);
		c->cmap = attr.colormap;
	}

	n = _getprop(c->window, wm_colormaps, XA_WINDOW, 100L, (unsigned char **) &cw);
	if (c->ncmapwins != 0) {
		XFree((char *) c->cmapwins);
		free((char *) c->wmcmaps);
	}
	if (n <= 0) {
		c->ncmapwins = 0;
		return;
	}

	c->ncmapwins = n;
	c->cmapwins = cw;

	c->wmcmaps = (Colormap *) malloc(n * sizeof(Colormap));
	for (i = 0; i < n; i++) {
		if (cw[i] == c->window)
			c->wmcmaps[i] = c->cmap;
		else {
			XSelectInput(dpy, cw[i], ColormapChangeMask);
			XGetWindowAttributes(dpy, cw[i], &attr);
			c->wmcmaps[i] = attr.colormap;
		}
	}
}

void
setlabel(Client * c)
{
	char *label, *p;

	if (c->iconname != 0) {
		label = c->iconname;
	} else if (c->name != 0) {
		label = c->name;
	} else if (c->instance != 0) {
		label = c->instance;
	} else if (c->class != 0) {
		label = c->class;
	} else {
		label = "no label";
	}
	while ((p = strstr(label, " - "))) {
		label = p + 3;
	}
	if ((p = strchr(label, ':')) != 0)
		*p = '\0';
	for (; *label == ' '; label += 1);
	c->label = label;
}

#ifdef	SHAPE
void
setshape(Client * c)
{
	int n, order;
	XRectangle *rect;

	/*
	 * don't try to add a border if the window is non-rectangular 
	 */
	rect = XShapeGetRectangles(dpy, c->window, ShapeBounding, &n, &order);
	if (n > 1)
		XShapeCombineShape(dpy, c->parent, ShapeBounding, BORDER - 1, BORDER - 1, c->window, ShapeBounding, ShapeSet);
	XFree((void *) rect);
}
#endif

int
_getprop(Window w, Atom a, Atom type, long len, unsigned char **p)
{
	Atom real_type;
	int format;
	unsigned long n, extra;
	int status;

	status = XGetWindowProperty(dpy, w, a, 0L, len, False, type, &real_type, &format, &n, &extra, p);
	if (status != Success || *p == 0)
		return -1;
	if (n == 0)
		XFree((void *) *p);
	/*
	 * could check real_type, format, extra here... 
	 */
	return n;
}

char *
getprop(Window w, Atom a)
{
	unsigned char *p;

	if (_getprop(w, a, utf8_string, 100L, &p) <= 0) {
		return 0;
	}
	return (char *) p;
}

int
get1prop(Window w, Atom a, Atom type)
{
	unsigned char *p;
	int ret;

	if (_getprop(w, a, type, 1L, &p) <= 0) {
		return 0;
	}
	ret = (int) (unsigned long int) (*p);
	XFree((void *) p);
	return ret;
}

Window
getwprop(Window w, Atom a)
{
	return get1prop(w, a, XA_WINDOW);
}

int
getiprop(Window w, Atom a)
{
	return get1prop(w, a, XA_INTEGER);
}

void
setwstate(Client * c, int state)
{
	long data[2];

	data[0] = (long) state;
	data[1] = (long) None;

	c->state = state;
	XChangeProperty(dpy, c->window, wm_state, wm_state, 32, PropModeReplace, (unsigned char *) data, 2);
}

int
getwstate(Window w, int *state)
{
	long *p = 0;

	if (_getprop(w, wm_state, wm_state, 2L, (unsigned char **) &p) <= 0)
		return 0;

	*state = (int) *p;
	XFree((char *) p);
	return 1;
}

void
getproto(Client * c)
{
	Atom *p;
	int i;
	long n;
	Window w;

	w = c->window;
	c->proto = 0;
	if ((n = _getprop(w, wm_protocols, XA_ATOM, 20L, (unsigned char **) &p)) <= 0)
		return;

	for (i = 0; i < n; i++)
		if (p[i] == wm_delete)
			c->proto |= Pdelete;
		else if (p[i] == wm_take_focus)
			c->proto |= Ptakefocus;

	XFree((char *) p);
}

static Client*
find_candidate_terminal(Client *new_client)
{
	Client *c;
	Client *best_candidate = NULL;
	
	/* Look for terminals in the same workspace that don't already have children */
	for (c = clients; c; c = c->next) {
		if (c->is_terminal && 
		    c->launched_child == NULL && 
		    c->workspace == new_client->workspace &&
		    normal(c)) {
			/* Prefer the current terminal if it exists */
			if (c == current) {
				best_candidate = c;
				break;
			}
			/* Otherwise, take the first suitable terminal */
			if (best_candidate == NULL) {
				best_candidate = c;
			}
		}
	}
	
	return best_candidate;
}

static void
check_terminal_launch(Client *c)
{
	Client *terminal;
	
	/* Don't process terminals or transient windows */
	if (c->is_terminal || c->trans != None)
		return;
		
	/* Don't process windows created by plumber */
	if (plumb_window_pending()) {
		fprintf(stderr, "manage: skipping terminal launcher for plumber window %s\n", 
		        c->class ? c->class : "unknown");
		return;
	}
	
	/* Don't process image viewers (likely launched by plumber) */
	if (plumb_is_image_viewer(c)) {
		fprintf(stderr, "manage: skipping terminal launcher for image viewer %s\n", 
		        c->class ? c->class : "unknown");
		return;
	}
		
	terminal = find_candidate_terminal(c);
	if (terminal != NULL) {
		/* Establish the relationship */
		c->terminal_parent = terminal;
		terminal->launched_child = c;
		
		/* Save terminal's current geometry */
		terminal->saved_x = terminal->x;
		terminal->saved_y = terminal->y;
		terminal->saved_dx = terminal->dx;
		terminal->saved_dy = terminal->dy;
		
		/* Apply terminal's geometry to the new window */
		c->x = terminal->x;
		c->y = terminal->y;
		c->dx = terminal->dx;
		c->dy = terminal->dy;
		
		/* Hide the terminal */
		XUnmapWindow(dpy, terminal->parent);
		XUnmapWindow(dpy, terminal->window);
		terminal->state = IconicState;
		
		/* Make sure the new window appears at the right size */
		XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER, 
		                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
		XMoveResizeWindow(dpy, c->window, BORDER - 1, 
		                  config.show_titlebars ? BORDER - 1 + config.titlebar_height : BORDER - 1, 
		                  c->dx, c->dy);
	}
}

void
restore_terminal_from_child(Client *child)
{
	Client *terminal;
	
	terminal = child->terminal_parent;
	if (!terminal)
		return;
		
	/* Update terminal geometry to match the child's final position/size */
	terminal->x = child->x;
	terminal->y = child->y;
	terminal->dx = child->dx;
	terminal->dy = child->dy;
	
	/* Clear the relationship */
	terminal->launched_child = NULL;
	child->terminal_parent = NULL;
	
	/* Restore the terminal */
	terminal->state = NormalState;
	
	/* Update terminal's parent window geometry */
	XMoveResizeWindow(dpy, terminal->parent, terminal->x - BORDER, terminal->y - BORDER,
	                  terminal->dx + 2 * (BORDER - 1), terminal->dy + 2 * (BORDER - 1));
	
	/* Update terminal window position within parent */
	XMoveResizeWindow(dpy, terminal->window, BORDER - 1,
	                  config.show_titlebars ? BORDER - 1 + config.titlebar_height : BORDER - 1,
	                  terminal->dx, terminal->dy);
	
	/* Show the terminal */
	XMapWindow(dpy, terminal->window);
	XMapRaised(dpy, terminal->parent);
	
	/* Make it active */
	active(terminal);
	top(terminal);
}

