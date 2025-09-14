/*
 * Copyright multiple authors, see README for licence details
 */
#include <stdio.h>
#include <ctype.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"
#include "config.h"

static char* prepare_menu_text(const char* original, char* buffer, int buffer_size);

#ifdef XFT
int get_text_width(const char* text);
int get_font_ascent(void);
int get_font_descent(void);
static void draw_text(ScreenInfo *s, Window win, int x, int y, const char* text, int highlight);
#endif

static char*
prepare_menu_text(const char* original, char* buffer, int buffer_size)
{
	int i;
	
	if (!original || !buffer || buffer_size <= 0) {
		return (char*)original;
	}
	
	if (config.lower) {
		/* Convert to lowercase */
		for (i = 0; i < buffer_size - 1 && original[i] != '\0'; i++) {
			buffer[i] = tolower(original[i]);
		}
		buffer[i] = '\0';
		return buffer;
	} else {
		/* Return original text */
		return (char*)original;
	}
}

#ifdef XFT
int
get_text_width(const char* text)
{
	if (use_xft && xft_font) {
		XGlyphInfo extents;
		XftTextExtentsUtf8(dpy, xft_font, (const FcChar8*)text, strlen(text), &extents);
		return extents.width;
	} else if (font) {
		return XTextWidth(font, text, strlen(text));
	}
	return 0;
}

int
get_font_ascent(void)
{
	if (use_xft && xft_font) {
		return xft_font->ascent;
	} else if (font) {
		return font->ascent;
	}
	return 0;
}

int
get_font_descent(void)
{
	if (use_xft && xft_font) {
		return xft_font->descent;
	} else if (font) {
		return font->descent;
	}
	return 0;
}

static void
draw_text(ScreenInfo *s, Window win, int x, int y, const char* text, int highlight)
{
	if (use_xft && xft_font && s->xft_draw) {
		/* Update XftDraw drawable to current window */
		XftDrawChange(s->xft_draw, win);
		XftDrawStringUtf8(s->xft_draw, 
			highlight ? &s->xft_highlight_color : &s->xft_color,
			xft_font, x, y, (const FcChar8*)text, strlen(text));
	} else if (font) {
		/* Use traditional X11 text drawing */
		XDrawString(dpy, win, 
			highlight ? s->menu_highlight_text_gc : s->text_gc,
			x, y, text, strlen(text));
	}
}
#endif

int
nobuttons(XButtonEvent * e)
{
	int state;

	state = (e->state & AllButtonMask);
	return (e->type == ButtonRelease) && (state & (state - 1)) == 0;
}

int
grab(Window w, Window constrain, int mask, Cursor curs, int t)
{
	int status;

	if (t == 0)
		t = timestamp();
	status = XGrabPointer(dpy, w, False, mask, GrabModeAsync, GrabModeAsync, constrain, curs, t);
	return status;
}

void
ungrab(XButtonEvent * e)
{
	XEvent ev;

	if (!nobuttons(e))
		for (;;) {
			XMaskEvent(dpy, ButtonMask | ButtonMotionMask, &ev);
			if (ev.type == MotionNotify)
				continue;
			e = &ev.xbutton;
			if (nobuttons(e))
				break;
		}
	XUngrabPointer(dpy, e->time);
	curtime = e->time;
}

int
menuhit(XButtonEvent * e, Menu * m)
{
	XEvent ev;
	int i, n, cur, old, wide, high, status, drawn, warp;
	int x, y, dx, dy, xmax, ymax;
	int tx, ty;
	int submenu_active = -1, in_submenu = 0, submenu_cur = -1;
	Time submenu_hide_time = 0;  /* When to hide submenu (0 = don't hide) */
	const int SUBMENU_DELAY_MS = 250;  /* 250ms delay before hiding */
	ScreenInfo *s;

#ifdef XFT
	if (!use_xft && font == 0) {
#else
	if (font == 0) {
#endif
		/* ERROR: No font available for menu rendering */;
		return -1;
	}
	
#ifdef XFT
	if (use_xft && xft_font) {
	} else
#endif
	if (font) {
	}
	s = getscreen(e->root);
	if (s == 0 || e->window == s->menuwin)	/* ugly event mangling */
		return -1;

	dx = 0;
	for (n = 0; m->item[n]; n++) {
#ifdef XFT
		wide = get_text_width(m->item[n]) + 4;
#else
		wide = XTextWidth(font, m->item[n], strlen(m->item[n])) + 4;
#endif
		if (wide > dx)
			dx = wide;
	}
	wide = dx;
	cur = m->lasthit;
	if (cur >= n)
		cur = n - 1;

#ifdef XFT
	high = get_font_ascent() + get_font_descent() + 1;
#else
	high = font->ascent + font->descent + 1;
#endif
	dy = n * high;
	x = e->x - wide / 2;
	y = e->y - cur * high - high / 2;
	warp = 0;
	xmax = DisplayWidth(dpy, s->num);
	ymax = DisplayHeight(dpy, s->num);
	if (x < 0) {
		e->x -= x;
		x = 0;
		warp++;
	}
	if (x + wide >= xmax) {
		e->x -= x + wide - xmax;
		x = xmax - wide;
		warp++;
	}
	if (y < 0) {
		e->y -= y;
		y = 0;
		warp++;
	}
	if (y + dy >= ymax) {
		e->y -= y + dy - ymax;
		y = ymax - dy;
		warp++;
	}
	if (warp)
		setmouse(e->x, e->y, s);
	XMoveResizeWindow(dpy, s->menuwin, x, y, dx, dy);
	XSelectInput(dpy, s->menuwin, MenuMask);
	
	
	XMapRaised(dpy, s->menuwin);
	status = grab(s->menuwin, None, MenuGrabMask, None, e->time);
	if (status != GrabSuccess) {
		/*
		 * graberror("menuhit", status); 
		 */
		XUnmapWindow(dpy, s->menuwin);
		return -1;
	}
	drawn = 0;
	for (;;) {
		XMaskEvent(dpy, MenuMask, &ev);
		
		/* Check if submenu hide timer has expired */
		if (submenu_hide_time > 0) {
			Time current_time = 0;
			/* Get current time from the event */
			switch (ev.type) {
			case ButtonPress:
			case ButtonRelease:
				current_time = ev.xbutton.time;
				break;
			case MotionNotify:
				current_time = ev.xmotion.time;
				break;
			default:
				current_time = CurrentTime;
				break;
			}
			
			if (current_time != CurrentTime && current_time >= submenu_hide_time) {
				/* Before hiding, check if mouse is actually over submenu */
				Window root_return, child_return;
				int root_x, root_y, win_x, win_y;
				unsigned int mask;
				
				if (XQueryPointer(dpy, s->root, &root_return, &child_return,
				                 &root_x, &root_y, &win_x, &win_y, &mask)) {
					
					/* Use stored absolute coordinates for comparison */
					if (root_x >= s->submenu_x && root_x < s->submenu_x + (int)s->submenu_w &&
					    root_y >= s->submenu_y && root_y < s->submenu_y + (int)s->submenu_h) {
						/* Mouse is over submenu - don't hide, reset timer */
						submenu_hide_time = current_time + SUBMENU_DELAY_MS;
					} else if (child_return == s->submenuwin) {
						/* XQueryPointer says mouse is in submenu window */
						submenu_hide_time = current_time + SUBMENU_DELAY_MS;
					} else {
						/* Mouse is not over submenu - hide it */
						hide_submenu_for(s);
						submenu_active = -1;
						in_submenu = 0;
						submenu_hide_time = 0;
					}
				}
			}
		}
		
		switch (ev.type) {
		default:
			fprintf(stderr, "9wm: menuhit: unknown ev.type %d\n", ev.type);
			break;
		case ButtonPress:
			break;
		case ButtonRelease:
			if (ev.xbutton.button != e->button)
				break;
			x = ev.xbutton.x;
			y = ev.xbutton.y;
			i = y / high;
			if (cur >= 0 && y >= cur * high - 3 && y < (cur + 1) * high + 3)
				i = cur;
			if (x < 0 || x > wide || y < -3)
				i = -1;
			else if (i < 0 || i >= n)
				i = -1;
			else
				m->lasthit = i;
			if (!nobuttons(&ev.xbutton))
				i = -1;
			
			/* Check if click was on submenu window or submenu area */
			if (submenu_active >= 0 && (ev.xbutton.window == s->submenuwin || 
			    (in_submenu && submenu_cur >= 0))) {
				/* Click was on submenu - handle submenu selection */
				int sub_result = -1;
				int sub_item_height;
				
#ifdef XFT
				sub_item_height = get_font_ascent() + get_font_descent() + 1;
#else
				sub_item_height = font->ascent + font->descent + 1;
#endif
				
				/* Calculate which submenu item was clicked */
				if (ev.xbutton.window == s->submenuwin) {
					/* Direct click on submenu window */
					sub_result = ev.xbutton.y / sub_item_height;
				} else {
					/* Click through main menu while over submenu - use tracked position */
					sub_result = submenu_cur;
				}
				
				/* Validate submenu item selection */
				if (sub_result >= 0 && sub_result < config.menu_items[submenu_active].submenu_count) {
					ungrab(&ev.xbutton);
					XUnmapWindow(dpy, s->menuwin);
					hide_submenu_for(s);
					
					/* Return encoded submenu result: 1000 + (parent_index * 100) + sub_index */
					return 1000 + (submenu_active * 100) + sub_result;
				} else {
					/* Invalid click area - ignore */
					break;
				}
			}
			
			ungrab(&ev.xbutton);
			XUnmapWindow(dpy, s->menuwin);
			hide_submenu_for(s);
			return i;
		case MotionNotify:
			if (!drawn)
				break;
			
			/* Check if motion is over submenu window */
			if (submenu_active >= 0 && ev.xmotion.window == s->submenuwin) {
				/* Mouse is over submenu - cancel hide timer and set submenu state */
				submenu_hide_time = 0;
				cur = submenu_active;
				in_submenu = 1;
				
				/* Calculate which submenu item is hovered for future highlighting */
				int sub_item_height;
#ifdef XFT
				sub_item_height = get_font_ascent() + get_font_descent() + 1;
#else
				sub_item_height = font->ascent + font->descent + 1;
#endif
				int new_submenu_cur = ev.xmotion.y / sub_item_height;
				if (new_submenu_cur < 0 || new_submenu_cur >= config.menu_items[submenu_active].submenu_count) {
					new_submenu_cur = -1;
				}
				
				/* Trigger redraw if submenu selection changed */
				if (new_submenu_cur != submenu_cur) {
					submenu_cur = new_submenu_cur;
					/* Trigger expose event to redraw submenu with new highlighting */
					XClearArea(dpy, s->submenuwin, 0, 0, 0, 0, True);
				}
				
				old = cur; /* Prevent unnecessary updates */
				break;
			}
			
			x = ev.xmotion.x;
			y = ev.xmotion.y;
			old = cur;
			cur = y / high;
			if (old >= 0 && y >= old * high - 3 && y < (old + 1) * high + 3)
				cur = old;
			
			/* Reset submenu state when over main menu */
			in_submenu = 0;
			/* Check if mouse is outside main menu */
			if (x < 0 || x > wide || y < -3) {
				/* If outside main menu, check if we're over the submenu */
				if (submenu_active >= 0) {
					/* Convert mouse coordinates to root coordinates */
					Window child;
					int root_x, root_y;
					XTranslateCoordinates(dpy, s->menuwin, s->root, x, y, &root_x, &root_y, &child);
					
					/* Check if mouse is over submenu using stored coordinates */
					if (root_x >= s->submenu_x && root_x < s->submenu_x + (int)s->submenu_w &&
					    root_y >= s->submenu_y && root_y < s->submenu_y + (int)s->submenu_h) {
						/* Mouse is over submenu - keep current state and cancel hide timer */
						cur = submenu_active;
						in_submenu = 1;
						submenu_hide_time = 0;  /* Cancel hide timer */
						
						/* Calculate which submenu item is hovered */
						int sub_item_height;
#ifdef XFT
						sub_item_height = get_font_ascent() + get_font_descent() + 1;
#else
						sub_item_height = font->ascent + font->descent + 1;
#endif
						/* Calculate submenu item based on position relative to submenu window */
						int new_submenu_cur = (root_y - s->submenu_y) / sub_item_height;
						if (new_submenu_cur < 0 || new_submenu_cur >= config.menu_items[submenu_active].submenu_count) {
							new_submenu_cur = -1;
						}
						
						/* Trigger redraw if submenu selection changed */
						if (new_submenu_cur != submenu_cur) {
							submenu_cur = new_submenu_cur;
							/* Trigger expose event to redraw submenu with new highlighting */
							XClearArea(dpy, s->submenuwin, 0, 0, 0, 0, True);
						}
					} else {
						/* Mouse is outside both menus */
						cur = -1;
						submenu_cur = -1;
					}
				} else {
					cur = -1;
				}
			}
			else if (cur < 0 || cur >= n)
				cur = -1;
			/* Handle submenu hover logic */
			if (cur != old) {
				/* Handle submenu timing when moving to different item */
				if (submenu_active != cur && !in_submenu) {
					if (submenu_active >= 0) {
						/* Start hide timer instead of immediately hiding */
						submenu_hide_time = ev.xmotion.time + SUBMENU_DELAY_MS;
					}
				} else if (submenu_active == cur) {
					/* Mouse returned to folder item - cancel hide timer */
					submenu_hide_time = 0;
				}
				
				/* Show submenu for folder items */
				if (cur >= 0 && cur < config.menu_count && 
				    config.menu_items[cur].is_folder) {
					/* Get actual menu window position */
					Window root_return;
					int menu_x, menu_y;
					unsigned int menu_width, menu_height, border_width, depth;
					
					XGetGeometry(dpy, s->menuwin, &root_return, &menu_x, &menu_y, 
					            &menu_width, &menu_height, &border_width, &depth);
					
					
					show_submenu_at(e, cur, s, menu_x, menu_y, menu_width, high);
					submenu_active = cur;
				}
			}
			
			if (cur == old)
				break;
			if (old >= 0 && old < n) {
				/* Redraw old item normally */
				char *item = m->item[old];
				char text_buffer[256];
				char *display_text;
				int tx, ty;
				
				/* Clear the area completely first */
				XClearArea(dpy, s->menuwin, 0, old * high, wide, high, False);
				
				/* Prepare text (with optional lowercase) */
				display_text = prepare_menu_text(item, text_buffer, sizeof(text_buffer));
				
				/* Center all text */
#ifdef XFT
				tx = (wide - get_text_width(display_text)) / 2;
				ty = old * high + get_font_ascent() + 1;
				draw_text(s, s->menuwin, tx, ty, display_text, 0);
#else
				tx = (wide - XTextWidth(font, display_text, strlen(display_text))) / 2;
				ty = old * high + font->ascent + 1;
				XDrawString(dpy, s->menuwin, s->text_gc, tx, ty, display_text, strlen(display_text));
#endif
			}
			if (cur >= 0 && cur < n) {
				/* Draw current item highlighted */
				char *item = m->item[cur];
				char text_buffer[256];
				char *display_text;
				int tx, ty;
				
				/* Fill with blue background */
				XFillRectangle(dpy, s->menuwin, s->menu_highlight_gc, 0, cur * high, wide, high);
				
				/* Prepare text (with optional lowercase) */
				display_text = prepare_menu_text(item, text_buffer, sizeof(text_buffer));
				
				/* Center all text */
#ifdef XFT
				tx = (wide - get_text_width(display_text)) / 2;
				ty = cur * high + get_font_ascent() + 1;
				draw_text(s, s->menuwin, tx, ty, display_text, 1);
#else
				tx = (wide - XTextWidth(font, display_text, strlen(display_text))) / 2;
				ty = cur * high + font->ascent + 1;
				XDrawString(dpy, s->menuwin, s->menu_highlight_text_gc, tx, ty, display_text, strlen(display_text));
#endif
			}
			break;
		case Expose:
			if (ev.xexpose.window == s->submenuwin && submenu_active >= 0) {
				/* Handle submenu expose */
				int sub_n, sub_wide, sub_high, sub_i, sub_tx, sub_ty;
				char **sub_items;
				
				/* Build submenu if needed */
				build_submenu_for_rendering(submenu_active);
				sub_items = get_submenu_items();
				
				/* Calculate submenu dimensions */
				sub_wide = 0;
				for (sub_n = 0; sub_items[sub_n]; sub_n++) {
#ifdef XFT
					int item_width = get_text_width(sub_items[sub_n]) + 4;
#else
					int item_width = XTextWidth(font, sub_items[sub_n], strlen(sub_items[sub_n])) + 4;
#endif
					if (item_width > sub_wide)
						sub_wide = item_width;
				}
				
#ifdef XFT
				sub_high = get_font_ascent() + get_font_descent() + 1;
#else
				sub_high = font->ascent + font->descent + 1;
#endif
				
				XClearWindow(dpy, s->submenuwin);
				for (sub_i = 0; sub_i < sub_n; sub_i++) {
					char *sub_item = sub_items[sub_i];
					char sub_text_buffer[256];
					char *sub_display_text;
					
					/* Prepare text (with optional lowercase) */
					sub_display_text = prepare_menu_text(sub_item, sub_text_buffer, sizeof(sub_text_buffer));
					
					/* Center all text */
#ifdef XFT
					sub_tx = (sub_wide - get_text_width(sub_display_text)) / 2;
					sub_ty = sub_i * sub_high + get_font_ascent() + 1;
					draw_text(s, s->submenuwin, sub_tx, sub_ty, sub_display_text, 0);
#else
					sub_tx = (sub_wide - XTextWidth(font, sub_display_text, strlen(sub_display_text))) / 2;
					sub_ty = sub_i * sub_high + font->ascent + 1;
					XDrawString(dpy, s->submenuwin, s->text_gc, sub_tx, sub_ty, sub_display_text, strlen(sub_display_text));
#endif
				}
				
				/* Draw highlighted submenu item if any */
				if (submenu_cur >= 0 && submenu_cur < sub_n) {
					char *sub_item = sub_items[submenu_cur];
					char sub_text_buffer[256];
					char *sub_display_text;
					
					/* Fill with blue background */
					XFillRectangle(dpy, s->submenuwin, s->menu_highlight_gc, 0, submenu_cur * sub_high, sub_wide, sub_high);
					
					/* Prepare text (with optional lowercase) */
					sub_display_text = prepare_menu_text(sub_item, sub_text_buffer, sizeof(sub_text_buffer));
					
					/* Center highlighted text */
#ifdef XFT
					sub_tx = (sub_wide - get_text_width(sub_display_text)) / 2;
					sub_ty = submenu_cur * sub_high + get_font_ascent() + 1;
					draw_text(s, s->submenuwin, sub_tx, sub_ty, sub_display_text, 1);
#else
					sub_tx = (sub_wide - XTextWidth(font, sub_display_text, strlen(sub_display_text))) / 2;
					sub_ty = submenu_cur * sub_high + font->ascent + 1;
					XDrawString(dpy, s->submenuwin, s->menu_highlight_text_gc, sub_tx, sub_ty, sub_display_text, strlen(sub_display_text));
#endif
				}
			} else {
				/* Handle main menu expose */
				XClearWindow(dpy, s->menuwin);
			for (i = 0; i < n; i++) {
				char *item = m->item[i];
				char text_buffer[256];
				char *display_text;
				
				/* Prepare text (with optional lowercase) */
				display_text = prepare_menu_text(item, text_buffer, sizeof(text_buffer));
				
				/* Center all text */
#ifdef XFT
				tx = (wide - get_text_width(display_text)) / 2;
				ty = i * high + get_font_ascent() + 1;
				draw_text(s, s->menuwin, tx, ty, display_text, 0);
#else
				tx = (wide - XTextWidth(font, display_text, strlen(display_text))) / 2;
				ty = i * high + font->ascent + 1;
				XDrawString(dpy, s->menuwin, s->text_gc, tx, ty, display_text, strlen(display_text));
#endif
			}
			if (cur >= 0 && cur < n) {
				/* Draw highlighted item */
				char *item = m->item[cur];
				char text_buffer[256];
				char *display_text;
				
				/* Fill with blue background */
				XFillRectangle(dpy, s->menuwin, s->menu_highlight_gc, 0, cur * high, wide, high);
				
				/* Prepare text (with optional lowercase) */
				display_text = prepare_menu_text(item, text_buffer, sizeof(text_buffer));
				
				/* Center all text */
#ifdef XFT
				tx = (wide - get_text_width(display_text)) / 2;
				ty = cur * high + get_font_ascent() + 1;
				draw_text(s, s->menuwin, tx, ty, display_text, 1);
#else
				tx = (wide - XTextWidth(font, display_text, strlen(display_text))) / 2;
				ty = cur * high + font->ascent + 1;
				XDrawString(dpy, s->menuwin, s->menu_highlight_text_gc, tx, ty, display_text, strlen(display_text));
#endif
			}
			}
			drawn = 1;
		}
	}
}

Client *
selectwin(int release, int *shift, ScreenInfo * s)
{
	XEvent ev;
	XButtonEvent *e;
	int status;
	Window w;
	Client *c;

	status = grab(s->root, s->root, ButtonMask, s->target, 0);
	if (status != GrabSuccess) {
		graberror("selectwin", status);	/* */
		return 0;
	}
	w = None;
	for (;;) {
		XMaskEvent(dpy, ButtonMask, &ev);
		e = &ev.xbutton;
		switch (ev.type) {
		case ButtonPress:
			if (e->button != Button3) {
				ungrab(e);
				return 0;
			}
			w = e->subwindow;
			if (!release) {
				c = getclient(w, 0);
				if (c == 0)
					ungrab(e);
				if (shift != 0)
					*shift = (e->state & ShiftMask) != 0;
				return c;
			}
			break;
		case ButtonRelease:
			ungrab(e);
			if (e->button != Button3 || e->subwindow != w)
				return 0;
			if (shift != 0)
				*shift = (e->state & ShiftMask) != 0;
			return getclient(w, 0);
		}
	}
}

void
sweepcalc(Client * c, int x, int y)
{
	int dx, dy, sx, sy;

	dx = x - c->x;
	dy = y - c->y;
	sx = sy = 1;
	if (dx < 0) {
		dx = -dx;
		sx = -1;
	}
	if (dy < 0) {
		dy = -dy;
		sy = -1;
	}

	dx -= 2 * BORDER;
	dy -= 2 * BORDER;
	if (c->size.flags & PResizeInc) {
		dx = c->min_dx + (dx - c->min_dx) / c->size.width_inc * c->size.width_inc;
		dy = c->min_dy + (dy - c->min_dy) / c->size.height_inc * c->size.height_inc;
	}

	if (c->size.flags & PMaxSize) {
		if (dx > c->size.max_width)
			dx = c->size.max_width;
		if (dy > c->size.max_height)
			dy = c->size.max_height;
	}
	c->dx = sx * (dx + 2 * BORDER);
	c->dy = sy * (dy + 2 * BORDER);
}

void
dragcalc(Client * c, int x, int y)
{
	/* Apply the stored offset so window moves relative to initial click position */
	c->x = x - c->drag_offset_x;
	c->y = y - c->drag_offset_y;
}

void
drawbound(Client * c)
{
	int x, y, dx, dy;
	ScreenInfo *s;

	s = c->screen;
	x = c->x;
	y = c->y;
	dx = c->dx;
	dy = c->dy;
	if (dx < 0) {
		x += dx;
		dx = -dx;
	}
	if (dy < 0) {
		y += dy;
		dy = -dy;
	}
	if (dx <= 2 || dy <= 2)
		return;
	XDrawRectangle(dpy, s->root, s->gc, x, y, dx - 1, dy - 1);
	XDrawRectangle(dpy, s->root, s->gc, x + 1, y + 1, dx - 3, dy - 3);
}

void
misleep(int msec)
{
	struct timeval t;

	t.tv_sec = msec / 1000;
	t.tv_usec = (msec % 1000) * 1000;
	select(0, 0, 0, 0, &t);
}

int
sweepdrag(Client * c, XButtonEvent * e0, void (*recalc) ())
{
	XEvent ev;
	int idle;
	int cx, cy, rx, ry;
	int ox, oy, odx, ody;
	XButtonEvent *e;

	ox = c->x;
	oy = c->y;
	odx = c->dx;
	ody = c->dy;
	c->x -= BORDER;
	c->y -= BORDER;
	c->dx += 2 * BORDER;
	c->dy += 2 * BORDER;
	if (e0) {
		c->x = cx = e0->x;
		c->y = cy = e0->y;
		recalc(c, e0->x, e0->y);
	} else
		getmouse(&cx, &cy, c->screen);
	XGrabServer(dpy);
	drawbound(c);
	idle = 0;
	for (;;) {
		if (XCheckMaskEvent(dpy, ButtonMask, &ev) == 0) {
			getmouse(&rx, &ry, c->screen);
			if (rx != cx || ry != cy || ++idle > 300) {
				drawbound(c);
				if (rx == cx && ry == cy) {
					XUngrabServer(dpy);
					XFlush(dpy);
					misleep(500);
					XGrabServer(dpy);
					idle = 0;
				}
				recalc(c, rx, ry);
				cx = rx;
				cy = ry;
				drawbound(c);
				XFlush(dpy);
			}
			misleep(50);
			continue;
		}
		e = &ev.xbutton;
		switch (ev.type) {
		case ButtonPress:
		case ButtonRelease:
			drawbound(c);
			ungrab(e);
			XUngrabServer(dpy);
			recalc(c, ev.xbutton.x, ev.xbutton.y);
			if (c->dx < 0) {
				c->x += c->dx;
				c->dx = -c->dx;
			}
			if (c->dy < 0) {
				c->y += c->dy;
				c->dy = -c->dy;
			}
			c->x += BORDER;
			c->y += BORDER;
			c->dx -= 2 * BORDER;
			c->dy -= 2 * BORDER;
			if (c->dx < 4 || c->dy < 4 || c->dx < c->min_dx || c->dy < c->min_dy)
				goto bad;
			return 1;
		}
	}
      bad:
	c->x = ox;
	c->y = oy;
	c->dx = odx;
	c->dy = ody;
	return 0;
}

int
sweep(Client * c)
{
	XEvent ev;
	int status;
	XButtonEvent *e;
	ScreenInfo *s;

	s = c->screen;
	status = grab(s->root, s->root, ButtonMask, s->sweep0, 0);
	if (status != GrabSuccess) {
		graberror("sweep", status);	/* */
		return 0;
	}

	XMaskEvent(dpy, ButtonMask, &ev);
	e = &ev.xbutton;
	if (e->button != Button3) {
		ungrab(e);
		return 0;
	}
	if (c->size.flags & (PMinSize | PBaseSize))
		setmouse(e->x + c->min_dx, e->y + c->min_dy, s);
	XChangeActivePointerGrab(dpy, ButtonMask, s->boxcurs, e->time);
	return sweepdrag(c, e, sweepcalc);
}

int
drag(Client * c)
{
	int status;
	ScreenInfo *s;

	s = c->screen;
	/* Store original window position and calculate mouse offset */
	int mouse_x, mouse_y;
	getmouse(&mouse_x, &mouse_y, s);	/* get current mouse position */
	
	/* Store the offset from mouse to window corner for drag calculations */
	c->drag_offset_x = mouse_x - (c->x - BORDER);
	c->drag_offset_y = mouse_y - (c->y - BORDER);
	status = grab(s->root, s->root, ButtonMask, s->boxcurs, 0);
	if (status != GrabSuccess) {
		graberror("drag", status);	/* */
		return 0;
	}
	return sweepdrag(c, 0, dragcalc);
}

void
getmouse(int *x, int *y, ScreenInfo * s)
{
	Window dw1, dw2;
	int t1, t2;
	unsigned int t3;

	XQueryPointer(dpy, s->root, &dw1, &dw2, x, y, &t1, &t2, &t3);
}

void
setmouse(int x, int y, ScreenInfo * s)
{
	XWarpPointer(dpy, None, s->root, None, None, None, None, x, y);
}

int
sweep_area(ScreenInfo * s, int *x, int *y, int *width, int *height)
{
	XEvent ev;
	int status;
	XButtonEvent *e;
	int x1, y1, x2, y2;
	int pressed = 0;
	int last_x2 = -1, last_y2 = -1;

	status = grab(s->root, s->root, ButtonMask | PointerMotionMask, s->sweep0, 0);
	if (status != GrabSuccess) {
		graberror("sweep_area", status);
		return 0;
	}

	XGrabServer(dpy);

	for (;;) {
		XMaskEvent(dpy, ButtonMask | PointerMotionMask, &ev);
		e = &ev.xbutton;
		
		switch (ev.type) {
		case ButtonPress:
			if (e->button != Button3) {
				XUngrabServer(dpy);
				ungrab(e);
				return 0;
			}
			x1 = e->x;
			y1 = e->y;
			pressed = 1;
			XChangeActivePointerGrab(dpy, ButtonMask | PointerMotionMask, s->boxcurs, e->time);
			break;
			
		case MotionNotify:
			if (!pressed)
				break;
			
			/* Erase previous box */
			if (last_x2 != -1) {
				int dx = last_x2 - x1;
				int dy = last_y2 - y1;
				int rx = x1, ry = y1;
				if (dx < 0) { rx = last_x2; dx = -dx; }
				if (dy < 0) { ry = last_y2; dy = -dy; }
				if (dx > 0 && dy > 0) {
					XDrawRectangle(dpy, s->root, s->gc, rx, ry, dx, dy);
				}
			}
			
			/* Draw new box */
			x2 = ev.xmotion.x;
			y2 = ev.xmotion.y;
			{
				int dx = x2 - x1;
				int dy = y2 - y1;
				int rx = x1, ry = y1;
				if (dx < 0) { rx = x2; dx = -dx; }
				if (dy < 0) { ry = y2; dy = -dy; }
				if (dx > 0 && dy > 0) {
					XDrawRectangle(dpy, s->root, s->gc, rx, ry, dx, dy);
				}
			}
			last_x2 = x2;
			last_y2 = y2;
			XFlush(dpy);
			break;
			
		case ButtonRelease:
			if (!pressed || e->button != Button3) {
				XUngrabServer(dpy);
				ungrab(e);
				return 0;
			}
			
			/* Erase final box */
			if (last_x2 != -1) {
				int dx = last_x2 - x1;
				int dy = last_y2 - y1;
				int rx = x1, ry = y1;
				if (dx < 0) { rx = last_x2; dx = -dx; }
				if (dy < 0) { ry = last_y2; dy = -dy; }
				if (dx > 0 && dy > 0) {
					XDrawRectangle(dpy, s->root, s->gc, rx, ry, dx, dy);
				}
			}
			
			x2 = e->x;
			y2 = e->y;
			XUngrabServer(dpy);
			ungrab(e);
			
			if (x1 > x2) { int tmp = x1; x1 = x2; x2 = tmp; }
			if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
			
			*x = x1;
			*y = y1;
			*width = x2 - x1;
			*height = y2 - y1;
			
			if (*width < 50 || *height < 50)
				return 0;
				
			return 1;
		}
	}
}

