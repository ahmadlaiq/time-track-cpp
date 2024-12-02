#ifndef TIMETRACKERAPP_H
#define TIMETRACKERAPP_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QProcess>

class QLabel;
class QLineEdit;
class QPushButton;
class QTimer;
class QComboBox;
class QDialog;
class QNetworkReply;

class TimeTrackerApp : public QMainWindow
{
    Q_OBJECT
public:
    explicit TimeTrackerApp(QWidget *parent = nullptr);
    ~TimeTrackerApp();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Login slots
    void handleLogin();
    void handleLoginResponse(QNetworkReply* reply);

    // Timer control slots
    void startTimer();
    void pauseTimer();
    void resumeTimer();
    void stopTimer();
    void updateTimer();

    // AFK detection
    void resetLastActivity();
    void checkAfk();

    // Screenshot
    void takeScreenshot();
    void handleScreenshotUpload(QNetworkReply* reply);

    // Screen sharing
    void startScreenShare();
    void stopScreenShare();
    void captureScreen();
    void handleFFmpegOutput();
    void handleFFmpegError(QProcess::ProcessError error);

private:
    // UI components
    QWidget *loginWidget;
    QLabel *emailLabel;
    QLineEdit *emailEdit;
    QLabel *passwordLabel;
    QLineEdit *passwordEdit;
    QPushButton *loginButton;

    QWidget *mainWidget;
    QLabel *userInfoLabel;
    QPushButton *logoutButton;
    QComboBox *taskComboBox;
    QLabel *timerLabel;
    QPushButton *startButton;
    QPushButton *pauseButton;
    QPushButton *resumeButton;
    QPushButton *stopButton;

    QPushButton *startScreenShareButton;
    QPushButton *stopScreenShareButton;
    QLabel *screenPreview;

    QDialog *afkDialog;

    // Network
    QNetworkAccessManager *networkManager;
    QString token;

    // Timer
    int timeElapsed; // seconds
    QTimer *timer;
    bool isRunning;
    bool isPaused;

    // AFK detection
    QTimer *afkTimer;
    qint64 lastActivity;
    bool isAfkDialogShown;

    // Screenshot
    QTimer *screenshotTimer;

    // Screen sharing
    QProcess *ffmpegProcess;
    bool isSharingScreen;
    QTimer *captureTimer;

    // Other
    void setupLoginUI();
    void setupMainUI();
    void showLoginUI();
    void showMainUI();

    QString API_URL;

    // User data
    QString userName;
    int userId;
    QList<QPair<QString, int>> tasks; // Task name and ID
    int selectedTaskId;
    QString currentSessionId;

    // Helper methods
    void saveTimerState();
    void restoreTimerState();
};

#endif // TIMETRACKERAPP_H
