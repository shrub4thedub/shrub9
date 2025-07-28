/*
 * Configuration system for shrub9 (9wm fork)
 * Copyright multiple authors, see README for licence details
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <pwd.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "config.h"
#include "dat.h"
#include "fns.h"

Config config = {0};

static char* trim_whitespace(char *str);
static int parse_key_value(char *line, char *key, char *value, int max_len);
static unsigned int parse_modifiers(const char *mod_str);
static KeySym parse_keysym(const char *key_str);
static void config_set_defaults(void);
static int ensure_config_dir(const char *path);

int
config_init(void)
{
	struct passwd *pw;
	char *home;
	
	config_set_defaults();
	
	home = getenv("HOME");
	if (home == NULL) {
		pw = getpwuid(getuid());
		if (pw == NULL) {
			fprintf(stderr, "shrub9: cannot determine home directory\n");
			return 0;
		}
		home = pw->pw_dir;
	}
	
	snprintf(config.config_path, CONFIG_MAX_STRING, "%s/.config/shrub9/config", home);
	
	if (access(config.config_path, R_OK) == 0) {
		return config_load(config.config_path);
	} else {
		char config_dir[CONFIG_MAX_STRING];
		snprintf(config_dir, CONFIG_MAX_STRING, "%s/.config/shrub9", home);
		ensure_config_dir(config_dir);
		return config_load_default();
	}
}

int
config_load(const char *path)
{
	FILE *fp;
	char line[CONFIG_MAX_STRING];
	char key[CONFIG_MAX_STRING];
	char value[CONFIG_MAX_STRING];
	int line_num = 0;
	
	fp = fopen(path, "r");
	if (fp == NULL) {
		fprintf(stderr, "shrub9: cannot open config file %s\n", path);
		return 0;
	}
	
	while (fgets(line, sizeof(line), fp)) {
		line_num++;
		
		if (line[0] == '#' || line[0] == '\n' || line[0] == '\0')
			continue;
			
		if (!parse_key_value(line, key, value, CONFIG_MAX_STRING))
			continue;
		
		if (strcmp(key, "active_color") == 0) {
			strncpy(config.active_color, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "inactive_color") == 0) {
			strncpy(config.inactive_color, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "menu_bg_color") == 0) {
			strncpy(config.menu_bg_color, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "menu_fg_color") == 0) {
			strncpy(config.menu_fg_color, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "font") == 0) {
			strncpy(config.font, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "show_titlebars") == 0) {
			config.show_titlebars = atoi(value);
		} else if (strcmp(key, "titlebar_height") == 0) {
			config.titlebar_height = atoi(value);
		} else if (strcmp(key, "titlebar_bg_color") == 0) {
			strncpy(config.titlebar_bg_color, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "titlebar_fg_color") == 0) {
			strncpy(config.titlebar_fg_color, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "terminal") == 0) {
			strncpy(config.terminal, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "cursor_style") == 0) {
			strncpy(config.cursor_style, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "terminal_launcher_mode") == 0) {
			config.terminal_launcher_mode = atoi(value);
		} else if (strcmp(key, "terminal_classes") == 0) {
			strncpy(config.terminal_classes, value, CONFIG_MAX_STRING - 1);
		} else if (strcmp(key, "border_width") == 0) {
			config.border_width = atoi(value);
		} else if (strcmp(key, "inset_width") == 0) {
			config.inset_width = atoi(value);
		} else if (strcmp(key, "rounding") == 0) {
			config.rounding = atoi(value);
		} else if (strcmp(key, "rounding_radius") == 0) {
			config.rounding_radius = atoi(value);
		} else if (strcmp(key, "lower") == 0) {
			config.lower = atoi(value);
		} else if (strcmp(key, "workspace_count") == 0) {
			int count = atoi(value);
			if (count > 0 && count <= CONFIG_MAX_WORKSPACES) {
				config.workspaces.count = count;
				config.workspaces.enabled = 1;
			}
		} else if (strncmp(key, "menu_", 5) == 0 && strstr(key, "_label")) {
			int idx = atoi(key + 5);
			if (idx >= 0 && idx < CONFIG_MAX_MENU_ITEMS) {
				strncpy(config.menu_items[idx].label, value, CONFIG_MAX_STRING - 1);
				if (idx >= config.menu_count)
					config.menu_count = idx + 1;
			}
		} else if (strncmp(key, "menu_", 5) == 0 && strstr(key, "_command")) {
			int idx = atoi(key + 5);
			if (idx >= 0 && idx < CONFIG_MAX_MENU_ITEMS) {
				strncpy(config.menu_items[idx].command, value, CONFIG_MAX_STRING - 1);
			}
		} else if (strncmp(key, "workspace_key_", 14) == 0) {
			int ws = atoi(key + 14);
			if (ws >= 1 && ws <= CONFIG_MAX_WORKSPACES) {
				char *plus = strchr(value, '+');
				if (plus) {
					*plus = '\0';
					config.workspaces.switch_keys[ws-1].modifiers = parse_modifiers(value);
					config.workspaces.switch_keys[ws-1].keysym = parse_keysym(plus + 1);
					config.workspaces.switch_keys[ws-1].workspace_num = ws - 1;
					config.workspaces.switch_keys[ws-1].is_workspace_switch = 1;
				}
			}
		} else if (strcmp(key, "wallpaper") == 0) {
			strncpy(config.wallpaper_path, value, CONFIG_MAX_STRING - 1);
			config.wallpaper_enabled = 1;
		} else if (strcmp(key, "wallpaper_enabled") == 0) {
			config.wallpaper_enabled = atoi(value);
		} else {
			fprintf(stderr, "shrub9: unknown config key '%s' at line %d\n", key, line_num);
		}
	}
	
	fclose(fp);
	config.loaded = 1;
	return 1;
}

int
config_load_default(void)
{
	config_set_defaults();
	config.loaded = 1;
	return 1;
}

void
config_free(void)
{
	memset(&config, 0, sizeof(config));
}

int
config_get_workspace_key(KeySym keysym, unsigned int modifiers)
{
	int i;
	
	if (!config.workspaces.enabled)
		return -1;
		
	for (i = 0; i < config.workspaces.count; i++) {
		if (config.workspaces.switch_keys[i].keysym == keysym &&
		    config.workspaces.switch_keys[i].modifiers == modifiers) {
			return config.workspaces.switch_keys[i].workspace_num;
		}
	}
	return -1;
}

const char*
config_get_menu_command(int index)
{
	if (index >= 0 && index < config.menu_count)
		return config.menu_items[index].command;
	return NULL;
}

const char*
config_get_menu_label(int index)
{
	if (index >= 0 && index < config.menu_count)
		return config.menu_items[index].label;
	return NULL;
}

int
config_parse_color(const char *colorstr, unsigned long *pixel, Colormap cmap)
{
	XColor color;
	
	if (colorstr == NULL || pixel == NULL)
		return 0;
		
	if (XParseColor(dpy, cmap, colorstr, &color) &&
	    XAllocColor(dpy, cmap, &color)) {
		*pixel = color.pixel;
		return 1;
	}
	return 0;
}

XFontStruct*
config_load_font(const char *requested_font)
{
	XFontStruct *font = NULL;
	char **font_list;
	int font_count;
	char font_pattern[512];
	char *common_fonts[] = {
		"-*-dejavu sans-bold-r-*-*-14-*-*-*-p-*-*-*",
		"-*-liberation sans-bold-r-*-*-14-*-*-*-p-*-*-*",
		"-*-arial-bold-r-*-*-14-*-*-*-p-*-*-*", 
		"-*-helvetica-bold-r-*-*-14-*-*-*-p-*-*-*",
		"-adobe-helvetica-bold-r-*-*-14-*-*-*-p-*-*-*",
		"lucm.latin1.9",
		"blit",
		"9x15bold",
		"lucidasanstypewriter-12",
		"fixed",
		"*",
		NULL
	};
	int i;
	char **font_path;
	int path_count;
	
	if (requested_font && strlen(requested_font) > 0) {
		printf("[FONT DEBUG] Attempting to load requested font: %s\n", requested_font);
		
		/* Debug: Show current font path */
		font_path = XGetFontPath(dpy, &path_count);
		printf("[FONT DEBUG] Current X11 font path (%d entries):\n", path_count);
		for (i = 0; i < path_count; i++) {
			printf("[FONT DEBUG]   %d: %s\n", i, font_path[i]);
		}
		XFreeFontPath(font_path);
		
		/* Try exact font name first */
		font = XLoadQueryFont(dpy, requested_font);
		if (font) {
			printf("[FONT DEBUG] Successfully loaded exact font: %s\n", requested_font);
			printf("[FONT DEBUG] Font details - ascent: %d, descent: %d\n", font->ascent, font->descent);
			return font;
		}
		
		/* If it's a simple name (no dashes), try to build XLFD patterns */
		if (strchr(requested_font, '-') == NULL) {
			/* Try as family name with wildcards */
			snprintf(font_pattern, sizeof(font_pattern), "-*-%s-*-*-*-*-*-*-*-*-*-*-*-*", requested_font);
			printf("[FONT DEBUG] Trying XLFD pattern: %s\n", font_pattern);
			font = XLoadQueryFont(dpy, font_pattern);
			if (font) {
				printf("[FONT DEBUG] Successfully loaded with XLFD pattern: %s\n", font_pattern);
				printf("[FONT DEBUG] Font details - ascent: %d, descent: %d\n", font->ascent, font->descent);
				return font;
			}
			
			/* Try different variations */
			snprintf(font_pattern, sizeof(font_pattern), "-*-%s-bold-*-*-*-14-*-*-*-*-*-*-*", requested_font);
			printf("Trying bold XLFD pattern: %s\n", font_pattern);
			font = XLoadQueryFont(dpy, font_pattern);
			if (font) {
				printf("Successfully loaded with bold XLFD pattern: %s\n", font_pattern);
				return font;
			}
			
			/* Try medium weight */
			snprintf(font_pattern, sizeof(font_pattern), "-*-%s-medium-*-*-*-14-*-*-*-*-*-*-*", requested_font);
			printf("Trying medium XLFD pattern: %s\n", font_pattern);
			font = XLoadQueryFont(dpy, font_pattern);
			if (font) {
				printf("Successfully loaded with medium XLFD pattern: %s\n", font_pattern);
				return font;
			}
		}
		
		printf("Failed to load requested font: %s\n", requested_font);
	}
	
	/* Try to find fonts that contain the requested pattern using wildcards */
	if (requested_font && strlen(requested_font) > 0 && strchr(requested_font, '-') == NULL) {
		snprintf(font_pattern, sizeof(font_pattern), "*%s*", requested_font);
		printf("[FONT DEBUG] Trying wildcard pattern: %s\n", font_pattern);
		font_list = XListFonts(dpy, font_pattern, 50, &font_count);
		if (font_count > 0) {
			printf("[FONT DEBUG] Found %d matching fonts for pattern '%s':\n", font_count, font_pattern);
			for (i = 0; i < font_count; i++) {
				printf("[FONT DEBUG]   %d: %s\n", i, font_list[i]);
			}
			for (i = 0; i < font_count && i < 3; i++) {
				printf("[FONT DEBUG]   Trying to load: %s\n", font_list[i]);
				font = XLoadQueryFont(dpy, font_list[i]);
				if (font) {
					printf("[FONT DEBUG] Successfully loaded: %s\n", font_list[i]);
					printf("[FONT DEBUG] Font details - ascent: %d, descent: %d\n", font->ascent, font->descent);
					XFreeFontNames(font_list);
					return font;
				} else {
					printf("[FONT DEBUG]   Failed to load: %s\n", font_list[i]);
				}
			}
			XFreeFontNames(font_list);
		} else {
			printf("[FONT DEBUG] No fonts found for wildcard pattern: %s\n", font_pattern);
		}
	}
	
	/* Fall back to common fonts */
	printf("Falling back to common fonts...\n");
	for (i = 0; common_fonts[i] != NULL; i++) {
		printf("  Trying common font: %s\n", common_fonts[i]);
		font = XLoadQueryFont(dpy, common_fonts[i]);
		if (font) {
			printf("Successfully loaded common font: %s\n", common_fonts[i]);
			return font;
		}
	}
	
	printf("ERROR: Could not load any font!\n");
	return NULL;
}

static void
config_set_defaults(void)
{
	strncpy(config.active_color, DEFAULT_ACTIVE_COLOR, CONFIG_MAX_STRING - 1);
	strncpy(config.inactive_color, DEFAULT_INACTIVE_COLOR, CONFIG_MAX_STRING - 1);
	strncpy(config.menu_bg_color, DEFAULT_MENU_BG_COLOR, CONFIG_MAX_STRING - 1);
	strncpy(config.menu_fg_color, DEFAULT_MENU_FG_COLOR, CONFIG_MAX_STRING - 1);
	strncpy(config.font, DEFAULT_FONT, CONFIG_MAX_STRING - 1);
	strncpy(config.terminal, DEFAULT_TERMINAL, CONFIG_MAX_STRING - 1);
	strncpy(config.cursor_style, DEFAULT_CURSOR, CONFIG_MAX_STRING - 1);
	
	config.terminal_launcher_mode = DEFAULT_TERMINAL_LAUNCHER_MODE;
	strncpy(config.terminal_classes, DEFAULT_TERMINAL_CLASSES, CONFIG_MAX_STRING - 1);
	
	config.border_width = DEFAULT_BORDER_WIDTH;
	config.inset_width = DEFAULT_INSET_WIDTH;
	
	config.rounding = DEFAULT_ROUNDING;
	config.rounding_radius = DEFAULT_ROUNDING_RADIUS;
	
	config.lower = DEFAULT_LOWER;
	
	config.show_titlebars = DEFAULT_SHOW_TITLEBARS;
	config.titlebar_height = DEFAULT_TITLEBAR_HEIGHT;
	strncpy(config.titlebar_bg_color, DEFAULT_TITLEBAR_BG_COLOR, CONFIG_MAX_STRING - 1);
	strncpy(config.titlebar_fg_color, DEFAULT_TITLEBAR_FG_COLOR, CONFIG_MAX_STRING - 1);
	
	config.workspaces.enabled = 1;
	config.workspaces.count = DEFAULT_WORKSPACE_COUNT;
	
	config.wallpaper_enabled = 0;
	strncpy(config.wallpaper_path, "", CONFIG_MAX_STRING - 1);
	
	config.menu_count = 6;
	strncpy(config.menu_items[0].label, "New", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[0].command, "terminal", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[1].label, "Reshape", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[1].command, "reshape", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[2].label, "Move", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[2].command, "move", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[3].label, "Delete", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[3].command, "delete", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[4].label, "Hide", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[4].command, "hide", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[5].label, "Tile", CONFIG_MAX_STRING - 1);
	strncpy(config.menu_items[5].command, "tile", CONFIG_MAX_STRING - 1);
	
	{
		int i;
		for (i = 0; i < DEFAULT_WORKSPACE_COUNT; i++) {
			config.workspaces.switch_keys[i].modifiers = DEFAULT_WORKSPACE_MOD;
			config.workspaces.switch_keys[i].keysym = XK_1 + i;
			config.workspaces.switch_keys[i].workspace_num = i;
			config.workspaces.switch_keys[i].is_workspace_switch = 1;
		}
	}
}

static char*
trim_whitespace(char *str)
{
	char *end;
	
	while (isspace((unsigned char)*str))
		str++;
	
	if (*str == 0)
		return str;
	
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end))
		end--;
	
	end[1] = '\0';
	return str;
}

static int
parse_key_value(char *line, char *key, char *value, int max_len)
{
	char *eq_pos = strchr(line, '=');
	char *trimmed_key, *trimmed_value;
	
	if (eq_pos == NULL)
		return 0;
	
	*eq_pos = '\0';
	trimmed_key = trim_whitespace(line);
	trimmed_value = trim_whitespace(eq_pos + 1);
	
	if (strlen(trimmed_key) >= max_len || strlen(trimmed_value) >= max_len)
		return 0;
	
	strcpy(key, trimmed_key);
	strcpy(value, trimmed_value);
	return 1;
}

static unsigned int
parse_modifiers(const char *mod_str)
{
	unsigned int mods = 0;
	char *str = strdup(mod_str);
	char *token = strtok(str, "|+");
	
	while (token != NULL) {
		token = trim_whitespace(token);
		if (strcmp(token, "super") == 0 || strcmp(token, "Super") == 0 || strcmp(token, "Mod4") == 0)
			mods |= Mod4Mask;
		else if (strcmp(token, "alt") == 0 || strcmp(token, "Alt") == 0 || strcmp(token, "Mod1") == 0)
			mods |= Mod1Mask;
		else if (strcmp(token, "ctrl") == 0 || strcmp(token, "Ctrl") == 0 || strcmp(token, "Control") == 0)
			mods |= ControlMask;
		else if (strcmp(token, "shift") == 0 || strcmp(token, "Shift") == 0)
			mods |= ShiftMask;
		token = strtok(NULL, "|+");
	}
	
	free(str);
	return mods;
}

static KeySym
parse_keysym(const char *key_str)
{
	char *trimmed = strdup(key_str);
	KeySym ks;
	trimmed = trim_whitespace(trimmed);
	ks = XStringToKeysym(trimmed);
	free(trimmed);
	return ks;
}

static int
ensure_config_dir(const char *path)
{
	struct stat st = {0};
	
	if (stat(path, &st) == -1) {
		if (mkdir(path, 0700) == -1) {
			fprintf(stderr, "shrub9: cannot create config directory %s\n", path);
			return 0;
		}
	}
	return 1;
}

int
is_terminal_class(const char *class_name)
{
	char *classes_copy, *token;
	int match = 0;
	
	if (!class_name || !config.terminal_launcher_mode)
		return 0;
		
	classes_copy = strdup(config.terminal_classes);
	if (!classes_copy)
		return 0;
		
	token = strtok(classes_copy, ",");
	while (token != NULL) {
		char *end;
		/* Trim whitespace */
		while (isspace((unsigned char)*token))
			token++;
		end = token + strlen(token) - 1;
		while (end > token && isspace((unsigned char)*end))
			end--;
		end[1] = '\0';
		
		if (strcasecmp(token, class_name) == 0) {
			match = 1;
			break;
		}
		token = strtok(NULL, ",");
	}
	
	free(classes_copy);
	return match;
}

int
config_apply_wallpaper(void)
{
	char command[CONFIG_MAX_STRING * 2];
	
	if (!config.wallpaper_enabled || strlen(config.wallpaper_path) == 0) {
		return 1; /* Success, no wallpaper to set */
	}
	
	/* Verify file exists and is readable */
	if (access(config.wallpaper_path, R_OK) != 0) {
		fprintf(stderr, "shrub9: wallpaper file not accessible: %s\n", config.wallpaper_path);
		return 0;
	}
	
	/* Check if feh is available */
	if (system("which feh >/dev/null 2>&1") != 0) {
		fprintf(stderr, "shrub9: warning: feh not found, wallpaper disabled\n");
		config.wallpaper_enabled = 0;
		return 1; /* Don't fail, just disable wallpaper */
	}
	
	/* Use feh to set wallpaper */
	snprintf(command, sizeof(command), "feh --bg-scale \"%s\" 2>/dev/null", config.wallpaper_path);
	
	if (system(command) != 0) {
		fprintf(stderr, "shrub9: failed to set wallpaper with feh\n");
		return 0;
	}
	
	return 1;
}