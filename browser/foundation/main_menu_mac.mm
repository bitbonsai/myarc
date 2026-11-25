#import "myarc/main_menu_mac.h"

#import <Cocoa/Cocoa.h>

#include <cstdint>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "myarc/browser_workspace_adapter.h"
#include "myarc/command_ids.h"

namespace {

constexpr NSInteger kSpacesListStartTag = 35490;
constexpr NSInteger kSpacesListEndTag = 35491;
constexpr NSInteger kDynamicSpaceItemTag = 35492;

NSMenuItem* MakeCommandItem(NSString* title,
                            NSInteger command_id,
                            NSString* key_equivalent = @"",
                            NSEventModifierFlags modifiers = 0) {
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                action:@selector(commandDispatch:)
                                         keyEquivalent:key_equivalent];
  item.tag = command_id;
  item.keyEquivalentModifierMask = modifiers;
  return item;
}

NSString* ArrowKey(unichar key) {
  return [NSString stringWithCharacters:&key length:1];
}

void RemoveItemWithTag(NSMenu* menu, NSInteger tag) {
  if (NSMenuItem* item = [menu itemWithTag:tag]) {
    [menu removeItem:item];
  }
}

void CollapseAdjacentSeparators(NSMenu* menu) {
  BOOL previous_was_separator = YES;
  for (NSInteger i = 0; i < menu.numberOfItems;) {
    NSMenuItem* item = [menu itemAtIndex:i];
    if (item.separatorItem) {
      if (previous_was_separator) {
        [menu removeItemAtIndex:i];
        continue;
      }
      previous_was_separator = YES;
    } else {
      previous_was_separator = NO;
    }
    ++i;
  }
  while (menu.numberOfItems > 0 &&
         [menu itemAtIndex:menu.numberOfItems - 1].separatorItem) {
    [menu removeItemAtIndex:menu.numberOfItems - 1];
  }
}

void UpdateAppMenu(NSMenu* main_menu) {
  if (main_menu.numberOfItems == 0) {
    return;
  }
  NSMenu* app_menu = [main_menu itemAtIndex:0].submenu;
  if (!app_menu) {
    return;
  }
  if (NSMenuItem* import_item = [app_menu itemWithTag:IDC_IMPORT_SETTINGS]) {
    import_item.title = @"Import from Another Browser\u2026";
  }
  NSInteger settings_index = [app_menu indexOfItemWithTag:IDC_OPTIONS];
  if (settings_index >= 0) {
    [app_menu insertItem:MakeCommandItem(@"Set as Default Browser",
                                         IDC_MYARC_SET_DEFAULT_BROWSER)
                 atIndex:settings_index + 1];
  }
}

void UpdateEditMenu(NSMenu* main_menu) {
  NSMenu* edit_menu = [main_menu itemWithTag:IDC_EDIT_MENU].submenu;
  if (!edit_menu) {
    return;
  }
  NSInteger copy_index = [edit_menu indexOfItemWithTag:IDC_CONTENT_CONTEXT_COPY];
  NSInteger insert_at = copy_index >= 0 ? copy_index + 1 : 0;
  [edit_menu
      insertItem:MakeCommandItem(@"Copy URL", IDC_COPY_URL, @"c",
                                 NSEventModifierFlagCommand |
                                     NSEventModifierFlagShift)
         atIndex:insert_at++];
  [edit_menu
      insertItem:MakeCommandItem(@"Copy URL as Markdown",
                                 IDC_MYARC_COPY_URL_MARKDOWN, @"c",
                                 NSEventModifierFlagCommand |
                                     NSEventModifierFlagShift |
                                     NSEventModifierFlagOption)
         atIndex:insert_at++];
  [edit_menu
      insertItem:MakeCommandItem(@"Copy URL as Quote", IDC_MYARC_COPY_URL_QUOTE,
                                 @"c",
                                 NSEventModifierFlagCommand |
                                     NSEventModifierFlagShift |
                                     NSEventModifierFlagControl)
         atIndex:insert_at++];
}

void UpdateViewMenu(NSMenu* main_menu) {
  NSMenu* view_menu = [main_menu itemWithTag:IDC_VIEW_MENU].submenu;
  if (!view_menu) {
    return;
  }
  for (NSInteger tag : {
           IDC_SHOW_BOOKMARK_BAR,
           IDC_TOGGLE_FULLSCREEN_TOOLBAR,
           IDC_SHOW_FULL_URLS,
           IDC_SHOW_GOOGLE_LENS_SHORTCUT,
           IDC_SHOW_AI_MODE_OMNIBOX_BUTTON,
           IDC_SHOW_SEARCH_TOOLS,
           IDC_TOGGLE_VERTICAL_TABS,
           IDC_CUSTOMIZE_TOUCH_BAR,
       }) {
    RemoveItemWithTag(view_menu, tag);
  }

  // Insert Split View actions right above Expand Current Split (which was
  // placed after the reload items by main_menu_builder.mm).
  NSInteger expand_index = [view_menu indexOfItemWithTag:IDC_MYARC_EXPAND_SPLIT];
  if (expand_index >= 0) {
    [view_menu insertItem:[NSMenuItem separatorItem] atIndex:expand_index++];
    [view_menu
        insertItem:MakeCommandItem(@"Add Split View", IDC_NEW_SPLIT_TAB, @"=",
                                   NSEventModifierFlagControl |
                                       NSEventModifierFlagShift)
           atIndex:expand_index++];
    [view_menu
        insertItem:MakeCommandItem(@"Close this Split View",
                                   IDC_MYARC_SEPARATE_SPLIT, @"-",
                                   NSEventModifierFlagControl |
                                       NSEventModifierFlagShift)
           atIndex:expand_index++];
    [view_menu
        insertItem:MakeCommandItem(@"Separate Page from Split View",
                                   IDC_MYARC_SEPARATE_SPLIT)
           atIndex:expand_index++];
  }

  CollapseAdjacentSeparators(view_menu);
}

void UpdateWindowMenu(NSMenu* main_menu) {
  NSMenu* window_menu = [main_menu itemWithTag:IDC_WINDOW_MENU].submenu;
  if (!window_menu) {
    return;
  }
  [window_menu insertItem:MakeCommandItem(@"Stay On Top", IDC_MYARC_STAY_ON_TOP)
                  atIndex:0];

  // Extensions and Ad Blocking live in the top-level Extensions menu.
  RemoveItemWithTag(window_menu, IDC_MANAGE_EXTENSIONS);
  RemoveItemWithTag(window_menu, IDC_MYARC_BLOCKER_POPUP);

  NSInteger downloads_index =
      [window_menu indexOfItemWithTag:IDC_SHOW_DOWNLOADS];
  if (downloads_index >= 0) {
    [window_menu itemAtIndex:downloads_index].title =
        @"View Downloads\u2026";
    [window_menu
        insertItem:MakeCommandItem(@"Open Library\u2026",
                                   IDC_MYARC_OPEN_LIBRARY, @"l",
                                   NSEventModifierFlagCommand |
                                       NSEventModifierFlagShift)
           atIndex:downloads_index];
    NSInteger after_downloads = downloads_index + 2;
    [window_menu
        insertItem:MakeCommandItem(@"View Media\u2026", IDC_MYARC_VIEW_MEDIA)
           atIndex:after_downloads++];
    [window_menu
        insertItem:MakeCommandItem(@"View Boosts\u2026", IDC_MYARC_VIEW_BOOSTS)
           atIndex:after_downloads++];
  }
  CollapseAdjacentSeparators(window_menu);
}

void UpdateHelpMenu(NSMenu* main_menu, NSApplication* nsapp) {
  NSMenu* help_menu = nsapp ? nsapp.helpMenu : nil;
  if (!help_menu && main_menu.numberOfItems > 0) {
    help_menu = [main_menu itemAtIndex:main_menu.numberOfItems - 1].submenu;
  }
  if (!help_menu) {
    return;
  }
  NSInteger index = 0;
  [help_menu
      insertItem:MakeCommandItem(@"Getting Started",
                                 IDC_MYARC_HELP_GETTING_STARTED)
         atIndex:index++];
  [help_menu
      insertItem:MakeCommandItem(@"Essential Keyboard Shortcuts",
                                 IDC_MYARC_HELP_SHORTCUTS)
         atIndex:index++];
  [help_menu
      insertItem:MakeCommandItem(@"Restore Data", IDC_MYARC_HELP_RESTORE_DATA)
         atIndex:index++];
  [help_menu insertItem:[NSMenuItem separatorItem] atIndex:index++];

  NSMenuItem* troubleshooting_item =
      [[NSMenuItem alloc] initWithTitle:@"Troubleshooting"
                                 action:nil
                          keyEquivalent:@""];
  NSMenu* troubleshooting_menu =
      [[NSMenu alloc] initWithTitle:@"Troubleshooting"];
  [troubleshooting_menu
      addItem:MakeCommandItem(@"Clear Cache and Refresh",
                              IDC_MYARC_CLEAR_CACHE_RELOAD)];
  [troubleshooting_menu
      addItem:MakeCommandItem(@"Clear Cookies and Refresh",
                              IDC_MYARC_CLEAR_COOKIES_RELOAD)];
  [troubleshooting_menu
      addItem:MakeCommandItem(@"Task Manager", IDC_TASK_MANAGER_MAIN_MENU)];
  [troubleshooting_menu addItem:[NSMenuItem separatorItem]];
  [troubleshooting_menu
      addItem:MakeCommandItem(@"Reveal Profile Folder",
                              IDC_MYARC_OPEN_PROFILE_FOLDER)];
  troubleshooting_item.submenu = troubleshooting_menu;
  [help_menu insertItem:troubleshooting_item atIndex:index++];
  [help_menu insertItem:[NSMenuItem separatorItem] atIndex:index++];
  CollapseAdjacentSeparators(help_menu);
}

}  // namespace

@interface MyarcSpacesMenuDelegate : NSObject <NSMenuDelegate>
- (void)selectSpaceFromMenu:(NSMenuItem*)sender;
@end

@implementation MyarcSpacesMenuDelegate

- (void)menuNeedsUpdate:(NSMenu*)menu {
  NSInteger start = [menu indexOfItemWithTag:kSpacesListStartTag];
  NSInteger end = [menu indexOfItemWithTag:kSpacesListEndTag];
  if (start < 0 || end <= start) {
    return;
  }
  for (NSInteger i = end - 1; i > start; --i) {
    [menu removeItemAtIndex:i];
  }

  BrowserWindowInterface* browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  auto* adapter =
      browser ? myarc::BrowserWorkspaceAdapter::ForBrowser(*browser) : nullptr;
  if (!adapter || !adapter->ready()) {
    return;
  }

  const auto sidebar = adapter->Sidebar();
  NSInteger insert_at = start + 1;
  const size_t total = sidebar.spaces.size();
  for (size_t i = 0; i < total; ++i) {
    const auto& space = sidebar.spaces[i];
    std::string name(space.name.data(), space.name.size());
    NSString* title =
        name.empty() ? @"Untitled" : base::SysUTF8ToNSString(name);
    NSString* key = @"";
    NSEventModifierFlags mods = 0;
    if (i < 8) {
      key = [NSString stringWithFormat:@"%zu", i + 1];
      mods = NSEventModifierFlagControl;
    } else if (i + 1 == total) {
      key = @"9";
      mods = NSEventModifierFlagControl;
    }
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:@selector(selectSpaceFromMenu:)
                                           keyEquivalent:key];
    item.target = self;
    item.tag = kDynamicSpaceItemTag;
    item.keyEquivalentModifierMask = mods;
    item.representedObject = @(space.id);
    item.state = (space.id == sidebar.current_space.id)
                     ? NSControlStateValueOn
                     : NSControlStateValueOff;
    [menu insertItem:item atIndex:insert_at++];
  }
}

- (void)selectSpaceFromMenu:(NSMenuItem*)sender {
  NSNumber* space_id = base::apple::ObjCCast<NSNumber>(sender.representedObject);
  if (!space_id) {
    return;
  }
  BrowserWindowInterface* browser =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  auto* adapter =
      browser ? myarc::BrowserWorkspaceAdapter::ForBrowser(*browser) : nullptr;
  if (adapter && adapter->ready()) {
    adapter->SelectSpace(space_id.unsignedLongLongValue);
  }
}

@end

namespace myarc {

namespace {

MyarcSpacesMenuDelegate* SharedSpacesMenuDelegate() {
  static MyarcSpacesMenuDelegate* delegate =
      [[MyarcSpacesMenuDelegate alloc] init];
  return delegate;
}

NSMenuItem* BuildSpacesMenu() {
  NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:@"Spaces"
                                               action:nil
                                        keyEquivalent:@""];
  top.tag = IDC_MYARC_SPACES_MENU;
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Spaces"];
  [menu addItem:MakeCommandItem(@"New Space\u2026", IDC_MYARC_NEW_SPACE)];
  [menu addItem:MakeCommandItem(@"Edit Theme\u2026", IDC_MYARC_EDIT_SPACE_THEME)];
  [menu addItem:MakeCommandItem(@"Rename Space", IDC_MYARC_RENAME_SPACE)];
  [menu addItem:MakeCommandItem(@"Change Profile\u2026",
                                IDC_MYARC_CHANGE_PROFILE)];
  [menu addItem:[NSMenuItem separatorItem]];
  [menu addItem:MakeCommandItem(@"Next Space", IDC_MYARC_SELECT_NEXT_SPACE,
                                ArrowKey(NSRightArrowFunctionKey),
                                NSEventModifierFlagCommand |
                                    NSEventModifierFlagOption)];
  [menu addItem:MakeCommandItem(@"Previous Space",
                                IDC_MYARC_SELECT_PREVIOUS_SPACE,
                                ArrowKey(NSLeftArrowFunctionKey),
                                NSEventModifierFlagCommand |
                                    NSEventModifierFlagOption)];
  NSMenuItem* start_sep = [NSMenuItem separatorItem];
  start_sep.tag = kSpacesListStartTag;
  [menu addItem:start_sep];
  NSMenuItem* end_sep = [NSMenuItem separatorItem];
  end_sep.tag = kSpacesListEndTag;
  [menu addItem:end_sep];
  [menu addItem:MakeCommandItem(@"Manage Spaces\u2026",
                                IDC_MYARC_MANAGE_SPACES)];
  menu.delegate = SharedSpacesMenuDelegate();
  top.submenu = menu;
  return top;
}

NSMenuItem* BuildTabsMenu() {
  NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:@"Tabs"
                                               action:nil
                                        keyEquivalent:@""];
  top.tag = IDC_MYARC_TABS_MENU;
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Tabs"];
  [menu addItem:MakeCommandItem(@"New Tab\u2026", IDC_NEW_TAB, @"t",
                                NSEventModifierFlagCommand)];
  [menu addItem:MakeCommandItem(@"Pin/Unpin Tab", IDC_MYARC_TOGGLE_PIN, @"d",
                                NSEventModifierFlagCommand)];
  [menu addItem:MakeCommandItem(@"New Folder\u2026", IDC_MYARC_NEW_FOLDER)];
  [menu addItem:[NSMenuItem separatorItem]];
  [menu addItem:MakeCommandItem(@"Next Tab", IDC_SELECT_NEXT_TAB,
                                ArrowKey(NSDownArrowFunctionKey),
                                NSEventModifierFlagCommand |
                                    NSEventModifierFlagOption)];
  [menu addItem:MakeCommandItem(@"Previous Tab", IDC_SELECT_PREVIOUS_TAB,
                                ArrowKey(NSUpArrowFunctionKey),
                                NSEventModifierFlagCommand |
                                    NSEventModifierFlagOption)];
  [menu addItem:MakeCommandItem(@"Reveal Tab in Sidebar", IDC_MYARC_REVEAL_TAB)];
  [menu addItem:[NSMenuItem separatorItem]];
  [menu addItem:MakeCommandItem(@"Clear Today", IDC_MYARC_CLEAR_TODAY, @"k",
                                NSEventModifierFlagCommand |
                                    NSEventModifierFlagShift)];
  [menu addItem:MakeCommandItem(@"Reset all tabs in this Space",
                                IDC_MYARC_RESET_SPACE_TABS)];
  top.submenu = menu;
  return top;
}

NSMenuItem* BuildArchiveMenu() {
  NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:@"Archive"
                                               action:nil
                                        keyEquivalent:@""];
  top.tag = IDC_MYARC_ARCHIVE_MENU;
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Archive"];
  [menu addItem:MakeCommandItem(@"Go Back", IDC_BACK, @"[",
                                NSEventModifierFlagCommand)];
  [menu addItem:MakeCommandItem(@"Go Forward", IDC_FORWARD, @"]",
                                NSEventModifierFlagCommand)];
  [menu addItem:[NSMenuItem separatorItem]];
  [menu addItem:MakeCommandItem(@"View History", IDC_SHOW_HISTORY, @"y",
                                NSEventModifierFlagCommand)];
  [menu addItem:MakeCommandItem(@"View Archive\u2026", IDC_MYARC_VIEW_ARCHIVE)];
  [menu addItem:MakeCommandItem(@"Clear Archive", IDC_MYARC_CLEAR_ARCHIVE)];
  top.submenu = menu;
  return top;
}

NSMenuItem* BuildExtensionsMenu() {
  NSMenuItem* top = [[NSMenuItem alloc] initWithTitle:@"Extensions"
                                               action:nil
                                        keyEquivalent:@""];
  top.tag = IDC_MYARC_EXTENSIONS_MENU;
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Extensions"];
  [menu addItem:MakeCommandItem(@"Add Extension\u2026",
                                IDC_MYARC_ADD_EXTENSION)];
  [menu addItem:MakeCommandItem(@"Manage Extensions\u2026",
                                IDC_MANAGE_EXTENSIONS)];
  [menu addItem:[NSMenuItem separatorItem]];
  [menu addItem:MakeCommandItem(@"Ad Blocking for This Site\u2026",
                                IDC_MYARC_BLOCKER_POPUP)];
  top.submenu = menu;
  return top;
}

}  // namespace

void ApplyMainMenu(NSMenu* main_menu, NSApplication* nsapp) {
  if (!main_menu) {
    return;
  }
  UpdateAppMenu(main_menu);
  UpdateEditMenu(main_menu);
  UpdateViewMenu(main_menu);
  UpdateWindowMenu(main_menu);
  UpdateHelpMenu(main_menu, nsapp);

  // Drop Chromium menus that Arc replaces with Spaces, Tabs, Archive, and
  // Extensions.
  for (NSInteger tag : {
           IDC_HISTORY_MENU,
           IDC_BOOKMARKS_MENU,
           IDC_PROFILE_MAIN_MENU,
           IDC_TAB_MENU,
       }) {
    RemoveItemWithTag(main_menu, tag);
  }

  NSInteger view_index = [main_menu indexOfItemWithTag:IDC_VIEW_MENU];
  NSInteger insert_at = view_index >= 0 ? view_index + 1 : main_menu.numberOfItems;
  [main_menu insertItem:BuildSpacesMenu() atIndex:insert_at++];
  [main_menu insertItem:BuildTabsMenu() atIndex:insert_at++];
  [main_menu insertItem:BuildArchiveMenu() atIndex:insert_at++];
  [main_menu insertItem:BuildExtensionsMenu() atIndex:insert_at++];
}

}  // namespace myarc
