/*
 *  Copyright (C) 2009 Aleksey Palazhchenko
 *  Copyright (C) 2014 Sergey Shambir
 *  Copyright (C) 2016 Alexander Makarov
 *
 * This file is a part of Breakpad-qt library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */
 
#include <QCoreApplication>
#include <QString>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QHttpMultiPart>

#include "QBreakpadHttpUploader.h"
#include "QAuthenticator"

QBreakpadHttpUploader::QBreakpadHttpUploader(QObject *parent) :
    QObject(parent),
    m_file(0)
{

}

QBreakpadHttpUploader::QBreakpadHttpUploader(const QUrl &url, QObject *parent) :
    QObject(parent),
    m_file(0)
{
    m_request.setUrl(url);
}

QBreakpadHttpUploader::~QBreakpadHttpUploader()
{
	if(m_reply) {
		qWarning("m_reply is not NULL");
		m_reply->deleteLater();
	}

	delete m_file;
}

QString QBreakpadHttpUploader::remoteUrl() const
{
    return m_request.url().toString();
}

void QBreakpadHttpUploader::setUrl(const QUrl &url)
{
    m_request.setUrl(url);
}

void QBreakpadHttpUploader::uploadDump(const QString& abs_file_path)
{
    Q_ASSERT(!m_file);
    Q_ASSERT(!m_reply);
    Q_ASSERT(QDir().exists(abs_file_path));
    QFileInfo fileInfo(abs_file_path);

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    //product name parameter
    QHttpPart prodPart;
#if defined(SOCORRO)
    prodPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"ProductName\""));      // Socorro
#elif defined(CALIPER)
    prodPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"prod\""));     // Caliper
#endif
    prodPart.setBody(qApp->applicationName().toLatin1());
    //product version parameter
    QHttpPart verPart;
#if defined(SOCORRO)
    verPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"Version\""));      // Socorro
#elif defined(CALIPER)
    verPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"ver\""));     // Caliper
#endif
    verPart.setBody(qApp->applicationVersion().toLatin1());

    // file_minidump name & file_binary in one part
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"upload_file_minidump\"; filename=\""+ fileInfo.fileName()+ "\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));

    m_file = new QFile(abs_file_path);
    if(!m_file->open(QIODevice::ReadOnly)) return;

    filePart.setBodyDevice(m_file);
    m_file->setParent(multiPart);

    multiPart->append(prodPart);
    multiPart->append(verPart);
    multiPart->append(filePart);

    m_request.setRawHeader("User-Agent", qApp->applicationName().toLatin1()+"/"+qApp->applicationVersion().toLatin1());
#if defined(SOCORRO)
    m_request.setRawHeader("Host", qApp->applicationName().toLower().toLatin1()+"_reports");
    m_request.setRawHeader("Accept", "*/*");
#endif

    m_reply = m_manager.post(m_request, multiPart);
    multiPart->setParent(m_reply);

    connect(m_reply, SIGNAL(uploadProgress(qint64, qint64)),
            this,      SLOT(onUploadProgress(qint64,qint64)));

    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this,      SLOT(onError(QNetworkReply::NetworkError)));

    connect(m_reply, SIGNAL(finished()),
            this,      SLOT(onUploadFinished()));
}

void QBreakpadHttpUploader::onUploadProgress(qint64 sent, qint64 total)
{
    qDebug("upload progress: %lld/%lld", sent, total);
}

void QBreakpadHttpUploader::onError(QNetworkReply::NetworkError err)
{
    qDebug() << err;
}

void QBreakpadHttpUploader::onUploadFinished()
{
    QString data = (QString)m_reply->readAll();
    qDebug() << "Upload finished";
    qDebug() << "Answer: " << data;

	if(m_reply->error() != QNetworkReply::NoError) {
        qWarning("Upload error: %d - %s", m_reply->error(), qPrintable(m_reply->errorString()));
	} else {
        qDebug() << "Upload to " << remoteUrl() << " success!";
        m_file->remove();
	}
    emit finished(data);

	m_reply->close();
	m_reply->deleteLater();
	m_reply = 0;

	delete m_file;
	m_file = 0;
}

//////////////////////////////////////////////////////////////////////////


IBEBreakpadFtploader::IBEBreakpadFtploader(QObject* parent) :
    QObject(parent),
    m_file(0)
{

}

IBEBreakpadFtploader::IBEBreakpadFtploader(const QUrl& url, QObject* parent) :
    QObject(parent),
    m_file(0)
{
    m_request.setUrl(url);
}

IBEBreakpadFtploader::~IBEBreakpadFtploader()
{
    if (m_reply)
    {
        qWarning("m_reply is not NULL");
        m_reply->deleteLater();
    }

    delete m_file;
}

QString IBEBreakpadFtploader::remoteUrl() const
{
    return m_request.url().toString();
}

void IBEBreakpadFtploader::setUrl(const QUrl& url)
{
    m_request.setUrl(url);
}

void IBEBreakpadFtploader::authenticationRequired(QNetworkReply*, QAuthenticator* authenticator)
{
	// �ṩFTP��������֤������û���������
	authenticator->setUser("autotest");
	authenticator->setPassword("autotest");
}

void IBEBreakpadFtploader::uploadDump(const QString& abs_file_path)
{
    Q_ASSERT(!m_file);
    Q_ASSERT(!m_reply);
    Q_ASSERT(QDir().exists(abs_file_path));
    QFileInfo fileInfo(abs_file_path);

    //! ����д��
	//connect(&m_manager, &QNetworkAccessManager::authenticationRequired, &authenticationRequired);
	connect(&m_manager, SIGNAL(QNetworkAccessManager::authenticationRequired)
        , this, SLOT(authenticationRequired));

	// ׼���ϴ����ļ�·��
	QFile file(abs_file_path);
	if (!file.open(QIODevice::ReadOnly))
	{
		qDebug() << "Failed to open file for reading";
		//return -1;
	}

	// FTP��������URL��������������¼���������Ҫ��֤��FTP����ο������ӵ����������û���������
	//ftp://your-ftp-server.com/remote/path/on/server/file.txt
	//ftp://172.30.100.34/%C1%D9%CA%B1%C4%BF%C2%BC/lhc/
	//ftp://172.30.100.34/��ʱĿ¼/lhc/
	//ftp://autotest@172.30.100.34/��ʱĿ¼/lhc
	QUrl ftpUrl("ftp://172.30.100.34/%C1%D9%CA%B1%C4%BF%C2%BC/lhc/");

	// ��������
	//m_request.setRawHeader("User-Agent", qApp->applicationName().toLatin1() + "/" + qApp->applicationVersion().toLatin1());
	m_request.setRawHeader("User-Agent", "MyFTPClient 1.0"); // ��ѡ�������û�����

	// ����PUT��������ļ��ϴ�
	m_reply = m_manager.put(m_request, &file);
	//QObject::connect(m_reply, &QNetworkReply::finished, &onUploadFinished);

	connect(m_reply, SIGNAL(uploadProgress(qint64, qint64)),
		this, SLOT(onUploadProgress(qint64, qint64)));

	connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
		this, SLOT(onError(QNetworkReply::NetworkError)));

	connect(m_reply, SIGNAL(finished()),//&QNetworkReply::finished
		this, SLOT(onUploadFinished()));

	// �������ͬ���ȴ�������ɣ�����ʹ���¼�ѭ��������ע������������߳�
	// QEventLoop loop;
	// connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	// loop.exec(); // ����ע�͵��ˣ�ʵ��ʹ��ʱ������Ҫѡ���Ƿ�����

}

void IBEBreakpadFtploader::onUploadProgress(qint64 sent, qint64 total)
{
    qDebug("upload progress: %lld/%lld", sent, total);
}

void IBEBreakpadFtploader::onError(QNetworkReply::NetworkError err)
{
    qDebug() << err;
}

void IBEBreakpadFtploader::onUploadFinished()
{
    QString data = (QString)m_reply->readAll();
    qDebug() << "Upload finished";
    qDebug() << "Answer: " << data;

    if (m_reply->error() != QNetworkReply::NoError) {
        qWarning("Upload error: %d - %s", m_reply->error(), qPrintable(m_reply->errorString()));
    }
    else {
        qDebug() << "Upload to " << remoteUrl() << " success!";
        m_file->remove();
    }
    emit finished(data);

    m_reply->close();
    m_reply->deleteLater();
    m_reply = 0;

    delete m_file;
    m_file = 0;
}

