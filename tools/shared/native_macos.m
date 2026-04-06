/*
 * native_macos.m — Native macOS file dialogs + application menu bar
 *
 * Objective-C implementation using NSOpenPanel, NSSavePanel, NSMenu.
 * All functions are C-callable (declared in native_macos.h).
 */

#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>
#include "native_macos.h"

/* ── File dialogs ─────────────────────────────────────────── */

char *dialog_open_file(const char *title, const char *filter_ext) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setTitle:[NSString stringWithUTF8String:title]];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];

        if (filter_ext && filter_ext[0]) {
            NSString *ext = [NSString stringWithUTF8String:filter_ext];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [panel setAllowedFileTypes:@[ext]];
#pragma clang diagnostic pop
        }

        if ([panel runModal] == NSModalResponseOK) {
            const char *path = [[[panel URL] path] UTF8String];
            return strdup(path);
        }
    }
    return NULL;
}

char *dialog_save_file(const char *title, const char *default_name) {
    @autoreleasepool {
        NSSavePanel *panel = [NSSavePanel savePanel];
        [panel setTitle:[NSString stringWithUTF8String:title]];
        if (default_name && default_name[0]) {
            [panel setNameFieldStringValue:[NSString stringWithUTF8String:default_name]];
        }

        if ([panel runModal] == NSModalResponseOK) {
            const char *path = [[[panel URL] path] UTF8String];
            return strdup(path);
        }
    }
    return NULL;
}

char *dialog_open_folder(const char *title) {
    @autoreleasepool {
        NSOpenPanel *panel = [NSOpenPanel openPanel];
        [panel setTitle:[NSString stringWithUTF8String:title]];
        [panel setCanChooseFiles:NO];
        [panel setCanChooseDirectories:YES];
        [panel setAllowsMultipleSelection:NO];

        if ([panel runModal] == NSModalResponseOK) {
            const char *path = [[[panel URL] path] UTF8String];
            return strdup(path);
        }
    }
    return NULL;
}

/* ── Menu bar ─────────────────────────────────────────────── */

static volatile MenuAction g_menu_action = MENU_NONE;

/* Menu target — receives Objective-C selectors, sets g_menu_action */
@interface MenuTarget : NSObject
- (void)menuNew:(id)sender;
- (void)menuOpen:(id)sender;
- (void)menuSave:(id)sender;
- (void)menuSaveAs:(id)sender;
- (void)menuExport:(id)sender;
- (void)menuQuit:(id)sender;
- (void)menuCopy:(id)sender;
- (void)menuPaste:(id)sender;
- (void)menuClear:(id)sender;
@end

@implementation MenuTarget
- (void)menuNew:(id)sender    { g_menu_action = MENU_NEW; }
- (void)menuOpen:(id)sender   { g_menu_action = MENU_OPEN; }
- (void)menuSave:(id)sender   { g_menu_action = MENU_SAVE; }
- (void)menuSaveAs:(id)sender { g_menu_action = MENU_SAVE_AS; }
- (void)menuExport:(id)sender { g_menu_action = MENU_EXPORT; }
- (void)menuQuit:(id)sender   { g_menu_action = MENU_QUIT; }
- (void)menuCopy:(id)sender   { g_menu_action = MENU_COPY; }
- (void)menuPaste:(id)sender  { g_menu_action = MENU_PASTE; }
- (void)menuClear:(id)sender  { g_menu_action = MENU_CLEAR; }
@end

static MenuTarget *g_target = nil;

static NSMenuItem *make_item(NSString *title, SEL action, NSString *key, NSUInteger mods) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                 action:action
                                          keyEquivalent:key];
    [item setTarget:g_target];
    if (mods) [item setKeyEquivalentModifierMask:mods];
    return item;
}

void native_menu_init(void) { native_menu_init_ex("Workbench"); }

void native_menu_init_ex(const char *app_name) {
    @autoreleasepool {
        g_target = [[MenuTarget alloc] init];

        /* Override app name in macOS menu bar */
        NSString *name = [NSString stringWithUTF8String:app_name];
        [[NSProcessInfo processInfo] setValue:name forKey:@"processName"];
        /* Also set CFBundleName for SDL apps without a bundle */
        NSMutableDictionary *info = [[[NSBundle mainBundle] infoDictionary] mutableCopy];
        if (info) {
            [info setObject:name forKey:@"CFBundleName"];
        }

        NSMenu *menuBar = [[NSMenu alloc] init];
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        NSMenu *appMenu = [[NSMenu alloc] initWithTitle:name];
        [appMenu addItem:make_item(@"Quit", @selector(menuQuit:), @"q", NSEventModifierFlagCommand)];
        [appMenuItem setSubmenu:appMenu];
        [menuBar addItem:appMenuItem];

        /* File menu */
        NSMenuItem *fileMenuItem = [[NSMenuItem alloc] init];
        NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
        [fileMenu addItem:make_item(@"New Project", @selector(menuNew:), @"n", NSEventModifierFlagCommand)];
        [fileMenu addItem:make_item(@"Open Project...", @selector(menuOpen:), @"o", NSEventModifierFlagCommand)];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItem:make_item(@"Save Project", @selector(menuSave:), @"s", NSEventModifierFlagCommand)];
        [fileMenu addItem:make_item(@"Save Project As...", @selector(menuSaveAs:), @"S",
                                    NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItem:make_item(@"Export C Files...", @selector(menuExport:), @"e", NSEventModifierFlagCommand)];
        [fileMenuItem setSubmenu:fileMenu];
        [menuBar addItem:fileMenuItem];

        /* Edit menu */
        NSMenuItem *editMenuItem = [[NSMenuItem alloc] init];
        NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
        [editMenu addItem:make_item(@"Copy Sprite", @selector(menuCopy:), @"c", NSEventModifierFlagCommand)];
        [editMenu addItem:make_item(@"Paste Sprite", @selector(menuPaste:), @"v", NSEventModifierFlagCommand)];
        [editMenu addItem:[NSMenuItem separatorItem]];
        [editMenu addItem:make_item(@"Clear Sprite", @selector(menuClear:), @"", 0)];
        [editMenuItem setSubmenu:editMenu];
        [menuBar addItem:editMenuItem];

        [NSApp setMainMenu:menuBar];
    }
}

MenuAction native_menu_poll(void) {
    MenuAction a = g_menu_action;
    g_menu_action = MENU_NONE;
    return a;
}

/* ── Dock icon ───────────────────────────────────────────── */

void native_set_dock_icon(const char *icns_path) {
    @autoreleasepool {
        NSString *path = [NSString stringWithUTF8String:icns_path];
        NSImage *icon = [[NSImage alloc] initWithContentsOfFile:path];
        if (icon) {
            [NSApp setApplicationIconImage:icon];
        }
    }
}
