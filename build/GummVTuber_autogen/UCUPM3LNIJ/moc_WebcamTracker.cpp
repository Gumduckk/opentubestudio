/****************************************************************************
** Meta object code from reading C++ file 'WebcamTracker.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/track/WebcamTracker.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'WebcamTracker.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13WebcamTrackerE_t {};
} // unnamed namespace

template <> constexpr inline auto WebcamTracker::qt_create_metaobjectdata<qt_meta_tag_ZN13WebcamTrackerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "WebcamTracker",
        "failed",
        "",
        "reason",
        "logLine",
        "line",
        "neutralCaptured",
        "FaceNeutral",
        "n",
        "faceFoundChanged",
        "found",
        "debugFrame",
        "QImage",
        "img",
        "onReadyReadStdout",
        "onReadyReadStderr",
        "onFinished",
        "exitCode",
        "QProcess::ExitStatus",
        "status"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'failed'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'logLine'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'neutralCaptured'
        QtMocHelpers::SignalData<void(const FaceNeutral &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Signal 'faceFoundChanged'
        QtMocHelpers::SignalData<void(bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 },
        }}),
        // Signal 'debugFrame'
        QtMocHelpers::SignalData<void(const QImage &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Slot 'onReadyReadStdout'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onReadyReadStderr'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFinished'
        QtMocHelpers::SlotData<void(int, QProcess::ExitStatus)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 17 }, { 0x80000000 | 18, 19 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<WebcamTracker, qt_meta_tag_ZN13WebcamTrackerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject WebcamTracker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13WebcamTrackerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13WebcamTrackerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13WebcamTrackerE_t>.metaTypes,
    nullptr
} };

void WebcamTracker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<WebcamTracker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->failed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->logLine((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->neutralCaptured((*reinterpret_cast<std::add_pointer_t<FaceNeutral>>(_a[1]))); break;
        case 3: _t->faceFoundChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->debugFrame((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1]))); break;
        case 5: _t->onReadyReadStdout(); break;
        case 6: _t->onReadyReadStderr(); break;
        case 7: _t->onFinished((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (WebcamTracker::*)(const QString & )>(_a, &WebcamTracker::failed, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (WebcamTracker::*)(const QString & )>(_a, &WebcamTracker::logLine, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (WebcamTracker::*)(const FaceNeutral & )>(_a, &WebcamTracker::neutralCaptured, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (WebcamTracker::*)(bool )>(_a, &WebcamTracker::faceFoundChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (WebcamTracker::*)(const QImage & )>(_a, &WebcamTracker::debugFrame, 4))
            return;
    }
}

const QMetaObject *WebcamTracker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *WebcamTracker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13WebcamTrackerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int WebcamTracker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void WebcamTracker::failed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void WebcamTracker::logLine(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void WebcamTracker::neutralCaptured(const FaceNeutral & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void WebcamTracker::faceFoundChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void WebcamTracker::debugFrame(const QImage & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
