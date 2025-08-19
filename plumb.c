/*
 * Plumber integration for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include "dat.h"
#include "fns.h"
#include "config.h"
#include "plumb.h"

/* Global plumber state */
int plumb_enabled = 0;
int plumb_fd = -1;

/* X11 selection atoms */
static Atom xa_primary;
static Atom xa_clipboard;
static Atom xa_string;
static Atom xa_utf8_string;

int
plumb_init(void)
{
	struct stat st;
	
	if (!config.plumb_enabled) {
		plumb_enabled = 0;
		return 0;
	}
	
	/* Check if plumber send port exists */
	if (stat(config.plumb_send_path, &st) != 0) {
		fprintf(stderr, "plumb: plumber send port not found at %s\n", 
		        config.plumb_send_path);
		plumb_enabled = 0;
		return 0;
	}
	
	/* Initialize X11 selection atoms */
	xa_primary = XInternAtom(dpy, "PRIMARY", False);
	xa_clipboard = XInternAtom(dpy, "CLIPBOARD", False);
	xa_string = XInternAtom(dpy, "STRING", False);
	xa_utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
	
	plumb_enabled = 1;
	fprintf(stderr, "plumb: plumber integration enabled\n");
	return 1;
}

void
plumb_cleanup(void)
{
	if (plumb_fd >= 0) {
		close(plumb_fd);
		plumb_fd = -1;
	}
	plumb_enabled = 0;
}

int
plumb_open_port(const char *port, int mode)
{
	/* For Plan 9 plumber, we don't open individual ports here.
	   Instead, we'll use the plumb command directly */
	if (!plumb_enabled)
		return -1;
		
	/* Return a dummy fd - we'll handle sending via plumb command */
	return 1;
}

void
plumb_close_port(int fd)
{
	if (fd >= 0)
		close(fd);
}

PlumbMsg*
plumb_msg_new(void)
{
	PlumbMsg *m;
	
	if ((m = malloc(sizeof(PlumbMsg))) == NULL)
		return NULL;
	
	memset(m, 0, sizeof(PlumbMsg));
	return m;
}

void
plumb_msg_free(PlumbMsg *m)
{
	PlumbAttr *a, *next;
	
	if (!m)
		return;
		
	free(m->src);
	free(m->dst);
	free(m->wdir);
	free(m->type);
	free(m->data);
	
	for (a = m->attr; a; a = next) {
		next = a->next;
		free(a->name);
		free(a->value);
		free(a);
	}
	
	free(m);
}

void
plumb_attr_add(PlumbMsg *m, const char *name, const char *value)
{
	PlumbAttr *a, **ap;
	
	if (!m || !name || !value)
		return;
		
	if ((a = malloc(sizeof(PlumbAttr))) == NULL)
		return;
	
	a->name = strdup(name);
	a->value = strdup(value);
	a->next = NULL;
	
	/* Check if strdup succeeded */
	if (!a->name || !a->value) {
		free(a->name);
		free(a->value);
		free(a);
		return;
	}
	
	/* Add to end of list */
	for (ap = &m->attr; *ap; ap = &(*ap)->next)
		;
	*ap = a;
}

char*
plumb_format_message(const char *src, const char *dst, const char *wdir,
                    const char *type, const char *data, PlumbAttr *attr)
{
	char *msg, *p;
	PlumbAttr *a;
	int len = 0;
	
	/* Calculate message length */
	len += strlen(src) + strlen(dst) + strlen(wdir) + strlen(type) + strlen(data);
	len += 16; /* for field separators and newlines */
	
	for (a = attr; a; a = a->next)
		len += strlen(a->name) + strlen(a->value) + 2;
	
	if ((msg = malloc(len + 1)) == NULL)
		return NULL;
	
	p = msg;
	p += sprintf(p, "%s\n%s\n%s\n%s\n", src, dst, wdir, type);
	
	/* Add attributes */
	for (a = attr; a; a = a->next)
		p += sprintf(p, "%s=%s\n", a->name, a->value);
	
	p += sprintf(p, "\n%s", data);
	
	return msg;
}

int
plumb_msg_send(PlumbMsg *m)
{
	char cmd[1024];
	int result;
	
	if (!m || !plumb_enabled || !m->data)
		return 0;
	
	/* Use the Plan 9 plumb command to send the message */
	snprintf(cmd, sizeof(cmd), "echo '%s' | plumb -s '%s' -d '%s' -w '%s' -t '%s' -i", 
	         m->data ? m->data : "",
	         m->src ? m->src : "shrub9",
	         m->dst ? m->dst : "edit", 
	         m->wdir ? m->wdir : getenv("HOME") ? getenv("HOME") : "/",
	         m->type ? m->type : "text");
	
	result = system(cmd);
	return (result == 0);
}

char*
plumb_get_window_selection(Window w)
{
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *prop_data = NULL;
	char *result = NULL;
	
	/* Try to get PRIMARY selection first */
	if (XGetWindowProperty(dpy, w, xa_primary, 0, PLUMB_MAX_TEXT/4, False,
	                      AnyPropertyType, &actual_type, &actual_format,
	                      &nitems, &bytes_after, &prop_data) == Success) {
		if (prop_data && nitems > 0) {
			result = malloc(nitems + 1);
			if (result) {
				memcpy(result, prop_data, nitems);
				result[nitems] = '\0';
			}
		}
		if (prop_data)
			XFree(prop_data);
	}
	
	return result;
}

char*
plumb_get_selection(Client *c, int x, int y)
{
	Window target_window;
	char *selection = NULL;
	
	if (!c || !plumb_enabled)
		return NULL;
	
	/* Try to get selection from the client window */
	target_window = c->window;
	
	/* Request current selection */
	XConvertSelection(dpy, xa_primary, xa_string, xa_primary, 
	                 target_window, CurrentTime);
	XFlush(dpy);
	
	/* For now, return a placeholder - in a real implementation,
	   we would need to handle SelectionNotify events properly */
	selection = plumb_get_window_selection(target_window);
	
	return selection;
}

int
plumb_is_text_selected(Client *c, int x, int y)
{
	/* Simple heuristic - assume text might be selected
	   In a real implementation, this would check for actual selection */
	return (c != NULL);
}

int
plumb_send_text(const char *text, Client *c)
{
	PlumbMsg *m;
	char workspace_str[16];
	int result = 0;
	
	if (!text || !c || !plumb_enabled)
		return 0;
	
	if ((m = plumb_msg_new()) == NULL)
		return 0;
	
	/* Set up plumber message with safe string handling */
	m->src = strdup("shrub9");
	if (!m->src) {
		plumb_msg_free(m);
		return 0;
	}
	
	m->dst = strdup("edit");  /* Default to edit port */
	if (!m->dst) {
		plumb_msg_free(m);
		return 0;
	}
	
	m->wdir = strdup(getenv("HOME") ? getenv("HOME") : "/");
	if (!m->wdir) {
		plumb_msg_free(m);
		return 0;
	}
	
	m->type = strdup("text");
	if (!m->type) {
		plumb_msg_free(m);
		return 0;
	}
	
	m->data = strdup(text);
	if (!m->data) {
		plumb_msg_free(m);
		return 0;
	}
	m->ndata = strlen(text);
	
	/* Add context attributes safely */
	if (c->class)
		plumb_attr_add(m, "class", c->class);
	if (c->name)
		plumb_attr_add(m, "window", c->name);
	
	snprintf(workspace_str, sizeof(workspace_str), "%d", c->workspace);
	plumb_attr_add(m, "workspace", workspace_str);
	
	result = plumb_msg_send(m);
	
	if (result)
		fprintf(stderr, "plumb: sent text '%.*s%s' from %s\n", 
		        (int)(strlen(text) > 20 ? 20 : strlen(text)), text,
		        strlen(text) > 20 ? "..." : "",
		        c->class ? c->class : "unknown");
	else
		fprintf(stderr, "plumb: failed to send text\n");
	
	plumb_msg_free(m);
	return result;
}

int
plumb_handle_selection(Client *c, int x, int y)
{
	char test_text[256];
	int result;
	
	if (!plumb_enabled || !c)
		return 0;
	
	/* For now, send a simple test message to verify plumber works
	   Real selection handling would need more complex X11 code */
	snprintf(test_text, sizeof(test_text), "test from %s at %d,%d", 
	         c->class ? c->class : "unknown", x, y);
	
	result = plumb_send_text(test_text, c);
	
	fprintf(stderr, "plumb: handled middle-click, sent test message, result=%d\n", result);
	
	/* For testing, always return 1 to consume the event */
	return 1;
}