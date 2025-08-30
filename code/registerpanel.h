#ifndef REGISTERPANEL_H
#define REGISTERPANEL_H

#include <QDialog>
#include "databasemanager.h"
#include "uicontroller.h"
#include <QAbstractButton>
namespace Ui {
class RegisterPanel;
}

class RegisterPanel : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterPanel(QWidget *parent = nullptr);
    ~RegisterPanel();

private slots:


    void on_buttonBox_accepted();
    // 新增槽函数来处理注册结果
    void handleRegistrationSuccess();
    void handleRegistrationFailed(const QString &reason);

private:
    Ui::RegisterPanel *ui;
    UiController& m_controller; // 存储对 UiController 单例的引用
};

#endif // REGISTERPANEL_H
