
#include "ffenestri.h"
#include "ffenestri_darwin.h"
#include "menu.h"
//#include "contextmenus_darwin.h"
//#include "traymenustore_darwin.h"
//#include "traymenu_darwin.h"

// References to assets
#include "assets.h"
extern const unsigned char runtime;

// Dialog icons
extern const unsigned char *defaultDialogIcons[];
#include "userdialogicons.h"
#include "menu_darwin.h"

extern void messageFromWindowCallback(const char *);
typedef void (*ffenestriCallback)(const char *);

// MAIN DEBUG FLAG
int debug;

// A cache for all our dialog icons
struct hashmap_s dialogIconCache;

// Dispatch Method
typedef void (^dispatchMethod)(void);

// dispatch will execute the given `func` pointer
void dispatch(dispatchMethod func) {
	dispatch_async(dispatch_get_main_queue(), func);
}
// yes command simply returns YES!
BOOL yes(id self, SEL cmd)
{
	return YES;
}

// Prints a hashmap entry
int hashmap_log(void *const context, struct hashmap_element_s *const e) {
  printf("%s: %p ", (char*)e->key, e->data);
  return 0;
}

// Utility function to visualise a hashmap
void dumpHashmap(const char *name, struct hashmap_s *hashmap) {
  printf("%s = { ", name);
  if (0!=hashmap_iterate_pairs(hashmap, hashmap_log, NULL)) {
	fprintf(stderr, "Failed to dump hashmap entries\n");
  }
  printf("}\n");
}

void HideMouse() {
	msg(c("NSCursor"), s("hide"));
}

void ShowMouse() {
	msg(c("NSCursor"), s("unhide"));
}

struct Application {

	// Cocoa data
	id application;
	id delegate;
	id mainWindow;
	id wkwebview;
	id manager;
	id config;
	id mouseEvent;
	id mouseDownMonitor;
	id mouseUpMonitor;

	// Window Data
	const char *title;
	int width;
	int height;
	int minWidth;
	int minHeight;
	int maxWidth;
	int maxHeight;
	int resizable;
	int devtools;
	int fullscreen;
	int red;
	int green;
	int blue;
	int alpha;
	int webviewIsTranparent;
	const char *appearance;
	int decorations;
	int logLevel;

	// Features
	int frame;
	int startHidden;
	int maximised;
	int titlebarAppearsTransparent;
	int hideTitle;
	int hideTitleBar;
	int fullSizeContent;
	int useToolBar;
	int hideToolbarSeparator;
	int windowBackgroundIsTranslucent;

//	// Menu
//	Menu *applicationMenu;
//
//	// Tray
//    TrayMenuStore* trayMenuStore;

    MenuManager* menuManager;

	// Context Menus
//	ContextMenuStore *contextMenuStore;

	// Callback
	ffenestriCallback sendMessageToBackend;

	// Bindings
	const char *bindings;

	// Flags
	bool running;

};

// Debug works like sprintf but mutes if the global debug flag is true
// Credit: https://stackoverflow.com/a/20639708

#define MAXMESSAGE 1024*10
char logbuffer[MAXMESSAGE];

void Debug(struct Application *app, const char *message, ... ) {
	if ( debug ) {
		const char *temp = concat("LTFfenestri (C) | ", message);
		va_list args;
		va_start(args, message);
		vsnprintf(logbuffer, MAXMESSAGE, temp, args);
		app->sendMessageToBackend(&logbuffer[0]);
		MEMFREE(temp);
		va_end(args);
	}
}

void Fatal(struct Application *app, const char *message, ... ) {
  const char *temp = concat("LFFfenestri (C) | ", message);
  va_list args;
  va_start(args, message);
  vsnprintf(logbuffer, MAXMESSAGE, temp, args);
  app->sendMessageToBackend(&logbuffer[0]);
  MEMFREE(temp);
  va_end(args);
}

bool isRetina(struct Application *app) {
	CGFloat scale = GET_BACKINGSCALEFACTOR(app->mainWindow);
	if( (int)scale == 1 ) {
		return false;
	}
	return true;
}

void TitlebarAppearsTransparent(struct Application* app) {
	app->titlebarAppearsTransparent = 1;
}

void HideTitle(struct Application *app) {
	app->hideTitle = 1;
}

void HideTitleBar(struct Application *app) {
	app->hideTitleBar = 1;
}

void HideToolbarSeparator(struct Application *app) {
	app->hideToolbarSeparator = 1;
}

void UseToolbar(struct Application *app) {
	app->useToolBar = 1;
}

// WebviewIsTransparent will make the webview transparent
// revealing the Cocoa window underneath
void WebviewIsTransparent(struct Application *app) {
	app->webviewIsTranparent = 1;
}

// SetAppearance will set the window's Appearance to the
// given value
void SetAppearance(struct Application *app, const char *appearance) {
	app->appearance = appearance;
}


void applyWindowColour(struct Application *app) {
	// Apply the colour only if the window has been created
	if( app->mainWindow != NULL ) {
		ON_MAIN_THREAD(
			id colour = msg(c("NSColor"), s("colorWithCalibratedRed:green:blue:alpha:"),
								(float)app->red / 255.0,
								(float)app->green / 255.0,
								(float)app->blue / 255.0,
								(float)app->alpha / 255.0);
			msg(app->mainWindow, s("setBackgroundColor:"), colour);
		);
	}
}

void SetColour(struct Application *app, int red, int green, int blue, int alpha) {
	app->red = red;
	app->green = green;
	app->blue = blue;
	app->alpha = alpha;

	applyWindowColour(app);
}

void FullSizeContent(struct Application *app) {
	app->fullSizeContent = 1;
}

void Hide(struct Application *app) {
	ON_MAIN_THREAD(
		msg(app->application, s("hide:"))
	);
}

void Show(struct Application *app) {
	ON_MAIN_THREAD(
		msg(app->mainWindow, s("makeKeyAndOrderFront:"), NULL);
		msg(app->application, s("activateIgnoringOtherApps:"), YES);
	);
}

void WindowBackgroundIsTranslucent(struct Application *app) {
	app->windowBackgroundIsTranslucent = 1;
}

// Sends messages to the backend
void messageHandler(id self, SEL cmd, id contentController, id message) {
	struct Application *app = (struct Application *)objc_getAssociatedObject(
							  self, "application");
	const char *name = (const char *)msg(msg(message, s("name")), s("UTF8String"));
	if( strcmp(name, "completed") == 0) {
		// Delete handler
		msg(app->manager, s("removeScriptMessageHandlerForName:"), str("completed"));

		// TODO: Notify backend we're ready and get them to call back for the Show()
		if (app->startHidden == 0) {
			Show(app);
		}

        // Setup initial trays
        ShowTrayMenus(app->menuManager);

		// TODO: Check this actually does reduce flicker
		msg(app->config, s("setValue:forKey:"), msg(c("NSNumber"), s("numberWithBool:"), 0), str("suppressesIncrementalRendering"));

		// Notify backend we are ready (system startup)
        app->sendMessageToBackend("SS");

	} else if( strcmp(name, "windowDrag") == 0 ) {
		// Guard against null events
		if( app->mouseEvent != NULL ) {
			HideMouse();
			ON_MAIN_THREAD(
				msg(app->mainWindow, s("performWindowDragWithEvent:"), app->mouseEvent);
			);
		}
	} else if( strcmp(name, "contextMenu") == 0 ) {

		// Did we get a context menu selector?
		if( message == NULL) {
			return;
		}

		const char *contextMenuMessage = cstr(msg(message, s("body")));

		if( contextMenuMessage == NULL ) {
			Debug(app, "EMPTY CONTEXT MENU MESSAGE!!\n");
			return;
		}

		// Parse the message
		JsonNode *contextMenuMessageJSON = json_decode(contextMenuMessage);
		if( contextMenuMessageJSON == NULL ) {
			Debug(app, "Error decoding context menu message: %s", contextMenuMessage);
			return;
		}

		// Get menu ID
		JsonNode *contextMenuIDNode = json_find_member(contextMenuMessageJSON, "id");
		if( contextMenuIDNode == NULL ) {
			Debug(app, "Error decoding context menu ID: %s", contextMenuMessage);
			json_delete(contextMenuMessageJSON);
			return;
		}
		if( contextMenuIDNode->tag != JSON_STRING ) {
			Debug(app, "Error decoding context menu ID (Not a string): %s", contextMenuMessage);
			json_delete(contextMenuMessageJSON);
			return;
		}

		// Get menu Data
		JsonNode *contextMenuDataNode = json_find_member(contextMenuMessageJSON, "data");
		if( contextMenuDataNode == NULL ) {
			Debug(app, "Error decoding context menu data: %s", contextMenuMessage);
			json_delete(contextMenuMessageJSON);
			return;
		}
		if( contextMenuDataNode->tag != JSON_STRING ) {
			Debug(app, "Error decoding context menu data (Not a string): %s", contextMenuMessage);
			json_delete(contextMenuMessageJSON);
			return;
		}

		// We need to copy these as the JSON node will be destroyed on this thread and the
		// string data will become corrupt. These need to be freed by the context menu code.
		const char* contextMenuID = STRCOPY(contextMenuIDNode->string_);
		const char* contextMenuData = STRCOPY(contextMenuDataNode->string_);

		ON_MAIN_THREAD(
		        //TODO: FIX UP
//			ShowContextMenu(app->contextMenuStore, app->mainWindow, contextMenuID, contextMenuData);
		);

		json_delete(contextMenuMessageJSON);

	} else {
		// const char *m = (const char *)msg(msg(message, s("body")), s("UTF8String"));
		const char *m = cstr(msg(message, s("body")));
		app->sendMessageToBackend(m);
	}
}

// closeWindow is called when the close button is pressed
void closeWindow(id self, SEL cmd, id sender) {
	struct Application *app = (struct Application *) objc_getAssociatedObject(self, "application");
	app->sendMessageToBackend("WC");
}

bool isDarkMode(struct Application *app) {
	id userDefaults = msg(c("NSUserDefaults"), s("standardUserDefaults"));
	const char *mode = cstr(msg(userDefaults,  s("stringForKey:"), str("AppleInterfaceStyle")));
	return ( mode != NULL && strcmp(mode, "Dark") == 0 );
}

void ExecJS(struct Application *app, const char *js) {
	ON_MAIN_THREAD(
		msg(app->wkwebview,
			s("evaluateJavaScript:completionHandler:"),
			str(js),
			NULL);
	);
}

void willFinishLaunching(id self, SEL cmd, id sender) {
	struct Application *app = (struct Application *) objc_getAssociatedObject(self, "application");
	messageFromWindowCallback("Ej{\"name\":\"wails:launched\",\"data\":[]}");
}

void emitThemeChange(struct Application *app) {
	bool currentThemeIsDark = isDarkMode(app);
	if (currentThemeIsDark) {
		messageFromWindowCallback("Ej{\"name\":\"wails:system:themechange\",\"data\":[true]}");
	} else {
		messageFromWindowCallback("Ej{\"name\":\"wails:system:themechange\",\"data\":[false]}");
	}
}

void themeChanged(id self, SEL cmd, id sender) {
	struct Application *app = (struct Application *)objc_getAssociatedObject(
							  self, "application");
//    emitThemeChange(app);
    bool currentThemeIsDark = isDarkMode(app);
	if ( currentThemeIsDark ) {
		ExecJS(app, "window.wails.Events.Emit( 'wails:system:themechange', true );");
	} else {
		ExecJS(app, "window.wails.Events.Emit( 'wails:system:themechange', false );");
	}
}

// void willFinishLaunching(id self) {
//     struct Application *app = (struct Application *) objc_getAssociatedObject(self, "application");
//     Debug(app, "willFinishLaunching called!");
// }


int releaseNSObject(void *const context, struct hashmap_element_s *const e) {
    msg(e->data, s("release"));
    return -1;
}

void destroyContextMenus(struct Application *app) {
//    DeleteContextMenuStore(app->contextMenuStore);
}

void freeDialogIconCache(struct Application *app) {
	// Release the dialog cache images
    if( hashmap_num_entries(&dialogIconCache) > 0 ) {
        if (0!=hashmap_iterate_pairs(&dialogIconCache, releaseNSObject, NULL)) {
            Fatal(app, "failed to release hashmap entries!");
        }
    }

    //Free radio groups hashmap
    hashmap_destroy(&dialogIconCache);
}

void DestroyApplication(struct Application *app) {
	Debug(app, "Destroying Application");

	// Free the bindings
	if (app->bindings != NULL) {
		MEMFREE(app->bindings);
	} else {
		Debug(app, "Almost a double free for app->bindings");
	}

	// Remove mouse monitors
	if( app->mouseDownMonitor != NULL ) {
		msg( c("NSEvent"), s("removeMonitor:"), app->mouseDownMonitor);
	}
	if( app->mouseUpMonitor != NULL ) {
		msg( c("NSEvent"), s("removeMonitor:"), app->mouseUpMonitor);
	}

    // Delete the tray menu store
    DeleteMenuManager(app->menuManager);

	// Free dialog icon cache
	freeDialogIconCache(app);

    // Unload the tray Icons
    UnloadTrayIcons();

	// Remove script handlers
	msg(app->manager, s("removeScriptMessageHandlerForName:"), str("contextMenu"));
	msg(app->manager, s("removeScriptMessageHandlerForName:"), str("windowDrag"));
	msg(app->manager, s("removeScriptMessageHandlerForName:"), str("external"));

	// Close main window
	msg(app->mainWindow, s("close"));

	// Terminate app
	msg(c("NSApp"), s("terminate:"), NULL);
	Debug(app, "Finished Destroying Application");
}

// Quit will stop the cocoa application and free up all the memory
// used by the application
void Quit(struct Application *app) {
	Debug(app, "Quit Called");
	DestroyApplication(app);
}

// SetTitle sets the main window title to the given string
void SetTitle(struct Application *app, const char *title) {
	Debug(app, "SetTitle Called");
	ON_MAIN_THREAD(
		msg(app->mainWindow, s("setTitle:"), str(title));
	);
}

void ToggleFullscreen(struct Application *app) {
	ON_MAIN_THREAD(
		app->fullscreen = !app->fullscreen;
		MAIN_WINDOW_CALL("toggleFullScreen:");
	);
}

bool isFullScreen(struct Application *app) {
	int mask = (int)msg(app->mainWindow, s("styleMask"));
	bool result = (mask & NSWindowStyleMaskFullscreen) == NSWindowStyleMaskFullscreen;
	return result;
}

// Fullscreen sets the main window to be fullscreen
void Fullscreen(struct Application *app) {
	Debug(app, "Fullscreen Called");
	if( ! isFullScreen(app) ) {
		ToggleFullscreen(app);
	}
}

// UnFullscreen resets the main window after a fullscreen
void UnFullscreen(struct Application *app) {
	Debug(app, "UnFullscreen Called");
	if( isFullScreen(app) ) {
		ToggleFullscreen(app);
	}
}

void Center(struct Application *app) {
	Debug(app, "Center Called");
	ON_MAIN_THREAD(
		MAIN_WINDOW_CALL("center");
	);
}

void ToggleMaximise(struct Application *app) {
	ON_MAIN_THREAD(
		app->maximised = !app->maximised;
		MAIN_WINDOW_CALL("zoom:");
	);
}

void Maximise(struct Application *app) {
	if( app->maximised == 0) {
		ToggleMaximise(app);
	}
}

void Unmaximise(struct Application *app) {
	if( app->maximised == 1) {
		ToggleMaximise(app);
	}
}

void Minimise(struct Application *app) {
	ON_MAIN_THREAD(
		MAIN_WINDOW_CALL("miniaturize:");
	);
 }
void Unminimise(struct Application *app) {
	ON_MAIN_THREAD(
		MAIN_WINDOW_CALL("deminiaturize:");
	);
}

id getCurrentScreen(struct Application *app) {
	id screen = NULL;
	screen = msg(app->mainWindow, s("screen"));
	if( screen == NULL ) {
		screen = msg(c("NSScreen"), u("mainScreen"));
	}
	return screen;
}

void dumpFrame(struct Application *app, const char *message, CGRect frame) {
	Debug(app, message);
	Debug(app, "origin.x %f", frame.origin.x);
	Debug(app, "origin.y %f", frame.origin.y);
	Debug(app, "size.width %f", frame.size.width);
	Debug(app, "size.height %f", frame.size.height);
}

void SetSize(struct Application *app, int width, int height) {
	ON_MAIN_THREAD(
		id screen = getCurrentScreen(app);

		// Get the rect for the window
		CGRect frame = GET_FRAME(app->mainWindow);

		// Credit: https://github.com/patr0nus/DeskGap/blob/73c0ac9f2c73f55b6e81f64f6673a7962b5719cd/lib/src/platform/mac/util/NSScreen%2BGeometry.m
		frame.origin.y = (frame.origin.y + frame.size.height) - (float)height;
		frame.size.width = (float)width;
		frame.size.height = (float)height;

		msg(app->mainWindow, s("setFrame:display:animate:"), frame, 1, 0);
	);
}

void SetPosition(struct Application *app, int x, int y) {
	ON_MAIN_THREAD(
		id screen = getCurrentScreen(app);
		CGRect screenFrame = GET_FRAME(screen);
		CGRect windowFrame = GET_FRAME(app->mainWindow);

		windowFrame.origin.x = screenFrame.origin.x + (float)x;
		windowFrame.origin.y = (screenFrame.origin.y + screenFrame.size.height) - windowFrame.size.height - (float)y;
		msg(app->mainWindow, s("setFrame:display:animate:"), windowFrame, 1, 0);
	);
}

void processDialogButton(id alert, const char *buttonTitle, const char *cancelButton, const char *defaultButton) {
	// If this button is set
	if( STR_HAS_CHARS(buttonTitle) ) {
        id button = msg(alert, s("addButtonWithTitle:"), str(buttonTitle));
        if ( STREQ( buttonTitle, defaultButton) ) {
            msg(button, s("setKeyEquivalent:"), str("\r"));
        }
        if ( STREQ( buttonTitle, cancelButton) ) {
            msg(button, s("setKeyEquivalent:"), str("\033"));
        }
    }
}

extern void MessageDialog(struct Application *app, const char *callbackID, const char *type, const char *title, const char *message, const char *icon, const char *button1, const char *button2, const char *button3, const char *button4, const char *defaultButton, const char *cancelButton) {
	ON_MAIN_THREAD(
	    id alert = ALLOC_INIT("NSAlert");
	    const char *dialogType = type;
	    const char *dialogIcon = type;

	    // Default to info type
	    if( dialogType == NULL ) {
	        dialogType = "info";
	    }

	    // Set the dialog style
	    if( STREQ(dialogType, "info") || STREQ(dialogType, "question") ) {
	        msg(alert, s("setAlertStyle:"), NSAlertStyleInformational);
	    } else if( STREQ(dialogType, "warning") ) {
            msg(alert, s("setAlertStyle:"), NSAlertStyleWarning);
        } else if( STREQ(dialogType, "error") ) {
            msg(alert, s("setAlertStyle:"), NSAlertStyleCritical);
        }

		// Set title if given
		if( strlen(title) > 0 ) {
		    msg(alert, s("setMessageText:"), str(title));
		}

		// Set message if given
		if( strlen(message) > 0) {
		    msg(alert, s("setInformativeText:"), str(message));
		}

		// Process buttons
		processDialogButton(alert, button1, cancelButton, defaultButton);
		processDialogButton(alert, button2, cancelButton, defaultButton);
		processDialogButton(alert, button3, cancelButton, defaultButton);
		processDialogButton(alert, button4, cancelButton, defaultButton);

	    // Check for custom dialog icon
	    if( strlen(icon) > 0 ) {
	        dialogIcon = icon;
	    }

	    // TODO: move dialog icons + methods to own file

	    // Determine what dialog icon we are looking for
	    id dialogImage = NULL;
	    // Look for `name-theme2x` first
	    char *themeIcon = concat(dialogIcon, (isDarkMode(app) ? "-dark" : "-light") );
	    if( isRetina(app) ) {
	        char *dialogIcon2x = concat(themeIcon, "2x");
	        dialogImage = hashmap_get(&dialogIconCache, dialogIcon2x, strlen(dialogIcon2x));
//	        if (dialogImage != NULL ) printf("Using %s\n", dialogIcon2x);
	        MEMFREE(dialogIcon2x);

			// Now look for non-themed icon `name2x`
			if ( dialogImage == NULL ) {
	            dialogIcon2x = concat(dialogIcon, "2x");
	            dialogImage = hashmap_get(&dialogIconCache, dialogIcon2x, strlen(dialogIcon2x));
//		        if (dialogImage != NULL ) printf("Using %s\n", dialogIcon2x);
	            MEMFREE(dialogIcon2x);
            }
	    }

	    // If we don't have a retina icon, try the 1x name-theme icon
	    if( dialogImage == NULL ) {
	        dialogImage = hashmap_get(&dialogIconCache, themeIcon, strlen(themeIcon));
//            if (dialogImage != NULL ) printf("Using %s\n", themeIcon);
	    }

	    // Free the theme icon memory
	    MEMFREE(themeIcon);

	    // Finally try the name itself
	    if( dialogImage == NULL ) {
	        dialogImage = hashmap_get(&dialogIconCache, dialogIcon, strlen(dialogIcon));
//            if (dialogImage != NULL ) printf("Using %s\n", dialogIcon);
	    }

	    if (dialogImage != NULL ) {
	        msg(alert, s("setIcon:"), dialogImage);
	    }

		// Run modal
		const char *buttonPressed;
	    int response = (int)msg(alert, s("runModal"));
	    if( response == NSAlertFirstButtonReturn ) {
	        buttonPressed = button1;
	    }
	    else if( response == NSAlertSecondButtonReturn ) {
	        buttonPressed = button2;
	    }
	    else if( response == NSAlertThirdButtonReturn ) {
	        buttonPressed = button3;
	    }
	    else {
	        buttonPressed = button4;
	    }

        // Construct callback message. Format "DS<callbackID>|<selected button index>"
        const char *callback = concat("DM", callbackID);
        const char *header = concat(callback, "|");
        const char *responseMessage = concat(header, buttonPressed);

        // Send message to backend
        app->sendMessageToBackend(responseMessage);

        // Free memory
        MEMFREE(header);
        MEMFREE(callback);
        MEMFREE(responseMessage);
    );
}

// OpenDialog opens a dialog to select files/directories
void OpenDialog(struct Application *app, const char *callbackID, const char *title, const char *filters, const char *defaultFilename, const char *defaultDir, int allowFiles, int allowDirs, int allowMultiple, int showHiddenFiles, int canCreateDirectories, int resolvesAliases, int treatPackagesAsDirectories) {
	Debug(app, "OpenDialog Called with callback id: %s", callbackID);

	// Create an open panel
	ON_MAIN_THREAD(

		// Create the dialog
		id dialog = msg(c("NSOpenPanel"), s("openPanel"));

		// Valid but appears to do nothing.... :/
		msg(dialog, s("setTitle:"), str(title));

		// Filters
		if( filters != NULL && strlen(filters) > 0) {
			id filterString = msg(str(filters), s("stringByReplacingOccurrencesOfString:withString:"), str("*."), str(""));
			filterString = msg(filterString, s("stringByReplacingOccurrencesOfString:withString:"), str(" "), str(""));
			id filterList = msg(filterString, s("componentsSeparatedByString:"), str(","));
			msg(dialog, s("setAllowedFileTypes:"), filterList);
		} else {
			msg(dialog, s("setAllowsOtherFileTypes:"), YES);
		}

		// Default Directory
		if( defaultDir != NULL && strlen(defaultDir) > 0 ) {
			msg(dialog, s("setDirectoryURL:"), url(defaultDir));
		}

		// Default Filename
		if( defaultFilename != NULL && strlen(defaultFilename) > 0 ) {
			msg(dialog, s("setNameFieldStringValue:"), str(defaultFilename));
		}

		// Setup Options
		msg(dialog, s("setCanChooseFiles:"), allowFiles);
		msg(dialog, s("setCanChooseDirectories:"), allowDirs);
		msg(dialog, s("setAllowsMultipleSelection:"), allowMultiple);
		msg(dialog, s("setShowsHiddenFiles:"), showHiddenFiles);
		msg(dialog, s("setCanCreateDirectories:"), canCreateDirectories);
		msg(dialog, s("setResolvesAliases:"), resolvesAliases);
		msg(dialog, s("setTreatsFilePackagesAsDirectories:"), treatPackagesAsDirectories);

		// Setup callback handler
		msg(dialog, s("beginSheetModalForWindow:completionHandler:"), app->mainWindow, ^(id result) {

			// Create the response JSON object
			JsonNode *response = json_mkarray();

			// If the user selected some files
			if( result == (id)1 ) {
				// Grab the URLs returned
				id urls = msg(dialog, s("URLs"));

				// Iterate over all the selected files
				long noOfResults = (long)msg(urls, s("count"));
				for( int index = 0; index < noOfResults; index++ ) {

					// Extract the filename
					id url = msg(urls, s("objectAtIndex:"), index);
					const char *filename = (const char *)msg(msg(url, s("path")), s("UTF8String"));

					// Add the the response array
					json_append_element(response, json_mkstring(filename));
				}
			}

			// Create JSON string and free json memory
			char *encoded = json_stringify(response, "");
			json_delete(response);

			// Construct callback message. Format "D<callbackID>|<json array of strings>"
			const char *callback = concat("DO", callbackID);
			const char *header = concat(callback, "|");
			const char *responseMessage = concat(header, encoded);

			// Send message to backend
			app->sendMessageToBackend(responseMessage);

			// Free memory
			MEMFREE(header);
			MEMFREE(callback);
			MEMFREE(responseMessage);
		});

		msg( c("NSApp"), s("runModalForWindow:"), app->mainWindow);
	);
}

// SaveDialog opens a dialog to select files/directories
void SaveDialog(struct Application *app, const char *callbackID, const char *title, const char *filters, const char *defaultFilename, const char *defaultDir, int showHiddenFiles, int canCreateDirectories, int treatPackagesAsDirectories) {
	Debug(app, "SaveDialog Called with callback id: %s", callbackID);

	// Create an open panel
	ON_MAIN_THREAD(

		// Create the dialog
		id dialog = msg(c("NSSavePanel"), s("savePanel"));

		// Valid but appears to do nothing.... :/
		msg(dialog, s("setTitle:"), str(title));

		// Filters
		if( filters != NULL && strlen(filters) > 0) {
			id filterString = msg(str(filters), s("stringByReplacingOccurrencesOfString:withString:"), str("*."), str(""));
			filterString = msg(filterString, s("stringByReplacingOccurrencesOfString:withString:"), str(" "), str(""));
			id filterList = msg(filterString, s("componentsSeparatedByString:"), str(","));
			msg(dialog, s("setAllowedFileTypes:"), filterList);
		} else {
			msg(dialog, s("setAllowsOtherFileTypes:"), YES);
		}

		// Default Directory
		if( defaultDir != NULL && strlen(defaultDir) > 0 ) {
			msg(dialog, s("setDirectoryURL:"), url(defaultDir));
		}

		// Default Filename
		if( defaultFilename != NULL && strlen(defaultFilename) > 0 ) {
			msg(dialog, s("setNameFieldStringValue:"), str(defaultFilename));
		}

		// Setup Options
		msg(dialog, s("setShowsHiddenFiles:"), showHiddenFiles);
		msg(dialog, s("setCanCreateDirectories:"), canCreateDirectories);
		msg(dialog, s("setTreatsFilePackagesAsDirectories:"), treatPackagesAsDirectories);

		// Setup callback handler
		msg(dialog, s("beginSheetModalForWindow:completionHandler:"), app->mainWindow, ^(id result) {

			// Default is blank
			const char *filename = "";

			// If the user selected some files
			if( result == (id)1 ) {
				// Grab the URL returned
				id url = msg(dialog, s("URL"));
				filename = (const char *)msg(msg(url, s("path")), s("UTF8String"));
			}

			// Construct callback message. Format "DS<callbackID>|<json array of strings>"
			const char *callback = concat("DS", callbackID);
			const char *header = concat(callback, "|");
			const char *responseMessage = concat(header, filename);

			// Send message to backend
			app->sendMessageToBackend(responseMessage);

			// Free memory
			MEMFREE(header);
			MEMFREE(callback);
			MEMFREE(responseMessage);
		});

		msg( c("NSApp"), s("runModalForWindow:"), app->mainWindow);
	);
}

const char *invoke = "window.external={invoke:function(x){window.webkit.messageHandlers.external.postMessage(x);}};";

// DisableFrame disables the window frame
void DisableFrame(struct Application *app)
{
   app->frame = 0;
}

void setMinMaxSize(struct Application *app)
{
	if (app->maxHeight > 0 && app->maxWidth > 0)
	{
		msg(app->mainWindow, s("setMaxSize:"), CGSizeMake(app->maxWidth, app->maxHeight));
	}
	if (app->minHeight > 0 && app->minWidth > 0)
	{
		msg(app->mainWindow, s("setMinSize:"), CGSizeMake(app->minWidth, app->minHeight));
	}
}

void SetMinWindowSize(struct Application *app, int minWidth, int minHeight)
{
	app->minWidth = minWidth;
	app->minHeight = minHeight;

	// Apply if the window is created
	if( app->mainWindow != NULL ) {
		ON_MAIN_THREAD(
			setMinMaxSize(app);
		);
	}
}

void SetMaxWindowSize(struct Application *app, int maxWidth, int maxHeight)
{
	app->maxWidth = maxWidth;
	app->maxHeight = maxHeight;

	// Apply if the window is created
	if( app->mainWindow != NULL ) {
		ON_MAIN_THREAD(
			setMinMaxSize(app);
		);
	}
}


void SetDebug(struct Application *applicationPointer, int flag) {
	debug = flag;
}



// AddContextMenu sets the context menu map for this application
void AddContextMenu(struct Application *app, const char *contextMenuJSON) {
//	AddContextMenuToStore(app->contextMenuStore, contextMenuJSON);
}

void UpdateContextMenu(struct Application *app, const char* contextMenuJSON) {
//    UpdateContextMenuInStore(app->contextMenuStore, contextMenuJSON);
}

void AddTrayMenu(struct Application *app, const char *trayMenuJSON) {
	AddTrayMenuToManager(app->menuManager, trayMenuJSON);
}

void SetTrayMenu(struct Application *app, const char* trayMenuJSON) {
    TrayMenu *menu = AddTrayMenuToManager(app->menuManager, trayMenuJSON);
    if( app->running ) {
        ON_MAIN_THREAD(
                ShowTrayMenu(menu)
        );
    }
}

//void UpdateTrayMenuLabel(struct Application* app, const char* JSON) {
//    ON_MAIN_THREAD(
//        UpdateTrayMenuLabelInStore(app->trayMenuStore, JSON);
//    );
//}


void SetBindings(struct Application *app, const char *bindings) {
	const char* temp = concat("window.wailsbindings = \"", bindings);
	const char* jscall = concat(temp, "\";");
	MEMFREE(temp);
	app->bindings = jscall;
}

void makeWindowBackgroundTranslucent(struct Application *app) {
	id contentView = msg(app->mainWindow, s("contentView"));
	id effectView = msg(c("NSVisualEffectView"), s("alloc"));
	CGRect bounds = GET_BOUNDS(contentView);
	effectView = msg(effectView, s("initWithFrame:"), bounds);

	msg(effectView, s("setAutoresizingMask:"), NSViewWidthSizable | NSViewHeightSizable);
	msg(effectView, s("setBlendingMode:"), NSVisualEffectBlendingModeBehindWindow);
	msg(effectView, s("setState:"), NSVisualEffectStateActive);
	msg(contentView, s("addSubview:positioned:relativeTo:"), effectView, NSWindowBelow, NULL);
}

void enableBoolConfig(id config, const char *setting) {
	msg(msg(config, s("preferences")), s("setValue:forKey:"), msg(c("NSNumber"), s("numberWithBool:"), 1), str(setting));
}

void disableBoolConfig(id config, const char *setting) {
	msg(msg(config, s("preferences")), s("setValue:forKey:"), msg(c("NSNumber"), s("numberWithBool:"), 0), str(setting));
}

void processDecorations(struct Application *app) {

	int decorations = 0;

	if (app->frame == 1 ) {
		if( app->hideTitleBar == 0) {
			decorations |= NSWindowStyleMaskTitled;
		}
		decorations |= NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
	}

	if (app->resizable) {
		decorations |= NSWindowStyleMaskResizable;
	}

	if (app->fullscreen) {
		decorations |= NSWindowStyleMaskFullscreen;
	}

	if( app->fullSizeContent || app->frame == 0) {
		decorations |= NSWindowStyleMaskFullSizeContentView;
	}

	app->decorations = decorations;
}

void createApplication(struct Application *app) {
	id application = msg(c("NSApplication"), s("sharedApplication"));
	app->application = application;
	msg(application, s("setActivationPolicy:"), 0);
}

void DarkModeEnabled(struct Application *app, const char *callbackID) {
	ON_MAIN_THREAD(
		const char *result = isDarkMode(app) ? "T" : "F";

		// Construct callback message. Format "SD<callbackID>|<json array of strings>"
		const char *callback = concat("SD", callbackID);
		const char *header = concat(callback, "|");
		const char *responseMessage = concat(header, result);
		// Send message to backend
		app->sendMessageToBackend(responseMessage);

		// Free memory
		MEMFREE(header);
		MEMFREE(callback);
		MEMFREE(responseMessage);
	);
}

void createDelegate(struct Application *app) {
		// Define delegate
	Class delegateClass = objc_allocateClassPair((Class) c("NSObject"), "AppDelegate", 0);
	  bool resultAddProtoc = class_addProtocol(delegateClass, objc_getProtocol("NSApplicationDelegate"));
	class_addMethod(delegateClass, s("applicationShouldTerminateAfterLastWindowClosed:"), (IMP) yes, "c@:@");
	class_addMethod(delegateClass, s("applicationWillTerminate:"), (IMP) closeWindow, "v@:@");
	class_addMethod(delegateClass, s("applicationWillFinishLaunching:"), (IMP) willFinishLaunching, "v@:@");

	// All Menu Items use a common callback
    class_addMethod(delegateClass, s("menuItemCallback:"), (IMP)PlatformMenuItemCallback, "v@:@");

	// Script handler
	class_addMethod(delegateClass, s("userContentController:didReceiveScriptMessage:"), (IMP) messageHandler, "v@:@@");
	objc_registerClassPair(delegateClass);

	// Create delegate
	id delegate = msg((id)delegateClass, s("new"));
	objc_setAssociatedObject(delegate, "application", (id)app, OBJC_ASSOCIATION_ASSIGN);

	// Theme change listener
	class_addMethod(delegateClass, s("themeChanged:"), (IMP) themeChanged, "v@:@@");

	// Get defaultCenter
	id defaultCenter = msg(c("NSDistributedNotificationCenter"), s("defaultCenter"));
	msg(defaultCenter, s("addObserver:selector:name:object:"), delegate, s("themeChanged:"), str("AppleInterfaceThemeChangedNotification"), NULL);

	app->delegate = delegate;

	msg(app->application, s("setDelegate:"), delegate);
}

void createMainWindow(struct Application *app) {
	// Create main window
	id mainWindow = ALLOC("NSWindow");
	mainWindow = msg(mainWindow, s("initWithContentRect:styleMask:backing:defer:"),
		  CGRectMake(0, 0, app->width, app->height), app->decorations, NSBackingStoreBuffered, NO);
	msg(mainWindow, s("autorelease"));

	// Set Appearance
	if( app->appearance != NULL ) {
		msg(mainWindow, s("setAppearance:"),
			msg(c("NSAppearance"), s("appearanceNamed:"), str(app->appearance))
		);
	}

	// Set Title appearance
	msg(mainWindow, s("setTitlebarAppearsTransparent:"), app->titlebarAppearsTransparent ? YES : NO);
	msg(mainWindow, s("setTitleVisibility:"), app->hideTitle);

	app->mainWindow = mainWindow;
}

const char* getInitialState(struct Application *app) {
	const char *result = "";
	if( isDarkMode(app) ) {
		result = "window.wails.System.IsDarkMode.set(true);";
	} else {
		result = "window.wails.System.IsDarkMode.set(false);";
	}
	char buffer[999];
	snprintf(&buffer[0], sizeof(buffer), "window.wails.System.LogLevel.set(%d);", app->logLevel);
	result = concat(result, &buffer[0]);
	Debug(app, "initialstate = %s", result);
	return result;
}

//void dumpMemberList(const char *name, id *memberList) {
//  void *member = memberList[0];
//  int count = 0;
//  printf("%s = %p -> [ ", name, memberList);
//  while( member != NULL ) {
//	printf("%p ", member);
//	count = count + 1;
//	member = memberList[count];
//  }
//  printf("]\n");
//}

// SetApplicationMenu sets the initial menu for the application
//void SetApplicationMenu(struct Application *app, const char *menuAsJSON) {
//    if ( app->applicationMenu == NULL ) {
//	    app->applicationMenu = NewApplicationMenu(menuAsJSON, (struct TrayMenuStore *) app->trayMenuStore);
//	    return;
//	}
//
//    // Update menu
//    updateMenu(app, menuAsJSON);
//}

//void processDialogIcons(struct hashmap_s *hashmap, const unsigned char *dialogIcons[]) {
//
//	unsigned int count = 0;
//    while( 1 ) {
//        const unsigned char *name = dialogIcons[count++];
//        if( name == 0x00 ) {
//            break;
//        }
//        const unsigned char *lengthAsString = dialogIcons[count++];
//        if( name == 0x00 ) {
//            break;
//        }
//        const unsigned char *data = dialogIcons[count++];
//        if( data == 0x00 ) {
//            break;
//        }
//        int length = atoi((const char *)lengthAsString);
//
//        // Create the icon and add to the hashmap
//        id imageData = msg(c("NSData"), s("dataWithBytes:length:"), data, length);
//        id dialogImage = ALLOC("NSImage");
//        msg(dialogImage, s("initWithData:"), imageData);
//        hashmap_put(hashmap, (const char *)name, strlen((const char *)name), dialogImage);
//    }
//
//}
//
//void processUserDialogIcons(struct Application *app) {
//
//	// Allocate the Dialog icon hashmap
//	if( 0 != hashmap_create((const unsigned)4, &dialogIconCache)) {
//	   // Couldn't allocate map
//	   Fatal(app, "Not enough memory to allocate dialogIconCache!");
//	   return;
//	}
//
//	processDialogIcons(&dialogIconCache, defaultDialogIcons);
//	processDialogIcons(&dialogIconCache, userDialogIcons);
//
//}


void Run(struct Application *app, int argc, char **argv) {

	// Process window decorations
	processDecorations(app);

	// Create the application
	createApplication(app);

	// Define delegate
	createDelegate(app);

	// Create the main window
	createMainWindow(app);

	// Create Content View
	id contentView = msg( ALLOC("NSView"), s("init") );
	msg(app->mainWindow, s("setContentView:"), contentView);

	// Set the main window title
	SetTitle(app, app->title);

	// Center Window
	Center(app);

	// Set Colour
	applyWindowColour(app);

	// Process translucency
	if (app->windowBackgroundIsTranslucent) {
		makeWindowBackgroundTranslucent(app);
	}

    // We set it to be invisible by default. It will become visible when everything has initialised
    msg(app->mainWindow, s("setIsVisible:"), NO);

	// Setup webview
	id config = msg(c("WKWebViewConfiguration"), s("new"));
	msg(config, s("setValue:forKey:"), msg(c("NSNumber"), s("numberWithBool:"), 1), str("suppressesIncrementalRendering"));
	if (app->devtools) {
	  Debug(app, "Enabling devtools");
	  enableBoolConfig(config, "developerExtrasEnabled");
	}
	app->config = config;

	id manager = msg(config, s("userContentController"));
	msg(manager, s("addScriptMessageHandler:name:"), app->delegate, str("external"));
	msg(manager, s("addScriptMessageHandler:name:"), app->delegate, str("completed"));
	app->manager = manager;

	id wkwebview = msg(c("WKWebView"), s("alloc"));
	app->wkwebview = wkwebview;

	msg(wkwebview, s("initWithFrame:configuration:"), CGRectMake(0, 0, 0, 0), config);

	msg(contentView, s("addSubview:"), wkwebview);
	msg(wkwebview, s("setAutoresizingMask:"), NSViewWidthSizable | NSViewHeightSizable);
	CGRect contentViewBounds = GET_BOUNDS(contentView);
	msg(wkwebview, s("setFrame:"), contentViewBounds );

	// Disable damn smart quotes
	// Credit: https://stackoverflow.com/a/31640511
	id userDefaults = msg(c("NSUserDefaults"), s("standardUserDefaults"));
	msg(userDefaults, s("setBool:forKey:"), NO, str("NSAutomaticQuoteSubstitutionEnabled"));

	// Setup drag message handler
	msg(manager, s("addScriptMessageHandler:name:"), app->delegate, str("windowDrag"));
	// Add mouse event hooks
	app->mouseDownMonitor = msg(c("NSEvent"), u("addLocalMonitorForEventsMatchingMask:handler:"), NSEventMaskLeftMouseDown, ^(id incomingEvent) {
		// Make sure the mouse click was in the window, not the tray
		id window = msg(incomingEvent, s("window"));
		if (window == app->mainWindow) {
			app->mouseEvent = incomingEvent;
		}
		return incomingEvent;
	});
	app->mouseUpMonitor = msg(c("NSEvent"), u("addLocalMonitorForEventsMatchingMask:handler:"), NSEventMaskLeftMouseUp, ^(id incomingEvent) {
		app->mouseEvent = NULL;
		ShowMouse();
		return incomingEvent;
	});

	// Setup context menu message handler
	msg(manager, s("addScriptMessageHandler:name:"), app->delegate, str("contextMenu"));

	// Toolbar
	if( app->useToolBar ) {
		Debug(app, "Setting Toolbar");
		id toolbar = msg(c("NSToolbar"),s("alloc"));
		msg(toolbar, s("initWithIdentifier:"), str("wails.toolbar"));
		msg(toolbar, s("autorelease"));

		// Separator
		if( app->hideToolbarSeparator ) {
			msg(toolbar, s("setShowsBaselineSeparator:"), NO);
		}

		msg(app->mainWindow, s("setToolbar:"), toolbar);
	}

	// Fix up resizing
	if (app->resizable == 0) {
		app->minHeight = app->maxHeight = app->height;
		app->minWidth = app->maxWidth = app->width;
	}
	setMinMaxSize(app);

	// Load HTML
	id html = msg(c("NSURL"), s("URLWithString:"), str(assets[0]));
	msg(wkwebview, s("loadRequest:"), msg(c("NSURLRequest"), s("requestWithURL:"), html));

	Debug(app, "Loading Internal Code");
	// We want to evaluate the internal code plus runtime before the assets
	const char *temp = concat(invoke, app->bindings);
	const char *internalCode = concat(temp, (const char*)&runtime);
	MEMFREE(temp);

	// Add code that sets up the initial state, EG: State Stores.
	temp = concat(internalCode, getInitialState(app));
	MEMFREE(internalCode);
	internalCode = temp;

	  // Loop over assets and build up one giant Mother Of All Evals
	int index = 1;
	while(1) {
		// Get next asset pointer
		const unsigned char *asset = assets[index];

		// If we have no more assets, break
		if (asset == 0x00) {
			break;
		}

		temp = concat(internalCode, (const char *)asset);
		MEMFREE(internalCode);
		internalCode = temp;
		index++;
	};

	// Disable context menu if not in debug mode
	if( debug != 1 ) {
		temp = concat(internalCode, "wails._.DisableDefaultContextMenu();");
		MEMFREE(internalCode);
		internalCode = temp;
	}

	// class_addMethod(delegateClass, s("applicationWillFinishLaunching:"), (IMP) willFinishLaunching, "@@:@");
	// Include callback after evaluation
	temp = concat(internalCode, "webkit.messageHandlers.completed.postMessage(true);");
	MEMFREE(internalCode);
	internalCode = temp;

	// const char *viewportScriptString = "var meta = document.createElement('meta'); meta.setAttribute('name', 'viewport'); meta.setAttribute('content', 'width=device-width'); meta.setAttribute('initial-scale', '1.0'); meta.setAttribute('maximum-scale', '1.0'); meta.setAttribute('minimum-scale', '1.0'); meta.setAttribute('user-scalable', 'no'); document.getElementsByTagName('head')[0].appendChild(meta);";
	// ExecJS(app, viewportScriptString);


	// This evaluates the MOAE once the Dom has finished loading
	msg(manager,
		s("addUserScript:"),
		msg(msg(c("WKUserScript"), s("alloc")),
					s("initWithSource:injectionTime:forMainFrameOnly:"),
					str(internalCode),
					1,
					1));


	// Emit theme change event to notify of current system them
	emitThemeChange(app);

	// If we want the webview to be transparent...
	if( app->webviewIsTranparent == 1 ) {
		msg(wkwebview, s("setValue:forKey:"), msg(c("NSNumber"), s("numberWithBool:"), 0), str("drawsBackground"));
	}

//	// If we have an application menu, process it
//	if( app->applicationMenu != NULL ) {
//	    msg(msg(c("NSApplication"), s("sharedApplication")), s("setMainMenu:"), app->applicationMenu->menu);
//	}

	// Process dialog icons
//	processUserDialogIcons(app);

	app->running = true;

	// Finally call run
	Debug(app, "Run called");
	msg(app->application, s("run"));

	MEMFREE(internalCode);
}


struct Application* NewApplication(const char *title, int width, int height, int resizable, int devtools, int fullscreen, int startHidden, int logLevel) {

    // Load the tray icons
    LoadTrayIcons();

	// Setup main application struct
	struct Application *result = malloc(sizeof(struct Application));
	result->title = title;
	result->width = width;
	result->height = height;
	result->minWidth = 0;
	result->minHeight = 0;
	result->maxWidth = 0;
	result->maxHeight = 0;
	result->resizable = resizable;
	result->devtools = devtools;
	result->fullscreen = fullscreen;
	result->maximised = 0;
	result->startHidden = startHidden;
	result->decorations = 0;
	result->logLevel = logLevel;

	result->mainWindow = NULL;
	result->mouseEvent = NULL;
	result->mouseDownMonitor = NULL;
	result->mouseUpMonitor = NULL;

	// Features
	result->frame = 1;
	result->hideTitle = 0;
	result->hideTitleBar = 0;
	result->fullSizeContent = 0;
	result->useToolBar = 0;
	result->hideToolbarSeparator = 0;
	result->appearance = NULL;
	result->windowBackgroundIsTranslucent = 0;

	// Window data
	result->delegate = NULL;

	// Menu
	result->menuManager = NewMenuManager();

	// Context Menus
//	result->contextMenuStore = NewContextMenuStore();

	// Window Appearance
	result->titlebarAppearsTransparent = 0;
	result->webviewIsTranparent = 0;

	result->sendMessageToBackend = (ffenestriCallback) messageFromWindowCallback;

	result->running = false;

    return (void*) result;
}
