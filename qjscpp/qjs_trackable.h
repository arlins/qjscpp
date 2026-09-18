#pragma once
#include "qjs_global.h"

NAMESPACE_QJS_BEGIN

// QcIObjectTracker
class QcIObjectTracker {
public:
    virtual ~QcIObjectTracker() = default;

    virtual void TrackerOnObjectDestroyed() noexcept = 0;
    virtual void TrackerAddRef() noexcept = 0;
    virtual void TrackerRelease() noexcept = 0;
};

// =======================================
// QcTrackable
// Base class of native classes for trackable. 
// Please note that copying or moving operations will cause the 
// tracker to become invalid.
// =======================================

// QcTrackable
class QcTrackable {
public:
    QcTrackable() = default;

    virtual ~QcTrackable() {
        _qc_ResetTracker();
    }

    // Copy: The new object's m_tracker must be nullptr.
    QcTrackable(const QcTrackable& other) noexcept
        : m_tracker(nullptr) {
    }

    // Copy Assign: Clear up this.tracker
    QcTrackable& operator=(const QcTrackable& other) noexcept {
        if (this != &other) {
            _qc_ResetTracker();
        }
        return *this;
    }

    // Move: Clean up other.tracker and set the new object tracker to nullptr.
    QcTrackable(QcTrackable&& other) noexcept
        : m_tracker(nullptr) {
        other._qc_ResetTracker();
    }

    // Move Assign：Clean up this.tracker and other.tracker;
    QcTrackable& operator=(QcTrackable&& other) noexcept {
        if (this != &other) {
            _qc_ResetTracker();
            other._qc_ResetTracker();
        }
        return *this;
    }

    QcIObjectTracker* qc_GetTracker() const noexcept {
        return m_tracker;
    }

    void qc_SetTracker(QcIObjectTracker* tracker) {
        if (tracker == nullptr || m_tracker == tracker) {
            return;
        }
        if (m_tracker != nullptr) {
            QJS_ASSERT_M(false, "Can not set tracker multiple times");
        }

        _qc_ResetTracker();
        m_tracker = tracker;
        m_tracker->TrackerAddRef();
    }

private:
    // Destroy and release the object's tracker
    void _qc_ResetTracker() noexcept {
        if (m_tracker) {
            QcIObjectTracker* old_tracker = m_tracker;
            m_tracker = nullptr;
            old_tracker->TrackerOnObjectDestroyed();
            old_tracker->TrackerRelease();
        }
    }

private:
    QcIObjectTracker* m_tracker{ nullptr };
};

NAMESPACE_QJS_END