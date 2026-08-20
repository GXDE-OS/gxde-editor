#include "waylandactivator.h"

#include <cstdint>
#include <unistd.h>

#include <QGuiApplication>

#include "kywc-toplevel-v1-client-protocol.h"

// ---- registry listener ----
void WaylandActivator::handleGlobal(void *data, struct wl_registry *registry, uint32_t name,
                                    const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandActivator *>(data);
    if (strcmp(interface, kywc_toplevel_manager_v1_interface.name) == 0) {
        self->m_manager = static_cast<kywc_toplevel_manager_v1 *>(
            wl_registry_bind(registry, name, &kywc_toplevel_manager_v1_interface,
                             qMin(version, 1u)));
        if (self->m_manager) {
            kywc_toplevel_manager_v1_add_listener(self->m_manager, managerListener(), self);
        }
    }
}

void WaylandActivator::handleGlobalRemove(void *data, struct wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(name)
}

// ---- manager listener ----
void WaylandActivator::handleManagerToplevel(void *data, struct kywc_toplevel_manager_v1 *manager,
                                             struct kywc_toplevel_v1 *toplevel, const char *uuid)
{
    Q_UNUSED(manager)
    Q_UNUSED(uuid)
    auto *self = static_cast<WaylandActivator *>(data);
    kywc_toplevel_v1_add_listener(toplevel, toplevelListener(), self);
}

void WaylandActivator::handleManagerFinished(void *data,
                                             struct kywc_toplevel_manager_v1 *manager)
{
    Q_UNUSED(data)
    Q_UNUSED(manager)
}

// ---- toplevel listener ----
void WaylandActivator::handleToplevelClosed(void *data, struct kywc_toplevel_v1 *toplevel)
{
    auto *self = static_cast<WaylandActivator *>(data);
    self->m_ownToplevels.removeAll(toplevel);
    kywc_toplevel_v1_destroy(toplevel);
}

void WaylandActivator::handleToplevelDone(void *data, struct kywc_toplevel_v1 *toplevel)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
}

void WaylandActivator::handleToplevelTitle(void *data, struct kywc_toplevel_v1 *toplevel,
                                           const char *title)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(title)
}

void WaylandActivator::handleToplevelAppId(void *data, struct kywc_toplevel_v1 *toplevel,
                                           const char *app_id)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(app_id)
}

void WaylandActivator::handleToplevelPrimaryOutput(void *data, struct kywc_toplevel_v1 *toplevel,
                                                   const char *output)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(output)
}

void WaylandActivator::handleToplevelWorkspaceEnter(void *data,
                                                   struct kywc_toplevel_v1 *toplevel,
                                                   const char *workspace)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(workspace)
}

void WaylandActivator::handleToplevelWorkspaceLeave(void *data,
                                                   struct kywc_toplevel_v1 *toplevel,
                                                   const char *workspace)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(workspace)
}

void WaylandActivator::handleToplevelCapabilities(void *data, struct kywc_toplevel_v1 *toplevel,
                                                  uint32_t capabilities)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(capabilities)
}

void WaylandActivator::handleToplevelState(void *data, struct kywc_toplevel_v1 *toplevel,
                                           uint32_t state)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(state)
}

void WaylandActivator::handleToplevelParent(void *data, struct kywc_toplevel_v1 *toplevel,
                                            struct kywc_toplevel_v1 *parent)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(parent)
}

void WaylandActivator::handleToplevelIcon(void *data, struct kywc_toplevel_v1 *toplevel,
                                          const char *icon)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(icon)
}

void WaylandActivator::handleToplevelGeometry(void *data, struct kywc_toplevel_v1 *toplevel,
                                              int32_t x, int32_t y, uint32_t width,
                                              uint32_t height)
{
    Q_UNUSED(data)
    Q_UNUSED(toplevel)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(width)
    Q_UNUSED(height)
}

void WaylandActivator::handleToplevelPid(void *data, struct kywc_toplevel_v1 *toplevel,
                                         uint32_t pid)
{
    auto *self = static_cast<WaylandActivator *>(data);
    if (pid == static_cast<uint32_t>(getpid())) {
        if (!self->m_ownToplevels.contains(toplevel)) {
            self->m_ownToplevels.append(toplevel);
        }
    }
}

// ---- static listener structs ----
const struct wl_registry_listener *WaylandActivator::registryListener()
{
    static const struct wl_registry_listener listener = {
        handleGlobal,
        handleGlobalRemove,
    };
    return &listener;
}

const struct kywc_toplevel_manager_v1_listener *WaylandActivator::managerListener()
{
    static const struct kywc_toplevel_manager_v1_listener listener = {
        handleManagerToplevel,
        handleManagerFinished,
    };
    return &listener;
}

const struct kywc_toplevel_v1_listener *WaylandActivator::toplevelListener()
{
    static const struct kywc_toplevel_v1_listener listener = {
        handleToplevelClosed,
        handleToplevelDone,
        handleToplevelTitle,
        handleToplevelAppId,
        handleToplevelPrimaryOutput,
        handleToplevelWorkspaceEnter,
        handleToplevelWorkspaceLeave,
        handleToplevelCapabilities,
        handleToplevelState,
        handleToplevelParent,
        handleToplevelIcon,
        handleToplevelGeometry,
        handleToplevelPid,
    };
    return &listener;
}

// ---- class impl ----
WaylandActivator &WaylandActivator::instance()
{
    static WaylandActivator inst;
    return inst;
}

WaylandActivator::WaylandActivator(QObject *parent)
    : QObject(parent)
{
    bind();
}

WaylandActivator::~WaylandActivator()
{
    for (kywc_toplevel_v1 *t : std::as_const(m_ownToplevels)) {
        kywc_toplevel_v1_destroy(t);
    }
    m_ownToplevels.clear();
    if (m_manager) {
        kywc_toplevel_manager_v1_destroy(m_manager);
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
    }
    if (m_display) {
        wl_display_disconnect(m_display);
    }
}

void WaylandActivator::bind()
{
    if (!QGuiApplication::platformName().startsWith("wayland")) {
        return;
    }

    for (kywc_toplevel_v1 *t : std::as_const(m_ownToplevels)) {
        kywc_toplevel_v1_destroy(t);
    }
    m_ownToplevels.clear();
    if (m_manager) {
        kywc_toplevel_manager_v1_destroy(m_manager);
        m_manager = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
    }

    // kywc_toplevel objects are server-created (their IDs live in the
    // compositor half of the Wayland object map).  Keeping them on Qt's
    // shared display but on libwayland's default event queue can collide
    // with a server ID reused by Qt's clipboard queue before a queued
    // toplevel.closed event has run.  Use an independent connection: this
    // protocol identifies our windows by PID and does not need Qt's
    // wl_surface objects.
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        return;
    }
    wl_registry_add_listener(m_registry, registryListener(), this);
    wl_display_roundtrip(m_display);
    wl_display_roundtrip(m_display);

    m_initialized = (m_manager != nullptr);
}

bool WaylandActivator::activateOwnWindow()
{
    // Re-enumerate on demand.  No event-loop integration is needed for this
    // short-lived auxiliary connection, and newly created windows are always
    // included in the activation request.
    bind();
    if (!m_initialized || m_ownToplevels.isEmpty()) {
        return false;
    }
    for (kywc_toplevel_v1 *t : std::as_const(m_ownToplevels)) {
        kywc_toplevel_v1_activate(t);
    }
    wl_display_flush(m_display);
    return true;
}
