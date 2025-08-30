#ifndef LOGINPANEL_H
#define LOGINPANEL_H

#include <QDialog>
#include "uicontroller.h"

namespace Ui {
class LoginPanel;
}

class LoginPanel : public QDialog
{
    Q_OBJECT

public:
    explicit LoginPanel(QWidget *parent = nullptr);
    ~LoginPanel();

private slots:
    void on_RegisterBtn_clicked();
    void on_LoginBtn_clicked();

    // 处理UiController发出的登录成功信号，LoginPanel只需要知道成功了就可以关闭
    void handleAnyLoginSuccess();
    // 处理UiController发出的登录失败信号
    void handleLoginFailed(const QString &reason);

private:
    Ui::LoginPanel *ui;
    UiController& m_controller; // 存储对 UiController 单例的引用
};

#endif // LOGINPANEL_H
