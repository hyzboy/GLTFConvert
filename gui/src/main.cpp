#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStringList>
#include <QDir>
#include "MainWindow.h"

namespace
{
    void ApplyDarkTheme(QApplication &app)
    {
        app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

        QPalette pal;
        pal.setColor(QPalette::Window,          QColor(45, 45, 48));
        pal.setColor(QPalette::WindowText,      QColor(208, 208, 208));
        pal.setColor(QPalette::Base,            QColor(30, 30, 32));
        pal.setColor(QPalette::AlternateBase,   QColor(38, 38, 40));
        pal.setColor(QPalette::ToolTipBase,     QColor(45, 45, 48));
        pal.setColor(QPalette::ToolTipText,     QColor(208, 208, 208));
        pal.setColor(QPalette::Text,            QColor(208, 208, 208));
        pal.setColor(QPalette::Disabled, QPalette::Text, QColor(100, 100, 100));
        pal.setColor(QPalette::Dark,            QColor(30, 30, 32));
        pal.setColor(QPalette::Shadow,          QColor(20, 20, 20));
        pal.setColor(QPalette::Button,          QColor(53, 53, 56));
        pal.setColor(QPalette::ButtonText,      QColor(208, 208, 208));
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
        pal.setColor(QPalette::BrightText,      QColor(255, 80, 80));
        pal.setColor(QPalette::Link,            QColor(42, 130, 218));
        pal.setColor(QPalette::Highlight,       QColor(42, 130, 218));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(60, 60, 65));
        pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(100, 100, 100));
        pal.setColor(QPalette::PlaceholderText, QColor(110, 110, 110));

        app.setPalette(pal);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("GLTFConvertQt"));
    app.setOrganizationName(QStringLiteral("ULRE"));

    ApplyDarkTheme(app);

    QStringList initial_files;
    for (int i = 1; i < argc; ++i)
    {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (!arg.startsWith(QLatin1Char('-')))
        {
            initial_files.append(QDir::fromNativeSeparators(arg));
        }
    }

    MainWindow w(initial_files);
    w.show();

    return app.exec();
}
