/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "player.h"

#include "playercontrols.h"
#include "playlistmodel.h"
#include "videowidget.h"

#include <QMediaService>
#include <QMediaPlaylist>
#include <QVideoProbe>
#include <QAudioProbe>
#include <QMediaMetaData>
#include <QtWidgets>
#include <QtConcurrent/QtConcurrent>
#include <cmath>
#include <QNetworkReply>

Player::Player(QWidget *parent)
    : QWidget(parent)
{
//! [create-objs]
    m_player = new QMediaPlayer(this);
    m_player->setAudioRole(QAudio::VideoRole);
    qInfo() << "Supported audio roles:";
    for (QAudio::Role role : m_player->supportedAudioRoles())
        qInfo() << "    " << role;
    // owned by PlaylistModel
    m_playlist = new QMediaPlaylist();
    m_player->setPlaylist(m_playlist);
//! [create-objs]

    connect(m_player, &QMediaPlayer::durationChanged, this, &Player::durationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &Player::positionChanged);
    connect(m_player, QOverload<>::of(&QMediaPlayer::metaDataChanged), this, &Player::metaDataChanged);
    connect(m_playlist, &QMediaPlaylist::currentIndexChanged, this, &Player::playlistPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &Player::statusChanged);
    connect(m_player, &QMediaPlayer::bufferStatusChanged, this, &Player::bufferingProgress);
    //connect(m_player, &QMediaPlayer::videoAvailableChanged, this, &Player::videoAvailableChanged);
    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, &Player::displayErrorMessage);
    connect(m_player, &QMediaPlayer::stateChanged, this, &Player::stateChanged);

//! [2]
    m_videoWidget = new VideoWidget(this);
    m_player->setVideoOutput(m_videoWidget);

    m_playlistModel = new PlaylistModel(this);
    m_playlistModel->setPlaylist(m_playlist);
//! [2]

    m_playlistView = new QListView(this);
    m_playlistView->setModel(m_playlistModel);

    //set current index
    currentIndex = m_playlist->currentIndex();
    m_playlistView->setCurrentIndex(m_playlistModel->index(m_playlist->currentIndex(), 0));

    connect(m_playlistView, &QAbstractItemView::activated, this, &Player::jump);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, m_player->duration() / 1000);

    m_labelDuration = new QLabel(this);
    connect(m_slider, &QSlider::sliderMoved, this, &Player::seek);

    QPushButton *openVideoButton = new QPushButton(tr("Open Video"), this);
    QPushButton *addSRTButton = new QPushButton(tr("Add SRT file"), this);

    connect(openVideoButton, &QPushButton::clicked, this, &Player::open);

    PlayerControls *controls = new PlayerControls(this);
    controls->setState(m_player->state());
    controls->setVolume(m_player->volume());
    controls->setMuted(controls->isMuted());

    connect(controls, &PlayerControls::play, m_player, &QMediaPlayer::play);
    connect(controls, &PlayerControls::pause, m_player, &QMediaPlayer::pause);
    connect(controls, &PlayerControls::stop, m_player, &QMediaPlayer::stop);
    connect(controls, &PlayerControls::next, m_playlist, &QMediaPlaylist::next);
    connect(controls, &PlayerControls::previous, this, &Player::previousClicked);
    connect(controls, &PlayerControls::changeVolume, m_player, &QMediaPlayer::setVolume);
    connect(controls, &PlayerControls::changeMuting, m_player, &QMediaPlayer::setMuted);
    connect(controls, &PlayerControls::changeRate, m_player, &QMediaPlayer::setPlaybackRate);
    connect(controls, &PlayerControls::stop, m_videoWidget, QOverload<>::of(&QVideoWidget::update));

    connect(m_player, &QMediaPlayer::stateChanged, controls, &PlayerControls::setState);
    connect(m_player, &QMediaPlayer::volumeChanged, controls, &PlayerControls::setVolume);
    connect(m_player, &QMediaPlayer::mutedChanged, controls, &PlayerControls::setMuted);


    //TEST

    m_transcript = new QTextEdit(this);
    m_transcript->setReadOnly(true);
    //m_transcript->setTextInteractionFlags(Qt::NoTextInteraction);
    m_transcript->ensureCursorVisible();
    connect(m_transcript, &QTextEdit::copyAvailable, this, &Player::wordHighlighted);

    m_subtitles = new QTextEdit(parent);
    m_subtitles->setReadOnly(true);
    connect(this, &Player::drawSubtitles_signal, this, &Player::drawSubtitles);
    connect(m_subtitles, &QTextEdit::copyAvailable, this, &Player::wordHighlighted);

    QSplitter* splitter1 = new QSplitter(Qt::Vertical, parent);

    QVBoxLayout* transcriptVlayout = new QVBoxLayout();
    splitter1->addWidget(m_videoWidget);
    transcriptVlayout->addWidget(splitter1);
    splitter1->addWidget(m_subtitles);
    splitter1->setStretchFactor(0, 1);
    splitter1->setStretchFactor(1, 0);
    int videoWidget_index = splitter1->indexOf(m_videoWidget);
    m_videoWidget->setMinimumHeight(500);
    splitter1->setCollapsible(videoWidget_index, false);
    splitter1->setChildrenCollapsible(false);

    QHBoxLayout* transcriptHlayout = new QHBoxLayout();
    transcriptHlayout->addWidget(m_playlistView, 0.5);
    transcriptHlayout->addLayout(transcriptVlayout, 3);
    transcriptHlayout->addWidget(m_transcript, 1);

    QBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->setMargin(0);
    controlLayout->addWidget(openVideoButton);
    controlLayout->addWidget(addSRTButton);
    controlLayout->addStretch(1);
    controlLayout->addWidget(controls);
    controlLayout->addStretch(1);

    QBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(transcriptHlayout);
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->addWidget(m_slider);
    hLayout->addWidget(m_labelDuration);
    layout->addLayout(hLayout);
    layout->addLayout(controlLayout);

    QPushButton *resetTranscriptPos_button = new QPushButton(tr("Recenter transcript"), this);
    connect(resetTranscriptPos_button, &QPushButton::clicked, this, &Player::moveScrollBar);
    controlLayout->addWidget(resetTranscriptPos_button);

    setLayout(layout);

    if (!isPlayerAvailable()) {
        QMessageBox::warning(this, tr("Service not available"),
                             tr("The QMediaPlayer object does not have a valid service.\n"\
                                "Please check the media service plugins are installed."));

        controls->setEnabled(false);
        m_playlistView->setEnabled(false);
        openVideoButton->setEnabled(false);
        //addSRTButton->setEnabled(false);
    }

    metaDataChanged();

    //functions using threads
    processSubtitles();
    highlight_currentLine();

    //connect network manager signal/slots
    manager = new QNetworkAccessManager();
    connect(manager, &QNetworkAccessManager::finished, this, &Player::managerFinished);
}

Player::~Player()
{
    subtitle_thread.join();
    highlightline_thread.join();

    delete manager;
}


void Player::wordHighlighted(bool yes)
{
    if (yes == false)
    {
        return;
    }

    //detect highlighted word only - no digits
    QObject* sender = this->sender();
    QTextEdit* origin_txtedit = qobject_cast<QTextEdit*>(sender);
    origin_txtedit->copy();
    curSelectedWord = QApplication::clipboard()->text();
    QRegularExpression re("[^\\d\\W]");
    QRegularExpressionMatch match = re.match(curSelectedWord);
    if (!match.hasMatch())
    {
        return;
    }

    if (m_player->state() == QMediaPlayer::PlayingState)
    {
        m_player->pause();
    }

    APIRequest();
}


void Player::APIRequest()
{
    QString endpoint = "entries";
    QString language_code = "en-gb";

    auto url = QUrl("https://od-api.oxforddictionaries.com/api/v2/" + endpoint + "/" + language_code + "/" + curSelectedWord);

    request.setUrl(url);
    request.setRawHeader("app_id", app_id);
    request.setRawHeader("app_key", app_key);
    manager->get(request);
}

void Player::managerFinished(QNetworkReply *reply)
{
    if (reply->error()) {
        QMessageBox msgBox;
        msgBox.setWindowFlags(Qt::Popup);
        msgBox.setText("Dictionary entry for '" + curSelectedWord + "' is not available");
        msgBox.exec();

        if (m_player->state() == QMediaPlayer::PausedState)
        {
            m_player->play();
        }

        return;
    }

    QByteArray answer = reply->readAll();
    QString outputString = parse_JSON_Response(answer);

    QMessageBox msgBox;
    msgBox.setWindowFlags(Qt::Popup);
    msgBox.setText(outputString);
    msgBox.exec();

    if (m_player->state() == QMediaPlayer::PausedState)
    {
        m_player->play();
    }
}

QString Player::parse_JSON_Response(QByteArray answer)
{
    QStringList outputList;

    QJsonDocument jsonResponse = QJsonDocument::fromJson(answer);

    QJsonObject jsonObject = jsonResponse.object();

    QJsonArray results_array = jsonObject["results"].toArray();
    QJsonObject results_obj = results_array.at(0).toObject();

    //word
    QString word = results_obj["id"].toString();
    outputList.push_back("<p style='font-size:20px'>");
    outputList.push_back("<b>");
    outputList.push_back(word);
    outputList.push_back("</b>");
    outputList.push_back("</p>");
    outputList.push_back("<br>");

    QJsonArray lexicalEntries_array = results_obj["lexicalEntries"].toArray();

    for (auto lexicalEntry : lexicalEntries_array)
    {
        QJsonObject lexEntry_obj = lexicalEntry.toObject();

        auto lexCat_obj = lexEntry_obj["lexicalCategory"].toObject();
        //lexical category
        outputList.push_back("<i>" + lexCat_obj["text"].toString() + "</i>" + "</li>" + "<br>");
        outputList.push_back("<br>");

        QJsonArray entries_array = lexEntry_obj["entries"].toArray();
        QJsonObject entry_obj = entries_array.at(0).toObject();

        QJsonArray pronunc_array = entry_obj["pronunciations"].toArray();

        for (auto pronunc : pronunc_array)
        {
            QJsonObject pronunc_obj = pronunc.toObject();
            //pronunciation
            outputList.push_back("<a href='" + pronunc_obj["audioFile"].toString() + "'>Pronunciation</a>");
            outputList.push_back("<br>");
            outputList.push_back("<br>");
        }

        QJsonArray senses_array = entry_obj["senses"].toArray();

        for (auto sense : senses_array)
        {
            QJsonObject sense_obj = sense.toObject();
            QJsonArray definition_array = sense_obj["definitions"].toArray();
            //definition
            outputList.push_back("<b>");
            outputList.push_back(definition_array.at(0).toString());
            outputList.push_back("</b>");
            outputList.push_back("<br>");

            QJsonArray xReference_array = sense_obj["crossReferenceMarkers"].toArray();
            //cross-references
            for (auto xReference : xReference_array)
            {
                outputList.push_back("<b>");
                outputList.push_back(xReference.toString());
                outputList.push_back("</b>");
                outputList.push_back("<br>");
            }

            QJsonArray examples_array = sense_obj["examples"].toArray();

            if (!examples_array.empty())
            {
                for (auto example : examples_array)
                {
                    QJsonObject example_obj = example.toObject();
                    //example
                    outputList.push_back("<ul>");
                    outputList.push_back("<li>");
                    outputList.push_back("Example: " + example_obj["text"].toString());
                    outputList.push_back("</ul>");
                }
            }

            QJsonArray synonym_array = sense_obj["synonyms"].toArray();

            QString synonymStr;

            if (!synonym_array.empty())
            {
                for (auto synonym : synonym_array)
                {
                    QJsonObject synonym_obj = synonym.toObject();
                    synonymStr += QString(", " + synonym_obj["text"].toString());
                }
                //synonym
                outputList.push_back("<ul>");
                outputList.push_back("<li>");
                outputList.push_back("Synonyms: " + synonymStr);
                outputList.push_back("</ul>");
                outputList.push_back("<br>");
            }
            else
            {
                outputList.push_back("<br>");
            }
        }
        outputList.push_back("<hr>");
        outputList.push_back("<br>");   //line break
    }

    outputList.push_back("<p align='right'>");
    QString more_url = "https://www.google.com/search?dictcorpus=en-gb&hl=en&forcedict=" +
            word + "&q=define%20" + word;
    outputList.push_back("<a href='" + more_url + "'>" + "View More (Google Search)</a>");
    outputList.push_back("</p>");

    QString outputString;
    for (auto line : outputList)
    {
        outputString += line;
    }

    return outputString;
}

void Player::highlight_currentLine()
{
    highlightline_thread = std::thread([&]()
    {
        while (1)
        {
            setTranscriptPosition();
        }
    });
}

void Player::setTranscriptPosition()
{
    if (currentIndex < 0)
    {
        return;
    }

    auto cur_subtitles = subtitle_List.at(m_playlistModel->index(currentIndex, 0).row());
    for (size_t i = 0; i < cur_subtitles.size(); ++i)
    {
        if (cur_subtitles.at(i).contains("-->"))
        {
            if (isWithinSubPeriod(m_player->position(), cur_subtitles.at(i)))
            {
                QString searchString = cur_subtitles.at(i);
                m_transcript->find(searchString);
                moveScrollBar();
            }
        }
    }
}

void Player::moveScrollBar()
{
    int cursorY = m_transcript->cursorRect().top();
    QScrollBar *vbar = m_transcript->verticalScrollBar();
    vbar->setValue(vbar->value() + cursorY - m_transcript->height()/2);
}

void Player::processSubtitles()
{
    subtitle_thread = std::thread([&]()
    {
        while (1)
        {
            if (currentIndex < 0 || m_player->state() != QMediaPlayer::PlayingState)
            {
                continue;
            }

            auto cur_subtitles = subtitle_List.at(m_playlistModel->index(currentIndex, 0).row());
            for (size_t i = 0; i < cur_subtitles.size(); ++i)
            {
                if (cur_subtitles.at(i).contains("-->"))
                {
                    if (isWithinSubPeriod(m_player->position(), cur_subtitles.at(i)))
                    {
                        QString fullSub = cur_subtitles.at(i+1) + "\n" + cur_subtitles.at(i+2);
                        emit drawSubtitles_signal(fullSub);
                    }
                }
            }
        }
    });
}

qint64 Player::SRTStartTime_to_milliseconds(QString subtitle_time)
{
    auto startHour = subtitle_time.mid(0, 2).toInt();
    auto startMinutes = subtitle_time.mid(3, 2).toInt();
    auto startSeconds = subtitle_time.mid(6, 2).toInt();
    auto startRemainder = subtitle_time.mid(9, 3).toInt();

    return (startHour * 3600000) + (startMinutes * 60000) + (startSeconds * 1000) + (startRemainder);
}

qint64 Player::SRTEndTime_to_milliseconds(QString subtitle_time)
{
    auto endHour = subtitle_time.mid(17, 2).toInt();
    auto endMinutes = subtitle_time.mid(20, 2).toInt();
    auto endSeconds = subtitle_time.mid(23, 2).toInt();
    auto endRemainder = subtitle_time.mid(26, 3).toInt();

    return (endHour * 3600000) + (endMinutes * 60000) + (endSeconds * 1000) + (endRemainder);
}

bool Player::isWithinSubPeriod(qint64 curPos, QString subtitle_time)
{
    return SRTStartTime_to_milliseconds(subtitle_time) <= curPos && SRTEndTime_to_milliseconds(subtitle_time) >= curPos;
}

QString Player::format_time(int time)
{
    if (time < 10)
    {
        return "0" + QString::number(time);
    }
    else
    {
        return QString::number(time);
    }
}

void Player::drawSubtitles(QString subtitle)
{
    m_subtitles->setText(subtitle);
}

bool Player::isPlayerAvailable() const
{
    return m_player->isAvailable();
}

void Player::open()
{
    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setWindowTitle(tr("Open Files"));
    QStringList supportedMimeTypes = m_player->supportedMimeTypes();
    if (!supportedMimeTypes.isEmpty()) {
        supportedMimeTypes.append("audio/x-m3u"); // MP3 playlists
        fileDialog.setMimeTypeFilters(supportedMimeTypes);
    }
    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).value(0, QDir::homePath()));
    if (fileDialog.exec() == QDialog::Accepted)
    {
        addToPlaylist(fileDialog.selectedUrls());
        for (auto url : fileDialog.selectedUrls())
        {
            QString path = url.path();
            if (QFileInfo(path).exists())
            {
                QString subtitle_FileName = QFileInfo(path).path() + "/" + QFileInfo(path).completeBaseName() + ".srt";

                if (QFileInfo(subtitle_FileName).exists())
                {
                    QFile file(subtitle_FileName);
                    if(!file.open(QIODevice::ReadOnly))
                    {
                        QMessageBox::information(0, "error", file.errorString());
                    }

                    QTextStream in(&file);

                    QStringList fullSubtitles;
                    while(!in.atEnd())
                    {
                        fullSubtitles.push_back(in.readLine());
                    }

                    subtitle_List.push_back(fullSubtitles);

                    file.close();
                }
            }
        }
    }

    //load transcript
    m_transcript->clear();
    for (auto line : subtitle_List.back())
    {
        m_transcript->append(line);
    }

    m_transcript -> moveCursor(QTextCursor::Start) ;
}

static bool isPlaylist(const QUrl &url) // Check for ".m3u" playlists.
{
    if (!url.isLocalFile())
        return false;
    const QFileInfo fileInfo(url.toLocalFile());
    return fileInfo.exists() && !fileInfo.suffix().compare(QLatin1String("m3u"), Qt::CaseInsensitive);
}

void Player::addToPlaylist(const QList<QUrl> &urls)
{
    for (auto &url: urls) {
        if (isPlaylist(url))
            m_playlist->load(url);
        else
            m_playlist->addMedia(url);
    }
}

void Player::setCustomAudioRole(const QString &role)
{
    m_player->setCustomAudioRole(role);
}

void Player::durationChanged(qint64 duration)
{
    m_duration = duration / 1000;
    m_slider->setMaximum(m_duration);
}

void Player::positionChanged(qint64 progress)
{
    if (!m_slider->isSliderDown())
        m_slider->setValue(progress / 1000);

    //clear current subtitles
    //m_subtitles->clear();

    //reset cursor position so word finding can
    //start from beginning of doc
    auto cursor = m_transcript->textCursor();
    cursor.setPosition(0);
    m_transcript->setTextCursor(cursor);

    updateDurationInfo(progress / 1000);
}

void Player::metaDataChanged()
{
    if (m_player->isMetaDataAvailable()) {
        setTrackInfo(QString("%1 - %2")
                .arg(m_player->metaData(QMediaMetaData::AlbumArtist).toString())
                .arg(m_player->metaData(QMediaMetaData::Title).toString()));

        if (m_coverLabel) {
            QUrl url = m_player->metaData(QMediaMetaData::CoverArtUrlLarge).value<QUrl>();

            m_coverLabel->setPixmap(!url.isEmpty()
                    ? QPixmap(url.toString())
                    : QPixmap());
        }
    }
}

void Player::previousClicked()
{
    // Go to previous track if we are within the first 5 seconds of playback
    // Otherwise, seek to the beginning.
    if (m_player->position() <= 5000)
        m_playlist->previous();
    else
        m_player->setPosition(0);
}

void Player::jump(const QModelIndex &index)
{
    if (index.isValid()) {
        m_playlist->setCurrentIndex(index.row());
        m_player->play();
    }
}

void Player::playlistPositionChanged(int currentItem)
{
    currentIndex = currentItem;
    m_playlistView->setCurrentIndex(m_playlistModel->index(currentItem, 0));

    //load transcript
    m_transcript->clear();
    for (auto line : subtitle_List.at(m_playlistModel->index(currentItem, 0).row()))
    {
        m_transcript->append(line);
    }

    m_transcript -> moveCursor(QTextCursor::Start) ;
}

void Player::seek(int seconds)
{
    m_player->setPosition(seconds * 1000);
}

void Player::statusChanged(QMediaPlayer::MediaStatus status)
{
    handleCursor(status);

    // handle status message
    switch (status) {
    case QMediaPlayer::UnknownMediaStatus:
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadedMedia:
        setStatusInfo(QString());
        break;
    case QMediaPlayer::LoadingMedia:
        setStatusInfo(tr("Loading..."));
        break;
    case QMediaPlayer::BufferingMedia:
    case QMediaPlayer::BufferedMedia:
        setStatusInfo(tr("Buffering %1%").arg(m_player->bufferStatus()));
        break;
    case QMediaPlayer::StalledMedia:
        setStatusInfo(tr("Stalled %1%").arg(m_player->bufferStatus()));
        break;
    case QMediaPlayer::EndOfMedia:
        QApplication::alert(this);
        break;
    case QMediaPlayer::InvalidMedia:
        displayErrorMessage();
        break;
    }
}

void Player::stateChanged(QMediaPlayer::State state)
{
}

void Player::handleCursor(QMediaPlayer::MediaStatus status)
{
#ifndef QT_NO_CURSOR
    if (status == QMediaPlayer::LoadingMedia ||
        status == QMediaPlayer::BufferingMedia ||
        status == QMediaPlayer::StalledMedia)
        setCursor(QCursor(Qt::BusyCursor));
    else
        unsetCursor();
#endif
}

void Player::bufferingProgress(int progress)
{
    if (m_player->mediaStatus() == QMediaPlayer::StalledMedia)
        setStatusInfo(tr("Stalled %1%").arg(progress));
    else
        setStatusInfo(tr("Buffering %1%").arg(progress));
}

void Player::setTrackInfo(const QString &info)
{
    m_trackInfo = info;

    if (m_statusBar) {
        m_statusBar->showMessage(m_trackInfo);
        m_statusLabel->setText(m_statusInfo);
    } else {
        if (!m_statusInfo.isEmpty())
            setWindowTitle(QString("%1 | %2").arg(m_trackInfo).arg(m_statusInfo));
        else
            setWindowTitle(m_trackInfo);
    }
}

void Player::setStatusInfo(const QString &info)
{
    m_statusInfo = info;

    if (m_statusBar) {
        m_statusBar->showMessage(m_trackInfo);
        m_statusLabel->setText(m_statusInfo);
    } else {
        if (!m_statusInfo.isEmpty())
            setWindowTitle(QString("%1 | %2").arg(m_trackInfo).arg(m_statusInfo));
        else
            setWindowTitle(m_trackInfo);
    }
}

void Player::displayErrorMessage()
{
    setStatusInfo(m_player->errorString());
}

void Player::updateDurationInfo(qint64 currentInfo)
{
    QString tStr;
    if (currentInfo || m_duration) {
        QTime currentTime((currentInfo / 3600) % 60, (currentInfo / 60) % 60,
            currentInfo % 60, (currentInfo * 1000) % 1000);
        QTime totalTime((m_duration / 3600) % 60, (m_duration / 60) % 60,
            m_duration % 60, (m_duration * 1000) % 1000);
        QString format = "mm:ss";
        if (m_duration > 3600)
            format = "hh:mm:ss";
        tStr = currentTime.toString(format) + " / " + totalTime.toString(format);
    }
    m_labelDuration->setText(tStr);
}
