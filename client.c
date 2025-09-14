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
	/* Regular rectangular border */
	XSetWindowBackground(dpy, c->parent, active ? c->screen->active : c->screen->inactive);
	XClearWindow(dpy, c->parent);
	
	if (c->hold && active) {
		XDrawRectangle(dpy, c->parent, c->screen->gc, INSET, INSET, 
			c->dx + BORDER - INSET, c->dy + BORDER - INSET);
	}
	
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

