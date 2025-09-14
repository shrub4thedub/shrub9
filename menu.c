/*
 * Copyright multiple authors, see README for licence details
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "dat.h"
#include "fns.h"
#include "config.h"
#include "spaces.h"

Client *hiddenc[MAXHIDDEN];

int numhidden;

char *b3items[CONFIG_MAX_MENU_ITEMS + MAXHIDDEN + 1];

Menu b3menu = {
	b3items,
};

static int current_submenu = -1;
static char *submenu_items[CONFIG_MAX_SUBMENU_ITEMS + 1];
static Menu submenu = {
	submenu_items,
};

static void
build_submenu(int menu_idx)
{
	int i;
	
	/* Clear submenu */
	for (i = 0; i < CONFIG_MAX_SUBMENU_ITEMS + 1; i++)
		submenu_items[i] = NULL;
	
	if (menu_idx >= 0 && menu_idx < config.menu_count && 
	    config.menu_items[menu_idx].is_folder &&
	    config.menu_items[menu_idx].submenu_items) {
		
		/* Add submenu items */
		for (i = 0; i < config.menu_items[menu_idx].submenu_count && i < CONFIG_MAX_SUBMENU_ITEMS; i++) {
			if (config.menu_items[menu_idx].submenu_items[i].label[0] != '\0') {
				submenu_items[i] = config.menu_items[menu_idx].submenu_items[i].label;
			}
		}
	}
	
	/* Null terminate */
	submenu_items[i] = NULL;
}

int
show_submenu_at(XButtonEvent *e, int menu_idx, ScreenInfo *s, int main_x, int main_y, int main_width, int item_height)
{
	int sub_dx = 0, sub_dy, n, i, wide;
	int x, y;
	
	if (menu_idx < 0 || menu_idx >= config.menu_count || 
	    !config.menu_items[menu_idx].is_folder ||
	    !config.menu_items[menu_idx].submenu_items ||
	    config.menu_items[menu_idx].submenu_count == 0)
		return -1;
	
	build_submenu(menu_idx);
	
	/* Calculate submenu dimensions */
	for (n = 0; submenu.item[n]; n++) {
#ifdef XFT
		wide = get_text_width(submenu.item[n]) + 4;
#else
		wide = XTextWidth(font, submenu.item[n], strlen(submenu.item[n])) + 4;
#endif
		if (wide > sub_dx)
			sub_dx = wide;
	}
	
	if (n == 0) return -1;
	
#ifdef XFT
	sub_dy = n * (get_font_ascent() + get_font_descent() + 1);
#else
	sub_dy = n * (font->ascent + font->descent + 1);
#endif
	
	/* Position submenu next to main menu */
	x = main_x + main_width;
	y = main_y + menu_idx * item_height;
	
	
	/* Keep submenu on screen */
	int xmax = DisplayWidth(dpy, s->num);
	int ymax = DisplayHeight(dpy, s->num);
	
	if (x + sub_dx >= xmax)
		x = main_x - sub_dx;
	if (y + sub_dy >= ymax)
		y = ymax - sub_dy;
	if (y < 0) y = 0;
	
	/* Show submenu */
	XMoveResizeWindow(dpy, s->submenuwin, x, y, sub_dx, sub_dy);
	
	/* Store absolute position for mouse detection */
	s->submenu_x = x;
	s->submenu_y = y;
	s->submenu_w = sub_dx;
	s->submenu_h = sub_dy;
	XSelectInput(dpy, s->submenuwin, MenuMask);
	
	
	XMapRaised(dpy, s->submenuwin);
	current_submenu = menu_idx;
	
	return 0;
}

void
hide_submenu_for(ScreenInfo *s)
{
	if (current_submenu >= 0) {
		XUnmapWindow(dpy, s->submenuwin);
		current_submenu = -1;
	}
}

const char*
get_submenu_command(int menu_idx, int sub_idx)
{
	if (menu_idx >= 0 && menu_idx < config.menu_count && 
	    config.menu_items[menu_idx].is_folder &&
	    config.menu_items[menu_idx].submenu_items &&
	    sub_idx >= 0 && sub_idx < config.menu_items[menu_idx].submenu_count) {
		return config.menu_items[menu_idx].submenu_items[sub_idx].command;
	}
	return NULL;
}

void
build_submenu_for_rendering(int menu_idx)
{
	build_submenu(menu_idx);
}

char**
get_submenu_items(void)
{
	return submenu_items;
}

void
rebuild_menu(void)
{
	int i;
	
	/* Clear menu */
	for (i = 0; i < CONFIG_MAX_MENU_ITEMS + MAXHIDDEN + 1; i++)
		b3items[i] = NULL;
	
	/* Add configured menu items */
	for (i = 0; i < config.menu_count && i < CONFIG_MAX_MENU_ITEMS; i++) {
		if (config.menu_items[i].label[0] != '\0') {
			b3items[i] = config.menu_items[i].label;
		}
	}
	
	/* Add hidden windows after configured items */
	for (i = 0; i < numhidden && i < MAXHIDDEN; i++) {
		if (hiddenc[i] && hiddenc[i]->label) {
			b3items[config.menu_count + i] = hiddenc[i]->label;
		}
	}
	
	/* Null terminate */
	b3items[config.menu_count + numhidden] = NULL;
}

Menu egg = {
	version,
};

void
button(XButtonEvent * e)
{
	int n, shift;
	Client *c;
	Window dw;
	ScreenInfo *s;

	curtime = e->time;
	s = getscreen(e->root);
	if (s == 0)
		return;
	
	/* Handle spaces overlay clicks */
	if (spaces_mode) {
		if (e->window == spaces_view.overlay) {
			spaces_handle_button(e);
			return;
		} else {
			/* Safety check: if spaces_mode is on but event is not for overlay, 
			   something is wrong - reset spaces mode */
			spaces_mode = 0;
			spaces_view.active = 0;
		}
	}
	c = getclient(e->window, 0);
	if (c) {
		/* Ignore clicks on titlebars for now */
		if (c->titlebar == e->window)
			return;
		e->x += c->x - BORDER + 1;
		e->y += c->y - BORDER + 1;
	} else if (e->window != e->root)
		XTranslateCoordinates(dpy, e->window, s->root, e->x, e->y, &e->x, &e->y, &dw);
	switch (e->button) {
	case Button1:
		if (c) {
			XMapRaised(dpy, c->parent);
			top(c);
			active(c);
		}
		return;
	case Button2:
		if ((e->state & (ShiftMask | ControlMask)) == (ShiftMask | ControlMask)) {
			menuhit(e, &egg);
		} else {
			spawn(s, "9wm-mm");
		}
		return;
	default:
		return;
	case Button3:
		break;
	}

	if (current && current->screen == s)
		cmapnofocus(s);
	
	rebuild_menu();
	switch (n = menuhit(e, &b3menu)) {
	case -1:		/* nothing */
		break;
	default:
		if (n >= 1000) {
			/* Submenu item selected - decode parent folder and item index */
			int encoded = n - 1000;
			int parent_idx = encoded / 100;
			int sub_idx = encoded % 100;
			
			if (parent_idx >= 0 && parent_idx < config.menu_count &&
			    config.menu_items[parent_idx].is_folder &&
			    sub_idx >= 0 && sub_idx < config.menu_items[parent_idx].submenu_count) {
				const char *cmd = config.menu_items[parent_idx].submenu_items[sub_idx].command;
				if (cmd && strlen(cmd) > 0) {
					spawn(s, (char*)cmd);
				}
			}
		} else if (n < config.menu_count) {
			const char *cmd = config_get_menu_command(n);
			if (cmd) {
				if (strcmp(cmd, "terminal") == 0) {
					spawn_terminal_interactive(s);
				} else if (strcmp(cmd, "reshape") == 0) {
					(void)reshape(selectwin(1, 0, s));
				} else if (strcmp(cmd, "move") == 0) {
					move(selectwin(0, 0, s));
				} else if (strcmp(cmd, "delete") == 0) {
					shift = 0;
					c = selectwin(1, &shift, s);
					delete(c, shift);
				} else if (strcmp(cmd, "hide") == 0) {
					hide(selectwin(1, 0, s));
				} else if (strcmp(cmd, "tile") == 0) {
					tile_windows(s);
				} else if (strcmp(cmd, "spaces") == 0) {
					spaces_show(s);
				} else {
					/* Custom command or folder */
					if (config.menu_items[n].is_folder) {
						/* This shouldn't happen in current design, but handle gracefully */
						fprintf(stderr, "9wm: folder selected without submenu interaction\n");
					} else {
						spawn(s, (char*)cmd);
					}
				}
			}
		} else {
			/* unhide window - adjust for new menu system */
			unhide(n - config.menu_count, 1);
		}
		break;
	}
	if (current && current->screen == s)
		cmapfocus(current);
}

void
spawn(ScreenInfo * s, char *prog)
{
	if (fork() == 0) {
		close(ConnectionNumber(dpy));
		if (s->display[0] != '\0') {
			putenv(s->display);
		}

		if (prog != NULL) {
			execl(shell, shell, "-c", prog, NULL);
			fprintf(stderr, "9wm: exec %s", shell);
			perror(" failed");
		}
		execlp("xterm", "xterm", NULL);
		perror("9wm: exec xterm failed");
		exit(1);
	}
}

void
spawn_sized(ScreenInfo * s, char *prog)
{
	int x, y, width, height;
	char geometry[64];
	char command[512];
	
	if (sweep_area(s, &x, &y, &width, &height) == 0)
		return;
	
	snprintf(geometry, sizeof(geometry), "%dx%d+%d+%d", 
		width / 8, height / 16, x, y);
	
	if (fork() == 0) {
		close(ConnectionNumber(dpy));
		if (s->display[0] != '\0') {
			putenv(s->display);
		}

		if (prog != NULL) {
			snprintf(command, sizeof(command), "%s -geometry %s", prog, geometry);
			execl(shell, shell, "-c", command, NULL);
			fprintf(stderr, "9wm: exec %s", shell);
			perror(" failed");
		}
		snprintf(command, sizeof(command), "%s -geometry %s", config.terminal, geometry);
		execl(shell, shell, "-c", command, NULL);
		perror("9wm: exec terminal failed");
		exit(1);
	}
}

void
spawn_terminal_interactive(ScreenInfo * s)
{
	pid_t pid;
	
	/* Fork and spawn terminal */
	pid = fork();
	if (pid == 0) {
		close(ConnectionNumber(dpy));
		if (s->display[0] != '\0') {
			putenv(s->display);
		}

		if (termprog != NULL) {
			execl(shell, shell, "-c", termprog, NULL);
			fprintf(stderr, "9wm: exec %s", shell);
			perror(" failed");
		}
		/* Fallback to config terminal */
		execl(shell, shell, "-c", config.terminal, NULL);
		perror("9wm: exec terminal failed");
		exit(1);
	}
	
	/* Set a flag so we know to auto-reshape the next new window */
	auto_reshape_next = 1;
}

int
reshape_ex(Client *c, int activate)
{
	int odx, ody;

	if (c == 0)
		return 0;
	odx = c->dx;
	ody = c->dy;
	if (sweep(c) == 0)
		return 0;
	if (activate) {
		active(c);
		top(c);
		XRaiseWindow(dpy, c->parent);
	}
	XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER, c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
	if (c->dx == odx && c->dy == ody)
		sendconfig(c);
	else
		XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
	return 1;
}

int
reshape(c)
     Client *c;
{
	return reshape_ex(c, 1);
}

void
move(Client * c)
{
	if (c == 0)
		return;
	if (drag(c) == 0)
		return;
	active(c);
	top(c);
	XRaiseWindow(dpy, c->parent);
	XMoveWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER);
	sendconfig(c);
}

void
delete(c, shift)
     Client *c;
     int shift;
{
	if (c == 0)
		return;
	if ((c->proto & Pdelete) && !shift)
		sendcmessage(c->window, wm_protocols, wm_delete, 0);
	else
		XKillClient(dpy, c->window);	/* let event clean up */
}

void
hide(Client * c)
{
	int i;

	if (c == 0 || numhidden == MAXHIDDEN)
		return;
	if (hidden(c)) {
		fprintf(stderr, "9wm: already hidden: %s\n", c->label);
		return;
	}
	XUnmapWindow(dpy, c->parent);
	XUnmapWindow(dpy, c->window);
	setwstate(c, IconicState);
	if (c == current)
		nofocus();

	for (i = numhidden; i > 0; i -= 1) {
		hiddenc[i] = hiddenc[i - 1];
	}
	hiddenc[0] = c;
	numhidden++;
}

void
unhide(int n, int map)
{
	Client *c;
	int i;

	if (n >= numhidden) {
		fprintf(stderr, "9wm: unhide: n %d numhidden %d\n", n, numhidden);
		return;
	}
	c = hiddenc[n];
	if (!hidden(c)) {
		fprintf(stderr, "9wm: unhide: not hidden: %s(0x%x)\n", c->label, (int) c->window);
		return;
	}

	if (map) {
		XMapWindow(dpy, c->window);
		XMapRaised(dpy, c->parent);
		setwstate(c, NormalState);
		active(c);
		top(c);
	}

	numhidden--;
	for (i = n; i < numhidden; i++) {
		hiddenc[i] = hiddenc[i + 1];
	}
}

void
unhidec(c, map)
     Client *c;
     int map;
{
	int i;

	for (i = 0; i < numhidden; i++)
		if (c == hiddenc[i]) {
			unhide(i, map);
			return;
		}
	fprintf(stderr, "9wm: unhidec: not hidden: %s(0x%x)\n", c->label, (int) c->window);
}

void
tile_windows(ScreenInfo *s)
{
	Client *c;
	Client **visible_clients;
	int num_visible = 0;
	int i;
	int screen_width, screen_height;
	int master_width, slave_width;
	int master_height, slave_height;
	int master_x, master_y;
	int slave_x, slave_y;
	int current_workspace;
	
	if (!s) return;
	
	/* Get current workspace */
	current_workspace = workspace_get_current();
	
	/* Count visible clients in current workspace */
	for (c = clients; c; c = c->next) {
		if (!hidden(c) && !withdrawn(c) && c->screen == s && c->workspace == current_workspace) {
			num_visible++;
		}
	}
	
	if (num_visible == 0) return;
	
	/* Allocate array for visible clients */
	visible_clients = malloc(num_visible * sizeof(Client*));
	if (!visible_clients) return;
	
	/* Collect visible clients */
	i = 0;
	for (c = clients; c; c = c->next) {
		if (!hidden(c) && !withdrawn(c) && c->screen == s && c->workspace == current_workspace) {
			visible_clients[i++] = c;
		}
	}
	
	/* Get screen dimensions */
	screen_width = DisplayWidth(dpy, s->num);
	screen_height = DisplayHeight(dpy, s->num);
	
	if (num_visible == 1) {
		/* Single window - full screen */
		c = visible_clients[0];
		c->x = 0;
		c->y = 0;
		c->dx = screen_width;
		c->dy = screen_height;
		
		XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER, 
		                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
		XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
		sendconfig(c);
		
	} else if (num_visible == 2) {
		/* Two windows - side by side, focused window gets more space */
		master_width = (int)(screen_width * 0.6);  /* 60% for focused window */
		slave_width = screen_width - master_width;
		master_height = slave_height = screen_height;
		master_x = 0;
		master_y = 0;
		slave_x = master_width;
		slave_y = 0;
		
		/* Put current/focused window as master */
		if (current && current->workspace == current_workspace) {
			/* Current window gets master position */
			current->x = master_x;
			current->y = master_y;
			current->dx = master_width;
			current->dy = master_height;
			
			XMoveResizeWindow(dpy, current->parent, current->x - BORDER, current->y - BORDER,
			                  current->dx + 2 * (BORDER - 1), current->dy + 2 * (BORDER - 1));
			XMoveResizeWindow(dpy, current->window, BORDER - 1, BORDER - 1, current->dx, current->dy);
			sendconfig(current);
			
			/* Other window gets slave position */
			for (i = 0; i < num_visible; i++) {
				if (visible_clients[i] != current) {
					c = visible_clients[i];
					c->x = slave_x;
					c->y = slave_y;
					c->dx = slave_width;
					c->dy = slave_height;
					
					XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER,
					                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
					XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
					sendconfig(c);
					break;
				}
			}
		} else {
			/* No current window, just split evenly */
			master_width = screen_width / 2;
			slave_width = screen_width - master_width;
			
			for (i = 0; i < num_visible && i < 2; i++) {
				c = visible_clients[i];
				c->x = (i == 0) ? 0 : master_width;
				c->y = 0;
				c->dx = (i == 0) ? master_width : slave_width;
				c->dy = screen_height;
				
				XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER,
				                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
				XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
				sendconfig(c);
			}
		}
		
	} else {
		/* Multiple windows - master/stack layout */
		master_width = (int)(screen_width * 0.5);  /* 50% for master */
		slave_width = screen_width - master_width;
		master_height = screen_height;
		slave_height = screen_height / (num_visible - 1);
		
		master_x = 0;
		master_y = 0;
		slave_x = master_width;
		slave_y = 0;
		
		/* Put current/focused window as master if it exists */
		if (current && current->workspace == current_workspace) {
			current->x = master_x;
			current->y = master_y;
			current->dx = master_width;
			current->dy = master_height;
			
			XMoveResizeWindow(dpy, current->parent, current->x - BORDER, current->y - BORDER,
			                  current->dx + 2 * (BORDER - 1), current->dy + 2 * (BORDER - 1));
			XMoveResizeWindow(dpy, current->window, BORDER - 1, BORDER - 1, current->dx, current->dy);
			sendconfig(current);
			
			/* Stack other windows vertically */
			i = 0;
			for (c = clients; c; c = c->next) {
				if (!hidden(c) && !withdrawn(c) && c->screen == s && 
				    c->workspace == current_workspace && c != current) {
					c->x = slave_x;
					c->y = slave_y + (i * slave_height);
					c->dx = slave_width;
					c->dy = slave_height;
					
					XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER,
					                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
					XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
					sendconfig(c);
					i++;
				}
			}
		} else {
			/* No current window, treat first as master */
			visible_clients[0]->x = master_x;
			visible_clients[0]->y = master_y;
			visible_clients[0]->dx = master_width;
			visible_clients[0]->dy = master_height;
			
			c = visible_clients[0];
			XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER,
			                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
			XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
			sendconfig(c);
			
			/* Stack remaining windows */
			for (i = 1; i < num_visible; i++) {
				c = visible_clients[i];
				c->x = slave_x;
				c->y = slave_y + ((i - 1) * slave_height);
				c->dx = slave_width;
				c->dy = slave_height;
				
				XMoveResizeWindow(dpy, c->parent, c->x - BORDER, c->y - BORDER,
				                  c->dx + 2 * (BORDER - 1), c->dy + 2 * (BORDER - 1));
				XMoveResizeWindow(dpy, c->window, BORDER - 1, BORDER - 1, c->dx, c->dy);
				sendconfig(c);
			}
		}
	}
	
	free(visible_clients);
}

void
renamec(c, name)
     Client *c;
     char *name;
{
	int i;

	if (name == 0)
		name = "???";
	c->label = name;
	if (!hidden(c))
		return;
	for (i = 0; i < numhidden; i++)
		if (c == hiddenc[i]) {
			b3items[B3FIXED + i] = name;
			return;
		}
}
