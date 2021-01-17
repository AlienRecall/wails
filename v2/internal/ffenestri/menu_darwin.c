//
// Created by Lea Anthony on 6/1/21.
//

#include "ffenestri_darwin.h"
#include "menu_darwin.h"
#include "contextmenus_darwin.h"

// NewMenu creates a new Menu struct, saving the given menu structure as JSON
Menu* NewMenu(JsonNode *menuData, JsonNode *radioGroups) {

    Menu *result = malloc(sizeof(Menu));

    // No title by default
    result->title = "";

    // Initialise menuCallbackDataCache
    vec_init(&result->callbackDataCache);

    // Allocate MenuItem Map
    if( 0 != hashmap_create((const unsigned)16, &result->menuItemMap)) {
        ABORT("[NewMenu] Not enough memory to allocate menuItemMap!");
    }
    // Allocate the Radio Group Map
    if( 0 != hashmap_create((const unsigned)4, &result->radioGroupMap)) {
        ABORT("[NewMenu] Not enough memory to allocate radioGroupMap!");
    }

    // Init other members
    result->menu = NULL;
    result->parentData = NULL;

    // Process the menu
    ProcessMenu(result, menuData, radioGroups);

    return result;
}

Menu* NewApplicationMenu(const char *menuAsJSON) {

    // Parse the menu json
    JsonNode *processedMenu = json_decode(menuAsJSON);
    if( processedMenu == NULL ) {
        // Parse error!
        ABORT("Unable to parse Menu JSON: %s", menuAsJSON);
    }

    // TODO - fixup
    Menu *result = NewMenu(processedMenu, NULL);
    result->menuType = ApplicationMenuType;
    return result;
}

MenuItemCallbackData* CreateMenuItemCallbackData(Menu *menu, id menuItem, const char *menuID, enum MenuItemType menuItemType) {
    MenuItemCallbackData* result = malloc(sizeof(MenuItemCallbackData));

    result->menu = menu;
    result->menuID = menuID;
    result->menuItem = menuItem;
    result->menuItemType = menuItemType;

    // Store reference to this so we can destroy later
    vec_push(&menu->callbackDataCache, result);

    return result;
}



void DeleteMenu(Menu *menu) {

    if( menu == NULL ) {
        return;
    }

    // Free menu item hashmap
    hashmap_destroy(&menu->menuItemMap);

    // Free radio group members
    if( hashmap_num_entries(&menu->radioGroupMap) > 0 ) {
        if (0 != hashmap_iterate_pairs(&menu->radioGroupMap, freeHashmapItem, NULL)) {
            ABORT("[DeleteMenu] Failed to release radioGroupMap entries!");
        }
    }

    // Free radio groups hashmap
    hashmap_destroy(&menu->radioGroupMap);

    // Release the vector memory
    vec_deinit(&menu->callbackDataCache);

    // Free nsmenu if we have it
    if ( menu->menu != NULL ) {
        msg(menu->menu, s("release"));
    }

    free(menu);
}

// Creates a JSON message for the given menuItemID and data
const char* createMenuClickedMessage(const char *menuItemID, const char *data) {

    JsonNode *jsonObject = json_mkobject();
    if (menuItemID == NULL ) {
        ABORT("Item ID NULL for menu!!\n");
    }
    json_append_member(jsonObject, "i", json_mkstring(menuItemID));
    if (data != NULL) {
        json_append_member(jsonObject, "data", json_mkstring(data));
    }
    const char *payload = json_encode(jsonObject);
    json_delete(jsonObject);
    const char *result = concat("MC", payload);
    MEMFREE(payload);
    return result;
}

// Callback for text menu items
void menuItemCallback(id self, SEL cmd, id sender) {
    MenuItemCallbackData *callbackData = (MenuItemCallbackData *)msg(msg(sender, s("representedObject")), s("pointerValue"));
    const char *message;

    // Update checkbox / radio item
    if( callbackData->menuItemType == Checkbox) {
        // Toggle state
        bool state = msg(callbackData->menuItem, s("state"));
        msg(callbackData->menuItem, s("setState:"), (state? NSControlStateValueOff : NSControlStateValueOn));
    } else if( callbackData->menuItemType == Radio ) {
        // Check the menu items' current state
        bool selected = msg(callbackData->menuItem, s("state"));

        // If it's already selected, exit early
        if (selected) return;

        // Get this item's radio group members and turn them off
        id *members = (id*)hashmap_get(&(callbackData->menu->radioGroupMap), (char*)callbackData->menuID, strlen(callbackData->menuID));

        // Uncheck all members of the group
        id thisMember = members[0];
        int count = 0;
        while(thisMember != NULL) {
            msg(thisMember, s("setState:"), NSControlStateValueOff);
            count = count + 1;
            thisMember = members[count];
        }

        // check the selected menu item
        msg(callbackData->menuItem, s("setState:"), NSControlStateValueOn);
    }

    const char *menuID = callbackData->menuID;
    const char *data = NULL;
    enum MenuType menuType = callbackData->menu->menuType;

    // Generate message to send to backend
    if( menuType == ContextMenuType ) {
        // Get the context menu data from the menu
        ContextMenu* contextMenu = (ContextMenu*) callbackData->menu->parentData;
        data = contextMenu->contextMenuData;
    }

    message = createMenuClickedMessage(menuID, data);

    // Notify the backend
    messageFromWindowCallback(message);
    MEMFREE(message);
}

id processAcceleratorKey(const char *key) {

    // Guard against no accelerator key
    if( key == NULL ) {
        return str("");
    }

    if( STREQ(key, "Backspace") ) {
        return strunicode(0x0008);
    }
    if( STREQ(key, "Tab") ) {
        return strunicode(0x0009);
    }
    if( STREQ(key, "Return") ) {
        return strunicode(0x000d);
    }
    if( STREQ(key, "Escape") ) {
        return strunicode(0x001b);
    }
    if( STREQ(key, "Left") ) {
        return strunicode(0x001c);
    }
    if( STREQ(key, "Right") ) {
        return strunicode(0x001d);
    }
    if( STREQ(key, "Up") ) {
        return strunicode(0x001e);
    }
    if( STREQ(key, "Down") ) {
        return strunicode(0x001f);
    }
    if( STREQ(key, "Space") ) {
        return strunicode(0x0020);
    }
    if( STREQ(key, "Delete") ) {
        return strunicode(0x007f);
    }
    if( STREQ(key, "Home") ) {
        return strunicode(0x2196);
    }
    if( STREQ(key, "End") ) {
        return strunicode(0x2198);
    }
    if( STREQ(key, "Page Up") ) {
        return strunicode(0x21de);
    }
    if( STREQ(key, "Page Down") ) {
        return strunicode(0x21df);
    }
    if( STREQ(key, "F1") ) {
        return strunicode(0xf704);
    }
    if( STREQ(key, "F2") ) {
        return strunicode(0xf705);
    }
    if( STREQ(key, "F3") ) {
        return strunicode(0xf706);
    }
    if( STREQ(key, "F4") ) {
        return strunicode(0xf707);
    }
    if( STREQ(key, "F5") ) {
        return strunicode(0xf708);
    }
    if( STREQ(key, "F6") ) {
        return strunicode(0xf709);
    }
    if( STREQ(key, "F7") ) {
        return strunicode(0xf70a);
    }
    if( STREQ(key, "F8") ) {
        return strunicode(0xf70b);
    }
    if( STREQ(key, "F9") ) {
        return strunicode(0xf70c);
    }
    if( STREQ(key, "F10") ) {
        return strunicode(0xf70d);
    }
    if( STREQ(key, "F11") ) {
        return strunicode(0xf70e);
    }
    if( STREQ(key, "F12") ) {
        return strunicode(0xf70f);
    }
    if( STREQ(key, "F13") ) {
        return strunicode(0xf710);
    }
    if( STREQ(key, "F14") ) {
        return strunicode(0xf711);
    }
    if( STREQ(key, "F15") ) {
        return strunicode(0xf712);
    }
    if( STREQ(key, "F16") ) {
        return strunicode(0xf713);
    }
    if( STREQ(key, "F17") ) {
        return strunicode(0xf714);
    }
    if( STREQ(key, "F18") ) {
        return strunicode(0xf715);
    }
    if( STREQ(key, "F19") ) {
        return strunicode(0xf716);
    }
    if( STREQ(key, "F20") ) {
        return strunicode(0xf717);
    }
    if( STREQ(key, "F21") ) {
        return strunicode(0xf718);
    }
    if( STREQ(key, "F22") ) {
        return strunicode(0xf719);
    }
    if( STREQ(key, "F23") ) {
        return strunicode(0xf71a);
    }
    if( STREQ(key, "F24") ) {
        return strunicode(0xf71b);
    }
    if( STREQ(key, "F25") ) {
        return strunicode(0xf71c);
    }
    if( STREQ(key, "F26") ) {
        return strunicode(0xf71d);
    }
    if( STREQ(key, "F27") ) {
        return strunicode(0xf71e);
    }
    if( STREQ(key, "F28") ) {
        return strunicode(0xf71f);
    }
    if( STREQ(key, "F29") ) {
        return strunicode(0xf720);
    }
    if( STREQ(key, "F30") ) {
        return strunicode(0xf721);
    }
    if( STREQ(key, "F31") ) {
        return strunicode(0xf722);
    }
    if( STREQ(key, "F32") ) {
        return strunicode(0xf723);
    }
    if( STREQ(key, "F33") ) {
        return strunicode(0xf724);
    }
    if( STREQ(key, "F34") ) {
        return strunicode(0xf725);
    }
    if( STREQ(key, "F35") ) {
        return strunicode(0xf726);
    }
//  if( STREQ(key, "Insert") ) {
//	return strunicode(0xf727);
//  }
//  if( STREQ(key, "PrintScreen") ) {
//	return strunicode(0xf72e);
//  }
//  if( STREQ(key, "ScrollLock") ) {
//	return strunicode(0xf72f);
//  }
    if( STREQ(key, "NumLock") ) {
        return strunicode(0xf739);
    }

    return str(key);
}


void addSeparator(id menu) {
    id item = msg(c("NSMenuItem"), s("separatorItem"));
    msg(menu, s("addItem:"), item);
}

id createMenuItemNoAutorelease( id title, const char *action, const char *key) {
    id item = ALLOC("NSMenuItem");
    msg(item, s("initWithTitle:action:keyEquivalent:"), title, s(action), str(key));
    return item;
}

id createMenuItem(id title, const char *action, const char *key) {
    id item = ALLOC("NSMenuItem");
    msg(item, s("initWithTitle:action:keyEquivalent:"), title, s(action), str(key));
    msg(item, s("autorelease"));
    return item;
}

id addMenuItem(id menu, const char *title, const char *action, const char *key, bool disabled) {
    id item = createMenuItem(str(title), action, key);
    msg(item, s("setEnabled:"), !disabled);
    msg(menu, s("addItem:"), item);
    return item;
}

id createMenu(id title) {
    id menu = ALLOC("NSMenu");
    msg(menu, s("initWithTitle:"), title);
    msg(menu, s("setAutoenablesItems:"), NO);
//  msg(menu, s("autorelease"));
    return menu;
}

void createDefaultAppMenu(id parentMenu) {
// App Menu
    id appName = msg(msg(c("NSProcessInfo"), s("processInfo")), s("processName"));
    id appMenuItem = createMenuItemNoAutorelease(appName, NULL, "");
    id appMenu = createMenu(appName);

    msg(appMenuItem, s("setSubmenu:"), appMenu);
    msg(parentMenu, s("addItem:"), appMenuItem);

    id title = msg(str("Hide "), s("stringByAppendingString:"), appName);
    id item = createMenuItem(title, "hide:", "h");
    msg(appMenu, s("addItem:"), item);

    id hideOthers = addMenuItem(appMenu, "Hide Others", "hideOtherApplications:", "h", FALSE);
    msg(hideOthers, s("setKeyEquivalentModifierMask:"), (NSEventModifierFlagOption | NSEventModifierFlagCommand));

    addMenuItem(appMenu, "Show All", "unhideAllApplications:", "", FALSE);

    addSeparator(appMenu);

    title = msg(str("Quit "), s("stringByAppendingString:"), appName);
    item = createMenuItem(title, "terminate:", "q");
    msg(appMenu, s("addItem:"), item);
}

void createDefaultEditMenu(id parentMenu) {
    // Edit Menu
    id editMenuItem = createMenuItemNoAutorelease(str("Edit"), NULL, "");
    id editMenu = createMenu(str("Edit"));

    msg(editMenuItem, s("setSubmenu:"), editMenu);
    msg(parentMenu, s("addItem:"), editMenuItem);

    addMenuItem(editMenu, "Undo", "undo:", "z", FALSE);
    addMenuItem(editMenu, "Redo", "redo:", "y", FALSE);
    addSeparator(editMenu);
    addMenuItem(editMenu, "Cut", "cut:", "x", FALSE);
    addMenuItem(editMenu, "Copy", "copy:", "c", FALSE);
    addMenuItem(editMenu, "Paste", "paste:", "v", FALSE);
    addMenuItem(editMenu, "Select All", "selectAll:", "a", FALSE);
}

void processMenuRole(Menu *menu, id parentMenu, JsonNode *item) {
    const char *roleName = item->string_;

    if ( STREQ(roleName, "appMenu") ) {
        createDefaultAppMenu(parentMenu);
        return;
    }
    if ( STREQ(roleName, "editMenu")) {
        createDefaultEditMenu(parentMenu);
        return;
    }
    if ( STREQ(roleName, "hide")) {
        addMenuItem(parentMenu, "Hide Window", "hide:", "h", FALSE);
        return;
    }
    if ( STREQ(roleName, "hideothers")) {
        id hideOthers = addMenuItem(parentMenu, "Hide Others", "hideOtherApplications:", "h", FALSE);
        msg(hideOthers, s("setKeyEquivalentModifierMask:"), (NSEventModifierFlagOption | NSEventModifierFlagCommand));
        return;
    }
    if ( STREQ(roleName, "unhide")) {
        addMenuItem(parentMenu, "Show All", "unhideAllApplications:", "", FALSE);
        return;
    }
    if ( STREQ(roleName, "front")) {
        addMenuItem(parentMenu, "Bring All to Front", "arrangeInFront:", "", FALSE);
        return;
    }
    if ( STREQ(roleName, "undo")) {
        addMenuItem(parentMenu, "Undo", "undo:", "z", FALSE);
        return;
    }
    if ( STREQ(roleName, "redo")) {
        addMenuItem(parentMenu, "Redo", "redo:", "y", FALSE);
        return;
    }
    if ( STREQ(roleName, "cut")) {
        addMenuItem(parentMenu, "Cut", "cut:", "x", FALSE);
        return;
    }
    if ( STREQ(roleName, "copy")) {
        addMenuItem(parentMenu, "Copy", "copy:", "c", FALSE);
        return;
    }
    if ( STREQ(roleName, "paste")) {
        addMenuItem(parentMenu, "Paste", "paste:", "v", FALSE);
        return;
    }
    if ( STREQ(roleName, "delete")) {
        addMenuItem(parentMenu, "Delete", "delete:", "", FALSE);
        return;
    }
    if( STREQ(roleName, "pasteandmatchstyle")) {
        id pasteandmatchstyle = addMenuItem(parentMenu, "Paste and Match Style", "pasteandmatchstyle:", "v", FALSE);
        msg(pasteandmatchstyle, s("setKeyEquivalentModifierMask:"), (NSEventModifierFlagOption | NSEventModifierFlagShift | NSEventModifierFlagCommand));
    }
    if ( STREQ(roleName, "selectall")) {
        addMenuItem(parentMenu, "Select All", "selectAll:", "a", FALSE);
        return;
    }
    if ( STREQ(roleName, "minimize")) {
        addMenuItem(parentMenu, "Minimize", "miniaturize:", "m", FALSE);
        return;
    }
    if ( STREQ(roleName, "zoom")) {
        addMenuItem(parentMenu, "Zoom", "performZoom:", "", FALSE);
        return;
    }
    if ( STREQ(roleName, "quit")) {
        addMenuItem(parentMenu, "Quit (More work TBD)", "terminate:", "q", FALSE);
        return;
    }
    if ( STREQ(roleName, "togglefullscreen")) {
        addMenuItem(parentMenu, "Toggle Full Screen", "toggleFullScreen:", "f", FALSE);
        return;
    }

}

// This converts a string array of modifiers into the
// equivalent MacOS Modifier Flags
unsigned long parseModifiers(const char **modifiers) {

    // Our result is a modifier flag list
    unsigned long result = 0;

    const char *thisModifier = modifiers[0];
    int count = 0;
    while( thisModifier != NULL ) {
        // Determine flags
        if( STREQ(thisModifier, "CmdOrCtrl") ) {
            result |= NSEventModifierFlagCommand;
        }
        if( STREQ(thisModifier, "OptionOrAlt") ) {
            result |= NSEventModifierFlagOption;
        }
        if( STREQ(thisModifier, "Shift") ) {
            result |= NSEventModifierFlagShift;
        }
        if( STREQ(thisModifier, "Super") ) {
            result |= NSEventModifierFlagCommand;
        }
        if( STREQ(thisModifier, "Control") ) {
            result |= NSEventModifierFlagControl;
        }
        count++;
        thisModifier = modifiers[count];
    }
    return result;
}

id processRadioMenuItem(Menu *menu, id parentmenu, const char *title, const char *menuid, bool disabled, bool checked, const char *acceleratorkey, bool hascallback) {
    id item = ALLOC("NSMenuItem");

    // Store the item in the menu item map
    hashmap_put(&menu->menuItemMap, (char*)menuid, strlen(menuid), item);

    if( hascallback ) {
        // Create a MenuItemCallbackData
        MenuItemCallbackData *callback = CreateMenuItemCallbackData(menu, item, menuid, Radio);

        id wrappedId = msg(c("NSValue"), s("valueWithPointer:"), callback);
        msg(item, s("setRepresentedObject:"), wrappedId);
    }


    id key = processAcceleratorKey(acceleratorkey);

    msg(item, s("initWithTitle:action:keyEquivalent:"), str(title), s("menuItemCallback:"), key);

    msg(item, s("setEnabled:"), !disabled);
    msg(item, s("autorelease"));
    msg(item, s("setState:"), (checked ? NSControlStateValueOn : NSControlStateValueOff));

    msg(parentmenu, s("addItem:"), item);
    return item;

}

id processCheckboxMenuItem(Menu *menu, id parentmenu, const char *title, const char *menuid, bool disabled, bool checked, const char *key, bool hascallback) {

    id item = ALLOC("NSMenuItem");

    // Store the item in the menu item map
    hashmap_put(&menu->menuItemMap, (char*)menuid, strlen(menuid), item);

    if( hascallback ) {
        // Create a MenuItemCallbackData
        MenuItemCallbackData *callback = CreateMenuItemCallbackData(menu, item, menuid, Checkbox);

        id wrappedId = msg(c("NSValue"), s("valueWithPointer:"), callback);
        msg(item, s("setRepresentedObject:"), wrappedId);
    }

    msg(item, s("initWithTitle:action:keyEquivalent:"), str(title), s("menuItemCallback:"), str(key));
    msg(item, s("setEnabled:"), !disabled);
    msg(item, s("autorelease"));
    msg(item, s("setState:"), (checked ? NSControlStateValueOn : NSControlStateValueOff));
    msg(parentmenu, s("addItem:"), item);
    return item;
}

id processTextMenuItem(Menu *menu, id parentMenu, const char *title, const char *menuid, bool disabled, const char *acceleratorkey, const char **modifiers, bool hascallback) {
    id item = ALLOC("NSMenuItem");

    if( hascallback ) {
        // Create a MenuItemCallbackData
        MenuItemCallbackData *callback = CreateMenuItemCallbackData(menu, item, menuid, Text);

        id wrappedId = msg(c("NSValue"), s("valueWithPointer:"), callback);
        msg(item, s("setRepresentedObject:"), wrappedId);
    }

    id key = processAcceleratorKey(acceleratorkey);
    msg(item, s("initWithTitle:action:keyEquivalent:"), str(title),
        s("menuItemCallback:"), key);

    msg(item, s("setEnabled:"), !disabled);
    msg(item, s("autorelease"));

    // Process modifiers
    if( modifiers != NULL ) {
        unsigned long modifierFlags = parseModifiers(modifiers);
        msg(item, s("setKeyEquivalentModifierMask:"), modifierFlags);
    }
    msg(parentMenu, s("addItem:"), item);

    return item;
}

void processMenuItem(Menu *menu, id parentMenu, JsonNode *item) {

    // Check if this item is hidden and if so, exit early!
    bool hidden = getJSONBool(item, "h");
    if( hidden ) {
        return;
    }

    // Get the role
    JsonNode *role = json_find_member(item, "r");
    if( role != NULL ) {
        processMenuRole(menu, parentMenu, role);
        return;
    }

    // Check if this is a submenu
    JsonNode *submenu = json_find_member(item, "S");
    if( submenu != NULL ) {
        // Get the label
        JsonNode *menuNameNode = json_find_member(item, "l");
        const char *name = "";
        if ( menuNameNode != NULL) {
            name = menuNameNode->string_;
        }

        id thisMenuItem = createMenuItemNoAutorelease(str(name), NULL, "");
        id thisMenu = createMenu(str(name));

        msg(thisMenuItem, s("setSubmenu:"), thisMenu);
        msg(parentMenu, s("addItem:"), thisMenuItem);

        JsonNode *submenuItems = json_find_member(submenu, "i");
        // If we have no items, just return
        if ( submenuItems == NULL ) {
            return;
        }

        // Loop over submenu items
        JsonNode *item;
        json_foreach(item, submenuItems) {
            // Get item label
            processMenuItem(menu, thisMenu, item);
        }

        return;
    }

    // This is a user menu. Get the common data
    // Get the label
    const char *label = getJSONString(item, "l");
    if ( label == NULL) {
        label = "(empty)";
    }

    const char *menuid = getJSONString(item, "I");
    if ( menuid == NULL) {
        menuid = "";
    }

    bool disabled = getJSONBool(item, "d");

    // Get the Accelerator
    JsonNode *accelerator = json_find_member(item, "a");
    const char *acceleratorkey = NULL;
    const char **modifiers = NULL;

    // If we have an accelerator
    if( accelerator != NULL ) {
        // Get the key
        acceleratorkey = getJSONString(accelerator, "Key");
        // Check if there are modifiers
        JsonNode *modifiersList = json_find_member(accelerator, "Modifiers");
        if ( modifiersList != NULL ) {
            // Allocate an array of strings
            int noOfModifiers = json_array_length(modifiersList);

            // Do we have any?
            if (noOfModifiers > 0) {
                modifiers = malloc(sizeof(const char *) * (noOfModifiers + 1));
                JsonNode *modifier;
                int count = 0;
                // Iterate the modifiers and save a reference to them in our new array
                json_foreach(modifier, modifiersList) {
                    // Get modifier name
                    modifiers[count] = modifier->string_;
                    count++;
                }
                // Null terminate the modifier list
                modifiers[count] = NULL;
            }
        }
    }

    // has callback?
    bool hascallback = getJSONBool(item, "C");

    // Get the Type
    JsonNode *type = json_find_member(item, "t");
    if( type != NULL ) {

        if( STREQ(type->string_, "t")) {
            processTextMenuItem(menu, parentMenu, label, menuid, disabled, acceleratorkey, modifiers, hascallback);
        }
        else if ( STREQ(type->string_, "s")) {
            addSeparator(parentMenu);
        }
        else if ( STREQ(type->string_, "c")) {
            // Get checked state
            bool checked = getJSONBool(item, "c");

            processCheckboxMenuItem(menu, parentMenu, label, menuid, disabled, checked, "", hascallback);
        }
        else if ( STREQ(type->string_, "r")) {
            // Get checked state
            bool checked = getJSONBool(item, "c");

            processRadioMenuItem(menu, parentMenu, label, menuid, disabled, checked, "", hascallback);
        }

    }

    if ( modifiers != NULL ) {
        free(modifiers);
    }

    return;
}

void processMenuData(Menu *menu, JsonNode *menuData) {
    // Iterate items
    JsonNode *item;
    json_foreach(item, menuData) {
        // Process each menu item
        processMenuItem(menu, menu->menu, item);
    }
}

void processRadioGroupJSON(Menu *menu, JsonNode *radioGroup) {

    int groupLength;
    getJSONInt(radioGroup, "Length", &groupLength);
    JsonNode *members = json_find_member(radioGroup, "Members");
    JsonNode *member;

    // Allocate array
    size_t arrayLength = sizeof(id)*(groupLength+1);
    id memberList[arrayLength];

    // Build the radio group items
    int count=0;
    json_foreach(member, members) {
        // Get menu by id
        id menuItem = (id)hashmap_get(&menu->menuItemMap, (char*)member->string_, strlen(member->string_));
        // Save Member
        memberList[count] = menuItem;
        count = count + 1;
    }
    // Null terminate array
    memberList[groupLength] = 0;

    // Store the members
    json_foreach(member, members) {
        // Copy the memberList
        char *newMemberList = (char *)malloc(arrayLength);
        memcpy(newMemberList, memberList, arrayLength);
        // add group to each member of group
        hashmap_put(&menu->radioGroupMap, member->string_, strlen(member->string_), newMemberList);
    }

}

id ProcessMenu(Menu *menu, JsonNode *menuData, JsonNode *radioGroups) {

    // exit if we have no meny
    if( menuData == NULL ) {
        return NULL;
    }

    menu->menu = createMenu(str(""));

    // Process the menu data
    processMenuData(menu, menuData);

    if( radioGroups != NULL ) {
        // Iterate radio groups
        JsonNode *radioGroup;
        json_foreach(radioGroup, radioGroups) {
            // Get item label
            processRadioGroupJSON(menu, radioGroup);
        }
    }

    return menu->menu;
}

