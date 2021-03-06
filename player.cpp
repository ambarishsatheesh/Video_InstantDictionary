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
#include <QCloseEvent>

#define DEFAULT_SUB_FONTSIZE 22
#define DEFAULT_TS_FONTSIZE 14

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

    //open video button
    QPushButton *openVideoButton = new QPushButton(tr("Open Video"), this);
    connect(openVideoButton, &QPushButton::clicked, this, &Player::open);

    //add subtitle button
    QPushButton *addSRTButton = new QPushButton(tr("Add SRT file"), this);
    connect(addSRTButton, &QPushButton::clicked, this, &Player::addSRT);

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

    //default font size

    m_transcript = new QTextEdit(this);
    m_transcript->setReadOnly(true);
    m_transcript->setFontPointSize(DEFAULT_TS_FONTSIZE);
    connect(m_transcript, &QTextEdit::copyAvailable, this, &Player::wordHighlighted);

    m_subtitles = new QTextEdit(parent);
    m_subtitles->setReadOnly(true);
    m_subtitles->setFontPointSize(DEFAULT_SUB_FONTSIZE);
    connect(this, &Player::drawSubtitles_signal, this, &Player::drawSubtitles);
    connect(m_subtitles, &QTextEdit::copyAvailable, this, &Player::wordHighlighted);

    QSplitter* splitter1 = new QSplitter(Qt::Vertical, parent);

    QVBoxLayout* transcriptVlayout = new QVBoxLayout();
    splitter1->addWidget(m_videoWidget);
    transcriptVlayout->addWidget(splitter1);
    splitter1->addWidget(m_subtitles);
    splitter1->setStretchFactor(0, 1);
    splitter1->setStretchFactor(1, 0);
    m_videoWidget->setMinimumHeight(500);
    int videoWidget_index = splitter1->indexOf(m_videoWidget);
    splitter1->setCollapsible(videoWidget_index, false);
    splitter1->setChildrenCollapsible(false);

    QHBoxLayout* transcriptHlayout = new QHBoxLayout();
    transcriptHlayout->addWidget(m_playlistView, 2);
    transcriptHlayout->addLayout(transcriptVlayout, 10);
    transcriptHlayout->addWidget(m_transcript, 3);

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

    setLayout(layout);

    if (!isPlayerAvailable()) {
        QMessageBox::warning(this, tr("Service not available"),
                             tr("The QMediaPlayer object does not have a valid service.\n"\
                                "Please check the media service plugins are installed."));

        controls->setEnabled(false);
        m_playlistView->setEnabled(false);
        openVideoButton->setEnabled(false);
        addSRTButton->setEnabled(false);
    }

    metaDataChanged();

    //functions using threads
    processSubtitles();
    highlight_currentLine();

    //connect network manager signal/slots
    manager = new QNetworkAccessManager();
    connect(manager, &QNetworkAccessManager::finished, this, &Player::managerFinished);

    //dictionary dialog
    definition_dialog = new QDialog(this);
    definition_dialog->setWindowFlags(Qt::Popup);

    dictionaryOutput = new QTextEdit();

    scroll = new QScrollArea(definition_dialog);
    scroll->setWidgetResizable(true);
    scroll->setWidget(dictionaryOutput);

    // Add a layout for QDialog
    dialog_layout = new QHBoxLayout(definition_dialog);
    definition_dialog->setLayout(dialog_layout);
    definition_dialog->layout()->addWidget(scroll);

    //TEST
    m_transcript->setText("TEST WORD LIST HERE");
    m_transcript->append("ANOTHER LINE HERE");
    m_transcript->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_transcript, &QTextEdit::customContextMenuRequested, this, &Player::showContextMenu);
}

Player::~Player()
{
    if (subtitle_thread.joinable())
    {
        subtitle_thread.join();
    }

    if (highlightline_thread.joinable())
    {
        highlightline_thread.join();
    }

    delete manager;
}

void Player::closeEvent (QCloseEvent *event)
{
    QMessageBox::StandardButton resBtn = QMessageBox::question( this, "Test Player",
                                                                tr("Are you sure you want to close this application?\n"),
                                                                QMessageBox::Cancel | QMessageBox::No | QMessageBox::Yes,
                                                                QMessageBox::Yes);
    if (resBtn != QMessageBox::Yes)
    {
        event->ignore();
    }
    else
    {
        threadRun = false;

        if (subtitle_thread.joinable())
        {
            subtitle_thread.join();
        }

        if (highlightline_thread.joinable())
        {
            highlightline_thread.join();
        }

        event->accept();
    }
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
        curSelectedWord.clear();
        return;
    }

    if (m_player->state() == QMediaPlayer::PlayingState)
    {
        m_player->pause();
    }

    if (!curSelectedWord.isEmpty())
    {
        APIRequest();
    }
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

    //populate dialog
    dict_answer = reply->readAll();
    outputString = parse_JSON_Response(dict_answer);
    dictionaryOutput->setText(outputString);
    definition_dialog->setMinimumSize(QSize(m_transcript->height()/2, m_transcript->height()));
    definition_dialog->exec();

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
    outputList.push_back("<p style='color:red'>");
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

        for (int i = 0; i < 2; ++i)
        {
            QJsonObject sense_obj = senses_array.at(i).toObject();
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
        while (threadRun)
        {
            setTranscriptPosition();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    });
}

void Player::showContextMenu(const QPoint &pos)
{
    if (!isDefMenu_constructed)
    {
        contextMenu = new QMenu(this);

        a_getDefinition = new QAction("Get Definition", this);
        connect(a_getDefinition, &QAction::triggered, this, &Player::getWord);

        contextMenu->addAction(a_getDefinition);
        isDefMenu_constructed = true;
    }

    QTextCursor tc = m_transcript->cursorForPosition(pos);
    tc.select(QTextCursor::WordUnderCursor);
    current_word = tc.selectedText();

    if(!current_word.isEmpty())
    {
        a_getDefinition->setText("Get Definition of '" + current_word + "'");
        contextMenu->popup(m_transcript->mapToGlobal(pos));
    }
}

void Player::getWord()
{
    emit drawSubtitles_signal(current_word);
    current_word.clear();
}

void Player::setTranscriptPosition()
{
    if (currentIndex < 0)
    {
        return;
    }

    auto cur_subtitles = subtitle_List.at(m_playlistModel->index(currentIndex, 0).row());
    for (int i = 0; i < cur_subtitles.size(); ++i)
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
    QScrollBar *vbar = m_transcript->verticalScrollBar();
    vbar->setValue(vbar->value() + m_transcript->cursorRect().top());
}

void Player::processSubtitles()
{
    subtitle_thread = std::thread([&]()
    {
        while (threadRun)
        {
            if (currentIndex < 0 || m_player->state() != QMediaPlayer::PlayingState)
            {
                continue;
            }

            auto cur_subtitles = subtitle_List.at(m_playlistModel->index(currentIndex, 0).row());
            for (int i = 0; i < cur_subtitles.size(); ++i)
            {
                if (cur_subtitles.at(i).contains("-->"))
                {
                    if (isWithinSubPeriod(m_player->position(), cur_subtitles.at(i)))
                    {
                        QString fullSub = cur_subtitles.at(i+1) + " " + cur_subtitles.at(i+2);
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

void Player::loadTranscript()
{
    m_transcript->clear();
    if (currentIndex >= 0)
    {
        for (auto line : subtitle_List.at(currentIndex))
        {
            m_transcript->append(line);
        }
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
    fileDialog.setNameFilter(tr("Videos (*.mkv *.mp4 *.avi)"));

    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).value(0, QDir::homePath()));
    if (fileDialog.exec() == QDialog::Accepted)
    {
        addToPlaylist(fileDialog.selectedUrls());

        //read subtitle file
        for (auto& url : fileDialog.selectedUrls())
        {
            QString path = url.toLocalFile();
//            url.setPath(path);
//            addToPlaylist(url);

            if (QFileInfo(path).exists())
            {
                QString subtitle_FileName = QFileInfo(path).path() + "/" +
                        QFileInfo(path).completeBaseName() + ".srt";

                if (QFileInfo(subtitle_FileName).exists())
                {
                    QFile file(subtitle_FileName);
                    if(!file.open(QIODevice::ReadOnly))
                    {
                        QMessageBox::information(0, "error", file.errorString());
                        QStringList dummySub;
                        subtitle_List.push_back(dummySub);
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
                //if sub file doesn't exist, add empty sub to list
                else
                {
                    //inform user that no subtitle file exists
                    QMessageBox msgBox;
                    msgBox.setWindowFlags(Qt::Popup);
                    msgBox.setText("No .srt subtitle file found! "
                                   "Please manually add an appropriate .srt file to access live subtitles and transcript.");
                    msgBox.exec();

                    QStringList dummySub;
                    subtitle_List.push_back(dummySub);
                }
            }
        }
    }

    loadTranscript();
}

void Player::addSRT()
{
    if (m_playlist->isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setWindowFlags(Qt::Popup);
        msgBox.setText("Open and load a video before adding subtitles!");
        msgBox.exec();

        return;
    }

    //if playlist hasnt been initialised completely
    //(i.e. play button has not been pressed yet)
    if (currentIndex < 0)
    {
        m_player->play();
        m_player->pause();
    }

    QFileDialog fileDialog(this);
    fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    fileDialog.setWindowTitle(tr("Add SRT file"));
    fileDialog.setFileMode(QFileDialog::ExistingFile);
    fileDialog.setNameFilter(tr("Subtitles (*.srt)"));

    fileDialog.setDirectory(QStandardPaths::standardLocations(QStandardPaths::MoviesLocation).value(0, QDir::homePath()));

    if (fileDialog.exec() == QDialog::Accepted)
    {
        auto subtitle_Files = fileDialog.selectedFiles();

        for (auto subtitle_FileName : subtitle_Files)
        {
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

                subtitle_List[currentIndex] = fullSubtitles;

                file.close();
            }
        }
    }

    loadTranscript();
}

static bool isPlaylist(const QUrl &url) // Check for ".m3u" playlists.
{
    if (!url.isLocalFile())
        return false;
    const QFileInfo fileInfo(url.toLocalFile());
    return fileInfo.exists() && !fileInfo.suffix().compare(QLatin1String("m3u"), Qt::CaseInsensitive);
}

void Player::addToPlaylist(const QList<QUrl>& urls)
{
    for (auto url : urls)
    {
        if (isPlaylist(url))
        {
            m_playlist->load(url);
        }
        else
        {
            m_playlist->addMedia(url);
        }
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
    m_playlistView->setCurrentIndex(m_playlistModel->index(currentIndex, 0));

    //load transcript
    loadTranscript();

    m_transcript -> moveCursor(QTextCursor::Start) ;
}

void Player::seek(int seconds)
{
    m_player->setPosition(seconds * 1000);

    //reset cursor position so word finding can
    //start from beginning of doc
    auto cursor = m_transcript->textCursor();
    cursor.setPosition(0);
    m_transcript->setTextCursor(cursor);

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
