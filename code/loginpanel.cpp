#include "loginpanel.h"
#include "ui_loginpanel.h"
#include "registerpanel.h"
#include "uicontroller.h"

#include <QMessageBox>
#include <QDebug>

LoginPanel::LoginPanel(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginPanel),
    m_controller(UiController::instance()) // 在构造函数初始化列表中获取 UiController 单例
{
    ui->setupUi(this);

    // 连接 UiController 的所有登录成功信号到 LoginPanel 的一个通用槽
    QObject::connect(&m_controller, &UiController::loginSuccessAdmin,
                     this, &LoginPanel::handleAnyLoginSuccess);
    QObject::connect(&m_controller, &UiController::loginSuccessDoctor,
                     this, &LoginPanel::handleAnyLoginSuccess);
    QObject::connect(&m_controller, &UiController::loginSuccessPatient,
                     this, &LoginPanel::handleAnyLoginSuccess);

    // 连接 UiController 的登录失败信号
    QObject::connect(&m_controller, &UiController::loginFailed,
                     this, &LoginPanel::handleLoginFailed);
}

LoginPanel::~LoginPanel()
{
    delete ui;
}

void LoginPanel::on_RegisterBtn_clicked()
{
    RegisterPanel* registerPanel = new RegisterPanel(this);
    if (registerPanel->exec() == QDialog::Rejected)
    {
        qDebug() << "用户取消注册或注册失败。";
    }

    delete registerPanel;
}

void LoginPanel::on_LoginBtn_clicked()
{
    QString email = ui->userInput->text();
    QString password = ui->pwdInput->text();

    // 调用 UiController 的登录方法，登录结果将通过信号异步通知
    m_controller.login(email, password);


}

// 槽函数：处理任何登录成功情况，只负责关闭 LoginPanel
void LoginPanel::handleAnyLoginSuccess()
{
    qDebug() << "LoginPanel 收到登录成功信号，正在关闭对话框。";
    accept(); // 关闭 LoginPanel 对话框
}

// 槽函数：处理登录失败情况
void LoginPanel::handleLoginFailed(const QString &reason)
{
    qDebug() << "登录失败：" << reason;
    QMessageBox::warning(this, tr("登录失败"), tr("登录失败，原因：") + reason);
    ui->pwdInput->clear(); // 清空密码，要求用户重新输入
    ui->pwdInput->setFocus(); // 将焦点设置回邮箱输入框
    // 保持 LoginPanel 打开
}
