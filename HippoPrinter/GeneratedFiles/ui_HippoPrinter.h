/********************************************************************************
** Form generated from reading UI file 'HippoPrinter.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HIPPOPRINTER_H
#define UI_HIPPOPRINTER_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_HippoPrinterClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *HippoPrinterClass)
    {
        if (HippoPrinterClass->objectName().isEmpty())
            HippoPrinterClass->setObjectName(QStringLiteral("HippoPrinterClass"));
        HippoPrinterClass->resize(600, 400);
        menuBar = new QMenuBar(HippoPrinterClass);
        menuBar->setObjectName(QStringLiteral("menuBar"));
        HippoPrinterClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(HippoPrinterClass);
        mainToolBar->setObjectName(QStringLiteral("mainToolBar"));
        HippoPrinterClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(HippoPrinterClass);
        centralWidget->setObjectName(QStringLiteral("centralWidget"));
        HippoPrinterClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(HippoPrinterClass);
        statusBar->setObjectName(QStringLiteral("statusBar"));
        HippoPrinterClass->setStatusBar(statusBar);

        retranslateUi(HippoPrinterClass);

        QMetaObject::connectSlotsByName(HippoPrinterClass);
    } // setupUi

    void retranslateUi(QMainWindow *HippoPrinterClass)
    {
        HippoPrinterClass->setWindowTitle(QApplication::translate("HippoPrinterClass", "HippoPrinter", 0));
    } // retranslateUi

};

namespace Ui {
    class HippoPrinterClass: public Ui_HippoPrinterClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HIPPOPRINTER_H
