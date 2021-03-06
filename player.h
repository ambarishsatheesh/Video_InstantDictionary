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

#ifndef PLAYER_H
#define PLAYER_H

#include <QWidget>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <thread>
#include <QTextCursor>
#include <QNetworkAccessManager>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QMenu>

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QLabel;
class QTextEdit;
class QMediaPlayer;
class QModelIndex;
class QPushButton;
class QSlider;
class QStatusBar;
class QVideoProbe;
class QVideoWidget;
class QAudioProbe;
QT_END_NAMESPACE

class PlaylistModel;
class HistogramWidget;

class Player : public QWidget
{
    Q_OBJECT

public:
    explicit Player(QWidget *parent = nullptr);
    ~Player();

    bool isPlayerAvailable() const;

    void addToPlaylist(const QList<QUrl> &urls);
    void setCustomAudioRole(const QString &role);

signals:
    void drawSubtitles_signal(QString subtitle);

private slots:
    void open();
    void durationChanged(qint64 duration);
    void positionChanged(qint64 progress);
    void metaDataChanged();

    void previousClicked();

    void seek(int seconds);
    void jump(const QModelIndex &index);
    void playlistPositionChanged(int);

    void statusChanged(QMediaPlayer::MediaStatus status);
    void bufferingProgress(int progress);
    //void videoAvailableChanged(bool available);

    void displayErrorMessage();
    void drawSubtitles(QString subtitle);
    void wordHighlighted(bool yes);

    void managerFinished(QNetworkReply *reply);

private:
    qint64 SRTStartTime_to_milliseconds(QString subtitle_time);
    qint64 SRTEndTime_to_milliseconds(QString subtitle_time);
    bool isWithinSubPeriod(qint64 curPos, QString subtitle_time);
    QString format_time(int time);
    void processSubtitles();
    void highlight_currentLine();
    void loadTranscript();

    void setTrackInfo(const QString &info);
    void setStatusInfo(const QString &info);
    void handleCursor(QMediaPlayer::MediaStatus status);
    void updateDurationInfo(qint64 currentInfo);

    void closeEvent (QCloseEvent *event);

    QMediaPlayer *m_player = nullptr;
    QMediaPlaylist *m_playlist = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QLabel *m_coverLabel = nullptr;
    QSlider *m_slider = nullptr;
    QLabel *m_labelDuration = nullptr;
    QLabel *m_statusLabel = nullptr;
    QStatusBar *m_statusBar = nullptr;

    PlaylistModel *m_playlistModel = nullptr;
    QAbstractItemView *m_playlistView = nullptr;
    QTextEdit *m_transcript = nullptr;
    QString m_trackInfo;
    QString m_statusInfo;
    qint64 m_duration;

    //subtitles
    int currentIndex;
    QTextEdit * m_subtitles = nullptr;
    QList<QStringList> subtitle_List;
    void addSRT();

    //thread to draw subs
    std::thread subtitle_thread;
    std::thread highlightline_thread;
    bool threadRun = true;

    //cursor
    void moveScrollBar();
    void setTranscriptPosition();

    //dictionary API
    const QByteArray app_id = "a74a5872";
    const QByteArray app_key = "46564d304f6f015945afbc97336f4f3c";
    QNetworkAccessManager *manager = nullptr;
    QNetworkRequest request;
    QNetworkReply *reply;
    QString curSelectedWord;
    void APIRequest();
    QString parse_JSON_Response(QByteArray answer);

    //dictionary popup
    QByteArray dict_answer;
    QString outputString;
    QDialog* definition_dialog;
    QTextEdit* dictionaryOutput;
    QHBoxLayout* dialog_layout;
    QScrollArea* scroll;

    //right click menu
    bool isDefMenu_constructed = false;
    QMenu* contextMenu = nullptr;
    QAction* a_getDefinition = nullptr;
    QString current_word;
    void getWord();
    void showContextMenu(const QPoint &pos);
};

#endif // PLAYER_H
