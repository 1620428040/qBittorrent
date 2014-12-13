/*
 * Bittorrent Client using Qt4 and libtorrent.
 * Copyright (C) 2006  Christophe Dumez
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 *
 * Contact : chris@qbittorrent.org
 */

#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QLibraryInfo>
#include <QDebug>

#ifndef DISABLE_GUI
#if defined(QBT_STATIC_QT)
#include <QtPlugin>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
Q_IMPORT_PLUGIN(QICOPlugin)
#else
Q_IMPORT_PLUGIN(qico)
#endif
#endif
#include <QMessageBox>
#include <QStyleFactory>
#include <QStyle>
#include <QSplashScreen>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QPushButton>
#include <QTimer>
#include "sessionapplication.h"
#include "mainwindow.h"
#include "ico.h"
#else
#include "qtsinglecoreapplication.h"
#include <iostream>
#include <stdio.h>
#include "headlessloader.h"
#endif

#include "preferences.h"
#if defined(Q_OS_UNIX)
#include <signal.h>
#include <execinfo.h>
#include "stacktrace.h"
#endif

#ifdef STACKTRACE_WIN
#include <signal.h>
#include "stacktrace_win.h"
#include "stacktrace_win_dlg.h"
#endif

#include <stdlib.h>
#include "misc.h"
#include "preferences.h"

#if defined(Q_OS_WIN) && !defined(QBT_HAS_GETCURRENTPID)
#error You seem to have updated QtSingleApplication without porting our custom QtSingleApplication::getRunningPid() function. Please see previous version to understate how it works.
#endif

void displayUsage(char* prg_name)
{
    std::cout << qPrintable(QObject::tr("Usage:")) << std::endl;
    std::cout << '\t' << prg_name << " --version: " << qPrintable(QObject::tr("displays program version")) << std::endl;
#ifndef DISABLE_GUI
    std::cout << '\t' << prg_name << " --no-splash: " << qPrintable(QObject::tr("disable splash screen")) << std::endl;
#else
    std::cout << '\t' << prg_name << " -d | --daemon: " << qPrintable(QObject::tr("run in daemon-mode (background)")) << std::endl;
#endif
    std::cout << '\t' << prg_name << " --help: " << qPrintable(QObject::tr("displays this help message")) << std::endl;
    std::cout << '\t' << prg_name << " --webui-port=x: " << qPrintable(QObject::tr("changes the webui port (current: %1)").arg(QString::number(Preferences::instance()->getWebUiPort()))) << std::endl;
    std::cout << '\t' << prg_name << " " << qPrintable(QObject::tr("[files or urls]: downloads the torrents passed by the user (optional)")) << std::endl;
}

bool userAgreesWithLegalNotice()
{
    Preferences* const pref = Preferences::instance();
    if (pref->getAcceptedLegal()) // Already accepted once
        return true;
#ifdef DISABLE_GUI
    std::cout << std::endl << "*** " << qPrintable(QObject::tr("Legal Notice")) << " ***" << std::endl;
    std::cout << qPrintable(QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued.")) << std::endl << std::endl;
    std::cout << qPrintable(QObject::tr("Press %1 key to accept and continue...").arg("'y'")) << std::endl;
    char ret = getchar(); // Read pressed key
    if (ret == 'y' || ret == 'Y') {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
    return false;
#else
    QMessageBox msgBox;
    msgBox.setText(QObject::tr("qBittorrent is a file sharing program. When you run a torrent, its data will be made available to others by means of upload. Any content you share is your sole responsibility.\n\nNo further notices will be issued."));
    msgBox.setWindowTitle(QObject::tr("Legal notice"));
    msgBox.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
    QAbstractButton *agree_button = msgBox.addButton(QObject::tr("I Agree"), QMessageBox::AcceptRole);
    msgBox.show(); // Need to be shown or to moveToCenter does not work
    msgBox.move(misc::screenCenter(&msgBox));
    msgBox.exec();
    if (msgBox.clickedButton() == agree_button) {
        // Save the answer
        pref->setAcceptedLegal(true);
        return true;
    }
    return false;
#endif
}

#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
void sigintHandler(int) {
  signal(SIGINT, 0);
  qDebug("Catching SIGINT, exiting cleanly");
  qApp->exit();
}

void sigtermHandler(int) {
  signal(SIGTERM, 0);
  qDebug("Catching SIGTERM, exiting cleanly");
  qApp->exit();
}
void sigsegvHandler(int) {
  signal(SIGABRT, 0);
  signal(SIGSEGV, 0);
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
  std::cerr << "\n\n*************************************************************\n";
  std::cerr << "Catching SIGSEGV, please report a bug at http://bug.qbittorrent.org\nand provide the following backtrace:\n";
  std::cerr << "qBittorrent version: " << VERSION << std::endl;
  print_stacktrace();
#else
#ifdef STACKTRACE_WIN
  StraceDlg dlg;
  dlg.setStacktraceString(straceWin::getBacktrace());
  dlg.exec();
#endif
#endif
  raise(SIGSEGV);
}
void sigabrtHandler(int) {
  signal(SIGABRT, 0);
  signal(SIGSEGV, 0);
#if !defined Q_OS_WIN && !defined Q_OS_HAIKU
  std::cerr << "\n\n*************************************************************\n";
  std::cerr << "Catching SIGABRT, please report a bug at http://bug.qbittorrent.org\nand provide the following backtrace:\n";
  std::cerr << "qBittorrent version: " << VERSION << std::endl;
  print_stacktrace();
#else
#ifdef STACKTRACE_WIN
  StraceDlg dlg;
  dlg.setStacktraceString(straceWin::getBacktrace());
  dlg.exec();
#endif
#endif
  raise(SIGABRT);
}
#endif

// Main
int main(int argc, char *argv[]) {
#if defined(Q_OS_MACX) && !defined(DISABLE_GUI)
  if ( QSysInfo::MacintoshVersion > QSysInfo::MV_10_8 )
  {
    // fix Mac OS X 10.9 (mavericks) font issue
    // https://bugreports.qt-project.org/browse/QTBUG-32789
    QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
  }
#endif
  // Create Application
  QString uid = misc::getUserIDString();
#ifdef DISABLE_GUI
  bool shouldDaemonize = false;
  for(int i=1; i<argc; i++) {
    if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--daemon") == 0) {
      shouldDaemonize = true;
      argc--;
      for(int j=i; j<argc; j++) {
        argv[j] = argv[j+1];
      }
      i--;
    }
  }
  QtSingleCoreApplication app("qBittorrent-"+uid, argc, argv);
#else
  SessionApplication app("qBittorrent-"+uid, argc, argv);
#endif

  // Check if qBittorrent is already running for this user
  if (app.isRunning()) {
    qDebug("qBittorrent is already running for this user.");
    // Read torrents given on command line
#ifdef Q_OS_WIN
    DWORD pid = (DWORD)app.getRunningPid();
    if (pid > 0) {
      BOOL b = AllowSetForegroundWindow(pid);
      qDebug("AllowSetForegroundWindow() returns %s", b ? "TRUE" : "FALSE");
    }
#endif
    QStringList torrentCmdLine = app.arguments();
    //Pass program parameters if any
    QString message;
    QFileInfo torrentPath;
    for (int a = 1; a < torrentCmdLine.size(); ++a) {
      if (torrentCmdLine[a].startsWith("--")) continue;
      torrentPath.setFile(torrentCmdLine[a]);
      if (torrentPath.exists())
        message += torrentPath.absoluteFilePath();
      else
        message += torrentCmdLine[a];
      if (a < argc-1)
        message += "|";
    }
    if (!message.isEmpty()) {
      qDebug("Passing program parameters to running instance...");
      qDebug("Message: %s", qPrintable(message));
      app.sendMessage(message);
    } else { // Raise main window
      app.sendMessage("qbt://show");
    }
    return 0;
  }

  srand(time(0));
  Preferences* const pref = Preferences::instance();
#ifndef DISABLE_GUI
  bool no_splash = false;
#else
  if(shouldDaemonize && daemon(1, 0) != 0) {
    qCritical("Something went wrong while daemonizing, exiting...");
    return EXIT_FAILURE;
  }
#endif

  // Load translation
  QString locale = pref->getLocale();
  QTranslator qtTranslator;
  QTranslator translator;
  if (locale.isEmpty()) {
    locale = QLocale::system().name();
    pref->setLocale(locale);
  }
  if (qtTranslator.load(
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
          QString::fromUtf8("qtbase_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)) ||
      qtTranslator.load(
#endif
          QString::fromUtf8("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
    qDebug("Qt %s locale recognized, using translation.", qPrintable(locale));
  }else{
    qDebug("Qt %s locale unrecognized, using default (en).", qPrintable(locale));
  }
  app.installTranslator(&qtTranslator);
  if (translator.load(QString::fromUtf8(":/lang/qbittorrent_") + locale)) {
    qDebug("%s locale recognized, using translation.", qPrintable(locale));
  }else{
    qDebug("%s locale unrecognized, using default (en).", qPrintable(locale));
  }
  app.installTranslator(&translator);
#ifndef DISABLE_GUI
  if (locale.startsWith("ar") || locale.startsWith("he")) {
    qDebug("Right to Left mode");
    app.setLayoutDirection(Qt::RightToLeft);
  } else {
    app.setLayoutDirection(Qt::LeftToRight);
  }
#endif
  app.setApplicationName(QString::fromUtf8("qBittorrent"));

  // Check for executable parameters
  if (argc > 1) {
    if (QString::fromLocal8Bit(argv[1]) == QString::fromUtf8("--version")) {
      std::cout << "qBittorrent " << VERSION << '\n';
      return 0;
    }
    if (QString::fromLocal8Bit(argv[1]) == QString::fromUtf8("--help")) {
      displayUsage(argv[0]);
      return 0;
    }

    for (int i=1; i<argc; ++i) {
#ifndef DISABLE_GUI
      if (QString::fromLocal8Bit(argv[i]) == QString::fromUtf8("--no-splash")) {
        no_splash = true;
      } else {
#endif
        if (QString::fromLocal8Bit(argv[i]).startsWith("--webui-port=")) {
          QStringList parts = QString::fromLocal8Bit(argv[i]).split("=");
          if (parts.size() == 2) {
            bool ok = false;
            int new_port = parts.last().toInt(&ok);
            if (ok && new_port > 0 && new_port <= 65535) {
              Preferences::instance()->setWebUiPort(new_port);
            }
          }
        }
#ifndef DISABLE_GUI
      }
#endif
    }
  }

#ifndef DISABLE_GUI
  if (pref->isSlashScreenDisabled()) {
    no_splash = true;
  }
  QSplashScreen *splash = 0;
  if (!no_splash) {
    QPixmap splash_img(":/Icons/skin/splash.png");
    QPainter painter(&splash_img);
    QString version = VERSION;
    painter.setPen(QPen(Qt::white));
    painter.setFont(QFont("Arial", 22, QFont::Black));
    painter.drawText(224 - painter.fontMetrics().width(version), 270, version);
    splash = new QSplashScreen(splash_img, Qt::WindowStaysOnTopHint);
    QTimer::singleShot(1500, splash, SLOT(deleteLater()));
    splash->show();
    app.processEvents();
  }
#endif
  // Set environment variable
  if (!qputenv("QBITTORRENT", QByteArray(VERSION))) {
    std::cerr << "Couldn't set environment variable...\n";
  }

#ifndef DISABLE_GUI
  app.setStyleSheet("QStatusBar::item { border-width: 0; }");
#endif

  if (!userAgreesWithLegalNotice()) {
    return 0;
  }
#ifndef DISABLE_GUI
  app.setQuitOnLastWindowClosed(false);
#endif
#if defined(Q_OS_UNIX) || defined(STACKTRACE_WIN)
  signal(SIGABRT, sigabrtHandler);
  signal(SIGTERM, sigtermHandler);
  signal(SIGINT, sigintHandler);
  signal(SIGSEGV, sigsegvHandler);
#endif
  // Read torrents given on command line
  QStringList torrents;
  QStringList appArguments = app.arguments();
  for (int i = 1; i < appArguments.size(); ++i) {
    if (!appArguments[i].startsWith("--")) {
      qDebug() << "Command line argument:" << appArguments[i];
      torrents << appArguments[i];
    }
  }

#ifndef DISABLE_GUI
  MainWindow window(0, torrents);
  QObject::connect(&app, SIGNAL(messageReceived(const QString&)),
                   &window, SLOT(processParams(const QString&)));
  app.setActivationWindow(&window);
#ifdef Q_OS_MAC
  static_cast<QMacApplication*>(&app)->setReadyToProcessEvents();
#endif // Q_OS_MAC
#else
  // Load Headless class
  HeadlessLoader loader(torrents);
  QObject::connect(&app, SIGNAL(messageReceived(const QString&)),
                   &loader, SLOT(processParams(const QString&)));
#endif

  int ret = app.exec();
  qDebug("Application has exited");
  return ret;
}


