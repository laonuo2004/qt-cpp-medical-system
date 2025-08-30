#include "engine.h"
#include "loginpanel.h"
#include "patientclient.h"
#include "doctorclient.h"
#include "manager.h"
#include "uicontroller.h"

#include <QApplication>
#include <QDebug>

Engine::Engine(QObject *parent) : QObject(parent)
{
    m_mainWindow = nullptr;


    UiController& controller = UiController::instance();
    QObject::connect(&controller, &UiController::loginSuccessAdmin,
                     this, &Engine::onLoginSuccessAdmin);
    QObject::connect(&controller, &UiController::loginSuccessDoctor,
                     this, &Engine::onLoginSuccessDoctor);
    QObject::connect(&controller, &UiController::loginSuccessPatient,
                     this, &Engine::onLoginSuccessPatient);
}

Engine *Engine::m_engine = nullptr;

Engine *Engine::instance()
{
    if (!m_engine)
    {
        m_engine = new Engine;
    }
    return m_engine;
}

void Engine::destroy()
{
    if (m_engine)
    {
        delete m_engine;
        m_engine = nullptr;
    }
}

void Engine::startApplicationFlow() // 新的启动函数
{
    if (m_mainWindow != nullptr)
    {

        m_mainWindow->close();
        delete m_mainWindow;
        m_mainWindow = nullptr;
    }

    LoginPanel* loginPanel = new LoginPanel;

    // 连接 LoginPanel 的 rejected 信号到退出应用程序
    QObject::connect(loginPanel, &QDialog::rejected, &QApplication::quit);

    // 显示登录对话框，并等待其结果。
    // 如果 LoginPanel 接收到 UiController 的成功信号，它会调用 accept()。
    // 如果用户点击取消，它会调用 reject()。
    int result = loginPanel->exec();

    delete loginPanel; // 登录对话框完成后即可删除

    // 如果 m_mainWindow 在 onLoginSuccess* 槽中被成功设置，则显示它
    if (m_mainWindow)
    {
        m_mainWindow->show();
    }
    else
    {
        // 如果登录对话框被接受（即调用了 accept()），但 m_mainWindow 却没有被设置，
        // 这通常表示一个内部错误或者逻辑不符预期。
        // 或者用户拒绝了登录对话框，导致应用程序退出。
        qDebug() << "Application flow ended without main window. Login failed or cancelled.";
        // 如果 LoginPanel 被接受但没有主窗口（不应该发生），或者 LoginPanel 被拒绝
        // 且 QApplication::quit() 没有被调用，则在这里强制退出。
        // 实际上，如果 rejected，QApplication::quit() 已经连接并会执行。
        // 这里的 else 分支主要捕获的是 LoginPanel 接受了但 Engine 没能创建主窗口的异常情况。
        if (result == QDialog::Accepted) { // LoginPanel 被接受，但主窗口未创建
             QApplication::quit();
        }
    }
}

// Engine 的槽函数：处理管理员登录成功
void Engine::onLoginSuccessAdmin()
{
    qDebug() << "Engine 收到管理员登录成功信号。正在创建 Manager 窗口。";
    if (m_mainWindow) {
        delete m_mainWindow; // 清理旧窗口，以防万一
    }
    m_mainWindow = new Manager;
    // 此时不立即 show()，让 startApplicationFlow() 在 loginPanel 销毁后统一 show。
}

// Engine 的槽函数：处理医生登录成功
void Engine::onLoginSuccessDoctor()
{
    qDebug() << "Engine 收到医生登录成功信号。正在创建 DoctorClient 窗口。";
    if (m_mainWindow) {
        delete m_mainWindow;
    }
    m_mainWindow = new DoctorClient;
}

// Engine 的槽函数：处理患者登录成功
void Engine::onLoginSuccessPatient()
{
    qDebug() << "Engine 收到患者登录成功信号。正在创建 PatientClient 窗口。";
    if (m_mainWindow) {
        delete m_mainWindow;
    }
    m_mainWindow = new PatientClient;
}
