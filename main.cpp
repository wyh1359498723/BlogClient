#include "BlogClient.h"
#include <QtWidgets/QApplication>
#include <QSettings>
#include "src/database/DatabaseManager.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 设置应用程序信息，用于QSettings
    QCoreApplication::setOrganizationName("PersonalBlog");
    QCoreApplication::setApplicationName("BlogClient");
    
    // 初始化数据库
    if (!DatabaseManager::instance().initialize()) {
        qDebug() << "数据库初始化失败，应用程序可能无法正常工作";
    } else {
        qDebug() << "数据库初始化成功";
    }
    
    BlogClient w;
    w.show();
    return a.exec();
}
