#include "preinclude.hpp"
#include "presentation/runtime_presenter.hpp"
#include "presentation/settings_presenter.hpp"

#include <ilias/platform/qt.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

auto main(int argc, char **argv) -> int
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("mksync"));
    application.setOrganizationName(QStringLiteral("Btk-Project"));

    // QIoContext delegates I/O readiness and timers to Qt's event loop. It is
    // installed before QML loads so later Presenter actions can spawn ilias
    // Tasks without creating a competing platform event loop.
    ilias::QIoContext ioContext;
    ioContext.install();

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    mks::gui::SettingsPresenter settingsPresenter;
    settingsPresenter.loadDefault();
    mks::gui::RuntimePresenter runtimePresenter;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("settingsPresenter"),
                                             &settingsPresenter);
    engine.rootContext()->setContextProperty(QStringLiteral("runtimePresenter"),
                                             &runtimePresenter);
    const auto mainUrl = QUrl{QStringLiteral("qrc:/qml/Main.qml")};
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &application,
                     &QCoreApplication::quit);
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }
    return application.exec();
}
