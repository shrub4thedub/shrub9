/*
 * Spaces (workspace overview) for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "dat.h"
#include "fns.h"
#include "workspace.h"
#include "config.h"
#include "spaces.h"

SpacesView spaces_view = {0};

void
spaces_init(ScreenInfo *s)
{
	spaces_view.screen = s;
	spaces_view.active = 0;
	spaces_view.selected_workspace = current_workspace;
	spaces_view.drag_active = 0;
	spaces_view.drag_client = NULL;
	spaces_view.overlay = None;
}

void
spaces_show(ScreenInfo *s)
{
	XSetWindowAttributes attr;
	int screen_width, screen_height;
	
	if (spaces_view.active)
		return;
		
	screen_width = DisplayWidth(dpy, s->num);
	screen_height = DisplayHeight(dpy, s->num);
	
	/* Calculate grid layout */
	spaces_view.margin = screen_width * 0.1; /* 10% margin */
	spaces_view.grid_x = (screen_width - 2 * spaces_view.margin) / SPACES_GRID_SIZE;
	spaces_view.grid_y = (screen_height - 2 * spaces_view.margin) / SPACES_GRID_SIZE;
	spaces_view.cell_width = spaces_view.grid_x - 20; /* Small gap between cells */
	spaces_view.cell_height = spaces_view.grid_y - 20;
	
	/* Create overlay window */
	attr.override_redirect = True;
	attr.background_pixel = s->menu_bg;  /* Use menu background color */
	attr.border_pixel = s->menu_fg;      /* Use menu foreground color for border */
	attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | 
	                  PointerMotionMask | KeyPressMask;
	
	spaces_view.overlay = XCreateWindow(dpy, s->root, 0, 0, 
	                                   screen_width, screen_height, 1,
	                                   CopyFromParent, InputOutput, CopyFromParent,
	                                   CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
	                                   &attr);
	
	XMapRaised(dpy, spaces_view.overlay);
	XGrabKeyboard(dpy, spaces_view.overlay, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XSetInputFocus(dpy, spaces_view.overlay, RevertToParent, CurrentTime);
	
	spaces_view.active = 1;
	spaces_mode = 1;
	
	spaces_draw();
}

void
spaces_hide(void)
{
	if (!spaces_view.active)
		return;
		
	/* Ungrab everything first */
	XUngrabKeyboard(dpy, CurrentTime);
	XUngrabPointer(dpy, CurrentTime);
	
	/* Unmap and destroy the overlay window */
	if (spaces_view.overlay != None) {
		XUnmapWindow(dpy, spaces_view.overlay);
		XDestroyWindow(dpy, spaces_view.overlay);
		spaces_view.overlay = None;
	}
	
	/* Reset all state */
	spaces_view.active = 0;
	spaces_mode = 0;
	spaces_view.drag_active = 0;
	spaces_view.drag_client = NULL;
	spaces_view.selected_workspace = current_workspace;
	
	/* Ensure X server is synchronized first */
	XSync(dpy, False);
	XFlush(dpy);
	
	/* Restore proper focus after sync */
	if (current && current->screen) {
		active(current);
	}
}

void
spaces_draw(void)
{
	int i, j, ws, x, y;
	
	if (!spaces_view.active)
		return;
		
	XClearWindow(dpy, spaces_view.overlay);
	
	
	/* Always draw 9 workspace boxes in 3x3 grid */
	ws = 0;
	for (i = 0; i < SPACES_GRID_SIZE; i++) {
		for (j = 0; j < SPACES_GRID_SIZE; j++) {
			x = spaces_view.margin + j * spaces_view.grid_x + 10;
			y = spaces_view.margin + i * spaces_view.grid_y + 10;
			
			/* Draw workspace box even if workspace doesn't exist */
			spaces_draw_workspace(ws, x, y, spaces_view.cell_width, spaces_view.cell_height);
			ws++;
		}
	}
	
	
	XFlush(dpy);
}

void
spaces_draw_workspace(int ws, int x, int y, int width, int height)
{
	Client *c;
	char label[16];
	int label_x, label_y;
	int is_current = (ws == current_workspace);
	int is_valid = (ws < workspace_count);
	int is_drag_target = (spaces_view.drag_active && ws == spaces_view.selected_workspace);
	unsigned long bg_color, fg_color, border_color;
	
	/* Use menu colors instead of one color and its inverse */
	bg_color = spaces_view.screen->menu_bg;  /* Menu background - brown */
	fg_color = spaces_view.screen->menu_fg;  /* Menu foreground - cream */
	border_color = spaces_view.screen->menu_fg; /* Use menu foreground for borders too */
	
	/* Always draw the box outline */
	if (is_current && is_valid) {
		/* Current workspace - draw thick border with background interior */
		XSetForeground(dpy, spaces_view.screen->gc, border_color);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x - 2, y - 2, width + 4, height + 4);
		XSetForeground(dpy, spaces_view.screen->gc, bg_color);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x, y, width, height);
	} else if (is_drag_target && is_valid) {
		/* Drag target - highlight border using menu foreground */
		XSetForeground(dpy, spaces_view.screen->gc, border_color);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x - 3, y - 3, width + 6, height + 6);
		XSetForeground(dpy, spaces_view.screen->gc, bg_color);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x, y, width, height);
	} else if (is_valid) {
		/* Valid workspace - background interior with foreground border */
		XSetForeground(dpy, spaces_view.screen->gc, bg_color);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x, y, width, height);
		XSetForeground(dpy, spaces_view.screen->gc, fg_color);
		XDrawRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x, y, width - 1, height - 1);
	} else {
		/* Invalid workspace - use menu colors for consistency */
		XSetForeground(dpy, spaces_view.screen->gc, bg_color);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x, y, width, height);
		XSetForeground(dpy, spaces_view.screen->gc, fg_color);
		XDrawRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               x, y, width - 1, height - 1);
	}
	
	/* Draw workspace label */
	sprintf(label, "%d", ws + 1);
	if (font) {
		label_x = x + 5;
		label_y = y + font->ascent + 5;
		
		/* Use appropriate color for label based on background */
		if (is_current && is_valid) {
			/* Use foreground color on background for current workspace */
			XSetForeground(dpy, spaces_view.screen->text_gc, fg_color);
		} else if (is_valid) {
			/* Use foreground color for valid workspaces */
			XSetForeground(dpy, spaces_view.screen->text_gc, fg_color);
		} else {
			/* Use contrasting color for invalid workspaces */
			XSetForeground(dpy, spaces_view.screen->text_gc, bg_color);
		}
		
		XDrawString(dpy, spaces_view.overlay, spaces_view.screen->text_gc, 
		           label_x, label_y, label, strlen(label));
		
		/* Reset to foreground color */
		XSetForeground(dpy, spaces_view.screen->text_gc, fg_color);
	}
	
	/* Draw window thumbnails only for valid workspaces */
	if (is_valid) {
		for (c = workspaces[ws].clients; c; c = c->workspace_next) {
			if (normal(c)) {
				/* Skip drawing the window being dragged in its original location */
				if (spaces_view.drag_active && c == spaces_view.drag_client && 
				    ws == spaces_view.drag_start_ws) {
					continue;
				}
				spaces_draw_window_thumbnail(c, x, y, width, height);
			}
		}
	}
}

void
spaces_draw_window_thumbnail(Client *c, int ws_x, int ws_y, int ws_width, int ws_height)
{
	int screen_width, screen_height;
	int thumb_x, thumb_y, thumb_width, thumb_height;
	double scale_x, scale_y;
	int content_x, content_y, content_width, content_height;
	
	screen_width = DisplayWidth(dpy, spaces_view.screen->num);
	screen_height = DisplayHeight(dpy, spaces_view.screen->num);
	
	/* Use content area of workspace (exclude label area) */
	content_x = ws_x + 2;
	content_y = ws_y + (font ? font->ascent + font->descent + 10 : 20);
	content_width = ws_width - 4;
	content_height = ws_height - (content_y - ws_y) - 2;
	
	/* Ensure content area is valid */
	if (content_width <= 0 || content_height <= 0)
		return;
	
	/* Calculate scaling factors based on content area */
	scale_x = (double)content_width / screen_width;
	scale_y = (double)content_height / screen_height;
	
	/* Calculate thumbnail position and size */
	thumb_x = content_x + (int)(c->x * scale_x);
	thumb_y = content_y + (int)(c->y * scale_y);
	thumb_width = (int)(c->dx * scale_x);
	thumb_height = (int)(c->dy * scale_y);
	
	/* Minimum size for visibility */
	if (thumb_width < 2) thumb_width = 2;
	if (thumb_height < 2) thumb_height = 2;
	
	/* Clip to content area */
	if (thumb_x < content_x) {
		thumb_width -= (content_x - thumb_x);
		thumb_x = content_x;
	}
	if (thumb_y < content_y) {
		thumb_height -= (content_y - thumb_y);
		thumb_y = content_y;
	}
	if (thumb_x + thumb_width > content_x + content_width)
		thumb_width = content_x + content_width - thumb_x;
	if (thumb_y + thumb_height > content_y + content_height)
		thumb_height = content_y + content_height - thumb_y;
	
	/* Only draw if thumbnail is valid */
	if (thumb_width > 0 && thumb_height > 0) {
		/* Draw filled rectangle for window using menu foreground color */
		XSetForeground(dpy, spaces_view.screen->gc, spaces_view.screen->menu_fg);
		XFillRectangle(dpy, spaces_view.overlay, spaces_view.screen->gc, 
		               thumb_x, thumb_y, thumb_width, thumb_height);
	}
}

int
spaces_get_workspace_at_point(int x, int y)
{
	int grid_i, grid_j, ws;
	int cell_start_x, cell_start_y;
	
	/* Check if point is within grid area */
	if (x < spaces_view.margin || x > DisplayWidth(dpy, spaces_view.screen->num) - spaces_view.margin ||
	    y < spaces_view.margin || y > DisplayHeight(dpy, spaces_view.screen->num) - spaces_view.margin) {
		return -1;
	}
	
	/* Calculate which grid cell we're in */
	grid_j = (x - spaces_view.margin - 10) / spaces_view.grid_x;
	grid_i = (y - spaces_view.margin - 10) / spaces_view.grid_y;
	
	if (grid_i < 0 || grid_i >= SPACES_GRID_SIZE || 
	    grid_j < 0 || grid_j >= SPACES_GRID_SIZE) {
		return -1;
	}
	
	/* Check if we're actually within the cell bounds */
	cell_start_x = spaces_view.margin + grid_j * spaces_view.grid_x + 10;
	cell_start_y = spaces_view.margin + grid_i * spaces_view.grid_y + 10;
	
	if (x < cell_start_x || x > cell_start_x + spaces_view.cell_width ||
	    y < cell_start_y || y > cell_start_y + spaces_view.cell_height) {
		return -1;
	}
	
	ws = grid_i * SPACES_GRID_SIZE + grid_j;
	
	/* Return workspace number (0-8) regardless of validity - 
	   caller will check if it's valid */
	return ws;
}

Client*
spaces_get_client_at_point(int x, int y, int ws)
{
	Client *c;
	int ws_x, ws_y, ws_width, ws_height;
	int content_x, content_y, content_width, content_height;
	int thumb_x, thumb_y, thumb_width, thumb_height;
	int screen_width, screen_height;
	double scale_x, scale_y;
	int grid_i, grid_j;
	
	if (ws < 0 || ws >= workspace_count)
		return NULL;
	
	/* Calculate workspace cell position */
	grid_i = ws / SPACES_GRID_SIZE;
	grid_j = ws % SPACES_GRID_SIZE;
	ws_x = spaces_view.margin + grid_j * spaces_view.grid_x + 10;
	ws_y = spaces_view.margin + grid_i * spaces_view.grid_y + 10;
	ws_width = spaces_view.cell_width;
	ws_height = spaces_view.cell_height;
	
	/* Calculate content area (exclude label area) */
	content_x = ws_x + 2;
	content_y = ws_y + (font ? font->ascent + font->descent + 10 : 20);
	content_width = ws_width - 4;
	content_height = ws_height - (content_y - ws_y) - 2;
	
	if (content_width <= 0 || content_height <= 0)
		return NULL;
		
	screen_width = DisplayWidth(dpy, spaces_view.screen->num);
	screen_height = DisplayHeight(dpy, spaces_view.screen->num);
	
	/* Calculate scaling factors */
	scale_x = (double)content_width / screen_width;
	scale_y = (double)content_height / screen_height;
	
	/* Check each client in this workspace */
	for (c = workspaces[ws].clients; c; c = c->workspace_next) {
		if (normal(c)) {
			/* Calculate thumbnail position and size */
			thumb_x = content_x + (int)(c->x * scale_x);
			thumb_y = content_y + (int)(c->y * scale_y);
			thumb_width = (int)(c->dx * scale_x);
			thumb_height = (int)(c->dy * scale_y);
			
			/* Minimum size for visibility */
			if (thumb_width < 2) thumb_width = 2;
			if (thumb_height < 2) thumb_height = 2;
			
			/* Check if point is within this thumbnail */
			if (x >= thumb_x && x <= thumb_x + thumb_width &&
			    y >= thumb_y && y <= thumb_y + thumb_height) {
				return c;
			}
		}
	}
	
	return NULL;
}

void
spaces_handle_button(XButtonEvent *e)
{
	int ws;
	Client *c;
	
	if (e->type == ButtonPress) {
		if (e->button == Button3) {
			/* Right-click: start drag operation */
			ws = spaces_get_workspace_at_point(e->x, e->y);
			if (ws >= 0 && ws < workspace_count) {
				c = spaces_get_client_at_point(e->x, e->y, ws);
				if (c) {
					spaces_view.drag_active = 1;
					spaces_view.drag_client = c;
					spaces_view.drag_start_ws = ws;
					/* Don't exit spaces mode during drag */
					return;
				}
			}
		} else if (e->button == Button1) {
			/* Left-click: switch workspace */
			ws = spaces_get_workspace_at_point(e->x, e->y);
			if (ws >= 0 && ws < workspace_count) {
				if (ws != current_workspace) {
					workspace_switch(ws);
				}
				/* Always exit spaces mode when clicking on a valid workspace */
				spaces_hide();
			}
		}
	} else if (e->type == ButtonRelease) {
		if (spaces_view.drag_active && e->button == Button3) {
			/* End drag operation */
			ws = spaces_get_workspace_at_point(e->x, e->y);
			if (ws >= 0 && ws < workspace_count && ws != spaces_view.drag_start_ws) {
				/* Move window to target workspace */
				fprintf(stderr, "spaces: dragging client %p from workspace %d to %d\n", 
					(void*)spaces_view.drag_client, spaces_view.drag_start_ws, ws);
				workspace_move_client(spaces_view.drag_client, ws);
				/* Rebuild menu since workspace contents have changed */
				rebuild_menu();
				spaces_draw(); /* Redraw to show new layout */
				fprintf(stderr, "spaces: drag operation completed\n");
			} else {
				fprintf(stderr, "spaces: drag operation cancelled or invalid target\n");
			}
			/* Reset drag state */
			spaces_view.drag_active = 0;
			spaces_view.drag_client = NULL;
			spaces_view.drag_start_ws = -1;
			spaces_view.selected_workspace = current_workspace;
			/* Redraw to clear any drag highlights */
			spaces_draw();
		}
	}
}

void
spaces_handle_motion(XMotionEvent *e)
{
	int ws;
	
	if (spaces_view.drag_active) {
		/* Update highlight for drag target */
		ws = spaces_get_workspace_at_point(e->x, e->y);
		if (ws >= 0 && ws != spaces_view.selected_workspace) {
			spaces_view.selected_workspace = ws;
			spaces_draw(); /* Redraw to show drag feedback */
		}
	} else {
		/* Normal hover highlighting */
		ws = spaces_get_workspace_at_point(e->x, e->y);
		if (ws >= 0 && ws != spaces_view.selected_workspace) {
			spaces_view.selected_workspace = ws;
		}
	}
}

void
spaces_handle_key(XKeyEvent *e)
{
	KeySym keysym = XLookupKeysym(e, 0);
	
	switch (keysym) {
	case XK_Escape:
		if (spaces_view.drag_active) {
			/* Cancel drag operation */
			spaces_view.drag_active = 0;
			spaces_view.drag_client = NULL;
			spaces_view.drag_start_ws = -1;
			spaces_view.selected_workspace = current_workspace;
			spaces_draw();
		} else {
			spaces_hide();
		}
		break;
	case XK_Return:
		/* Switch to selected workspace and exit */
		if (spaces_view.selected_workspace >= 0 && 
		    spaces_view.selected_workspace != current_workspace) {
			workspace_switch(spaces_view.selected_workspace);
		}
		spaces_hide();
		break;
	}
}