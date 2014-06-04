/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include <qprocesswrapper.h>
#include <qsettingswrapper.h>
#include <remoteclient.h>
#include <remotefileengine.h>
#include <remoteserver.h>

#include <QHostAddress>
#include <QSettings>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTest>
#include <QSignalSpy>

using namespace QInstaller;

class tst_ClientServer : public QObject
{
    Q_OBJECT

private slots:
    void testServerConnection()
    {
        RemoteServer server;
        server.init(39999, QHostAddress::LocalHost, RemoteServer::Debug);
        server.start();

        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, 39999);
        QVERIFY2(socket.waitForConnected(), "Could not connect to server.");
    }

    void testClientConnection()
    {
        RemoteServer server;
        server.init(39999, QHostAddress::LocalHost, RemoteServer::Debug);
        server.start();

        RemoteClient::instance().init(39999, QHostAddress::LocalHost, RemoteClient::Debug);
        QTcpSocket socket;
        QVERIFY2(RemoteClient::instance().connect(&socket), "Socket is NULL, could "
            "not connect to server.");
    }

    void testQSettingsWrapper()
    {
        RemoteServer server;
        server.init(39999, QHostAddress::LocalHost, RemoteServer::Debug);
        server.start();

        QSettingsWrapper wrapper("digia", "clientserver");
        QCOMPARE(wrapper.isConnectedToServer(), false);
        wrapper.clear();
        QCOMPARE(wrapper.isConnectedToServer(), true);
        wrapper.sync();
        wrapper.setFallbacksEnabled(false);

        QSettings settings("digia", "clientserver");
        settings.setFallbacksEnabled(false);

        QCOMPARE(settings.fileName(), wrapper.fileName());
        QCOMPARE(int(settings.format()), int(wrapper.format()));
        QCOMPARE(int(settings.scope()), int(wrapper.scope()));
        QCOMPARE(settings.organizationName(), wrapper.organizationName());
        QCOMPARE(settings.applicationName(), wrapper.applicationName());
        QCOMPARE(settings.fallbacksEnabled(), wrapper.fallbacksEnabled());

        wrapper.setValue("key", "value");
        wrapper.setValue("contains", "value");
        wrapper.sync();

        QCOMPARE(wrapper.value("key").toString(), QLatin1String("value"));
        QCOMPARE(settings.value("key").toString(), QLatin1String("value"));

        QCOMPARE(wrapper.contains("contains"), true);
        QCOMPARE(settings.contains("contains"), true);
        wrapper.remove("contains");
        wrapper.sync();
        QCOMPARE(wrapper.contains("contains"), false);
        QCOMPARE(settings.contains("contains"), false);

        wrapper.clear();
        wrapper.sync();
        QCOMPARE(wrapper.contains("key"), false);
        QCOMPARE(settings.contains("key"), false);

        wrapper.beginGroup("group");
        wrapper.setValue("key", "value");
        wrapper.endGroup();
        wrapper.sync();

        wrapper.beginGroup("group");
        settings.beginGroup("group");
        QCOMPARE(wrapper.value("key").toString(), QLatin1String("value"));
        QCOMPARE(settings.value("key").toString(), QLatin1String("value"));
        QCOMPARE(wrapper.group(), QLatin1String("group"));
        QCOMPARE(settings.group(), QLatin1String("group"));
        settings.endGroup();
        wrapper.endGroup();

        wrapper.beginWriteArray("array");
        wrapper.setArrayIndex(0);
        wrapper.setValue("key", "value");
        wrapper.endArray();
        wrapper.sync();

        wrapper.beginReadArray("array");
        settings.beginReadArray("array");
        wrapper.setArrayIndex(0);
        settings.setArrayIndex(0);
        QCOMPARE(wrapper.value("key").toString(), QLatin1String("value"));
        QCOMPARE(settings.value("key").toString(), QLatin1String("value"));
        settings.endArray();
        wrapper.endArray();

        wrapper.setValue("fridge/color", 3);
        wrapper.setValue("fridge/size", QSize(32, 96));
        wrapper.setValue("sofa", true);
        wrapper.setValue("tv", false);

        wrapper.remove("group");
        wrapper.remove("array");
        wrapper.sync();

        QStringList keys = wrapper.allKeys();
        QCOMPARE(keys.count(), 4);
        QCOMPARE(keys.contains("fridge/color"), true);
        QCOMPARE(keys.contains("fridge/size"), true);
        QCOMPARE(keys.contains("sofa"), true);
        QCOMPARE(keys.contains("tv"), true);

        wrapper.beginGroup("fridge");
        keys = wrapper.allKeys();
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.contains("color"), true);
        QCOMPARE(keys.contains("size"), true);
        wrapper.endGroup();

        keys = wrapper.childKeys();
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.contains("sofa"), true);
        QCOMPARE(keys.contains("tv"), true);

        wrapper.beginGroup("fridge");
        keys = wrapper.childKeys();
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.contains("color"), true);
        QCOMPARE(keys.contains("size"), true);
        wrapper.endGroup();

        QStringList groups = wrapper.childGroups();
        QCOMPARE(groups.count(), 1);
        QCOMPARE(groups.contains("fridge"), true);

        wrapper.beginGroup("fridge");
        groups = wrapper.childGroups();
        QCOMPARE(groups.count(), 0);
        wrapper.endGroup();
    }

    void testQProcessWrapper()
    {
        RemoteServer server;
        server.init(39999, QHostAddress::LocalHost, RemoteServer::Debug);
        server.start();

        {
            QProcess process;
            QProcessWrapper wrapper;

            QCOMPARE(wrapper.isConnectedToServer(), false);
            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::NotRunning));
            QCOMPARE(wrapper.isConnectedToServer(), true);

            QCOMPARE(process.workingDirectory(), wrapper.workingDirectory());
            process.setWorkingDirectory(QDir::tempPath());
            wrapper.setWorkingDirectory(QDir::tempPath());
            QCOMPARE(process.workingDirectory(), wrapper.workingDirectory());

            QCOMPARE(process.environment(), wrapper.environment());
            process.setEnvironment(QProcess::systemEnvironment());
            wrapper.setEnvironment(QProcess::systemEnvironment());
            QCOMPARE(process.environment(), wrapper.environment());

            QCOMPARE(int(process.readChannel()), int(wrapper.readChannel()));
            process.setReadChannel(QProcess::StandardError);
            wrapper.setReadChannel(QProcessWrapper::StandardError);
            QCOMPARE(int(process.readChannel()), int(wrapper.readChannel()));

            QCOMPARE(int(process.processChannelMode()), int(wrapper.processChannelMode()));
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            wrapper.setProcessChannelMode(QProcessWrapper::ForwardedChannels);
            QCOMPARE(int(process.processChannelMode()), int(wrapper.processChannelMode()));
        }

        {
            QProcessWrapper wrapper;

            QCOMPARE(wrapper.isConnectedToServer(), false);
            QCOMPARE(int(wrapper.exitCode()), 0);
            QCOMPARE(wrapper.isConnectedToServer(), true);

            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::NotRunning));
            QCOMPARE(int(wrapper.exitStatus()), int(QProcessWrapper::NormalExit));

            QString fileName;
            {
                QTemporaryFile file(QDir::tempPath() +
#ifdef Q_OS_WIN
                    QLatin1String("/XXXXXX.bat")
#else
                    QLatin1String("/XXXXXX.sh")
#endif
                );
                file.setAutoRemove(false);
                QCOMPARE(file.open(), true);
#ifdef Q_OS_WIN
                file.write("@echo off\necho Mega test output!");
#else
                file.write("#!/bin/bash\necho Mega test output!");
#endif
                file.setPermissions(file.permissions() | QFile::ExeOther | QFile::ExeGroup
                    | QFile::ExeUser);
                fileName = file.fileName();
            }

            QSignalSpy spy(&wrapper, SIGNAL(started()));
            QSignalSpy spy2(&wrapper, SIGNAL(finished(int)));
            QSignalSpy spy3(&wrapper, SIGNAL(finished(int, QProcess::ExitStatus)));

#ifdef Q_OS_WIN
            wrapper.start(fileName);
#else
            wrapper.start("sh", QStringList() << fileName);
#endif
            QCOMPARE(wrapper.waitForStarted(), true);
            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::Running));
            QCOMPARE(wrapper.waitForFinished(), true);
            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::NotRunning));
            QCOMPARE(wrapper.readAll().trimmed(), QByteArray("Mega test output!"));

            QTest::qWait(500);

            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy2.count(), 1);
            QList<QVariant> arguments = spy2.takeFirst();
            QCOMPARE(arguments.first().toInt(), 0);

            QCOMPARE(spy3.count(), 1);
            arguments = spy3.takeFirst();
            QCOMPARE(arguments.first().toInt(), 0);
            QCOMPARE(arguments.last().toInt(), int(QProcessWrapper::NormalExit));

            QFile::remove(fileName);
        }
    }

    void testRemoteFileEngine()
    {
        RemoteServer server;
        server.init(39999, QHostAddress::LocalHost, RemoteServer::Debug);
        server.start();

        QString filename;
        {
            QTemporaryFile file;
            file.setAutoRemove(false);
            QCOMPARE(file.open(), true);
            file.write(QProcess::systemEnvironment().join(QLatin1String("\n")).toLocal8Bit());
            filename = file.fileName();
        }

        RemoteFileEngineHandler handler;

        QFile file;
        file.setFileName(filename);
        file.open(QIODevice::ReadWrite);
        const QByteArray ba = file.readLine();
        file.seek(0);

        QByteArray ba2(32 * 1024 * 1024, '\0');
        file.readLine(ba2.data(), ba2.size());

        file.resize(0);
        file.write(QProcess::systemEnvironment().join(QLatin1String("\n")).toLocal8Bit());
    }
};

QTEST_MAIN(tst_ClientServer)

#include "tst_clientserver.moc"