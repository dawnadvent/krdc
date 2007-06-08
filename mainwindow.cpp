/****************************************************************************
**
** Copyright (C) 2007 Urs Wolfer <uwolfer @ kde.org>
**
** This file is part of KDE.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; see the file COPYING. If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
****************************************************************************/

#include "mainwindow.h"

#include "floatingtoolbar.h"
#include "rdpview.h"
#include "vncview.h"

#include <KAction>
#include <KActionCollection>
#include <KApplication>
#include <KEditToolBar>
#include <KHistoryComboBox>
#include <KIcon>
#include <KLocale>
#include <KNotifyConfigWidget>
#include <KMenuBar>
#include <KShortcut>
#include <KShortcutsDialog>
#include <KStatusBar>
#include <KTabWidget>
#include <KToggleAction>

#include <QClipboard>
#include <QLabel>
#include <QLayout>
#include <QX11EmbedContainer>
#include <QTimer>
#include <QToolButton>
#include <QScrollArea>

MainWindow::MainWindow(QWidget *parent)
  : KXmlGuiWindow(parent),
    m_fullscreenWindow(0),
    m_toolBar(0)
{
    setupActions();

    createGUI("krdcui.rc");

    setStandardToolBarMenuEnabled(true);

    m_tabWidget = new KTabWidget(this);
    m_tabWidget->setMinimumSize(500, 400);
    setCentralWidget(m_tabWidget);

    statusBar()->showMessage(i18n("KRDC started"));
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupActions()
{
    QAction *newDownloadAction = actionCollection()->addAction("new_connection");
    newDownloadAction->setText(i18n("&New Connection..."));
    newDownloadAction->setIcon(KIcon("document-new"));
    newDownloadAction->setShortcuts(KShortcut("Ctrl+N"));
    connect(newDownloadAction, SIGNAL(triggered()), SLOT(slotNewConnection()));

    QAction *screenshotAction = actionCollection()->addAction("take_screenshot");
    screenshotAction->setText(i18n("&Copy Screenshot to Clipboard"));
    screenshotAction->setIcon(KIcon("ksnapshot"));
    connect(screenshotAction, SIGNAL(triggered()), SLOT(slotTakeScreenshot()));

    QAction *fullscreenAction = actionCollection()->addAction("switch_fullscreen");
    fullscreenAction->setText(i18n("&Switch to Fullscreen Mode"));
    fullscreenAction->setIcon(KIcon("view-fullscreen"));
    connect(fullscreenAction, SIGNAL(triggered()), SLOT(slotSwitchFullscreen()));

    QAction *logoutAction = actionCollection()->addAction("logout");
    logoutAction->setText(i18n("&Log Out"));
    logoutAction->setIcon(KIcon("system-log-out"));
    connect(logoutAction, SIGNAL(triggered()), SLOT(slotLogout()));

    QAction *quitAction = KStandardAction::quit(this, SLOT(slotQuit()), actionCollection());
    actionCollection()->addAction("quit", quitAction);
    QAction *preferencesAction = KStandardAction::preferences(this, SLOT(slotPreferences()), actionCollection());
    actionCollection()->addAction("preferences", preferencesAction);
    QAction *configToolbarAction = KStandardAction::configureToolbars(this, SLOT(slotConfigureToolbars()), actionCollection());
    actionCollection()->addAction("configure_toolbars", configToolbarAction);
    QAction *keyBindingsAction = KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), actionCollection());
    actionCollection()->addAction("configure_keys", keyBindingsAction);
    QAction *cinfigNotifyAction = KStandardAction::configureNotifications(this, SLOT(slotConfigureNotifications()), actionCollection());
    actionCollection()->addAction("configure_notifications", cinfigNotifyAction);
    m_menubarAction = KStandardAction::showMenubar(this, SLOT(slotShowMenubar()), actionCollection());
    m_menubarAction->setChecked(!menuBar()->isHidden());
    actionCollection()->addAction("settings_showmenubar", m_menubarAction);

    m_addressComboBox = new KHistoryComboBox(this);

    connect(m_addressComboBox, SIGNAL(returnPressed()), this, SLOT(slotNewConnection()));

    QLabel *addressLabel = new QLabel(i18n("Remote desktop:"), this);

    QWidget *addressWidget = new QWidget(this);
    QHBoxLayout *addressLayout = new QHBoxLayout(addressWidget);
    addressLayout->setMargin(0);
    addressLayout->addWidget(addressLabel);
    addressLayout->addWidget(m_addressComboBox, 1);

    KAction *addressLineAction = new KAction(i18n("Address"), this);
    actionCollection()->addAction("address_line", addressLineAction);
    addressLineAction->setDefaultWidget(addressWidget);

    QAction *gotoAction = actionCollection()->addAction("goto_address");
    gotoAction->setText(i18n("&Goto address"));
    gotoAction->setIcon(KIcon("browser-go"));
    connect(gotoAction, SIGNAL(triggered()), SLOT(slotNewConnection()));

}

void MainWindow::slotNewConnection()
{
    QUrl url = m_addressComboBox->currentText();
    m_addressComboBox->clear();

    if (url.isEmpty())
        return;

    QScrollArea *scrollArea = new QScrollArea(m_tabWidget);
    scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    scrollArea->setBackgroundRole(QPalette::Dark);

    RemoteView *view;

#ifdef BUILDVNC
    if (url.scheme().toLower() == "vnc")
        view = new VncView(scrollArea, url.host(), url.port());
    else
#endif

    if (url.scheme().toLower() == "rdp")
        view = new RdpView(scrollArea, url.host(), url.port());
    else
        return;

    connect(view, SIGNAL(changeSize(int, int)), this, SLOT(resizeTabWidget(int, int)));

    view->resize(0, 0);
    view->start();

    scrollArea->setWidget(view);

    m_tabWidget->addTab(scrollArea, KIcon("krdc"), url.toString());

    statusBar()->showMessage(i18n("Connected to %1 via %2", url.host(), url.scheme().toUpper()));
}

void MainWindow::resizeTabWidget(int w, int h)
{
    kDebug(5010) << "tabwidget resize: w: " << w << ", h: " << h << endl;

    //WORKAROUND: QTabWidget resize problem. Let's see if there is a clean solution for this issue.
    m_tabWidget->setMinimumSize(w + 8, h + 38); // FIXME: do not use hardcoded values
    m_tabWidget->adjustSize();
    QCoreApplication::processEvents();
    m_tabWidget->setMinimumSize(500, 400);
}

void MainWindow::slotTakeScreenshot()
{
    QPixmap snapshot;

    if (!m_fullscreenWindow) {
        QScrollArea *tmp = qobject_cast<QScrollArea *>(m_tabWidget->currentWidget());
        snapshot = QPixmap::grabWidget(tmp->widget());
    } else
        snapshot = QPixmap::grabWidget(m_fullscreenWindowView);

    QApplication::clipboard()->setPixmap(snapshot);
}

void MainWindow::slotSwitchFullscreen()
{
    kDebug(5010) << "slotSwitchFullscreen" << endl;

    if (m_fullscreenWindow) {
        show();
        restoreGeometry(m_mainWindowGeometry);

        m_fullscreenWindow->setWindowState(0);
        m_fullscreenWindow->hide();

        QScrollArea *scrollArea = new QScrollArea(m_tabWidget);
        scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        scrollArea->setBackgroundRole(QPalette::Dark);
        scrollArea->setWidget(m_fullscreenWindowView);

        int currentTab = m_tabWidget->currentIndex();
        m_tabWidget->insertTab(currentTab, scrollArea, m_tabWidget->tabIcon(currentTab), m_tabWidget->tabText(currentTab));
        m_tabWidget->removeTab(m_tabWidget->currentIndex());
        m_tabWidget->setCurrentIndex(currentTab);

        resizeTabWidget(m_fullscreenWindowView->sizeHint().width(), m_fullscreenWindowView->sizeHint().height());

        if (m_toolBar) {
            m_toolBar->hideAndDestroy();
            m_toolBar = 0;
        }

        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-fullscreen"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Fullscreen Mode"));

        m_fullscreenWindow->deleteLater();

        m_fullscreenWindow = 0;
    } else {
        m_fullscreenWindow = new QWidget(this, Qt::Window);
        m_fullscreenWindow->setWindowTitle(i18n("KRDC Fullscreen"));

        QScrollArea *tmp = qobject_cast<QScrollArea *>(m_tabWidget->currentWidget());

        m_fullscreenWindowView = tmp->widget();

        QScrollArea *scrollArea = new QScrollArea(m_fullscreenWindow);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        scrollArea->setBackgroundRole(QPalette::Dark);
        scrollArea->setWidget(m_fullscreenWindowView);

        QVBoxLayout *fullscreenLayout = new QVBoxLayout();
        fullscreenLayout->setMargin(0);
        fullscreenLayout->addWidget(scrollArea);
        m_fullscreenWindow->setLayout(fullscreenLayout);

        m_fullscreenWindow->show();

        m_fullscreenWindow->setWindowState(Qt::WindowFullScreen);

        // show the toolbar after we have switched to fullscreen mode
        QTimer::singleShot(100, this, SLOT(showRemoteViewToolbar()));

        m_mainWindowGeometry = saveGeometry();
        hide();
    }
}

void MainWindow::slotLogout()
{
    kDebug(5010) << "slotLogout" << endl;

    if (m_fullscreenWindow) { // first close fullscreen view
        slotSwitchFullscreen();
        slotLogout();
    }

    QWidget *tmp = m_tabWidget->currentWidget();

    m_tabWidget->removeTab(m_tabWidget->currentIndex());

    tmp->deleteLater();
}

void MainWindow::showRemoteViewToolbar()
{
    kDebug(5010) << "showRemoteViewToolbar" << endl;

    m_fullscreenWindowView->repaint(); // be sure there are no artifacts on the remote view

    if (!m_toolBar) {
        actionCollection()->action("switch_fullscreen")->setIcon(KIcon("view-restore"));
        actionCollection()->action("switch_fullscreen")->setText(i18n("Switch to Window Mode"));

        m_toolBar = new FloatingToolBar(m_fullscreenWindow, m_fullscreenWindow);
        m_toolBar->setSide(FloatingToolBar::Top);
        m_toolBar->addAction(actionCollection()->action("switch_fullscreen"));
        m_toolBar->addAction(actionCollection()->action("take_screenshot"));
        m_toolBar->addAction(actionCollection()->action("logout"));
    }

    m_toolBar->showAndAnimate();
}

void MainWindow::slotPreferences()
{
}

void MainWindow::slotQuit()
{
    close();
}

void MainWindow::slotConfigureNotifications()
{
    KNotifyConfigWidget::configure(this);
}

void MainWindow::slotConfigureKeys()
{
    KShortcutsDialog::configure(actionCollection());
}

void MainWindow::slotConfigureToolbars()
{
    KEditToolBar edit(actionCollection());
    connect(&edit, SIGNAL(newToolbarConfig()), this, SLOT(slotNewToolbarConfig()));
    edit.exec();
}

void MainWindow::slotShowMenubar()
{
    if (m_menubarAction->isChecked())
        menuBar()->show();
    else
        menuBar()->hide();
}

#include "mainwindow.moc"
