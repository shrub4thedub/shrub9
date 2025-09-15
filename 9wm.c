/*
 * Copyright multiple authors, see README for licence details
 */
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include "dat.h"
#include "fns.h"
#include "config.h"
#include "workspace.h"
#include "spaces.h"
#include "plumb.h"

char *version[] = {
	"shrub9 version 1.0.0, Copyright (c) 2025 shrub (based on 9wm)", 0,
};

Display *dpy;
ScreenInfo *screens;
int initting;
XFontStruct *font;
#ifdef XFT
XftFont *xft_font = NULL;
int use_xft = 0;
#endif
int curs;
int border;
char **myargv;
char *termprog;
char *shell;
Bool shape;
int _border = 4;
int _inset = 1;
int curtime;
int debug;
int signalled;
int auto_reshape_next = 0;
int spaces_mode = 0;
int workspace_switching = 0;
int pending_workspace_unmaps = 0;
int num_screens;

#ifdef COLOR
char *activestr;
char *inactivestr;
#endif

Atom exit_9wm;
Atom restart_9wm;
Atom wm_state;
Atom wm_change_state;
Atom wm_protocols;
Atom wm_delete;
Atom wm_take_focus;
Atom wm_colormaps;
Atom wm_moveresize;
Atom net_wm_state;
Atom net_wm_state_fullscreen;
Atom active_window;
Atom utf8_string;
Atom _9wm_running;
Atom _9wm_hold_mode;

char *fontlist[] = {
	"-*-dejavu sans-bold-r-*-*-14-*-*-*-p-*-*-*",
	"-adobe-helvetica-bold-r-*-*-14-*-*-*-p-*-*-*",
	"lucm.latin1.9",
	"blit",
	"9x15bold",
	"lucidasanstypewriter-12",
	"fixed",
	"*",
	0,
};

void
sigchld(int signum)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
usage(void)
{
	fprintf(stderr, "usage: 9wm [-version] [-cursor cursor] [-border] [-font fname] [-term prog] [-active color] [-inactive color] [exit|restart]\n");
	exit(1);
}

#ifdef COLOR
Status
getcolor(Colormap cmap, unsigned long *pixel, char *str)
{
	if (str != NULL) {
		XColor color;
		Status stpc = 0;
		Status stac = 0;
		if (cmap != 0)
			stpc = XParseColor(dpy, cmap, str, &color);
		if (stpc != 0)
			stac = XAllocColor(dpy, cmap, &color);
		if (stac != 0) {
			*pixel = color.pixel;
			return 1;
		}
	}
	return 0;
}
#endif

int
main(int argc, char *argv[])
{
	int i, do_exit, do_restart;
	char *fname;
	int shape_event, dummy;
	myargv = argv;		/* for restart */

	do_exit = do_restart = 0;
	font = 0;
	fname = 0;
	for (i = 1; i < argc; i++)
		if (strcmp(argv[i], "-debug") == 0) {
			debug++;
#ifndef DEBUG
		fprintf(stderr,"9wm: WARNING! debug option enabled but debug code not compiled in!\nDebug info may not be complete!\n");
#endif
		} else if (strcmp(argv[i], "-font") == 0 && i + 1 < argc)
			fname = argv[++i];
		else if (strcmp(argv[i], "-term") == 0 && i + 1 < argc)
			termprog = argv[++i];
		else if (strcmp(argv[i], "-version") == 0) {
			fprintf(stderr, "%s\n", version[0]);
			exit(0);
		} else if (strcmp(argv[i],"-border") == 0)
			border++;
		else if (strcmp(argv[i],"-cursor")  == 0 && i + 1 < argc) {
			i++;
			if (strcmp(argv[i],"v1") == 0)
				curs = 1;
			else if (strcmp(argv[i],"blit") == 0)
				curs = 2;
			else if (strcmp(argv[i],"classic") == 0)
				curs = 3;
		} else if ( (strcmp(argv[i], "-active") == 0 || strcmp(argv[i], "-inactive") == 0) && i + 1 < argc) {
#ifdef COLOR
			if(argv[i][1] == 'a')
				activestr = argv[++i];
			else
				inactivestr = argv[++i];
#else
			fprintf(stderr,"9wm: border set but COLOR not defined\n");
			i++;
#endif
		} else if (argv[i][0] == '-')
			usage();
		else
			break;
	for (; i < argc; i++)
		if (strcmp(argv[i], "exit") == 0)
			do_exit++;
		else if (strcmp(argv[i], "restart") == 0)
			do_restart++;
		else
			usage();

	if (do_exit && do_restart)
		usage();

	shell = (char *) getenv("SHELL");
	if (shell == NULL)
		shell = DEFSHELL;

	dpy = XOpenDisplay("");
	if (dpy == 0)
		fatal("can't open display");

	if (!config_init()) {
		fprintf(stderr, "shrub9: warning: failed to load configuration, using defaults\n");
		config_load_default();
	}

	/* Apply cursor style from config if not set by command line */
	if (curs == 0) { /* Only apply if not already set by -cursor command line arg */
		if (strcmp(config.cursor_style, "v1") == 0) {
			curs = 1;
		} else if (strcmp(config.cursor_style, "blit") == 0) {
			curs = 2;
		} else if (strcmp(config.cursor_style, "classic") == 0) {
			curs = 3;
		}
		/* else keep curs = 0 for "modern" or unrecognized values */
	}

	initting = 1;
	XSetErrorHandler(handler);
	if (signal(SIGTERM, sighandler) == SIG_IGN)
		signal(SIGTERM, SIG_IGN);
	if (signal(SIGINT, sighandler) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, sighandler) == SIG_IGN)
		signal(SIGHUP, SIG_IGN);
	{
		struct sigaction act = {0};

		act.sa_handler = sigchld;
		sigaction(SIGCHLD, &act, NULL);
	}

	exit_9wm = XInternAtom(dpy, "9WM_EXIT", False);
	restart_9wm = XInternAtom(dpy, "9WM_RESTART", False);

	curtime = -1;		/* don't care */
	if (do_exit) {
		sendcmessage(DefaultRootWindow(dpy), exit_9wm, 0L, 1);
		XSync(dpy, False);
		exit(0);
	}
	if (do_restart) {
		sendcmessage(DefaultRootWindow(dpy), restart_9wm, 0L, 1);
		XSync(dpy, False);
		exit(0);
	}

	wm_state = XInternAtom(dpy, "WM_STATE", False);
	wm_change_state = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wm_take_focus = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
	wm_colormaps = XInternAtom(dpy, "WM_COLORMAP_WINDOWS", False);
	wm_moveresize = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
	active_window = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
	net_wm_state = XInternAtom(dpy, "_NET_WM_STATE",False);
	net_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
	_9wm_running = XInternAtom(dpy, "_9WM_RUNNING", False);
	_9wm_hold_mode = XInternAtom(dpy, "_9WM_HOLD_MODE", False);

	if (fname != 0) {
		printf("[MAIN DEBUG] Loading command-line font: %s\n", fname);
		if (!config_load_font_hybrid(fname)) {
			fprintf(stderr, "shrub9: warning: can't load font %s\n", fname);
		}
	}

	if (font == 0 && 
#ifdef XFT
	    xft_font == NULL
#else
	    1
#endif
	    ) {
		/* Try to add local fonts directory to X font path */
		char local_fonts_dir[512];
		struct passwd *pw = getpwuid(getuid());
		if (pw) {
			snprintf(local_fonts_dir, sizeof(local_fonts_dir), "%s/.local/share/fonts", pw->pw_dir);
			printf("[MAIN DEBUG] Adding local fonts directory to path: %s\n", local_fonts_dir);
			
			/* Check if directory exists and has fonts.dir */
			if (access(local_fonts_dir, R_OK) == 0) {
				char fonts_dir_path[600];
				snprintf(fonts_dir_path, sizeof(fonts_dir_path), "%s/fonts.dir", local_fonts_dir);
				if (access(fonts_dir_path, R_OK) == 0) {
					/* Get current font path and extend it */
					char **current_path;
					int current_count;
					char **new_path;
					int i;
					
					current_path = XGetFontPath(dpy, &current_count);
					
					/* Create new extended path */
					new_path = malloc((current_count + 1) * sizeof(char*));
					for (i = 0; i < current_count; i++) {
						new_path[i] = current_path[i];
					}
					new_path[current_count] = local_fonts_dir;
					
					if (XSetFontPath(dpy, new_path, current_count + 1) == 0) {
						printf("[MAIN DEBUG] Successfully added local fonts to path\n");
					} else {
						printf("[MAIN DEBUG] Failed to set extended font path\n");
					}
					
					free(new_path);
					XFreeFontPath(current_path);
				} else {
					printf("[MAIN DEBUG] fonts.dir not found in %s\n", local_fonts_dir);
				}
			} else {
				printf("[MAIN DEBUG] Local fonts directory not accessible: %s\n", local_fonts_dir);
			}
		}
		
		printf("[MAIN DEBUG] Loading config font: %s\n", config.font);
		if (!config_load_font_hybrid(config.font)) {
			fprintf(stderr, "shrub9: fatal: cannot load any font\n");
			exit(1);
		}
	}
	
	printf("[MAIN DEBUG] Final font loaded:\n");
#ifdef XFT
	if (use_xft && xft_font) {
		printf("[MAIN DEBUG]   Using Xft font - ascent: %d, descent: %d\n", 
		       xft_font->ascent, xft_font->descent);
	} else
#endif
	if (font) {
		printf("[MAIN DEBUG]   Using core font - ascent: %d, descent: %d\n", 
		       font->ascent, font->descent);
	} else {
		printf("[MAIN DEBUG]   ERROR: No font loaded!\n");
	}
	_border = config.border_width;
	_inset = config.inset_width;
	
	if (border) {
		_border--;
		_inset--;
	}
#ifdef	SHAPE
	shape = XShapeQueryExtension(dpy, &shape_event, &dummy);
#endif

	num_screens = ScreenCount(dpy);
	screens = (ScreenInfo *) malloc(sizeof(ScreenInfo) * num_screens);

	if (termprog == NULL)
		termprog = config.terminal;
		
	workspace_init(config.workspaces.count);
	plumb_init();
	rebuild_menu();

	for (i = 0; i < num_screens; i++) {
		initscreen(&screens[i], i);
		spaces_init(&screens[i]);
		
		/* Register workspace key bindings */
		if (config.workspaces.enabled) {
			int j;
			for (j = 0; j < config.workspaces.count; j++) {
				KeyCode keycode = XKeysymToKeycode(dpy, config.workspaces.switch_keys[j].keysym);
				if (keycode != 0) {
					XGrabKey(dpy, keycode, config.workspaces.switch_keys[j].modifiers,
						screens[i].root, True, GrabModeAsync, GrabModeAsync);
				}
			}
		}
	}

	/* Apply wallpaper if configured */
	config_apply_wallpaper();

	/*
	 * set selection so that 9term knows we're running 
	 */
	curtime = CurrentTime;
	XSetSelectionOwner(dpy, _9wm_running, screens[0].menuwin, timestamp());

	XSync(dpy, False);
	initting = 0;

	nofocus();

	for (i = 0; i < num_screens; i++)
		scanwins(&screens[i]);

	mainloop(shape_event);

	return 0;
}

void
initscreen(ScreenInfo * s, int i)
{
	char *ds, *colon, *dot1;
	unsigned long mask;
	XGCValues gv;
	XSetWindowAttributes attr;

	s->num = i;
	s->root = RootWindow(dpy, i);
	s->def_cmap = DefaultColormap(dpy, i);
	s->min_cmaps = MinCmapsOfScreen(ScreenOfDisplay(dpy, i));

	ds = DisplayString(dpy);
	colon = strrchr(ds, ':');
	if (colon && num_screens > 1) {
		strcpy(s->display, "DISPLAY=");
		strcat(s->display, ds);
		colon = s->display + 8 + (colon - ds);	/* use version in buf */
		dot1 = strchr(colon, '.');	/* first period after colon */
		if (!dot1)
			dot1 = colon + strlen(colon);	/* if not there, append */
		sprintf(dot1, ".%d", i);
	} else
		s->display[0] = '\0';

	s->black = BlackPixel(dpy, i);
	s->white = WhitePixel(dpy, i);
	/*
	 * Setup color for border
	 */
	s->active = s->black;
	s->inactive = s->white;
	s->menu_bg = s->white;
	s->menu_fg = s->black;
	s->frame_color = s->black;
#ifdef COLOR
	{
		Colormap cmap = DefaultColormap(dpy,s->num);
		unsigned long active, inactive, menu_bg, menu_fg, frame_color;
		
		if (cmap != 0) {
			if (activestr != NULL && getcolor(cmap, &active, activestr)) {
				s->active = active;
			} else if (config_parse_color(config.active_color, &active, cmap)) {
				s->active = active;
			}
			
			if (inactivestr != NULL && getcolor(cmap, &inactive, inactivestr)) {
				s->inactive = inactive;
			} else if (config_parse_color(config.inactive_color, &inactive, cmap)) {
				s->inactive = inactive;
			}
			
			/* Set menu colors from config */
			if (config_parse_color(config.menu_bg_color, &menu_bg, cmap)) {
				s->menu_bg = menu_bg;
			}
			if (config_parse_color(config.menu_fg_color, &menu_fg, cmap)) {
				s->menu_fg = menu_fg;
			}
			
			/* Set frame color from config */
			if (config_parse_color(config.window_frame_color, &frame_color, cmap)) {
				s->frame_color = frame_color;
			} else {
				s->frame_color = s->black;  /* Default to black */
			}
		}
	}
#endif
	/* Create XOR graphics context for drawing bounds/highlights */
	gv.foreground = s->black;
	gv.background = s->white;
	gv.function = GXinvert;
	gv.line_width = 0;
	gv.subwindow_mode = IncludeInferiors;
	mask = GCForeground | GCBackground | GCFunction | GCLineWidth | GCSubwindowMode;
	s->gc = XCreateGC(dpy, s->root, mask, &gv);
	
	/* Create normal graphics context for text rendering */
	gv.foreground = s->menu_fg;
	gv.background = s->menu_bg;
	gv.function = GXcopy;
	gv.line_width = 0;
	gv.subwindow_mode = IncludeInferiors;
	mask = GCForeground | GCBackground | GCFunction | GCLineWidth | GCSubwindowMode;
#ifdef XFT
	if (use_xft) {
		printf("[GC DEBUG] Using Xft rendering, not setting core font on GC for screen %d\n", s->num);
	} else
#endif
	if (font != 0) {
		printf("[GC DEBUG] Setting core font ID %ld in text GC for screen %d\n", font->fid, s->num);
		gv.font = font->fid;
		mask |= GCFont;
	} else {
		printf("[GC DEBUG] WARNING: No font available for text GC on screen %d\n", s->num);
	}
	s->text_gc = XCreateGC(dpy, s->root, mask, &gv);
	
	/* Create menu highlight graphics context for blue rectangle */
	gv.foreground = s->menu_fg;  /* Blue - for drawing the highlight rectangle */
	gv.background = s->menu_bg;  /* Cream */
	gv.function = GXcopy;
	gv.line_width = 0;
	gv.subwindow_mode = IncludeInferiors;
	mask = GCForeground | GCBackground | GCFunction | GCLineWidth | GCSubwindowMode;
	s->menu_highlight_gc = XCreateGC(dpy, s->root, mask, &gv);
	
	/* Create menu highlight text graphics context for cream text */
	gv.foreground = s->menu_bg;  /* Cream - for drawing text on blue background */
	gv.background = s->menu_fg;  /* Blue */
	gv.function = GXcopy;
	gv.line_width = 0;
	gv.subwindow_mode = IncludeInferiors;
	mask = GCForeground | GCBackground | GCFunction | GCLineWidth | GCSubwindowMode;
#ifdef XFT
	if (!use_xft)
#endif
	if (font != 0) {
		gv.font = font->fid;
		mask |= GCFont;
	}
	s->menu_highlight_text_gc = XCreateGC(dpy, s->root, mask, &gv);

#ifdef XFT
	/* Initialize Xft drawing context and colors */
	if (use_xft) {
		s->xft_draw = XftDrawCreate(dpy, s->menuwin, DefaultVisual(dpy, i), s->def_cmap);
		if (!s->xft_draw) {
			printf("[XFT DEBUG] Failed to create XftDraw for screen %d\n", s->num);
		} else {
			/* Set up Xft colors using proper color querying */
			XColor xcolor;
			XRenderColor render_color;
			
			/* Normal text color */
			xcolor.pixel = s->menu_fg;
			XQueryColor(dpy, s->def_cmap, &xcolor);
			render_color.red = xcolor.red;
			render_color.green = xcolor.green;
			render_color.blue = xcolor.blue;
			render_color.alpha = 0xFFFF;
			XftColorAllocValue(dpy, DefaultVisual(dpy, i), s->def_cmap, &render_color, &s->xft_color);
			
			/* Highlight text color */
			xcolor.pixel = s->menu_bg;
			XQueryColor(dpy, s->def_cmap, &xcolor);
			render_color.red = xcolor.red;
			render_color.green = xcolor.green;
			render_color.blue = xcolor.blue;
			render_color.alpha = 0xFFFF;
			XftColorAllocValue(dpy, DefaultVisual(dpy, i), s->def_cmap, &render_color, &s->xft_highlight_color);
			
			printf("[XFT DEBUG] Created XftDraw and colors for screen %d\n", s->num);
		}
	}
#endif

	initcurs(s);

	attr.cursor = s->arrow;
	attr.event_mask = SubstructureRedirectMask
	    | SubstructureNotifyMask | ColormapChangeMask | ButtonPressMask | ButtonReleaseMask | PropertyChangeMask | KeyPressMask;
	mask = CWCursor | CWEventMask;
	XChangeWindowAttributes(dpy, s->root, mask, &attr);
	XSync(dpy, False);

	s->menuwin = XCreateSimpleWindow(dpy, s->root, 0, 0, 1, 1, 1, s->menu_fg, s->menu_bg);
	s->submenuwin = XCreateSimpleWindow(dpy, s->root, 0, 0, 1, 1, 1, s->menu_fg, s->menu_bg);
	
	/* Set override redirect on submenu to prevent WM interference */
	attr.override_redirect = True;
	XChangeWindowAttributes(dpy, s->submenuwin, CWOverrideRedirect, &attr);
}

ScreenInfo *
getscreen(Window w)
{
	int i;

	for (i = 0; i < num_screens; i++)
		if (screens[i].root == w)
			return &screens[i];

	return 0;
}

Time
timestamp(void)
{
	XEvent ev;

	if (curtime == CurrentTime) {
		XChangeProperty(dpy, screens[0].root, _9wm_running, _9wm_running, 8, PropModeAppend, (unsigned char *) "", 0);
		XMaskEvent(dpy, PropertyChangeMask, &ev);
		curtime = ev.xproperty.time;
	}
	return curtime;
}

void
sendcmessage(Window w, Atom a, long x, int isroot)
{
	XEvent ev;
	int status;
	long mask;

	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = w;
	ev.xclient.message_type = a;
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = x;
	ev.xclient.data.l[1] = timestamp();
	mask = 0L;
	if (isroot)
		mask = SubstructureRedirectMask;	/* magic! */
	status = XSendEvent(dpy, w, False, mask, &ev);
	if (status == 0)
		fprintf(stderr, "9wm: sendcmessage failed\n");
}

void
sendconfig(c)
     Client *c;
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.event = c->window;
	ce.window = c->window;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->dx;
	ce.height = c->dy;
	ce.border_width = c->border;
	ce.above = None;
	ce.override_redirect = 0;
	XSendEvent(dpy, c->window, False, StructureNotifyMask, (XEvent *) & ce);
}

void
sighandler(void)
{
	signalled = 1;
}

void
getevent(XEvent * e)
{
	int fd;
	fd_set rfds;
	struct timeval t;

	if (!signalled) {
		if (QLength(dpy) > 0) {
			XNextEvent(dpy, e);
			return;
		}
		fd = ConnectionNumber(dpy);
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		t.tv_sec = t.tv_usec = 0;
		if (select(fd + 1, &rfds, NULL, NULL, &t) == 1) {
			XNextEvent(dpy, e);
			return;
		}
		XFlush(dpy);

		do {
			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);
			if (select(fd + 1, &rfds, NULL, NULL, NULL) == 1) {
				XNextEvent(dpy, e);
				return;
			}
		} while (errno == EINTR);
		if (!signalled) {
			perror("9wm: select failed");
			exit(1);
		}
	}
	fprintf(stderr, "9wm: exiting on signal\n");
	cleanup();
	exit(1);
}

void
cleanup(void)
{
	Client *c, *cc[2], *next;
	XWindowChanges wc;
	int i;

	/*
	 * order of un-reparenting determines final stacking order... 
	 */
	cc[0] = cc[1] = 0;
	for (c = clients; c; c = next) {
		next = c->next;
		i = normal(c);
		c->next = cc[i];
		cc[i] = c;
	}

	for (i = 0; i < 2; i++) {
		for (c = cc[i]; c; c = c->next) {
			if (!withdrawn(c)) {
				gravitate(c, 1);
				XReparentWindow(dpy, c->window, c->screen->root, c->x, c->y);
			}
			wc.border_width = c->border;
			XConfigureWindow(dpy, c->window, CWBorderWidth, &wc);
		}
	}

	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, timestamp());
	
	/* Free graphics contexts */
	for (i = 0; i < num_screens; i++) {
		cmapnofocus(&screens[i]);
		if (screens[i].gc != None)
			XFreeGC(dpy, screens[i].gc);
		if (screens[i].text_gc != None)
			XFreeGC(dpy, screens[i].text_gc);
#ifdef XFT
		if (screens[i].xft_draw) {
			XftColorFree(dpy, DefaultVisual(dpy, screens[i].num), screens[i].def_cmap, &screens[i].xft_color);
			XftColorFree(dpy, DefaultVisual(dpy, screens[i].num), screens[i].def_cmap, &screens[i].xft_highlight_color);
			XftDrawDestroy(screens[i].xft_draw);
		}
#endif
	}
	
#ifdef XFT
	if (xft_font) {
		XftFontClose(dpy, xft_font);
	}
#endif
	
	XCloseDisplay(dpy);
}
