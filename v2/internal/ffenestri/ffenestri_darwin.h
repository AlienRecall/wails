
#ifndef FFENESTRI_DARWIN_H
#define FFENESTRI_DARWIN_H


#define OBJC_OLD_DISPATCH_PROTOTYPES 1
#include <objc/objc-runtime.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include "json.h"
#include "hashmap.h"
#include "stdlib.h"

// Macros to make it slightly more sane
#define msg objc_msgSend
#define msg_reg ((id(*)(id, SEL))objc_msgSend)
#define msg_id ((id(*)(id, SEL, id))objc_msgSend)
#define msg_id_id ((id(*)(id, SEL, id, id))objc_msgSend)
#define msg_bool ((id(*)(id, SEL, BOOL))objc_msgSend)
#define msg_int ((id(*)(id, SEL, int))objc_msgSend)
#define msg_uint ((id(*)(id, SEL, unsigned int))objc_msgSend)
#define msg_float ((id(*)(id, SEL, float))objc_msgSend)
#define kInternetEventClass 'GURL'
#define kAEGetURL 'GURL'
#define keyDirectObject '----'

#define c(str) (id)objc_getClass(str)
#define s(str) sel_registerName(str)
#define u(str) sel_getUid(str)
#define str(input) ((id(*)(id, SEL, const char *))objc_msgSend)(c("NSString"), s("stringWithUTF8String:"), input)
#define strunicode(input) ((id(*)(id, SEL, id, unsigned short))objc_msgSend)(c("NSString"), s("stringWithFormat:"), str("%C"), (unsigned short)input)
#define cstr(input) (const char *)msg_reg(input, s("UTF8String"))
#define url(input) msg_id(c("NSURL"), s("fileURLWithPath:"), str(input))
#define ALLOC(classname) msg_reg(c(classname), s("alloc"))
#define ALLOC_INIT(classname) msg_reg(msg_reg(c(classname), s("alloc")), s("init"))

#if defined (__aarch64__)
#define GET_FRAME(receiver) ((CGRect(*)(id, SEL))objc_msgSend)(receiver, s("frame"))
#define GET_BOUNDS(receiver) ((CGRect(*)(id, SEL))objc_msgSend)(receiver, s("bounds"))
#endif

#if defined (__x86_64__)
#define GET_FRAME(receiver) ((CGRect(*)(id, SEL))objc_msgSend_stret)(receiver, s("frame"))
#define GET_BOUNDS(receiver) ((CGRect(*)(id, SEL))objc_msgSend_stret)(receiver, s("bounds"))
#endif

#define GET_BACKINGSCALEFACTOR(receiver) ((CGFloat(*)(id, SEL))objc_msgSend)(receiver, s("backingScaleFactor"))

#define ON_MAIN_THREAD(str) dispatch( ^{ str; } )
#define MAIN_WINDOW_CALL(str) msg_reg(app->mainWindow, s((str)))

#define NSBackingStoreBuffered 2

#define NSWindowStyleMaskBorderless 0
#define NSWindowStyleMaskTitled 1
#define NSWindowStyleMaskClosable 2
#define NSWindowStyleMaskMiniaturizable 4
#define NSWindowStyleMaskResizable 8
#define NSWindowStyleMaskFullscreen 1 << 14

#define NSVisualEffectMaterialWindowBackground 12
#define NSVisualEffectBlendingModeBehindWindow 0
#define NSVisualEffectStateFollowsWindowActiveState 0
#define NSVisualEffectStateActive 1
#define NSVisualEffectStateInactive 2

#define NSViewWidthSizable 2
#define NSViewHeightSizable 16

#define NSWindowBelow -1
#define NSWindowAbove 1

#define NSSquareStatusItemLength   -2.0
#define NSVariableStatusItemLength -1.0

#define NSWindowTitleHidden 1
#define NSWindowStyleMaskFullSizeContentView 1 << 15

#define NSEventModifierFlagCommand 1 << 20
#define NSEventModifierFlagOption 1 << 19
#define NSEventModifierFlagControl 1 << 18
#define NSEventModifierFlagShift 1 << 17

#define NSControlStateValueMixed -1
#define NSControlStateValueOff 0
#define NSControlStateValueOn 1

#define NSApplicationActivationPolicyRegular    0
#define NSApplicationActivationPolicyAccessory  1
#define NSApplicationActivationPolicyProhibited 2

// Unbelievably, if the user swaps their button preference
// then right buttons are reported as left buttons
#define NSEventMaskLeftMouseDown 1 << 1
#define NSEventMaskLeftMouseUp 1 << 2
#define NSEventMaskRightMouseDown 1 << 3
#define NSEventMaskRightMouseUp 1 << 4

#define NSEventTypeLeftMouseDown 1
#define NSEventTypeLeftMouseUp 2
#define NSEventTypeRightMouseDown 3
#define NSEventTypeRightMouseUp 4

#define NSNoImage       0
#define NSImageOnly     1
#define NSImageLeft     2
#define NSImageRight    3
#define NSImageBelow    4
#define NSImageAbove    5
#define NSImageOverlaps 6

#define NSAlertStyleWarning 0
#define NSAlertStyleInformational 1
#define NSAlertStyleCritical 2

#define NSAlertFirstButtonReturn   1000
#define NSAlertSecondButtonReturn  1001
#define NSAlertThirdButtonReturn   1002

struct Application;
int releaseNSObject(void *const context, struct hashmap_element_s *const e);
void TitlebarAppearsTransparent(struct Application* app);
void HideTitle(struct Application* app);
void HideTitleBar(struct Application* app);
void FullSizeContent(struct Application* app);
void UseToolbar(struct Application* app);
void HideToolbarSeparator(struct Application* app);
void DisableFrame(struct Application* app);
void SetAppearance(struct Application* app, const char *);
void WebviewIsTransparent(struct Application* app);
void WindowBackgroundIsTranslucent(struct Application* app);
void SetTray(struct Application* app, const char *, const char *, const char *);
//void SetContextMenus(struct Application* app, const char *);
void AddTrayMenu(struct Application* app, const char *);

void SetActivationPolicy(struct Application* app, int policy);

void* lookupStringConstant(id constantName);

void HasURLHandlers(struct Application* app);

#endif