//
// Created by Lea Anthony on 6/1/21.
//

#ifndef MENU_DARWIN_H
#define MENU_DARWIN_H

#include "common.h"
#include "ffenestri_darwin.h"

enum MenuItemType {Text = 0, Checkbox = 1, Radio = 2};
enum MenuType {ApplicationMenuType = 0, ContextMenuType = 1, TrayMenuType = 2};
static const char *MenuTypeAsString[] = {
        "ApplicationMenu", "ContextMenu", "TrayMenu",
};

extern void messageFromWindowCallback(const char *);

typedef struct {

    const char *title;

    /*** Internal ***/

    struct hashmap_s menuItemMap;
    struct hashmap_s radioGroupMap;

    // Vector to keep track of callback data memory
    vec_void_t callbackDataCache;

    // The NSMenu for this menu
    id menu;

    // The parent data, eg ContextMenuStore or Tray
    void *parentData;

    // The commands for the menu callbacks
    const char *callbackCommand;

    // This indicates if we are an Application Menu, tray menu or context menu
    enum MenuType menuType;


} Menu;


typedef struct {
    id menuItem;
    Menu *menu;
    const char *menuID;
    enum MenuItemType menuItemType;
} MenuItemCallbackData;



// NewMenu creates a new Menu struct, saving the given menu structure as JSON
Menu* NewMenu(JsonNode *menuData, JsonNode* radioGroups);

Menu* NewApplicationMenu(const char *menuAsJSON);
MenuItemCallbackData* CreateMenuItemCallbackData(Menu *menu, id menuItem, const char *menuID, enum MenuItemType menuItemType);

void DeleteMenu(Menu *menu);

// Creates a JSON message for the given menuItemID and data
const char* createMenuClickedMessage(const char *menuItemID, const char *data);

// Callback for text menu items
void menuItemCallback(id self, SEL cmd, id sender);
id processAcceleratorKey(const char *key);


void addSeparator(id menu);
id createMenuItemNoAutorelease( id title, const char *action, const char *key);

id createMenuItem(id title, const char *action, const char *key);

id addMenuItem(id menu, const char *title, const char *action, const char *key, bool disabled);

id createMenu(id title);
void createDefaultAppMenu(id parentMenu);
void createDefaultEditMenu(id parentMenu);

void processMenuRole(Menu *menu, id parentMenu, JsonNode *item);
// This converts a string array of modifiers into the
// equivalent MacOS Modifier Flags
unsigned long parseModifiers(const char **modifiers);
id processRadioMenuItem(Menu *menu, id parentmenu, const char *title, const char *menuid, bool disabled, bool checked, const char *acceleratorkey, bool hasCallback);

id processCheckboxMenuItem(Menu *menu, id parentmenu, const char *title, const char *menuid, bool disabled, bool checked, const char *key, bool hasCallback);

id processTextMenuItem(Menu *menu, id parentMenu, const char *title, const char *menuid, bool disabled, const char *acceleratorkey, const char **modifiers, bool hasCallback);

void processMenuItem(Menu *menu, id parentMenu, JsonNode *item);
void processMenuData(Menu *menu, JsonNode *menuData);

void processRadioGroupJSON(Menu *menu, JsonNode *radioGroup) ;
id ProcessMenu(Menu *menu, JsonNode *menuData, JsonNode *radioGroup);
#endif //ASSETS_C_MENU_DARWIN_H
