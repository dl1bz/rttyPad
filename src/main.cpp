#include "MainWindow.h"
#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("rttyPad"));
    QApplication::setOrganizationName(QStringLiteral("DL1BZ"));

    // Explicit dark appearance on all supported desktop platforms.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette dark;
    dark.setColor(QPalette::Window, QColor(30, 30, 30));
    dark.setColor(QPalette::WindowText, QColor(230, 230, 230));
    dark.setColor(QPalette::Base, QColor(20, 20, 20));
    dark.setColor(QPalette::AlternateBase, QColor(38, 38, 38));
    dark.setColor(QPalette::ToolTipBase, QColor(38, 38, 38));
    dark.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
    dark.setColor(QPalette::Text, QColor(230, 230, 230));
    dark.setColor(QPalette::Button, QColor(45, 45, 45));
    dark.setColor(QPalette::ButtonText, QColor(230, 230, 230));
    dark.setColor(QPalette::BrightText, QColor(255, 255, 255));
    dark.setColor(QPalette::Highlight, QColor(45, 105, 165));
    dark.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    dark.setColor(QPalette::Link, QColor(100, 170, 235));
    dark.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
    dark.setColor(QPalette::Disabled, QPalette::Text, QColor(115, 115, 115));
    dark.setColor(QPalette::Disabled, QPalette::WindowText, QColor(115, 115, 115));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(115, 115, 115));
    app.setPalette(dark);

    // Fusion's dark checkbox indicators can become nearly black.  Match the
    // button palette and use an explicit tick so checked state stays visible
    // on both macOS and Windows (including inside dialogs).
    app.setStyleSheet(QStringLiteral(
        "QCheckBox::indicator {"
        " width: 15px; height: 15px;"
        " background-color: palette(button);"
        " border: 1px solid #858585; border-radius: 3px;"
        " }"
        "QCheckBox::indicator:checked {"
        " image: url(:/icons/CheckboxTick.png);"
        " }"
        "QCheckBox::indicator:disabled {"
        " border-color: #555555;"
        " }"));

    app.setWindowIcon(QIcon(QStringLiteral(":/icons/AppIcon.png")));

    MainWindow window;
    window.show();
    return app.exec();
}
