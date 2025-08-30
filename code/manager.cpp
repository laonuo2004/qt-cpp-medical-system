#include "manager.h"
#include "ui_manager.h"
#include "engine.h"

Manager::Manager(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Manager)
{
    ui->setupUi(this);
    connect(ui->LogoutBtn, &QPushButton::clicked, Engine::instance(), &Engine::startApplicationFlow);
}

Manager::~Manager()
{
    delete ui;
}
