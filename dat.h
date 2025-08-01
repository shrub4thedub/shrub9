/*
 * Copyright multiple authors, see README for licence details
 */

#define BORDER		_border
#define	INSET		_inset
#define MAXHIDDEN	32
#define B3FIXED 	5

#define AllButtonMask	(Button1Mask|Button2Mask|Button3Mask \
			|Button4Mask|Button5Mask)
#define ButtonMask	(ButtonPressMask|ButtonReleaseMask)
#define MenuMask	(ButtonMask|ButtonMotionMask|ExposureMask)
#define MenuGrabMask	(ButtonMask|ButtonMotionMask|StructureNotifyMask)

#define DEFSHELL	"/bin/sh"

typedef struct Client Client;
typedef struct Menu Menu;
typedef struct ScreenInfo ScreenInfo;

struct Client {
	Window		window;
	Window		parent;
	Window		trans;
	Client		*next;
	Client		*revert;

	int 		x;
	int 		y;
	int 		dx;
	int 		dy;
	int 		border;

	XSizeHints	size;
	int 		min_dx;
	int 		min_dy;

	int 		state;
	int 		init;
	int 		reparenting;
	int 		hold;
	int 		proto;

	char		*label;
	char		*instance;
	char		*class;
	char		*name;
	char		*iconname;

	Colormap	cmap;
	int 		ncmapwins;
	Window		*cmapwins;
	Colormap	*wmcmaps;
	ScreenInfo	*screen;
	
	/* Workspace support */
	int		workspace;
	Client		*workspace_next;
	Client		*workspace_prev;
	
	/* Titlebar support */
	Window		titlebar;
	int		title_width;
	
	/* Terminal-launcher support */
	int		is_terminal;
	Client		*terminal_parent;
	Client		*launched_child;
	int		saved_x, saved_y;
	int		saved_dx, saved_dy;
};

#define hidden(c)	((c)->state == IconicState)
#define withdrawn(c)	((c)->state == WithdrawnState)
#define normal(c)	((c)->state == NormalState)

/* c->proto */
#define Pdelete 	1
#define Ptakefocus	2

struct Menu {
	char	**item;
	char	*(*gen)();
	int	lasthit;
};

struct ScreenInfo {
	int		num;
	Window		root;
	Window		menuwin;
	Colormap	def_cmap;
	GC		gc;
	GC		text_gc;
	GC		menu_highlight_gc;
	GC		menu_highlight_text_gc;
	unsigned long	black;
	unsigned long	white;
	unsigned long	active;
	unsigned long	inactive;
	unsigned long	menu_bg;
	unsigned long	menu_fg;
	int		min_cmaps;
	Cursor		target;
	Cursor		sweep0;
	Cursor		boxcurs;
	Cursor		arrow;
	Pixmap		root_pixmap;
	char		display[256];	/* arbitrary limit */
};

/* Nostalgia options */
enum {
	MODERN = 0,
	V1,
	BLIT
};

/* main.c */
extern Display		*dpy;
extern ScreenInfo	*screens;
extern int		num_screens;
extern int		initting;
extern XFontStruct	*font;
extern int 		curs;
extern char		**myargv;
extern Bool 		shape;
extern char 		*termprog;
extern char 		*shell;
extern char 		*version[];
extern int		_border;
extern int		_inset;
extern int		curtime;
extern int		debug;
extern int		auto_reshape_next;
extern int		spaces_mode;
extern int		workspace_switching;
extern int		pending_workspace_unmaps;

extern Atom		exit_9wm;
extern Atom		restart_9wm;
extern Atom 		wm_state;
extern Atom		wm_change_state;
extern Atom 		_9wm_hold_mode;
extern Atom 		wm_protocols;
extern Atom 		wm_delete;
extern Atom 		wm_take_focus;
extern Atom 		wm_colormaps;
extern Atom		utf8_string;
extern Atom		wm_moveresize;
extern Atom		active_window;
extern Atom		net_wm_state;
extern Atom		net_wm_state_fullscreen;

/* client.c */
extern Client		*clients;
extern Client		*current;

/* menu.c */
extern Client		*hiddenc[];
extern int 		numhidden;
extern char 		*b3items[];
extern Menu 		b3menu;

/* error.c */
extern int 		ignore_badwindow;
