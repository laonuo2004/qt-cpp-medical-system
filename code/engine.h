#ifndef ENGINE_H
#define ENGINE_H

#include <QObject>
#include <QMainWindow>
#include "uicontroller.h"

class Engine : public QObject
{
    Q_OBJECT
public:
    explicit Engine(QObject *parent = nullptr);
    static Engine* instance();
    static void destroy();
    void startApplicationFlow();

private slots:
    // Engine 的槽函数，用于响应 UiController 的登录成功信号
    void onLoginSuccessAdmin();
    void onLoginSuccessDoctor();
    void onLoginSuccessPatient();
    // Engine 不处理登录失败，LoginPanel 会处理并显示消息

private:
    static Engine* m_engine;
    QMainWindow* m_mainWindow; // 使用 QMainWindow* 作为基类来管理不同类型的主窗口
};

#endif // ENGINE_H
