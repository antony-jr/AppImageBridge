/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2018-2019, Antony jr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
*/

#include "qappimageupdate_p.hpp"
#include "helpers_p.hpp"


QAppImageUpdatePrivate::QAppImageUpdatePrivate(bool singleThreaded, QObject *parent)
    : QObject(parent) {
    setObjectName("QAppImageUpdatePrivate");
    if(!singleThreaded) {
        m_SharedThread.reset(new QThread);
        m_SharedThread->start();
    }


    m_SharedNetworkAccessManager.reset(new QNetworkAccessManager);
    m_UpdateInformation.reset(new AppImageUpdateInformationPrivate);
    m_DeltaWriter.reset(new ZsyncWriterPrivate(m_SharedNetworkAccessManager.data()));
    if(!singleThreaded) {
        m_SharedNetworkAccessManager->moveToThread(m_SharedThread.data());
        m_UpdateInformation->moveToThread(m_SharedThread.data());
        m_DeltaWriter->moveToThread(m_SharedThread.data());
    }
    m_ControlFileParser.reset(new ZsyncRemoteControlFileParserPrivate(m_SharedNetworkAccessManager.data()));
    if(!singleThreaded) {
        m_ControlFileParser->moveToThread(m_SharedThread.data());
    }
    m_ControlFileParser->setObjectName("ZsyncRemoteControlFileParserPrivate");
    m_UpdateInformation->setObjectName("AppImageUpdateInformationPrivate");
    m_DeltaWriter->setObjectName("ZsyncWriterPrivate");
    
    // Set logger name
    m_UpdateInformation->setLoggerName("UpdateInformation");
    m_ControlFileParser->setLoggerName("ControlFileParser");
    m_DeltaWriter->setLoggerName("DeltaWriter");


    // Update information
    connect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::logger,
            this, &QAppImageUpdatePrivate::logger,
            (Qt::ConnectionType)(Qt::DirectConnection | Qt::UniqueConnection));

    // Control file parsing
    connect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::logger,
            this, &QAppImageUpdatePrivate::logger,
            (Qt::ConnectionType)(Qt::DirectConnection | Qt::UniqueConnection));

    // Delta Writer and Downloader
    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::logger,
            this, &QAppImageUpdatePrivate::logger,
            (Qt::ConnectionType)(Qt::DirectConnection | Qt::UniqueConnection));
    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::started,
	     this, &QAppImageUpdatePrivate::handleUpdateStart,
	     (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
}

QAppImageUpdatePrivate::QAppImageUpdatePrivate(const QString &AppImagePath, bool singleThreaded, QObject *parent)
    : QAppImageUpdatePrivate(singleThreaded, parent) {
    setAppImage(AppImagePath);
    return;
}

QAppImageUpdatePrivate::QAppImageUpdatePrivate(QFile *AppImage, bool singleThreaded, QObject *parent)
    : QAppImageUpdatePrivate(singleThreaded, parent) {
    setAppImage(AppImage);
    return;
}

QAppImageUpdatePrivate::~QAppImageUpdatePrivate() {
    if(b_Started || b_Running){
	    cancel();
    }

    if(!m_SharedThread.isNull()) {
        m_SharedThread->quit();
        m_SharedThread->wait();
    }
    return;
}

void QAppImageUpdatePrivate::setIcon(QByteArray icon) {
	if(b_Started || b_Running) {
		return;
	}

	m_Icon = icon;	
}

void QAppImageUpdatePrivate::setGuiFlag(int flag) {
	if(b_Started || b_Running) {
		return;
	}

	n_GuiFlag = flag;
}

void QAppImageUpdatePrivate::setAppImage(const QString &AppImagePath) {
    if(b_Started || b_Running) {
        return;
    }

    clear();
    getMethod(m_UpdateInformation.data(), "setAppImage(const QString&)")
	    .invoke(m_UpdateInformation.data(), 
		    Qt::QueuedConnection,
                    Q_ARG(QString,AppImagePath));
    return;
}

void QAppImageUpdatePrivate::setAppImage(QFile *AppImage) {
    if(b_Started || b_Running) {
        return;
    }

    clear();
    getMethod(m_UpdateInformation.data(), "setAppImage(QFile *)")
	    .invoke(m_UpdateInformation.data(),
                    Qt::QueuedConnection,
                    Q_ARG(QFile*,AppImage));
}

void QAppImageUpdatePrivate::setShowLog(bool choice) {
    if(b_Started || b_Running) {
	    return;
    }

    getMethod(m_UpdateInformation.data(), "setShowLog(bool)")
	    .invoke(m_UpdateInformation.data(),
                    Qt::QueuedConnection, 
		    Q_ARG(bool, choice));

    getMethod(m_ControlFileParser.data(), "setShowLog(bool)")
	    .invoke(m_ControlFileParser.data(),
		    Qt::QueuedConnection, 
		    Q_ARG(bool, choice));

    getMethod(m_DeltaWriter.data(), "setShowLog(bool)")
	    .invoke(m_DeltaWriter.data(),
		    Qt::QueuedConnection, 
		    Q_ARG(bool, choice));
}

void QAppImageUpdatePrivate::setOutputDirectory(const QString &dir) {
    if(b_Started || b_Running) {
	    return;
    }

    getMethod(m_DeltaWriter.data(), "setOutputDirectory(const QString&)")
	    .invoke(m_DeltaWriter.data(),
                    Qt::QueuedConnection,
                    Q_ARG(QString, dir));
    return;
}

void QAppImageUpdatePrivate::setProxy(const QNetworkProxy &proxy) {
    if(b_Started || b_Running) {
	    return;
    }
    m_SharedNetworkAccessManager->setProxy(proxy);
    return;
}

void QAppImageUpdatePrivate::clear(void) {
    if(b_Started || b_Running) {
        return;
    }

    b_Started = b_Running = b_Finished = b_Canceled = b_CancelRequested = false;
    getMethod(m_UpdateInformation.data(), "clear(void)")
	    .invoke(m_UpdateInformation.data(), Qt::QueuedConnection);
    getMethod(m_ControlFileParser.data(), "clear(void)")
	    .invoke(m_ControlFileParser.data(), Qt::QueuedConnection);
    return;
}


void QAppImageUpdatePrivate::start(short action, int flags, QByteArray icon) {
    if(b_Started || b_Running){
        return;
    }

    b_Started = b_Running = true;
    b_Canceled = false;
    b_Finished = false;

    if(b_CancelRequested) {
	    b_Started = b_Running = false;
	    b_Canceled = true;
	    b_CancelRequested = false;
	    emit canceled(action);
	    return;
    }

    if(flags == GuiFlag::None) {
	    flags = (n_GuiFlag != GuiFlag::None) ? n_GuiFlag : GuiFlag::Default;
    }

    if(icon.isEmpty() && !m_Icon.isEmpty()){
	    icon = m_Icon;
    }

    if(action == Action::GetEmbeddedInfo){
	    n_CurrentAction = action;
	    connect(m_UpdateInformation.data(),  &AppImageUpdateInformationPrivate::info,
		this, &QAppImageUpdatePrivate::redirectEmbeddedInformation,
		Qt::QueuedConnection);
	    connect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::progress,
		    this, &QAppImageUpdatePrivate::handleGetEmbeddedInfoProgress);
	    connect(m_UpdateInformation.data(),  &AppImageUpdateInformationPrivate::error,
		this, &QAppImageUpdatePrivate::handleGetEmbeddedInfoError,
		Qt::QueuedConnection);

	    emit started(Action::GetEmbeddedInfo);
	    getMethod(m_UpdateInformation.data(), "getInfo(void)")
		    .invoke(m_UpdateInformation.data(), Qt::QueuedConnection);
    
    }else if(action == Action::CheckForUpdate) {
	    n_CurrentAction = action;
	    connect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
		     m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject)),
		     (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));
	   
	    connect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
		     m_ControlFileParser.data(), SLOT(getUpdateCheckInformation(void)),
		     (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));
	    
	    connect(m_ControlFileParser.data(), SIGNAL(updateCheckInformation(QJsonObject)),
		     this, SLOT(redirectUpdateCheck(QJsonObject)),
                     (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));	 
	    
	    connect(m_ControlFileParser.data(), SIGNAL(progress(int)),
		     this, SLOT(handleCheckForUpdateProgress(int)));
	    
	    connect(m_ControlFileParser.data(), SIGNAL(error(short)),
		     this, SLOT(handleCheckForUpdateError(short)),
                     (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));

	    connect(m_UpdateInformation.data(), SIGNAL(error(short)),
		     this, SLOT(handleCheckForUpdateError(short)),
                     (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));


	    emit started(Action::CheckForUpdate); 
    	    getMethod(m_UpdateInformation.data(), "getInfo(void)")
		    .invoke(m_UpdateInformation.data(), Qt::QueuedConnection);
    }else if(action == Action::Update) {
	    n_CurrentAction = action;
	    connect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
		     m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject)),
		     (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));
	    
	    connect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
		    m_ControlFileParser.data(), SLOT(getZsyncInformation(void)),
		    (Qt::ConnectionType)(Qt::UniqueConnection | Qt::QueuedConnection));
	    
            connect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::zsyncInformation,
                     m_DeltaWriter.data(), &ZsyncWriterPrivate::setConfiguration,
                     (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
           
	    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finishedConfiguring,
                    m_DeltaWriter.data(), &ZsyncWriterPrivate::start,
                    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));

	    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finished,
		    this, &QAppImageUpdatePrivate::handleUpdateFinished,
		    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));

	    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError,
		    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
		
	    connect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError,
		    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));
	    
	    connect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError,
		    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));

	    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::progress,
                    this, &QAppImageUpdatePrivate::handleUpdateProgress,
                    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));

	    connect(m_DeltaWriter.data(), &ZsyncWriterPrivate::canceled,
		    this, &QAppImageUpdatePrivate::handleUpdateCancel,
		    (Qt::ConnectionType)(Qt::QueuedConnection | Qt::UniqueConnection));

	    //// Started signal will be emitted by handleUpdateStart 
	    //// which is connected at the construction.
	    getMethod(m_UpdateInformation.data(), "getInfo(void)")
		    .invoke(m_UpdateInformation.data(), Qt::QueuedConnection);

    }else if(action == Action::UpdateWithGUI) {
	    n_CurrentAction = action;
    }else {
	    n_CurrentAction = Action::None;
	    b_Started = b_Running = b_Canceled = false;
    }
    return;
}

void QAppImageUpdatePrivate::cancel(void) {
    if(!b_Started && !b_Running) {
	    return;
    }

    b_CancelRequested = true;
    getMethod(m_DeltaWriter.data(),"cancel()")
		.invoke(m_DeltaWriter.data(), Qt::QueuedConnection);
    return;
}


/// * * *
/// Private Slots

void QAppImageUpdatePrivate::handleUpdateProgress(int percentage, 
						  qint64 bytesReceived, 
						  qint64 bytesTotal, 
						  double speed, 
						  QString units){
	emit progress(percentage, bytesReceived, bytesTotal, speed, units, Action::Update);
}

void QAppImageUpdatePrivate::handleGetEmbeddedInfoProgress(int percentage) {
	emit progress(percentage, 1, 1, 0, QString(), Action::GetEmbeddedInfo);
}

void QAppImageUpdatePrivate::handleCheckForUpdateProgress(int percentage) {
	emit progress(percentage, 1, 1, 0, QString(), Action::CheckForUpdate);
}

void QAppImageUpdatePrivate::handleGetEmbeddedInfoError(short code) {
	b_Canceled = b_Started = b_Running = false;	
	b_Finished = false;
	b_CancelRequested = false;
	disconnect(m_UpdateInformation.data(),  &AppImageUpdateInformationPrivate::error,
		   this, &QAppImageUpdatePrivate::handleGetEmbeddedInfoError);
	disconnect(m_UpdateInformation.data(),  &AppImageUpdateInformationPrivate::info,
		   this, &QAppImageUpdatePrivate::redirectEmbeddedInformation);   
	disconnect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::progress,
		    this, &QAppImageUpdatePrivate::handleGetEmbeddedInfoProgress);
	emit error(code, QAppImageUpdatePrivate::Action::GetEmbeddedInfo); 
}

void QAppImageUpdatePrivate::redirectEmbeddedInformation(QJsonObject info) {
	b_Canceled = b_Started = b_Running = false;	
	b_Finished = true;
	disconnect(m_UpdateInformation.data(),  &AppImageUpdateInformationPrivate::error,
		   this, &QAppImageUpdatePrivate::handleGetEmbeddedInfoError);
	disconnect(m_UpdateInformation.data(),  &AppImageUpdateInformationPrivate::info,
		   this, &QAppImageUpdatePrivate::redirectEmbeddedInformation);   
	disconnect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::progress,
		    this, &QAppImageUpdatePrivate::handleGetEmbeddedInfoProgress);
	
	if(b_CancelRequested) {
		b_CancelRequested = false;
		b_Canceled = true;
		emit canceled(Action::GetEmbeddedInfo);
		return;
	}
	emit finished(info, Action::GetEmbeddedInfo); 
}


void QAppImageUpdatePrivate::handleCheckForUpdateError(short code) {
	b_Canceled = b_Started = b_Running = false;	
	b_Finished = false;
	b_CancelRequested = false;

	disconnect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
	       m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject))); 
	disconnect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
	       m_ControlFileParser.data(), SLOT(getUpdateCheckInformation(void))); 
	disconnect(m_ControlFileParser.data(), SIGNAL(updateCheckInformation(QJsonObject)),
	       this, SLOT(redirectUpdateCheck(QJsonObject)));
	disconnect(m_ControlFileParser.data(), SIGNAL(error(short)),
	       this, SLOT(handleCheckForUpdateError(short)));
  	disconnect(m_UpdateInformation.data(), SIGNAL(error(short)),
		   this, SLOT(handleCheckForUpdateError(short)));
	disconnect(m_ControlFileParser.data(), SIGNAL(progress(int)),
		     this, SLOT(handleCheckForUpdateProgress(int)));
	   
	emit error(code, Action::CheckForUpdate); 
}

void QAppImageUpdatePrivate::redirectUpdateCheck(QJsonObject info) {
    disconnect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
	       m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject))); 
    disconnect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
	       m_ControlFileParser.data(), SLOT(getUpdateCheckInformation(void))); 
    disconnect(m_ControlFileParser.data(), SIGNAL(updateCheckInformation(QJsonObject)),
	       this, SLOT(redirectUpdateCheck(QJsonObject)));
    disconnect(m_ControlFileParser.data(), SIGNAL(error(short)),
	       this, SLOT(handleCheckForUpdateError(short)));
    disconnect(m_UpdateInformation.data(), SIGNAL(error(short)),
		   this, SLOT(handleCheckForUpdateError(short)));
    disconnect(m_ControlFileParser.data(), SIGNAL(progress(int)),
		     this, SLOT(handleCheckForUpdateProgress(int)));
	   
	
    // Can this happen without an error? 
    if(info.isEmpty()) {
        return;
    }

    auto releaseNotes = info["ReleaseNotes"].toString();
    auto embeddedUpdateInformation = info["EmbededUpdateInformation"].toObject();
    auto oldVersionInformation = embeddedUpdateInformation["FileInformation"].toObject();

    QString remoteTargetFileSHA1Hash = info["RemoteTargetFileSHA1Hash"].toString(), 
            localAppImageSHA1Hash = oldVersionInformation["AppImageSHA1Hash"].toString(),
            localAppImagePath = oldVersionInformation["AppImageFilePath"].toString();

    QJsonObject updateinfo {
	{ "UpdateAvailable", localAppImageSHA1Hash != remoteTargetFileSHA1Hash},
	{ "AbsolutePath", localAppImagePath},
	{ "Sha1Hash",  localAppImageSHA1Hash },
	{ "RemoteSha1Hash", remoteTargetFileSHA1Hash},
	{ "ReleaseNotes", releaseNotes}
    };
    
    b_Started = b_Running = false;
    b_Finished = true;
    b_Canceled = false;
    

    if(b_CancelRequested) {
		b_CancelRequested = false;
		b_Canceled = true;
		emit canceled(Action::CheckForUpdate);
		return;
    }
    emit finished(updateinfo, Action::CheckForUpdate);
}

void QAppImageUpdatePrivate::handleUpdateStart() {
	emit started(Action::Update);
}

void QAppImageUpdatePrivate::handleUpdateCancel() {
	    disconnect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
		     m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject)));
	    disconnect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
		    m_ControlFileParser.data(), SLOT(getZsyncInformation(void)));
            disconnect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::zsyncInformation,
                     m_DeltaWriter.data(), &ZsyncWriterPrivate::setConfiguration);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finishedConfiguring,
                    m_DeltaWriter.data(), &ZsyncWriterPrivate::start);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finished,
		    this, &QAppImageUpdatePrivate::handleUpdateFinished);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError);	
	    disconnect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError); 
	    disconnect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::progress,
                    this, &QAppImageUpdatePrivate::handleUpdateProgress);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::canceled,
		    this, &QAppImageUpdatePrivate::handleUpdateCancel);

    	    b_Started = b_Running = b_Finished = b_CancelRequested = false;
   	    b_Canceled = true; 

	    emit canceled(Action::Update);

}

void QAppImageUpdatePrivate::handleUpdateError(short ecode) {
	    disconnect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
		     m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject)));
	    disconnect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
		    m_ControlFileParser.data(), SLOT(getZsyncInformation(void)));
            disconnect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::zsyncInformation,
                     m_DeltaWriter.data(), &ZsyncWriterPrivate::setConfiguration);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finishedConfiguring,
                    m_DeltaWriter.data(), &ZsyncWriterPrivate::start);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finished,
		    this, &QAppImageUpdatePrivate::handleUpdateFinished);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError);	
	    disconnect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError); 
	    disconnect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::progress,
                    this, &QAppImageUpdatePrivate::handleUpdateProgress);
disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::canceled,
		    this, &QAppImageUpdatePrivate::handleUpdateCancel);


    	    b_Started = b_Running = b_Finished = false;
   	    b_Canceled = false;
    
    	    if(b_CancelRequested) {
		b_CancelRequested = false;
		b_Canceled = true;
		emit canceled(Action::Update);
		return;
    	    }

	    emit error(ecode, Action::Update);
}

void QAppImageUpdatePrivate::handleUpdateFinished(QJsonObject info, QString oldVersionPath) {
	    disconnect(m_UpdateInformation.data(), SIGNAL(info(QJsonObject)),
		     m_ControlFileParser.data(), SLOT(setControlFileUrl(QJsonObject)));
	    disconnect(m_ControlFileParser.data(), SIGNAL(receiveControlFile(void)),
		    m_ControlFileParser.data(), SLOT(getZsyncInformation(void)));
            disconnect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::zsyncInformation,
                     m_DeltaWriter.data(), &ZsyncWriterPrivate::setConfiguration);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finishedConfiguring,
                    m_DeltaWriter.data(), &ZsyncWriterPrivate::start);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::finished,
		    this, &QAppImageUpdatePrivate::handleUpdateFinished);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError);	
	    disconnect(m_UpdateInformation.data(), &AppImageUpdateInformationPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError); 
	    disconnect(m_ControlFileParser.data(), &ZsyncRemoteControlFileParserPrivate::error,
		    this, &QAppImageUpdatePrivate::handleUpdateError);
	    disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::progress,
                    this, &QAppImageUpdatePrivate::handleUpdateProgress);
disconnect(m_DeltaWriter.data(), &ZsyncWriterPrivate::canceled,
		    this, &QAppImageUpdatePrivate::handleUpdateCancel);


	QJsonObject result {
		{"OldVersionPath", oldVersionPath},
		{"NewVersionPath", info["AbsolutePath"].toString()},
		{"NewVersionSha1Hash", info["Sha1Hash"].toString()} 
	};
	b_Started = b_Running = false;
	b_Finished = true;
   	b_Canceled = false;
    
    	if(b_CancelRequested) {
		b_CancelRequested = false;
		b_Canceled = true;
		emit canceled(Action::Update);
		return;
    	}

	emit finished(result, Action::Update);
}
