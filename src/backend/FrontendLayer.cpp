// Pegasus Frontend
// Copyright (C) 2017  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include "FrontendLayer.h"

#include "Log.h"
#include "Paths.h"
#include "imggen/BlurhashProvider.h"
#include "utils/DiskCachedNAM.h"

#ifdef Q_OS_ANDROID
#include "platform/AndroidAppIconProvider.h"
#include "platform/AndroidPdfProvider.h"
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <QtAndroidExtras/QAndroidJniObject>
#include <jni.h>
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlNetworkAccessManagerFactory>
#include <QQuickWindow>
#include <atomic>


namespace {

class DiskCachedNAMFactory : public QQmlNetworkAccessManagerFactory {
public:
    QNetworkAccessManager* create(QObject* parent) override;
};

QNetworkAccessManager* DiskCachedNAMFactory::create(QObject* parent)
{
    return utils::create_disc_cached_nam(parent);
}

#ifdef Q_OS_ANDROID
// Pegasus X: SystemStatus.java's onLowMemory() calls this, on Android's
// main thread; the release itself runs on Qt's, queued
std::atomic<FrontendLayer*> g_frontend { nullptr };

void JNICALL native_low_memory(JNIEnv*, jclass)
{
    FrontendLayer* const frontend = g_frontend.load();
    if (frontend) {
        QMetaObject::invokeMethod(frontend, [frontend]{
            frontend->releaseMemory(QStringLiteral("Android is low on memory"));
        }, Qt::QueuedConnection);
    }
}

void JNICALL native_trim_memory(JNIEnv*, jclass, jint level)
{
    FrontendLayer* const frontend = g_frontend.load();
    if (frontend) {
        const int lvl = static_cast<int>(level);
        QMetaObject::invokeMethod(frontend, [frontend, lvl]{ frontend->trimMemory(lvl); }, Qt::QueuedConnection);
    }
}

void register_low_memory_callback()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    const JNINativeMethod methods[] {
        { "nativeLowMemory", "()V", reinterpret_cast<void*>(native_low_memory) },
        { "nativeTrimMemory", "(I)V", reinterpret_cast<void*>(native_trim_memory) },
    };
    // the class through Qt's class loader (an instance: Qt 5 has no findClass)
    const QAndroidJniObject status("org/pegasus_frontend/android/SystemStatus");
    QAndroidJniEnvironment env;
    if (!status.isValid()) {
        env->ExceptionClear();
        Log::warning(LOGMSG("Frontend: no low-memory callback (SystemStatus not found)"));
        return;
    }
    const jclass cls = env->GetObjectClass(status.object<jobject>());
    if (env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0])) < 0) {
        env->ExceptionClear();
        Log::warning(LOGMSG("Frontend: no low-memory callback (RegisterNatives failed)"));
    }
    env->DeleteLocalRef(cls);
}
#endif

} // namespace


FrontendLayer::FrontendLayer(QObject* const api_public, QObject* const api_private, QObject* parent)
    : QObject(parent)
    , m_api_public(api_public)
    , m_api_private(api_private)
    , m_engine(nullptr)
{
    // Note: the pointer to the Api is non-owning and constant during the runtime

    // Pegasus X: out of sight (a game runs, the home screen is shown): the
    // theme stays loaded, its caches go. Nothing more when it's back: what's
    // needed is made again as it's drawn.
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state){
        if (state == Qt::ApplicationHidden || state == Qt::ApplicationSuspended)
            releaseMemory(QStringLiteral("Pegasus X is in the background"));
        if (state == Qt::ApplicationActive)
            m_trimmed_level = 0;  // (Android's warnings count again)
    });
#ifdef Q_OS_ANDROID
    g_frontend.store(this);
    register_low_memory_callback();
#endif
}

void FrontendLayer::rebuild()
{
    Q_ASSERT(!m_engine);

    m_engine = new QQmlApplicationEngine(this);
    m_engine->addImportPath(QStringLiteral("lib/qml"));
    m_engine->addImportPath(QStringLiteral("qml"));
    m_engine->setNetworkAccessManagerFactory(new DiskCachedNAMFactory);

    m_engine->addImageProvider(QStringLiteral("blurhash"), new BlurhashProvider);
#ifdef Q_OS_ANDROID
    m_engine->addImageProvider(QStringLiteral("androidicons"), new AndroidAppIconProvider);
    m_engine->addImageProvider(QStringLiteral("pdf"), new AndroidPdfProvider);
#endif

    m_engine->rootContext()->setContextProperty(QStringLiteral("api"), m_api_public);
    m_engine->rootContext()->setContextProperty(QStringLiteral("Api"), m_api_public);
    m_engine->rootContext()->setContextProperty(QStringLiteral("Internal"), m_api_private);
    m_engine->load(QUrl(QStringLiteral("qrc:/frontend/main.qml")));

    emit rebuildComplete();
}

void FrontendLayer::teardown()
{
    Q_ASSERT(m_engine);

    // Pegasus X: the theme's pending api.memory changes are written first
    if (QObject* const memory = m_api_public->property("memory").value<QObject*>())
        QMetaObject::invokeMethod(memory, "flushNow", Qt::DirectConnection);

    // signal forwarding
    connect(m_engine, &QQmlApplicationEngine::destroyed,
            this, &FrontendLayer::teardownComplete);

    m_engine->deleteLater();
    m_engine = nullptr;
}

FrontendLayer::~FrontendLayer()
{
#ifdef Q_OS_ANDROID
    g_frontend.store(nullptr);
#endif
}

void FrontendLayer::releaseMemory(const QString& why)
{
    if (!m_engine)
        return;
    for (QObject* const root : m_engine->rootObjects()) {
        if (auto* const window = qobject_cast<QQuickWindow*>(root))
            window->releaseResources();
    }
    m_engine->trimComponentCache();
    Log::info(LOGMSG("Frontend: released cached graphics and unused QML components (%1)").arg(why));
}

void FrontendLayer::trimMemory(int level)
{
    // Android's ComponentCallbacks2 levels
    constexpr int RUNNING_LOW = 10;
    constexpr int RUNNING_CRITICAL = 15;
    constexpr int UI_HIDDEN = 20;
    constexpr int BACKGROUND = 40;
    if (!m_engine || level == UI_HIDDEN || level <= m_trimmed_level)
        return;
    // a background warning only reaches here once Qt runs again: if that's
    // because Pegasus X is back in front, letting go now would only slow the
    // return (going to the background already released what it could)
    if (level >= BACKGROUND && QGuiApplication::applicationState() == Qt::ApplicationActive) {
        Log::info(LOGMSG("Frontend: Android memory warning, level %1, from the background: Pegasus X is back, nothing let go").arg(level));
        return;
    }
    m_trimmed_level = level;
    if (level < RUNNING_LOW) {
        Log::info(LOGMSG("Frontend: Android memory warning, level %1: nothing let go").arg(level));
        return;  // RUNNING_MODERATE: nothing yet
    }
    m_engine->collectGarbage();
    if (level < RUNNING_CRITICAL) {
        m_engine->trimComponentCache();
        Log::info(LOGMSG("Frontend: Android memory warning, level %1: QML garbage collected, unused components let go").arg(level));
        return;
    }
    releaseMemory(level >= BACKGROUND
        ? QStringLiteral("Android memory warning, level %1, in the background").arg(level)
        : QStringLiteral("Android memory warning, level %1, in front").arg(level));
}

void FrontendLayer::clearCache()
{
    Q_ASSERT(m_engine);
    m_engine->clearComponentCache();
}
