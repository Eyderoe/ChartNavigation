#include "macWindow.hpp"

#include <QEvent>
#include <QGuiApplication>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QScopedValueRollback>
#include <QTimer>
#include <QWidget>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

namespace {

constexpr auto behaviorMask = NSWindowCollectionBehaviorCanJoinAllSpaces
                            | NSWindowCollectionBehaviorMoveToActiveSpace
                            | NSWindowCollectionBehaviorFullScreenPrimary
                            | NSWindowCollectionBehaviorFullScreenAuxiliary
                            | NSWindowCollectionBehaviorFullScreenNone
                            | NSWindowCollectionBehaviorPrimary
                            | NSWindowCollectionBehaviorAuxiliary
                            | NSWindowCollectionBehaviorCanJoinAllApplications;

bool otherFullScreenWindowOnScreen (NSWindow *window) {
    NSScreen *screen = window.screen ?: NSScreen.mainScreen;
    if (!screen)
        return false;
    const auto display = [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
    const CGRect displayBounds = CGDisplayBounds(display);
    const auto insets = screen.safeAreaInsets;
    const CGRect safeBounds = CGRectMake(displayBounds.origin.x + insets.left,
                                        displayBounds.origin.y + insets.top,
                                        displayBounds.size.width - insets.left - insets.right,
                                        displayBounds.size.height - insets.top - insets.bottom);
    // macOS can keep the menu bar visible even in a native full-screen space.
    const CGFloat menuHeight = qMax(insets.top, qMax(NSStatusBar.systemStatusBar.thickness,
        NSMaxY(screen.frame) - NSMaxY(screen.visibleFrame)));
    const CGRect menuBounds = CGRectMake(displayBounds.origin.x, displayBounds.origin.y + menuHeight,
                                        displayBounds.size.width, displayBounds.size.height - menuHeight);
    const auto matches = [](CGRect a, CGRect b) {
        return qAbs(a.origin.x - b.origin.x) <= 1 && qAbs(a.origin.y - b.origin.y) <= 1
            && qAbs(a.size.width - b.size.width) <= 1 && qAbs(a.size.height - b.size.height) <= 1;
    };
    // Inspect only on-screen geometry/layers, not titles or captured content.
    // Desktop background/icon windows distinguish a desktop with a maximized
    // window (including an auto-hidden Dock) from a native full-screen space.
    NSArray *windows = CFBridgingRelease(CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly, kCGNullWindowID));
    bool fullScreenWindow = false;
    for (NSDictionary *info in windows) {
        CGRect bounds;
        if (!CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)info[(__bridge NSString *)kCGWindowBounds], &bounds))
            continue;
        const NSInteger layer = [info[(__bridge NSString *)kCGWindowLayer] integerValue];
        if ((layer == CGWindowLevelForKey(kCGDesktopWindowLevelKey)
             || layer == CGWindowLevelForKey(kCGDesktopIconWindowLevelKey))
            && CGRectContainsRect(bounds, displayBounds))
            return false;
        if ([info[(__bridge NSString *)kCGWindowOwnerPID] intValue] == NSProcessInfo.processInfo.processIdentifier
            || layer != 0
            || [info[(__bridge NSString *)kCGWindowAlpha] doubleValue] <= 0)
            continue;
        fullScreenWindow |= matches(bounds, displayBounds) || matches(bounds, safeBounds) || matches(bounds, menuBounds);
    }
    return fullScreenWindow;
}

class MacWindowStayOnTop final : public QObject {
    public:
        explicit MacWindowStayOnTop (QWidget *window)
            : QObject(window), widget(window), originalFlags(window->windowFlags()),
              originalAlwaysShowToolWindow(window->testAttribute(Qt::WA_MacAlwaysShowToolWindow)),
              originalQuitOnClose(window->testAttribute(Qt::WA_QuitOnClose)),
              menuBar(window->findChild<QMenuBar *>(QString(), Qt::FindDirectChildrenOnly)),
              originalNativeMenuBar(menuBar && menuBar->isNativeMenuBar()) {
            NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
            originalDesktopLevel = view.window.level;
            window->installEventFilter(this);
            for (QMenu *menu : window->findChildren<QMenu *>())
                menu->installEventFilter(this);
            menuContextTimer.setInterval(250);
            connect(&menuContextTimer, &QTimer::timeout, this, [this] { updateWindowMode(); });
        }

        void setEnabled (bool value) {
            enabled = value;
            if (enabled)
                menuContextTimer.start();
            else
                menuContextTimer.stop();
            updateWindowMode();
        }

    private:
        void updateWindowMode () {
            if (updatingMode)
                return;
            QScopedValueRollback guard(updatingMode, true);
            NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
            const bool fullScreen = enabled && !widget->isFullScreen()
                                 && otherFullScreenWindowOnScreen(view.window);
            if (pinnedApplied != enabled || fullScreenContext != fullScreen) {
                restore();
                const QRect geometry = widget->geometry();
                const auto state = widget->windowState();
                const bool visible = widget->isVisible();
                pinnedApplied = enabled;
                fullScreenContext = fullScreen;
                widget->setAttribute(Qt::WA_MacAlwaysShowToolWindow,
                                     enabled || originalAlwaysShowToolWindow);
                // Desktop: a regular window with the system menubar. Another
                // app's full-screen space: a nonactivating QNSPanel with an
                // in-window menubar. Qt recreates the native window on changes.
                const auto flags = fullScreenContext
                    ? (originalFlags & ~Qt::WindowType_Mask) | Qt::Tool
                    : originalFlags;
                widget->setWindowFlags(enabled ? flags | Qt::WindowStaysOnTopHint : flags);
                // This remains the application's main window, even though its
                // Cocoa representation is a panel while floating.
                widget->setAttribute(Qt::WA_QuitOnClose, originalQuitOnClose);
                widget->setGeometry(geometry);
                widget->setWindowState(state);
                setNativeMenuBar(fullScreenContext ? false : originalNativeMenuBar);
                apply();
                if (visible)
                    widget->show();
            }
            apply();
        }

    protected:
        bool eventFilter (QObject *object, QEvent *event) override {
            if (auto *menu = qobject_cast<QMenu *>(object)) {
                if (fullScreenContext && event->type() == QEvent::Show)
                    applyPopup(menu);
                return QObject::eventFilter(object, event);
            }
            switch (event->type()) {
                case QEvent::Show:
                case QEvent::WinIdChange:
                case QEvent::WindowStateChange:
                case QEvent::Move:
                    // Cocoa may update the native window during these Qt events.
                    // Apply afterwards, and coalesce events from creating winId().
                    if (!updatePending) {
                        updatePending = true;
                        QTimer::singleShot(0, this, [this] {
                            updatePending = false;
                            updateWindowMode();
                        });
                    }
                    break;
                default:
                    break;
            }
            return QObject::eventFilter(object, event);
        }

    private:
        QWidget *widget;
        const Qt::WindowFlags originalFlags;
        const bool originalAlwaysShowToolWindow;
        const bool originalQuitOnClose;
        QPointer<QMenuBar> menuBar;
        const bool originalNativeMenuBar;
        __weak NSWindow *nativeWindow = nil;
        NSWindowCollectionBehavior originalBehavior{};
        NSInteger originalLevel{};
        NSInteger originalDesktopLevel{};
        bool enabled{false};
        bool pinnedApplied{false};
        bool fullScreenContext{false};
        bool updatingMode{false};
        bool updatePending{false};
        QTimer menuContextTimer;

        struct PopupState {
            __weak NSWindow *window;
            NSWindowCollectionBehavior behavior;
            NSWindowStyleMask style;
            NSInteger level;
        };
        QList<PopupState> popupStates;

        void setNativeMenuBar (bool native) {
            if (!menuBar || menuBar->isNativeMenuBar() == native)
                return;
            // Qt creates an empty platform menubar when native mode is restored.
            // Reparent it to the current window and replay its existing actions;
            // keep the QMenuBar/QMenu/QAction objects and their connections intact.
            const auto actions = menuBar->actions();
            for (QAction *action : actions)
                menuBar->removeAction(action);
            menuBar->setNativeMenuBar(native);
            if (native) {
                QWidget *parent = menuBar->parentWidget();
                menuBar->setParent(nullptr);
                if (auto *mainWindow = qobject_cast<QMainWindow *>(parent))
                    mainWindow->setMenuBar(menuBar);
                else
                    menuBar->setParent(parent);
            }
            for (QAction *action : actions)
                menuBar->addAction(action);
            menuBar->setVisible(!native);
        }

        void applyPopup (QMenu *menu) {
            NSView *view = (__bridge NSView *)reinterpret_cast<void *>(menu->winId());
            NSWindow *window = view.window;
            if (![window isKindOfClass:[NSPanel class]])
                return;
            bool saved = false;
            for (const auto &state : popupStates)
                saved |= state.window == window;
            if (!saved)
                popupStates.append({window, window.collectionBehavior, window.styleMask, window.level});
            // A QMenu is a separate Cocoa panel. Give it the same full-screen
            // eligibility as its owner, before Qt orders the popup on screen.
            window.styleMask |= NSWindowStyleMaskNonactivatingPanel;
            window.collectionBehavior = (window.collectionBehavior & ~behaviorMask)
                                      | NSWindowCollectionBehaviorCanJoinAllSpaces
                                      | NSWindowCollectionBehaviorFullScreenAuxiliary
                                      | NSWindowCollectionBehaviorCanJoinAllApplications;
        }

        void restore () {
            for (const auto &state : popupStates) {
                NSWindow *popup = state.window;
                if (!popup)
                    continue;
                popup.styleMask = state.style;
                popup.collectionBehavior = (popup.collectionBehavior & ~behaviorMask)
                                         | (state.behavior & behaviorMask);
                popup.level = state.level;
            }
            popupStates.clear();
            NSWindow *window = nativeWindow;
            if (!window)
                return;
            window.collectionBehavior = (window.collectionBehavior & ~behaviorMask)
                                      | (originalBehavior & behaviorMask);
            window.level = originalLevel;
        }

        void apply () {
            NSView *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
            NSWindow *window = view.window;
            if (!window)
                return;

            if (window != nativeWindow) {
                restore();
                nativeWindow = window;
                originalBehavior = window.collectionBehavior;
                originalLevel = window.level;
            }

            if (!enabled || widget->isFullScreen()) {
                restore();
                if (!enabled)
                    window.level = originalDesktopLevel;
                return;
            }

            if (!fullScreenContext) {
                window.collectionBehavior = (window.collectionBehavior & ~NSWindowCollectionBehaviorMoveToActiveSpace)
                                          | NSWindowCollectionBehaviorCanJoinAllSpaces;
                window.level = NSModalPanelWindowLevel;
                return;
            }

            // A normal NSWindow is not eligible to accompany another app's
            // full-screen window merely by setting collectionBehavior flags.
            if (![window isKindOfClass:[NSPanel class]])
                return;
            NSPanel *panel = (NSPanel *)window;
            panel.styleMask = (panel.styleMask & ~NSWindowStyleMaskUtilityWindow)
                            | NSWindowStyleMaskNonactivatingPanel;
            panel.hidesOnDeactivate = NO;
            panel.floatingPanel = YES;
            panel.becomesKeyOnlyIfNeeded = NO;
            window.collectionBehavior = (window.collectionBehavior & ~behaviorMask)
                                      | NSWindowCollectionBehaviorCanJoinAllSpaces
                                      | NSWindowCollectionBehaviorFullScreenAuxiliary
                                      | NSWindowCollectionBehaviorCanJoinAllApplications;
            // FullScreenAuxiliary makes the green button zoom while floating;
            // restoring the original behavior also restores its full-screen action.
            // Match Qt's stay-on-top level. Qt lowers level-3 tool windows on
            // app deactivation even when WA_MacAlwaysShowToolWindow is enabled.
            window.level = NSModalPanelWindowLevel;
        }
};

} // namespace

void setMacWindowStayOnTop (QWidget *window, bool enabled) {
    // Offscreen/minimal Qt backends have no NSView behind their numeric winId.
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        window->setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
        return;
    }
    // Keep the native state with the widget, including across show/state changes.
    MacWindowStayOnTop *controller = nullptr;
    for (QObject *child : window->children()) {
        controller = dynamic_cast<MacWindowStayOnTop *>(child);
        if (controller)
            break;
    }
    if (!controller)
        controller = new MacWindowStayOnTop(window);
    controller->setEnabled(enabled);
}
