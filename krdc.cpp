/***************************************************************************
                          krdc.cpp  -  main window
                             -------------------
    begin                : Tue May 13 23:07:42 CET 2002
    copyright            : (C) 2002 by Tim Jansen
    email                : tim@tjansen.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "newconnectiondialog.h"
#include "toolbar.h"
#include "fullscreentoolbar.h"
#include "krdc.h"
#include "vidmode.h"
#include <kdebug.h>
#include <kapplication.h>
#include <kcombobox.h>
#include <qevent.h>
#include <qsizepolicy.h>
#include <qcolor.h>
#include <kconfig.h>
#include <klocale.h>
#include <qlabel.h> 
#include <qtoolbutton.h> 

#include <kmessagebox.h>
#include <kglobal.h>
#include <kinstance.h>
#include <kstandarddirs.h>
#include <qcursor.h>
#include <qbitmap.h>

#define BUMP_SCROLL_CONSTANT (200)

QScrollView2::QScrollView2(QWidget *w, const char *name) :
	QScrollView(w, name) {
}

void QScrollView2::mouseMoveEvent( QMouseEvent *e )
{
    e->ignore();
}

QString KRDC::m_lastHost = "";
int KRDC::m_lastQuality = 0;

KRDC::KRDC(WindowMode wm, const QString &host, 
	   Quality q, const QString &encodings) : 
  QWidget(0, 0),
  m_layout(0),
  m_scrollView(0),
  m_progressDialog(0),
  m_view(0),
  m_fsToolbar(0),
  m_toolbar(0),
  m_ftAutoHide(false),
  m_showProgress(false),
  m_host(host),
  m_quality(q),
  m_encodings(encodings),
  m_isFullscreen(wm),
  m_oldResolution(0),
  m_fullscreenMinimized(false),
  m_wasScaling(false)
{
	connect(&m_autoHideTimer, SIGNAL(timeout()), SLOT(hideFullscreenToolbarNow()));
	connect(&m_bumpScrollTimer, SIGNAL(timeout()), SLOT(bumpScroll()));
}

bool KRDC::start(bool onlyFailOnCancel)
{
	KConfig *config = KApplication::kApplication()->config();
	QString vncServerHost;
	int vncServerPort = 5900;

	if (!m_host.isNull()) {
		parseHost(m_host, vncServerHost, vncServerPort);
		if (m_quality == QUALITY_UNKNOWN)
			m_quality = QUALITY_MEDIUM;
	} else {
		NewConnectionDialog ncd(0, 0, true);
		QStringList list = config->readListEntry("serverCompletions");
		ncd.serverInput->completionObject()->setItems(list);
		list = config->readListEntry("serverHistory");
		ncd.serverInput->setHistoryItems(list);
		ncd.serverInput->setEditText(m_lastHost);
		ncd.qualityCombo->setCurrentItem(m_lastQuality);

		if ((ncd.exec() == QDialog::Rejected) ||
		    (ncd.serverInput->currentText().length() == 0)) {
			return false;
		}

		QString host = ncd.serverInput->currentText();
		m_lastHost = host;
		parseHost(host, vncServerHost, vncServerPort);
		m_host = host;

		int ci = ncd.qualityCombo->currentItem();
		m_lastQuality = ci;
		if (ci == 0) 
			m_quality = QUALITY_HIGH;
		else if (ci == 1)
			m_quality = QUALITY_MEDIUM;
		else if (ci == 2)
			m_quality = QUALITY_LOW;
		else {
			kdDebug() << "Unknown quality";
			return false;
		}

		ncd.serverInput->addToHistory(host);
		list = ncd.serverInput->completionObject()->items();
		config->writeEntry("serverCompletions", list);
		list = ncd.serverInput->historyItems();
		config->writeEntry("serverHistory", list);
	}

	setCaption(i18n("%1 - Remote Desktop Connection").arg(m_host));
	configureApp(m_quality);

	m_view = new KVncView(this, 0, vncServerHost, vncServerPort,
			      &m_appData);
	connect(m_view, SIGNAL(changeSize(int,int)), SLOT(setSize(int,int)));
	connect(m_view, SIGNAL(connected()), SLOT(show()));
	connect(m_view, SIGNAL(disconnected()), SIGNAL(disconnected()));
	// note that the disconnectedError() will be disconnected when kvncview
	// is completely initialized
	connect(m_view, SIGNAL(disconnectedError()), SIGNAL(disconnectedError()));
	connect(m_view, SIGNAL(statusChanged(RemoteViewStatus)), 
		SLOT(changeProgress(RemoteViewStatus)));
	connect(m_view, SIGNAL(showingPasswordDialog(bool)), 
		SLOT(showingPasswordDialog(bool)));

	changeProgress(REMOTE_VIEW_CONNECTING);
	if ((!m_view->start()) && (!m_host.isNull()))
		return onlyFailOnCancel;
	return true;
}

void KRDC::changeProgress(RemoteViewStatus s) {
	if (!m_progressDialog) {
		m_progressDialog = new KProgressDialog(0, 0, QString::null, 
				   "1234567890", false);
		m_progressDialog->showCancelButton(true);
		m_progressDialog->setMinimumDuration(0x7fffffff);//disable effectively
		m_progress = m_progressDialog->progressBar();
		m_progress->setTextEnabled(false);
		m_progress->setTotalSteps(3);
		connect(m_progressDialog, SIGNAL(cancelClicked()), 
			SIGNAL(disconnectedError()));
	}

	if (s == REMOTE_VIEW_CONNECTING) {
		m_progress->setValue(0);
		m_progressDialog->setLabel(i18n("Establishing connection..."));
		showProgressDialog();
	}
	else if (s == REMOTE_VIEW_AUTHENTICATING) {
		m_progress->setValue(1);
		m_progressDialog->setLabel(i18n("Authenticating..."));	
	}
	else if (s == REMOTE_VIEW_PREPARING) {
		m_progress->setValue(2);
		m_progressDialog->setLabel(i18n("Preparing desktop..."));	
	}
	else if ((s == REMOTE_VIEW_CONNECTED) || 
		 (s == REMOTE_VIEW_DISCONNECTED)) {
		m_progress->setValue(3);
		hideProgressDialog();
		if (s == REMOTE_VIEW_CONNECTED) {
			QObject::disconnect(m_view, SIGNAL(disconnectedError()), 
					    this, SIGNAL(disconnectedError()));
			connect(m_view, SIGNAL(disconnectedError()), 
				SIGNAL(disconnected()));
		}
	}
}

void KRDC::showingPasswordDialog(bool b) {
	if (!m_progressDialog)
		return;
	if (b)
		hideProgressDialog();
	else
		showProgressDialog();
}

void KRDC::showProgressDialog() {
	m_showProgress = true;
	QTimer::singleShot(400, this, SLOT(showProgressTimeout()));
}

void KRDC::hideProgressDialog() {
	m_showProgress = false;
	m_progressDialog->hide();
}

void KRDC::showProgressTimeout() {
	if (!m_showProgress) 
		return;

	m_progressDialog->setMinimumSize(300, 50);
	m_progressDialog->show();
}

void KRDC::quit() {
	hide();
	vidmodeNormalSwitch(qt_xdisplay(), m_oldResolution);
	if (m_view)
		m_view->startQuitting();
	emit disconnected();
}

void KRDC::configureApp(Quality q) {
	m_appData.shareDesktop = 1;
	m_appData.viewOnly = 0;

	if (q == QUALITY_LOW) {
		m_appData.useBGR233 = 1;
		m_appData.encodingsString = "copyrect tight zlib hextile raw";
		m_appData.compressLevel = -1;
		m_appData.qualityLevel = 1;
	}
	else if ((q == QUALITY_MEDIUM) || (q == QUALITY_UNKNOWN)) {
		m_appData.useBGR233 = 0;
		m_appData.encodingsString = "copyrect tight zlib hextile raw";
		m_appData.compressLevel = -1;
		m_appData.qualityLevel = 6;
	}
	else if (q == QUALITY_HIGH) {
		m_appData.useBGR233 = 0;
		m_appData.encodingsString = "copyrect hextile raw";
		m_appData.compressLevel = -1;
		m_appData.qualityLevel = 9;
	}

	if (!m_encodings.isNull())
		m_appData.encodingsString = m_encodings.latin1();

	m_appData.nColours = 256;
	m_appData.useSharedColours = 1;
	m_appData.requestedDepth = 0;

	m_appData.rawDelay = 0;
	m_appData.copyRectDelay = 0;
}

void KRDC::parseHost(const QString &str, QString &serverHost, int &serverPort) {
	QString host;
	QString s = str;
	
	if (s.startsWith("vnc:"))
		s = s.mid(4);
	if (s.startsWith("//"))
		s = s.mid(2);

	int pos = s.find(':');
	if (pos < 0) {
		s+= ":0";
		pos = s.find(':');
	}

	bool portOk = false;
	QString portS = s.mid(pos+1);
	int port = portS.toInt(&portOk);
	if (portOk) {
		host = s.left(pos);
		if (port < 100)
			serverPort = port + 5900;
		else
			serverPort = port;
	}
	else {
		host = s;
		serverPort = 5900;
	}
	
	serverHost = host;
}

KRDC::~KRDC()
{
	if (m_progressDialog) 
		delete m_progressDialog;

	// kill explicitly to avoid xlib calls by the threads after closing the window!
	if (m_view)
		delete m_view;
}

void KRDC::enableFullscreen(bool on) 
{
	if (on) {
		if (m_isFullscreen != WINDOW_MODE_FULLSCREEN)
			switchToFullscreen();
	}
	else
		if (m_isFullscreen != WINDOW_MODE_NORMAL)
			switchToNormal(m_wasScaling);
}

void KRDC::switchToFullscreen() 
{
	int x, y;

	if (m_isFullscreen != WINDOW_MODE_AUTO)
		m_oldWindowGeometry = geometry();

	m_wasScaling = m_view->scaling();
	m_view->enableScaling(false);
	hide();
	m_oldResolution = vidmodeFullscreenSwitch(qt_xdisplay(), 
						  m_view->width(),
						  m_view->height(),
						  x, y);
	if (m_oldResolution != 0)
		m_fullscreenResolution = QSize(x, y);
	else
		m_fullscreenResolution = QApplication::desktop()->size();
	m_isFullscreen = WINDOW_MODE_FULLSCREEN;

	if (m_toolbar) {
		delete m_toolbar;
		m_toolbar = 0;
	}
	if (!m_scrollView) {
		m_scrollView = new QScrollView2(this, "remote scrollview");
		m_scrollView->setFrameStyle(QFrame::NoFrame);
		m_view->reparent(m_scrollView->viewport(), 
				 getWFlags() & ~WType_Mask, QPoint());
		m_scrollView->addChild(m_view);
	}

	if (m_layout)
		delete m_layout;
	m_layout = new QVBoxLayout(this);
	m_layout->addWidget(m_scrollView);

	m_fsToolbar = new KFullscreenPanel(this, 0, m_fullscreenResolution);
	FullscreenToolbar *t = new FullscreenToolbar(m_fsToolbar);
	t->hostLabel->setText(m_host);
	t->fullscreenButton->setOn(true);
	m_fsToolbar->setChild(t);
	connect(m_fsToolbar, SIGNAL(mouseEnter()), SLOT(showFullscreenToolbar()));
	connect(m_fsToolbar, SIGNAL(mouseLeave()), SLOT(hideFullscreenToolbarDelayed()));
	connect((QObject*)t->closeButton, SIGNAL(clicked()), SLOT(quit()));
	connect((QObject*)t->iconifyButton, SIGNAL(clicked()), SLOT(iconify()));
	connect((QObject*)t->fullscreenButton, SIGNAL(toggled(bool)), 
		SLOT(enableFullscreen(bool)));
	connect((QObject*)t->autohideButton, SIGNAL(toggled(bool)), 
		SLOT(setFsToolbarAutoHide(bool)));
	showFullScreen();

	repositionView(true);

	setMaximumSize(m_fullscreenResolution.width(), 
		       m_fullscreenResolution.height());
	setGeometry(0, 0, m_fullscreenResolution.width(), 
		    m_fullscreenResolution.height());

	if (m_oldResolution)
		grabInput(qt_xdisplay(), winId());
	setMouseTracking(m_ftAutoHide || 
			 (m_view->width()>m_scrollView->width()) || 
			 (m_view->height()>m_scrollView->height()));
}

void KRDC::switchToNormal(bool scaling)
{
	QRect oldGeometry;
	if (m_isFullscreen == WINDOW_MODE_NORMAL)
	    oldGeometry = frameGeometry();
	hide();
	m_isFullscreen = WINDOW_MODE_NORMAL;
	m_view->enableScaling(scaling);

	if (m_oldResolution) {
		ungrabInput(qt_xdisplay());
		vidmodeNormalSwitch(qt_xdisplay(), m_oldResolution);
		m_oldResolution = 0;
	}

	if (m_fsToolbar) {
		delete m_fsToolbar; 
		m_fsToolbar = 0;
	}

	if (!m_toolbar) {
		Toolbar *t = new Toolbar(this);
		m_toolbar = t;
		t->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, 
					     QSizePolicy::Fixed));
		t->fullscreenButton->setOn(false);
		t->scaleButton->setOn(m_wasScaling);
		connect((QObject*)t->fullscreenButton, SIGNAL(toggled(bool)), 
			SLOT(enableFullscreen(bool)));
		connect((QObject*)t->scaleButton, SIGNAL(toggled(bool)), 
			SLOT(switchToNormal(bool)));
	}

	if (scaling) {
		if (m_scrollView) {
			m_view->reparent(this, 
					 getWFlags() & ~WType_Mask, QPoint());
			delete m_scrollView;
			m_scrollView = 0;
		}
	}
	else {
		if (!m_scrollView) {
			m_scrollView = new QScrollView2(this, "Remote ScrollView");
			m_scrollView->setFrameStyle(QFrame::NoFrame);
			m_view->reparent(m_scrollView->viewport(), 
					 getWFlags() & ~WType_Mask, QPoint());
			m_scrollView->addChild(m_view);
		}
	}

	if (m_layout)
		delete m_layout;
	m_layout = new QVBoxLayout(this);
	m_layout->addWidget(m_toolbar);
	if (m_scrollView)
		m_layout->addWidget(m_scrollView);
	else
		m_layout->addWidget(m_view);
	
	QSize fbSize = m_view->framebufferSize();
	setMaximumSize(fbSize.width(), fbSize.height() + m_toolbar->height());
	if (m_wasScaling)
		m_view->enableScaling(true);

	repositionView(false);

	m_layout->activate();

	showNormal();
	    
	if (!oldGeometry.isNull())
		setGeometry(oldGeometry);

	setMouseTracking(false);
}

void KRDC::iconify()
{
	ungrabInput(qt_xdisplay());
	vidmodeNormalSwitch(qt_xdisplay(), m_oldResolution);
	m_oldResolution = 0;
	showNormal();
	showMinimized();
	m_fullscreenMinimized = true;
}

bool KRDC::event(QEvent *e) {
	if ((!m_fullscreenMinimized) || (e->type() != QEvent::Show)) 
		return QWidget::event(e);

	m_fullscreenMinimized = false;
	int x, y;
	m_oldResolution = vidmodeFullscreenSwitch(qt_xdisplay(), 
						  m_view->width(),
						  m_view->height(),
						  x, y);
	if (m_oldResolution != 0)
		m_fullscreenResolution = QSize(x, y);
	else
		m_fullscreenResolution = QApplication::desktop()->size();

	showFullScreen();
	setGeometry(0, 0, m_fullscreenResolution.width(), 
		    m_fullscreenResolution.height());
	grabInput(qt_xdisplay(), winId());
	return QWidget::event(e);
}

void KRDC::setSize(int w, int h)
{
	int dw, dh;

	QWidget *desktop = QApplication::desktop();
	dw = desktop->width();
	dh = desktop->height();

	switch (m_isFullscreen) {
	case WINDOW_MODE_AUTO:
		if ((w >= dw) || (h >= dh)) {
			switchToFullscreen();
		}
		else {
			switchToNormal(false);
		}
		break;
	case WINDOW_MODE_NORMAL:
		switchToNormal(false);
		break;
	case WINDOW_MODE_FULLSCREEN:
		switchToFullscreen();
 	        break;
	}
}

void KRDC::repositionView(bool fullscreen) {
	int ox = 0;
	int oy = 0;
	
	if (!m_scrollView)
		return;

	QSize s = m_view->size();

	if (fullscreen) {
		QSize d = m_fullscreenResolution;
		bool margin = false;
		if (d.width() > s.width())
			ox = (d.width() - s.width()) / 2;
		else if (d.width() < s.width()) 
			margin = true;

		if (d.height() > s.height())
			oy = (d.height() - s.height()) / 2;
		else if (d.height() < s.height())
			margin = true;

		if (margin) 
			m_layout->setMargin(1);
	}

	m_scrollView->moveChild(m_view, ox, oy);
}

void KRDC::setFsToolbarAutoHide(bool on) {

	if (on == m_ftAutoHide)
		return;
	if (m_isFullscreen != WINDOW_MODE_FULLSCREEN)
		return;

	m_ftAutoHide = on;

	setMouseTracking(m_ftAutoHide || 
			 (m_view->width()>m_scrollView->width()) || 
			 (m_view->height()>m_scrollView->height()));
	
	if (!on) 
		showFullscreenToolbar();
}

void KRDC::hideFullscreenToolbarNow() {
	if (m_fsToolbar && m_ftAutoHide) 
		m_fsToolbar->startHide();
}

void KRDC::bumpScroll() {
	int x = QCursor::pos().x();
	int y = QCursor::pos().y();
	QSize s = m_view->size();
	QSize d = m_fullscreenResolution;

	if (d.width() < s.width()) {
		if (x == 0)
			m_scrollView->scrollBy(-BUMP_SCROLL_CONSTANT, 0);
		else if (x >= d.width()-1)
			m_scrollView->scrollBy(BUMP_SCROLL_CONSTANT, 0);
	}
	if (d.height() < s.height()) {
		if (y == 0)
			m_scrollView->scrollBy(0, -BUMP_SCROLL_CONSTANT);
		else if (y >= d.height()-1)
			m_scrollView->scrollBy(0, BUMP_SCROLL_CONSTANT);
	}

	m_bumpScrollTimer.start(333, true);
}

void KRDC::showFullscreenToolbar() {
	m_fsToolbar->startShow();
	m_autoHideTimer.stop();
}

void KRDC::hideFullscreenToolbarDelayed() {
	if (!m_autoHideTimer.isActive())
	  m_autoHideTimer.start(TOOLBAR_AUTOHIDE_TIMEOUT, true);
}

void KRDC::mouseMoveEvent(QMouseEvent *e) {
if (!e->state())
kdDebug() << "pure mouse move! "<<e->state()<<endl;

	if (m_isFullscreen != WINDOW_MODE_FULLSCREEN)
		return;

	int x = e->x();
	int y = e->y();

	/* Bump Scrolling */

	QSize s = m_view->size();
	QSize d = m_fullscreenResolution;
	if ((d.width() < s.width()) || d.height() < s.height()) {
 		if ((x == 0) || (x >= d.width()-1) ||
		    (y == 0) || (y >= d.height()-1))
			bumpScroll();
		else
			m_bumpScrollTimer.stop();
	}

	/* Toolbar autohide */

	if (!m_ftAutoHide)
		return;

	int dw = d.width();
	int w = m_fsToolbar->width();
	int x1 = (dw - w)/4;
	int x2 = dw - x1;

	if (((y <= 0) && (x >= x1) && (x <= x2)))
		showFullscreenToolbar();
	else 
		hideFullscreenToolbarDelayed();
	e->accept();
}

#include "krdc.moc"
