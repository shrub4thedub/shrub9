# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**shrub9** is a customizable fork of the 9wm X11 window manager, inspired by Plan 9's rio. The project transforms the original minimalist 9wm into a configurable window manager with modern features while maintaining the Plan 9 aesthetic.

### Key Enhancements Over Original 9wm
- **Configuration System**: INI-style config file at `~/.config/shrub9/config`
- **Workspace Support**: Multiple virtual workspaces with configurable keybindings
- **Titlebar Support**: Optional titlebars with window names and configurable styling
- **Customizable Menus**: User-defined menu items and commands
- **Configurable Appearance**: Colors, fonts, and border styling

## Build System

### Building shrub9
```bash
make clean && make
```

The build produces the `shrub9` binary. Object files include the original 9wm components plus new modules:
- `config.o` - Configuration parsing and management
- `workspace.o` - Virtual workspace functionality

### Testing
```bash
# Run in nested X server for safe testing
~/scripts/test-shrub9.sh
```

### Installation
```bash
make install          # Install binary to /usr/bin
make install.man      # Install man page
```

## Architecture

### Core Data Structures

**Client Structure** (`dat.h`):
The central Client struct has been extended with:
- Workspace fields: `workspace`, `workspace_next`, `workspace_prev`
- Titlebar fields: `titlebar`, `title_width`

**Configuration System** (`config.h/config.c`):
- Global `Config config` structure holds all settings
- Supports appearance, behavior, workspace, and menu configuration
- Simple key=value parsing with validation and defaults

**Workspace Management** (`workspace.h/workspace.c`):
- Array of Workspace structs tracking clients per workspace
- Client visibility managed through X11 map/unmap operations
- Workspace switching updates both client visibility and focus

### Module Organization

**Core 9wm Files** (preserved from original):
- `9wm.c` - Main entry, initialization, X11 setup
- `client.c` - Window lifecycle and drawing (extended with titlebars)
- `event.c` - X11 event handling (extended with keyboard events)
- `menu.c` - Right-click menu system (extended with dynamic menus)
- `manage.c` - Window management and reparenting
- `grab.c` - Mouse interaction and menu display
- `cursor.c` - Cursor definitions
- `error.c` - X11 error handling

**New Extension Files**:
- `config.c/config.h` - Configuration system
- `workspace.c/workspace.h` - Virtual workspace functionality

**Header Files**:
- `dat.h` - Core data structures and global variables
- `fns.h` - Function prototypes for all modules

### Font System

The font system bridges modern font names with X11's XLFD format:
- Configuration accepts simple names (e.g., "dejavu sans")
- System attempts multiple loading strategies: exact name, XLFD patterns, wildcards
- Falls back to comprehensive list of common X11 fonts
- Global `font` variable used by both menu and titlebar rendering

### Event Flow

1. **Startup**: `9wm.c` loads config → initializes workspaces → sets up X11
2. **Window Creation**: `manage.c` creates parent → adds to workspace → creates titlebar if enabled
3. **Workspace Switching**: Keyboard events → hide current workspace clients → show target workspace clients
4. **Menu Interaction**: Right-click → rebuild dynamic menu → execute commands
5. **Window Cleanup**: Remove from workspace → destroy titlebar → normal 9wm cleanup

### Configuration Integration Points

Key locations where config values are applied:
- **Font Loading**: `9wm.c` main() uses `config.font`
- **Color Setup**: `initscreen()` in `9wm.c` applies border colors
- **Titlebar Creation**: `create_titlebar()` in `client.c` uses titlebar config
- **Menu Building**: `rebuild_menu()` in `menu.c` uses menu config
- **Workspace Setup**: `workspace_init()` uses workspace count and keybindings

## Configuration File Format

Location: `~/.config/shrub9/config`

```ini
# Appearance
active_color = #0066ff
inactive_color = #cccccc
font = -*-dejavu sans-bold-r-*-*-14-*-*-*-p-*-*-*

# Titlebars
show_titlebars = 1
titlebar_height = 20
titlebar_bg_color = #e0e0e0

# Workspaces
workspace_count = 6
workspace_key_1 = super+1

# Menu
menu_0_label = Terminal
menu_0_command = terminal
```

## Testing Environment

The project includes `~/scripts/test-shrub9.sh` which creates a safe nested X server using Xephyr for testing without disrupting the current desktop environment.

## Memory Management

Client structures are managed through:
- Allocation in `getclient()` with proper field initialization
- Workspace integration via linked lists (`workspace_next/prev`)
- Cleanup in `rmclient()` including titlebar and workspace removal