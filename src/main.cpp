#include <QApplication>
#include "TimeTrackerApp.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    TimeTrackerApp app;
    app.show();

    return a.exec();
}
