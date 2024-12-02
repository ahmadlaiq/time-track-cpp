#include "TimeTrackerApp.h"
#include <QtWidgets>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHttpMultiPart>
#include <QBuffer>
#include <QCloseEvent>
#include <QDateTime>
#include <QSettings>

TimeTrackerApp::TimeTrackerApp(QWidget *parent)
    : QMainWindow(parent),
      isRunning(false),
      isPaused(false),
      timeElapsed(0),
      lastActivity(QDateTime::currentMSecsSinceEpoch()),
      isAfkDialogShown(false),
      selectedTaskId(-1),
      isSharingScreen(false)
{
    // Set your actual API URL here
    API_URL = "127.0.0.1:3000"; // Replace with your API URL

    networkManager = new QNetworkAccessManager(this);

    setupLoginUI();
    setupMainUI();

    showLoginUI();

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &TimeTrackerApp::updateTimer);

    afkTimer = new QTimer(this);
    afkTimer->setInterval(1000); // Check every second
    connect(afkTimer, &QTimer::timeout, this, &TimeTrackerApp::checkAfk);

    screenshotTimer = new QTimer(this);
    connect(screenshotTimer, &QTimer::timeout, this, &TimeTrackerApp::takeScreenshot);

    // Restore timer state if exists
    restoreTimerState();
}

TimeTrackerApp::~TimeTrackerApp()
{
    if (ffmpegProcess->state() == QProcess::Running) {
        ffmpegProcess->terminate();
        ffmpegProcess->waitForFinished();
    }
}

void TimeTrackerApp::setupLoginUI()
{
    loginWidget = new QWidget(this);
    emailLabel = new QLabel("Email:", this);
    emailEdit = new QLineEdit(this);
    passwordLabel = new QLabel("Password:", this);
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    loginButton = new QPushButton("Login", this);

    QVBoxLayout *layout = new QVBoxLayout(loginWidget);
    layout->addWidget(emailLabel);
    layout->addWidget(emailEdit);
    layout->addWidget(passwordLabel);
    layout->addWidget(passwordEdit);
    layout->addWidget(loginButton);

    connect(loginButton, &QPushButton::clicked, this, &TimeTrackerApp::handleLogin);
}

void TimeTrackerApp::setupMainUI()
{
    // Main UI components
    mainWidget = new QWidget(this);
    userInfoLabel = new QLabel(this);
    logoutButton = new QPushButton("Logout", this);

    taskComboBox = new QComboBox(this);

    timerLabel = new QLabel("00:00:00", this);
    timerLabel->setAlignment(Qt::AlignCenter);
    QFont font = timerLabel->font();
    font.setPointSize(24);
    timerLabel->setFont(font);

    startButton = new QPushButton("Start", this);
    pauseButton = new QPushButton("Pause", this);
    resumeButton = new QPushButton("Resume", this);
    stopButton = new QPushButton("Stop", this);

    startScreenShareButton = new QPushButton("Start Screen Sharing", this);
    stopScreenShareButton = new QPushButton("Stop Screen Sharing", this);
    stopScreenShareButton->setEnabled(false); // Initially disabled

    screenPreview = new QLabel(this);
    screenPreview->setMinimumSize(640, 360);
    screenPreview->setScaledContents(true);

    // Layout setup
    QVBoxLayout *layout = new QVBoxLayout(mainWidget);
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addWidget(userInfoLabel);
    topLayout->addWidget(logoutButton);

    layout->addLayout(topLayout);
    layout->addWidget(taskComboBox);
    layout->addWidget(timerLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(resumeButton);
    buttonLayout->addWidget(stopButton);

    layout->addLayout(buttonLayout);

    layout->addWidget(screenPreview);

    QHBoxLayout *screenShareLayout = new QHBoxLayout();
    screenShareLayout->addWidget(startScreenShareButton);
    screenShareLayout->addWidget(stopScreenShareButton);

    layout->addLayout(screenShareLayout);

    connect(logoutButton, &QPushButton::clicked, this, &TimeTrackerApp::showLoginUI);
    connect(startButton, &QPushButton::clicked, this, &TimeTrackerApp::startTimer);
    connect(pauseButton, &QPushButton::clicked, this, &TimeTrackerApp::pauseTimer);
    connect(resumeButton, &QPushButton::clicked, this, &TimeTrackerApp::resumeTimer);
    connect(stopButton, &QPushButton::clicked, this, &TimeTrackerApp::stopTimer);

    connect(startScreenShareButton, &QPushButton::clicked, this, &TimeTrackerApp::startScreenShare);
    connect(stopScreenShareButton, &QPushButton::clicked, this, &TimeTrackerApp::stopScreenShare);

    // AFK dialog
    afkDialog = new QDialog(this);
    afkDialog->setModal(true);
    afkDialog->setWindowTitle("No Activity Detected");
    QLabel *afkLabel = new QLabel("You have been inactive for 3 minutes. Would you like to pause the timer or continue?", afkDialog);
    QPushButton *afkPauseButton = new QPushButton("Pause", afkDialog);
    QPushButton *afkContinueButton = new QPushButton("Continue", afkDialog);
    QHBoxLayout *afkButtonLayout = new QHBoxLayout();
    afkButtonLayout->addWidget(afkPauseButton);
    afkButtonLayout->addWidget(afkContinueButton);
    QVBoxLayout *afkLayout = new QVBoxLayout(afkDialog);
    afkLayout->addWidget(afkLabel);
    afkLayout->addLayout(afkButtonLayout);

    connect(afkPauseButton, &QPushButton::clicked, this, [this]() {
        afkDialog->hide();
        pauseTimer();
        isAfkDialogShown = false;
    });
    connect(afkContinueButton, &QPushButton::clicked, this, [this]() {
        afkDialog->hide();
        lastActivity = QDateTime::currentMSecsSinceEpoch();
        isAfkDialogShown = false;
    });

    // Screen sharing setup
    captureTimer = new QTimer(this);
    ffmpegProcess = new QProcess(this);

    connect(captureTimer, &QTimer::timeout, this, &TimeTrackerApp::captureScreen);
    connect(ffmpegProcess, &QProcess::readyReadStandardOutput, this, &TimeTrackerApp::handleFFmpegOutput);
    connect(ffmpegProcess, QOverload<QProcess::ProcessError>::of(&QProcess::errorOccurred), this, &TimeTrackerApp::handleFFmpegError);
}

void TimeTrackerApp::showLoginUI()
{
    // Save timer state before logout
    if (isRunning || isPaused) {
        saveTimerState();
    }

    setCentralWidget(loginWidget);
}

void TimeTrackerApp::showMainUI()
{
    setCentralWidget(mainWidget);
}

void TimeTrackerApp::handleLogin()
{
    // Send login request

    QString email = emailEdit->text();
    QString password = passwordEdit->text();

    QUrl url(API_URL + "/api/v1/login");
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["email"] = email;
    json["password"] = password;
    QByteArray data = QJsonDocument(json).toJson();

    QNetworkReply *reply = networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleLoginResponse(reply);
    });
}

void TimeTrackerApp::handleLoginResponse(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc(QJsonDocument::fromJson(responseData));
        QJsonObject jsonObj = jsonDoc.object();
        bool success = jsonObj["success"].toBool();
        if (success) {
            // Parse user data
            QJsonObject dataObj = jsonObj["data"].toObject();
            token = dataObj["token"].toString();
            QJsonObject userObj = dataObj["user"].toObject();
            userName = userObj["name"].toString();
            userId = userObj["id"].toInt();

            tasks.clear();
            QJsonArray taskArray = userObj["task"].toArray();
            for (const QJsonValue &value : taskArray) {
                QJsonObject taskObj = value.toObject();
                QString taskName = taskObj["name"].toString();
                int taskId = taskObj["id"].toInt();
                tasks.append(qMakePair(taskName, taskId));
            }
            // Update UI
            userInfoLabel->setText("Welcome, " + userName);
            taskComboBox->clear();
            taskComboBox->addItem("Select Task", -1);
            for (const auto &task : tasks) {
                taskComboBox->addItem(task.first, task.second);
            }

            connect(taskComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
                selectedTaskId = taskComboBox->itemData(index).toInt();
            });

            showMainUI();
        } else {
            QMessageBox::warning(this, "Login Failed", jsonObj["message"].toString());
        }
    } else {
        QMessageBox::warning(this, "Network Error", reply->errorString());
    }
    reply->deleteLater();
}

void TimeTrackerApp::startTimer()
{
    if (!isRunning && selectedTaskId != -1) {
        // Start the timer
        isRunning = true;
        isPaused = false;
        timer->start(1000); // Update every second
        lastActivity = QDateTime::currentMSecsSinceEpoch();
        afkTimer->start();

        // Prepare request to start session
        QUrl url(API_URL + "/api/v1/track-time");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QString authHeader = "Bearer " + token;
        request.setRawHeader("Authorization", authHeader.toUtf8());

        QJsonObject json;
        json["duration"] = 0;
        json["taskId"] = selectedTaskId;
        json["userId"] = userId;

        QByteArray data = QJsonDocument(json).toJson();

        QNetworkReply *reply = networkManager->post(request, data);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray responseData = reply->readAll();
                QJsonDocument jsonDoc(QJsonDocument::fromJson(responseData));
                QJsonObject jsonObj = jsonDoc.object();
                currentSessionId = QString::number(jsonObj["id"].toInt()); // Assuming id is an integer
            } else {
                QMessageBox::warning(this, "Network Error", reply->errorString());
            }
            reply->deleteLater();
        });

        // Start screenshot timer
        screenshotTimer->start(30000); // Every 30 seconds

        // Connect activity signals
        qApp->installEventFilter(this);
    }
}

void TimeTrackerApp::pauseTimer()
{
    if (isRunning && !isPaused) {
        timer->stop();
        isPaused = true;
        isRunning = false;
        afkTimer->stop();
        screenshotTimer->stop();

        qApp->removeEventFilter(this);
    }
}

void TimeTrackerApp::resumeTimer()
{
    if (isPaused) {
        timer->start(1000);
        isPaused = false;
        isRunning = true;
        lastActivity = QDateTime::currentMSecsSinceEpoch();
        afkTimer->start();
        screenshotTimer->start(30000);

        qApp->installEventFilter(this);
    }
}

void TimeTrackerApp::stopTimer()
{
    if (isRunning || isPaused) {
        timer->stop();
        isRunning = false;
        isPaused = false;
        afkTimer->stop();
        screenshotTimer->stop();

        qApp->removeEventFilter(this);

        // Send the stop time to server

        QUrl url(API_URL + "/api/v1/track-time");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QString authHeader = "Bearer " + token;
        request.setRawHeader("Authorization", authHeader.toUtf8());

        QJsonObject json;
        json["id"] = currentSessionId.toInt();
        json["duration"] = timeElapsed;
        json["taskId"] = selectedTaskId;
        json["userId"] = userId;

        QByteArray data = QJsonDocument(json).toJson();

        QNetworkReply *reply = networkManager->post(request, data);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() != QNetworkReply::NoError) {
                QMessageBox::warning(this, "Network Error", reply->errorString());
            }
            reply->deleteLater();
        });

        // Take final screenshot
        takeScreenshot();

        // Reset timer
        timeElapsed = 0;
        timerLabel->setText("00:00:00");

        // Reset other state variables
        currentSessionId = QString();
    }
}

void TimeTrackerApp::updateTimer()
{
    timeElapsed++;
    int hours = timeElapsed / 3600;
    int minutes = (timeElapsed % 3600) / 60;
    int seconds = timeElapsed % 60;
    QString timeString = QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
    timerLabel->setText(timeString);
}

void TimeTrackerApp::resetLastActivity()
{
    if (isRunning && !isPaused) {
        lastActivity = QDateTime::currentMSecsSinceEpoch();

        // Dismiss AFK dialog if shown
        if (isAfkDialogShown) {
            afkDialog->hide();
            isAfkDialogShown = false;
        }
    }
}


void TimeTrackerApp::checkAfk()
{
    if (isRunning && !isPaused) {
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime - lastActivity >= 3 * 60 * 1000 && !isAfkDialogShown) {
            // User has been inactive for 3 minutes
            afkDialog->show();
            isAfkDialogShown = true;
            pauseTimer();
        }
    }
}

void TimeTrackerApp::takeScreenshot()
{
    // Capture screenshot and upload

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QPixmap screenshot = screen->grabWindow(0);
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        screenshot.save(&buffer, "PNG");

        // Prepare request
        QUrl url(API_URL + "/api/v1/upload-screenshot");
        QNetworkRequest request(url);
        QString authHeader = "Bearer " + token;
        request.setRawHeader("Authorization", authHeader.toUtf8());

        // Use QHttpMultiPart for file upload
        QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        QHttpPart imagePart;
        imagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"screenshot\"; filename=\"screenshot.png\""));
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
        imagePart.setBody(ba);

        QHttpPart sessionIdPart;
        sessionIdPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"sessionId\""));
        sessionIdPart.setBody(currentSessionId.toUtf8());

        multiPart->append(imagePart);
        multiPart->append(sessionIdPart);

        QNetworkReply *reply = networkManager->post(request, multiPart);
        multiPart->setParent(reply); // so that it will be deleted when reply is deleted

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            handleScreenshotUpload(reply);
        });
    }
}

void TimeTrackerApp::handleScreenshotUpload(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        // Handle error
        QMessageBox::warning(this, "Screenshot Upload Error", reply->errorString());
    }
    reply->deleteLater();
}

void TimeTrackerApp::saveTimerState()
{
    QSettings settings("YourCompany", "TimeTrackerApp");
    settings.setValue("timeElapsed", timeElapsed);
    settings.setValue("isRunning", isRunning);
    settings.setValue("isPaused", isPaused);
    settings.setValue("selectedTaskId", selectedTaskId);
    settings.setValue("currentSessionId", currentSessionId);
}

void TimeTrackerApp::restoreTimerState()
{
    QSettings settings("YourCompany", "TimeTrackerApp");
    timeElapsed = settings.value("timeElapsed", 0).toInt();
    isRunning = false; // Always start paused
    isPaused = settings.value("isPaused", false).toBool();
    selectedTaskId = settings.value("selectedTaskId", -1).toInt();
    currentSessionId = settings.value("currentSessionId", "").toString();
}

void TimeTrackerApp::closeEvent(QCloseEvent *event)
{
    if (isRunning || isPaused) {
        // Save timer state
        saveTimerState();
    }

    QMainWindow::closeEvent(event);
}

void TimeTrackerApp::startScreenShare()
{
    if (!isSharingScreen) {
        isSharingScreen = true;
        startScreenShareButton->setEnabled(false);
        stopScreenShareButton->setEnabled(true);

        // Start FFmpeg process for RTMP streaming

#ifdef Q_OS_WIN
        QStringList ffmpegArgs = {
            "-f", "gdigrab",  // Windows screen capture
            "-framerate", "30",
            "-i", "desktop",
            "-c:v", "libx264",
            "-preset", "ultrafast",
            "-tune", "zerolatency",
            "-f", "flv",
            "rtmp://localhost:1935/live/stream"
        };
#elif defined(Q_OS_LINUX)
        QStringList ffmpegArgs = {
            "-f", "x11grab", // Linux screen capture
            "-framerate", "30",
            "-i", ":0.0",
            "-c:v", "libx264",
            "-preset", "ultrafast",
            "-tune", "zerolatency",
            "-f", "flv",
            "rtmp://localhost:1935/live/stream"
        };
#else
        // MacOS or others
        QStringList ffmpegArgs = {
            "-f", "avfoundation",
            "-framerate", "30",
            "-i", "1",
            "-c:v", "libx264",
            "-preset", "ultrafast",
            "-tune", "zerolatency",
            "-f", "flv",
            "rtmp://localhost:1935/live/stream"
        };
#endif

        ffmpegProcess->start("ffmpeg", ffmpegArgs);

        if (!ffmpegProcess->waitForStarted()) {
            QMessageBox::critical(this, "Error", "Could not start FFmpeg");
            stopScreenShare();
            return;
        }

        captureTimer->start(100);
    }
}

void TimeTrackerApp::stopScreenShare()
{
    if (isSharingScreen) {
        isSharingScreen = false;
        startScreenShareButton->setEnabled(true);
        stopScreenShareButton->setEnabled(false);

        captureTimer->stop();
        screenPreview->clear();

        if (ffmpegProcess->state() == QProcess::Running) {
            ffmpegProcess->terminate();
            ffmpegProcess->waitForFinished();
        }
    }
}

void TimeTrackerApp::captureScreen()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap screenShot = screen->grabWindow(0);
    QPixmap scaledShot = screenShot.scaled(
        screenPreview->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    screenPreview->setPixmap(scaledShot);
}

void TimeTrackerApp::handleFFmpegOutput()
{
    QByteArray output = ffmpegProcess->readAllStandardOutput();
    qDebug() << "FFmpeg Output:" << output;
}

void TimeTrackerApp::handleFFmpegError(QProcess::ProcessError error)
{
    QString errorMessage;
    switch (error) {
        case QProcess::FailedToStart:
            errorMessage = "FFmpeg failed to start";
            break;
        case QProcess::Crashed:
            errorMessage = "FFmpeg crashed";
            break;
        default:
            errorMessage = "Unknown FFmpeg error";
    }
    QMessageBox::critical(this, "FFmpeg Error", errorMessage);
}

