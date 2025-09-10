#include <stdio.h>
#include <X11/Xlib.h>
#include "config.h"
#include "dat.h"

Display *dpy;
XFontStruct *font = NULL;

#ifdef XFT
XftFont *xft_font = NULL;
int use_xft = 0;
#endif

int main() {
    dpy = XOpenDisplay("");
    if (!dpy) {
        printf("Cannot open display\n");
        return 1;
    }
    
    printf("Testing font loading for: Go Mono\n");
    
    if (config_load_font_hybrid("Go Mono")) {
        printf("SUCCESS: Font loaded successfully!\n");
#ifdef XFT
        if (use_xft && xft_font) {
            printf("  Using Xft font - ascent: %d, descent: %d\n", 
                   xft_font->ascent, xft_font->descent);
        } else
#endif
        if (font) {
            printf("  Using core font - ascent: %d, descent: %d\n", 
                   font->ascent, font->descent);
        }
    } else {
        printf("FAILED: Could not load font\n");
        return 1;
    }
    
    XCloseDisplay(dpy);
    return 0;
}