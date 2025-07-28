/*
 * Copyright multiple authors, see README for licence details
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"
#include "workspace.h"
#include "config.h"

static void draw_rounded_border(Client *c, int active);


Client *clients;
Client *current;

void
setactive(Client * c, int on)
{
	if (c->parent == c->screen->root) {
		return;
	}
	if (on) {
		XUngrabButton(dpy, AnyButton, AnyModifier, c->parent);
		XSetInputFocus(dpy, c->window, RevertToPointerRoot, timestamp());
		if (c->proto & Ptakefocus)
			sendcmessage(c->window, wm_protocols, wm_take_focus, 0);
		cmapfocus(c);
	} else
		XGrabButton(dpy, AnyButton, AnyModifier, c->parent, False, ButtonMask, GrabModeAsync, GrabModeSync, None, None);
	draw_border(c, on);
}

void
create_titlebar(Client * c)
{
	unsigned long titlebar_bg, titlebar_fg;
	
	if (!config.show_titlebars || c->titlebar != None)
		return;
		
	if (!config_parse_color(config.titlebar_bg_color, &titlebar_bg, c->screen->def_cmap))
		titlebar_bg = c->screen->white;
	if (!config_parse_color(config.titlebar_fg_color, &titlebar_fg, c->screen->def_cmap))
		titlebar_fg = c->screen->black;
	
	c->titlebar = XCreateSimpleWindow(dpy, c->parent, 
		BORDER - 1, BORDER - 1, 
		c->dx, config.titlebar_height, 
		0, titlebar_fg, titlebar_bg);
	
	XSelectInput(dpy, c->titlebar, ExposureMask | ButtonPressMask);
	XMapWindow(dpy, c->titlebar);
}

void
draw_titlebar(Client * c)
{
	unsigned long titlebar_fg;
	char *title;
	int title_len, text_width, text_x, text_y;
	GC titlebar_gc;
	XGCValues gv;
	
	if (!config.show_titlebars || c->titlebar == None)
		return;
		
	if (!config_parse_color(config.titlebar_fg_color, &titlebar_fg, c->screen->def_cmap))
		titlebar_fg = c->screen->black;
	
	/* Try multiple sources for the title */
	if (c->name && strlen(c->name) > 0) {
		title = c->name;
	} else if (c->label && strlen(c->label) > 0) {
		title = c->label;
	} else if (c->class && strlen(c->class) > 0) {
		title = c->class;
	} else if (c->instance && strlen(c->instance) > 0) {
		title = c->instance;
	} else {
		title = "Untitled";
	}
	
	title_len = strlen(title);
	
	XClearWindow(dpy, c->titlebar);
	
	if (font != 0) {
		/* Create a dedicated GC for the titlebar */
		gv.foreground = titlebar_fg;
		gv.background = c->screen->white;
		gv.font = font->fid;
		titlebar_gc = XCreateGC(dpy, c->titlebar, GCForeground | GCBackground | GCFont, &gv);
		
		text_width = XTextWidth(font, title, title_len);
		text_x = (c->dx - text_width) / 2;
		if (text_x < 4) text_x = 4;
		text_y = font->ascent + 2;
		
		XDrawString(dpy, c->titlebar, titlebar_gc, text_x, text_y, title, title_len);
		XFreeGC(dpy, titlebar_gc);
	}
}

void
destroy_titlebar(Client * c)
{
	if (c->titlebar != None) {
		XDestroyWindow(dpy, c->titlebar);
		c->titlebar = None;
	}
}

void
draw_border(Client * c, int active)
{
	if (config.rounding && config.rounding_radius > 0) {
		/* For rounded windows, draw custom rounded background */
		draw_rounded_border(c, active);
	} else {
		/* Regular rectangular border */
		XSetWindowBackground(dpy, c->parent, active ? c->screen->active : c->screen->inactive);
		XClearWindow(dpy, c->parent);
		
		if (c->hold && active) {
			XDrawRectangle(dpy, c->parent, c->screen->gc, INSET, INSET, 
				c->dx + BORDER - INSET, c->dy + BORDER - INSET);
		}
	}
	
	/* Reapply rounding after drawing */
	apply_window_rounding(c);
	
	/* Draw titlebar if enabled */
	if (config.show_titlebars) {
		draw_titlebar(c);
	}
}

void
active(Client * c)
{
	Client *cc;

	if (c == 0) {
		fprintf(stderr, "9wm: active(c==0)\n");
		return;
	}
	if (c == current)
		return;
	if (current) {
		setactive(current, 0);
		if (current->screen != c->screen)
			cmapnofocus(current->screen);
	}
	setactive(c, 1);
	for (cc = clients; cc; cc = cc->next)
		if (cc->revert == c)
			cc->revert = c->revert;
	c->revert = current;
	while (c->revert && !normal(c->revert))
		c->revert = c->revert->revert;
	current = c;
#ifdef	DEBUG
	if (debug)
		dump_revert();
#endif
}

void
nofocus(void)
{
	static Window w = 0;
	int mask;
	XSetWindowAttributes attr;
	Client *c;

	if (current) {
		setactive(current, 0);
		for (c = current->revert; c; c = c->revert)
			if (normal(c)) {
				active(c);
				return;
			}
		cmapnofocus(current->screen);
		/*
		 * if no candidates to revert to, fall through 
		 */
	}
	current = 0;
	if (w == 0) {
		mask = CWOverrideRedirect;
		attr.override_redirect = 1;
		w = XCreateWindow(dpy, screens[0].root, 0, 0, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent, mask, &attr);
		XMapWindow(dpy, w);
	}
	XSetInputFocus(dpy, w, RevertToPointerRoot, timestamp());
}

void
top(Client * c)
{
	Client **l, *cc;

	l = &clients;
	for (cc = *l; cc; cc = *l) {
		if (cc == c) {
			*l = c->next;
			c->next = clients;
			clients = c;
			return;
		}
		l = &cc->next;
	}
	fprintf(stderr, "9wm: %p not on client list in top()\n", (void *) c);
}

Client *
getclient(Window w, int create)
{
	Client *c;

	if (w == 0 || getscreen(w))
		return 0;

	for (c = clients; c; c = c->next)
		if (c->window == w || c->parent == w)
			return c;

	if (!create)
		return 0;

	c = (Client *) malloc(sizeof(Client));
	memset(c, 0, sizeof(Client));
	c->window = w;
	fprintf(stderr, "getclient: CREATED new client %p for window 0x%lx\n", (void*)c, w);
	/*
	 * c->parent will be set by the caller 
	 */
	c->parent = None;
	c->reparenting = 0;
	c->state = WithdrawnState;
	c->init = 0;
	c->cmap = None;
	c->label = c->class = 0;
	c->revert = 0;
	c->hold = 0;
	c->ncmapwins = 0;
	c->cmapwins = 0;
	c->wmcmaps = 0;
	
	/* Initialize workspace fields */
	c->workspace = -1;
	c->workspace_next = NULL;
	c->workspace_prev = NULL;
	fprintf(stderr, "getclient: initialized client %p with workspace=-1\n", (void*)c);
	
	/* Initialize titlebar fields */
	c->titlebar = None;
	c->title_width = 0;
	
	/* Initialize terminal-launcher fields */
	c->is_terminal = 0;
	c->terminal_parent = NULL;
	c->launched_child = NULL;
	c->saved_x = c->saved_y = 0;
	c->saved_dx = c->saved_dy = 0;
	
	c->next = clients;
	clients = c;
	fprintf(stderr, "getclient: added client %p to global clients list\n", (void*)c);
	return c;
}

void
rmclient(Client * c)
{
	Client *cc;

	for (cc = current; cc && cc->revert; cc = cc->revert)
		if (cc->revert == c)
			cc->revert = cc->revert->revert;

	if (c == clients)
		clients = c->next;
	for (cc = clients; cc && cc->next; cc = cc->next)
		if (cc->next == c)
			cc->next = cc->next->next;

	if (hidden(c))
		unhidec(c, 0);

	/* Remove client from workspace - with safety check */
	if (c && c->workspace >= 0) {
		workspace_remove_client(c);
	}

	/* Destroy titlebar if it exists */
	destroy_titlebar(c);

	if (c->parent != c->screen->root)
		XDestroyWindow(dpy, c->parent);

	c->parent = c->window = None;	/* paranoia */
	if (current == c) {
		current = c->revert;
		if (current == 0)
			nofocus();
		else {
			if (current->screen != c->screen)
				cmapnofocus(c->screen);
			setactive(current, 1);
		}
	}
	if (c->ncmapwins != 0) {
		XFree((char *) c->cmapwins);
		free((char *) c->wmcmaps);
	}
	if (c->iconname != 0)
		XFree((char *) c->iconname);
	if (c->name != 0)
		XFree((char *) c->name);
	if (c->instance != 0)
		XFree((char *) c->instance);
	if (c->class != 0)
		XFree((char *) c->class);
	memset(c, 0, sizeof(Client));	/* paranoia */
	free(c);
}

#ifdef	DEBUG
void
dump_revert(void)
{
	Client *c;
	int i;

	i = 0;
	for (c = current; c; c = c->revert) {
		fprintf(stderr, "%s(%lx:%d)", c->label ? c->label : "?", c->window, c->state);
		if (i++ > 100)
			break;
		if (c->revert)
			fprintf(stderr, " -> ");
	}
	if (current == 0)
		fprintf(stderr, "empty");
	fprintf(stderr, "\n");
}

void
dump_clients(void)
{
	Client *c;

	for (c = clients; c; c = c->next)
		fprintf(stderr, "w 0x%lx parent 0x%lx @ (%d, %d)\n", c->window, c->parent, c->x, c->y);
}
#endif

static void
draw_rounded_border(Client *c, int active)
{
	unsigned long bg_color = active ? c->screen->active : c->screen->inactive;
	int width = c->dx + 2 * (BORDER - 1);
	int height = c->dy + 2 * (BORDER - 1);
	int radius = config.rounding_radius;
	GC bg_gc;
	XGCValues gcv;
	
	if (config.show_titlebars) {
		height += config.titlebar_height;
	}
	
	/* Create GC for background drawing */
	gcv.foreground = bg_color;
	bg_gc = XCreateGC(dpy, c->parent, GCForeground, &gcv);
	
	/* Clear the window first */
	XClearWindow(dpy, c->parent);
	
	/* Draw rounded background */
	if (radius * 2 <= width && radius * 2 <= height && radius > 0) {
		/* Draw the main rectangles for background */
		XFillRectangle(dpy, c->parent, bg_gc, radius, 0, width - 2 * radius, height);
		XFillRectangle(dpy, c->parent, bg_gc, 0, radius, width, height - 2 * radius);
		
		/* Draw rounded corners for background */
		XFillArc(dpy, c->parent, bg_gc, 0, 0, radius * 2, radius * 2, 90 * 64, 90 * 64);
		XFillArc(dpy, c->parent, bg_gc, width - radius * 2, 0, radius * 2, radius * 2, 0 * 64, 90 * 64);
		XFillArc(dpy, c->parent, bg_gc, 0, height - radius * 2, radius * 2, radius * 2, 180 * 64, 90 * 64);
		XFillArc(dpy, c->parent, bg_gc, width - radius * 2, height - radius * 2, radius * 2, radius * 2, 270 * 64, 90 * 64);
		
		/* Only draw border outline when window is held (like original behavior) */
		if (c->hold && active) {
			XGCValues border_gcv;
			GC border_gc;
			int border_inset = 1;
			int inner_radius;
			
			border_gcv.foreground = c->screen->black;
			border_gc = XCreateGC(dpy, c->parent, GCForeground, &border_gcv);
			
			/* Draw a simple inset rounded border */
			inner_radius = radius - border_inset;
			if (inner_radius > 0) {
				/* Draw corner arcs only */
				XDrawArc(dpy, c->parent, border_gc, border_inset, border_inset, 
					inner_radius * 2, inner_radius * 2, 90 * 64, 90 * 64);
				XDrawArc(dpy, c->parent, border_gc, width - inner_radius * 2 - border_inset, border_inset, 
					inner_radius * 2, inner_radius * 2, 0 * 64, 90 * 64);
				XDrawArc(dpy, c->parent, border_gc, border_inset, height - inner_radius * 2 - border_inset, 
					inner_radius * 2, inner_radius * 2, 180 * 64, 90 * 64);
				XDrawArc(dpy, c->parent, border_gc, width - inner_radius * 2 - border_inset, height - inner_radius * 2 - border_inset, 
					inner_radius * 2, inner_radius * 2, 270 * 64, 90 * 64);
				
				/* Connect the arcs with straight lines */
				XDrawLine(dpy, c->parent, border_gc, inner_radius + border_inset, border_inset, 
					width - inner_radius - border_inset, border_inset);
				XDrawLine(dpy, c->parent, border_gc, inner_radius + border_inset, height - border_inset - 1, 
					width - inner_radius - border_inset, height - border_inset - 1);
				XDrawLine(dpy, c->parent, border_gc, border_inset, inner_radius + border_inset, 
					border_inset, height - inner_radius - border_inset);
				XDrawLine(dpy, c->parent, border_gc, width - border_inset - 1, inner_radius + border_inset, 
					width - border_inset - 1, height - inner_radius - border_inset);
			}
			
			XFreeGC(dpy, border_gc);
		}
	} else {
		/* Fallback to regular rectangle */
		XFillRectangle(dpy, c->parent, bg_gc, 0, 0, width, height);
		
		/* Draw regular border if held */
		if (c->hold && active) {
			XGCValues border_gcv;
			GC border_gc;
			border_gcv.foreground = c->screen->black;
			border_gc = XCreateGC(dpy, c->parent, GCForeground, &border_gcv);
			XDrawRectangle(dpy, c->parent, border_gc, INSET, INSET, width - 2 * INSET, height - 2 * INSET);
			XFreeGC(dpy, border_gc);
		}
	}
	
	XFreeGC(dpy, bg_gc);
}
