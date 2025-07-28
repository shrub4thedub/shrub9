/*
 * Spaces (workspace overview) for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#ifndef SPACES_H
#define SPACES_H

#include <X11/Xlib.h>

#define SPACES_GRID_SIZE 3
#define SPACES_MAX_WORKSPACES 9

typedef struct SpacesView SpacesView;

struct SpacesView {
	ScreenInfo *screen;
	Window overlay;
	int active;
	int grid_x, grid_y;          /* Grid position dimensions */
	int cell_width, cell_height; /* Individual workspace cell size */
	int margin;                  /* Border margin */
	int selected_workspace;      /* Currently highlighted workspace */
	int drag_active;            /* Whether dragging a window */
	Client *drag_client;        /* Client being dragged */
	int drag_start_ws;          /* Starting workspace for drag */
};

/* Global spaces state */
extern SpacesView spaces_view;
extern int spaces_mode;

/* Function prototypes */
void spaces_init(ScreenInfo *s);
void spaces_show(ScreenInfo *s);
void spaces_hide(void);
void spaces_draw(void);
void spaces_handle_button(XButtonEvent *e);
void spaces_handle_motion(XMotionEvent *e);
void spaces_handle_key(XKeyEvent *e);
int spaces_get_workspace_at_point(int x, int y);
Client* spaces_get_client_at_point(int x, int y, int ws);
void spaces_draw_workspace(int ws, int x, int y, int width, int height);
void spaces_draw_window_thumbnail(Client *c, int ws_x, int ws_y, int ws_width, int ws_height);

#endif /* SPACES_H */