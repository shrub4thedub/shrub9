/*
 * Copyright multiple authors, see README for licence details
 */

#ifdef	DEBUG
#define	trace(s, c, e)	dotrace((s), (c), (e))
#else
#define	trace(s, c, e)
#endif

/* 9wm.c */
void	usage();
void	initscreen();
ScreenInfo *getscreen();
Time	timestamp();
void	sendcmessage();
void	sendconfig();
void	sighandler();
void	getevent();
void	cleanup();

/* event.c */
void	mainloop();
void	configurereq();
void	mapreq();
void	circulatereq();
void	unmap();
void	newwindow();
void	destroy();
void	clientmesg();
void	cmap();
void	property();
void	shapenotify();
void	enter();
void	focusin();
void	reparent();
void	keypress();

/* manage.c */
int 	manage();
void	scanwins();
void	setshape();
void	withdraw();
void	gravitate();
void	cmapfocus();
void	cmapnofocus();
void	getcmaps();
int 	_getprop();
char	*getprop();
void	restore_terminal_from_child();
Window	getwprop();
int 	getiprop();
int 	getwstate();
void	setwstate();
void	setlabel();
void	getproto();
void	gettrans();

/* menu.c */
void	button();
void	spawn();
void	spawn_sized();
void	spawn_terminal_interactive();
int	reshape();
int	reshape_ex();
void	move();
void	delete();
void	hide();
void	unhide();
void	unhidec();
void	renamec();
void	rebuild_menu();
int	show_submenu_at(XButtonEvent *e, int menu_idx, ScreenInfo *s, int main_x, int main_y, int main_width, int item_height);
void	hide_submenu_for(ScreenInfo *s);
const char* get_submenu_command(int menu_idx, int sub_idx);
void	build_submenu_for_rendering(int menu_idx);
char**	get_submenu_items();
void	tile_windows();

/* client.c */
void	setactive();
void	draw_border();
void	create_titlebar();
void	draw_titlebar();
void	destroy_titlebar();
void	active();
void	nofocus();
void	top();
Client	*getclient();
void	rmclient();
void	dump_revert();
void	dump_clients();

/* grab.c */
int 	menuhit();
#ifdef XFT
int	get_text_width();
int	get_font_ascent();
int	get_font_descent();
#endif
Client	*selectwin();
int 	sweep();
int 	sweep_area();
int 	drag();
void	getmouse();
void	setmouse();

/* error.c */
int 	handler();
void	fatal();
void	graberror();
void	showhints();
void	dotrace();

/* cursor.c */
void	initcurs(ScreenInfo * s);

/* config.c */
int	config_init();
int	config_load();
int	config_load_default();
void	config_free();
int	config_get_workspace_key();
const char*	config_get_menu_command();
const char*	config_get_menu_label();
int	config_parse_color();
XFontStruct*	config_load_font();
int	config_apply_wallpaper();

/* workspace.c */
void	workspace_init();
void	workspace_switch();
void	workspace_add_client();
void	workspace_remove_client();
void	workspace_move_client();
int	workspace_get_current();
void	workspace_show_all_clients();
void	workspace_hide_all_clients();
Client*	workspace_get_next_client();
void	workspace_cleanup();

/* spaces.c */
void	spaces_init();
void	spaces_show();
void	spaces_hide();
void	spaces_draw();
void	spaces_handle_button();
void	spaces_handle_motion();
void	spaces_handle_key();
int	spaces_get_workspace_at_point();
void	spaces_draw_workspace();
void	spaces_draw_window_thumbnail();
