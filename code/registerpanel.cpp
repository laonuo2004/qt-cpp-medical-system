#include "registerpanel.h"
#include "ui_registerpanel.h"
#include <QDebug>
#include "databasemanager.h"
#include "uicontroller.h"
#include <QMessageBox>

RegisterPanel::RegisterPanel(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegisterPanel),
     m_controller(UiController::instance()) // 在构造函数中获取单例引用
{
    ui->setupUi(this);
    // 注意：这里连接的是 m_controller 的信号
    QObject::connect(&m_controller, &UiController::registrationSuccess,
                     this, &RegisterPanel::handleRegistrationSuccess);

    QObject::connect(&m_controller, &UiController::registrationFailed,
                     this, &RegisterPanel::handleRegistrationFailed);
    // 清空现有项目 (可选)
   ui->RoleInput->clear();

   // 添加项目并设置关联的 UserRole 数据
   ui->RoleInput->addItem("管理员", QVariant::fromValue(UserRole::Admin));
   ui->RoleInput->addItem("医生", QVariant::fromValue(UserRole::Doctor));
   ui->RoleInput->addItem("患者", QVariant::fromValue(UserRole::Patient));

   // 设置默认选择 (可选)
   ui->RoleInput->setCurrentIndex(2); // 默认选择 "患者"

}

RegisterPanel::~RegisterPanel()
{
    delete ui;
}



void RegisterPanel::on_buttonBox_accepted()
{

    UiController& controller = UiController::instance();
    QString username = ui->UserNameInput->text();
    QString email = ui->EmailInput->text();
    QString password = ui->PwdInput->text();
    // 从 QComboBox 中获取当前选中项的自定义数据
    QVariant selectedRoleData = ui->RoleInput->currentData();

    // 检查是否成功获取到数据，并尝试转换为 UserRole
    if (selectedRoleData.isValid() && selectedRoleData.canConvert<UserRole>())
    {
        UserRole role = selectedRoleData.value<UserRole>();

    // Direct call from C++
    controller.registerUser(username, email, password, role);
    }
}
// 槽函数：处理注册成功
void RegisterPanel::handleRegistrationSuccess()
{
    qDebug() << "User registered successfully!";
    QMessageBox::information(this, "注册成功", "恭喜，用户注册成功！");
    // 如果 RegisterPanel 是一个 QDialog，成功后可以关闭它
    accept(); // 或者 close();
    // 如果它是一个 QWidget 且不是独立的窗口，你可能需要父窗口来切换界面
}

// 槽函数：处理注册失败
void RegisterPanel::handleRegistrationFailed(const QString &reason)
{
    qDebug() << "User registration failed:" << reason;
    // 注册失败时，显示错误信息，并保持当前界面（RegisterPanel）打开
    QMessageBox::warning(this, "注册失败", "注册失败，原因：" + reason);

    // 此时 RegisterPanel 界面会保持不变，用户可以修改输入再次尝试
}
