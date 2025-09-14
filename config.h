/*
 * Configuration system for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <X11/Xlib.h>

#define CONFIG_MAX_STRING 256
#define CONFIG_MAX_MENU_ITEMS 32
#define CONFIG_MAX_SUBMENU_ITEMS 16
#define CONFIG_MAX_WORKSPACES 10
#define CONFIG_MAX_KEYBINDS 64

typedef struct KeyBind KeyBind;
typedef struct MenuConfig MenuConfig;
typedef struct WorkspaceConfig WorkspaceConfig;
typedef struct Config Config;

struct KeyBind {
	unsigned int modifiers;
	KeySym keysym;
	char command[CONFIG_MAX_STRING];
	int workspace_num;
	int is_workspace_switch;
};

struct MenuConfig {
	char label[CONFIG_MAX_STRING];
	char command[CONFIG_MAX_STRING];
	int is_folder;
	int submenu_count;
	struct MenuConfig *submenu_items;
};

struct WorkspaceConfig {
	int enabled;
	int count;
	KeyBind switch_keys[CONFIG_MAX_WORKSPACES];
};

struct Config {
	/* Appearance */
	char active_color[CONFIG_MAX_STRING];
	char inactive_color[CONFIG_MAX_STRING];
	char menu_bg_color[CONFIG_MAX_STRING];
	char menu_fg_color[CONFIG_MAX_STRING];
	char font[CONFIG_MAX_STRING];
	
	/* Titlebars */
	int show_titlebars;
	int titlebar_height;
	char titlebar_bg_color[CONFIG_MAX_STRING];
	char titlebar_fg_color[CONFIG_MAX_STRING];
	
	/* Behavior */
	int border_width;
	int inset_width;
	char terminal[CONFIG_MAX_STRING];
	char cursor_style[CONFIG_MAX_STRING];
	
	
	/* Terminal-launcher */
	int terminal_launcher_mode;
	char terminal_classes[CONFIG_MAX_STRING];
	
	/* Menu */
	MenuConfig menu_items[CONFIG_MAX_MENU_ITEMS];
	int menu_count;
	int lower;
	
	/* Workspaces */
	WorkspaceConfig workspaces;
	
	/* Wallpaper */
	char wallpaper_path[CONFIG_MAX_STRING];
	int wallpaper_enabled;
	
	/* Plumber */
	int plumb_enabled;
	char plumb_send_path[CONFIG_MAX_STRING];
	
	/* Keybindings */
	KeyBind keybinds[CONFIG_MAX_KEYBINDS];
	int keybind_count;
	
	/* Internal state */
	int loaded;
	char config_path[CONFIG_MAX_STRING];
};

/* Global configuration instance */
extern Config config;

/* Function prototypes */
int config_init(void);
int config_load(const char *path);
int config_load_default(void);
void config_free(void);
int config_get_workspace_key(KeySym keysym, unsigned int modifiers);
const char* config_get_menu_command(int index);
const char* config_get_menu_label(int index);

/* Color parsing helpers */
int config_parse_color(const char *colorstr, unsigned long *pixel, Colormap cmap);

/* Font loading helpers */
XFontStruct* config_load_font(const char *requested_font);
int config_load_font_hybrid(const char *requested_font);

/* Terminal detection helpers */
int is_terminal_class(const char *class_name);

/* Default values */
#define DEFAULT_ACTIVE_COLOR "black"
#define DEFAULT_INACTIVE_COLOR "white"
#define DEFAULT_MENU_BG_COLOR "white"
#define DEFAULT_MENU_FG_COLOR "black"
#define DEFAULT_FONT "-*-dejavu sans-bold-r-*-*-14-*-*-*-p-*-*-*"
#define DEFAULT_TERMINAL "xterm"
#define DEFAULT_CURSOR "modern"
#define DEFAULT_BORDER_WIDTH 4
#define DEFAULT_INSET_WIDTH 1
#define DEFAULT_WORKSPACE_COUNT 4
#define DEFAULT_WORKSPACE_MOD Mod4Mask
#define DEFAULT_SHOW_TITLEBARS 0
#define DEFAULT_TITLEBAR_HEIGHT 18
#define DEFAULT_TITLEBAR_BG_COLOR "#cccccc"
#define DEFAULT_TITLEBAR_FG_COLOR "#000000"
#define DEFAULT_LOWER 0
#define DEFAULT_TERMINAL_LAUNCHER_MODE 1
#define DEFAULT_TERMINAL_CLASSES "st,st-256color,alacritty,xterm,urxvt,kitty,gnome-terminal,xfce4-terminal,konsole"
#define DEFAULT_PLUMB_ENABLED 0
#define DEFAULT_PLUMB_SEND_PATH "/mnt/plumb/send"

#endif /* CONFIG_H */