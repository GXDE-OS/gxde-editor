#pragma once

#include <QList>
#include <QObject>

struct wl_display;
struct wl_registry;
struct kywc_toplevel_manager_v1;
struct kywc_toplevel_v1;

// Activates the application's own toplevel window on the gxde-wlcom
// compositor using the kywc_toplevel_v1 protocol.
//
// Wayland forbids clients from raising/focusing their own windows:
// QWidget::activateWindow() (and even QWindow::requestActivate()) are
// ignored by the compositor for security. wlcom exposes the
// kywc_toplevel_v1 protocol, which lets any client request activation of
// a toplevel it can identify. We match our own window by process pid
// (the compositor enumerates all existing toplevels, including already
// running ones, on bind). This is the reliable fix for
// "open file B from another app -> bring editor to front".
class WaylandActivator : public QObject
{
    Q_OBJECT

public:
    static WaylandActivator &instance();

    // Request the compositor to activate our own window(s).
    // Returns true if the activation request was dispatched.
    bool activateOwnWindow();

private:
    explicit WaylandActivator(QObject *parent = nullptr);
    ~WaylandActivator() override;

    void bind();

    static const struct wl_registry_listener *registryListener();
    static const struct kywc_toplevel_manager_v1_listener *managerListener();
    static const struct kywc_toplevel_v1_listener *toplevelListener();

    // registry listener
    static void handleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                             const char *interface, uint32_t version);
    static void handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name);

    // manager listener
    static void handleManagerToplevel(void *data, struct kywc_toplevel_manager_v1 *manager,
                                      struct kywc_toplevel_v1 *toplevel, const char *uuid);
    static void handleManagerFinished(void *data, struct kywc_toplevel_manager_v1 *manager);

    // toplevel listener
    static void handleToplevelClosed(void *data, struct kywc_toplevel_v1 *toplevel);
    static void handleToplevelDone(void *data, struct kywc_toplevel_v1 *toplevel);
    static void handleToplevelTitle(void *data, struct kywc_toplevel_v1 *toplevel,
                                    const char *title);
    static void handleToplevelAppId(void *data, struct kywc_toplevel_v1 *toplevel,
                                    const char *app_id);
    static void handleToplevelPrimaryOutput(void *data, struct kywc_toplevel_v1 *toplevel,
                                            const char *output);
    static void handleToplevelWorkspaceEnter(void *data, struct kywc_toplevel_v1 *toplevel,
                                             const char *workspace);
    static void handleToplevelWorkspaceLeave(void *data, struct kywc_toplevel_v1 *toplevel,
                                             const char *workspace);
    static void handleToplevelCapabilities(void *data, struct kywc_toplevel_v1 *toplevel,
                                           uint32_t capabilities);
    static void handleToplevelState(void *data, struct kywc_toplevel_v1 *toplevel,
                                    uint32_t state);
    static void handleToplevelParent(void *data, struct kywc_toplevel_v1 *toplevel,
                                     struct kywc_toplevel_v1 *parent);
    static void handleToplevelIcon(void *data, struct kywc_toplevel_v1 *toplevel,
                                   const char *icon);
    static void handleToplevelGeometry(void *data, struct kywc_toplevel_v1 *toplevel, int32_t x,
                                       int32_t y, uint32_t width, uint32_t height);
    static void handleToplevelPid(void *data, struct kywc_toplevel_v1 *toplevel, uint32_t pid);

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    kywc_toplevel_manager_v1 *m_manager = nullptr;
    bool m_initialized = false;

    // toplevel objects belonging to our own pid.
    QList<kywc_toplevel_v1 *> m_ownToplevels;
};
