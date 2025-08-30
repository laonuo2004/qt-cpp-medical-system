#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>
#include <map>
#include <vector>
#include <memory>
#include <QDate>

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    // 单例模式获取实例
    static DatabaseManager& instance();

    // 禁止拷贝构造和赋值操作
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    // 数据行表示
    using DataRow = std::map<QString, QVariant>;
    // 结果集表示
    using ResultSet = std::vector<DataRow>;

    // 检查数据库是否连接
    bool isConnected() const;

    // 获取最后一次错误信息
    QString lastError() const;

    // 初始化数据库，创建必要的表
    bool initDatabase();

    // 执行非查询SQL语句
    bool execute(const QString& sql);

    // 执行查询SQL语句
    ResultSet query(const QString& sql);

    // 插入数据
    bool insert(const QString& tableName, const DataRow& data);

    // 更新数据
    bool update(const QString& tableName, const DataRow& data, const QString& whereClause);

    // 删除数据
    bool remove(const QString& tableName, const QString& whereClause);

    // 事务操作
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // 获取数据库连接对象(谨慎使用)
    QSqlDatabase database() const;

    // --- 高级查询接口 ---

    /**
     * @brief 根据科室获取医生列表
     *
     * @param department 科室名称
     * @return ResultSet 医生信息结果集
     */
    ResultSet getDoctorsByDepartment(const QString& department);

    /**
     * @brief 获取患者的预约信息
     *
     * @param patientId 患者ID
     * @param date 查询日期（为空则查询所有）
     * @return ResultSet 预约信息结果集
     */
    ResultSet getPatientAppointments(int patientId, const QDate& date = QDate());

    /**
     * @brief 获取医生的排班信息
     *
     * @param doctorId 医生ID
     * @param date 查询日期
     * @return ResultSet 排班信息结果集
     */
    ResultSet getDoctorSchedule(const QString& doctorId, const QDate& date);

    /**
     * @brief 检查医生在指定时间是否可预约
     *
     * @param doctorId 医生ID
     * @param dateTime 预约时间
     * @return bool 是否可预约
     */
    bool isTimeSlotAvailable(const QString& doctorId, const QDateTime& dateTime);

    /**
     * @brief 获取患者的病历记录
     *
     * @param patientId 患者ID
     * @return ResultSet 病历记录结果集
     */
    ResultSet getPatientMedicalRecords(int patientId);

    /**
     * @brief 获取聊天历史记录
     *
     * @param user1Id 用户1 ID
     * @param user2Id 用户2 ID
     * @param limit 限制条数（默认50条）
     * @return ResultSet 聊天记录结果集
     */
    ResultSet getChatHistory(int user1Id, int user2Id, int limit = 50);

    // 可以根据需求添加更多高级查询接口

signals:
    // 数据库连接状态变化信号
    void connectionStatusChanged(bool isConnected);

    // 错误发生信号
    void errorOccurred(const QString& errorMessage);

private:
    // 私有构造函数，实现单例模式
    explicit DatabaseManager(QObject *parent = nullptr);

    // 析构函数
    ~DatabaseManager() override;

    // 初始化数据库连接
    bool initConnection(const QString& dbName = "medical_system.db");

    // 创建数据表
    bool createTables();

    // 数据库连接对象
    QSqlDatabase m_db;

    // 最后一次错误信息
    mutable QString m_lastError;
};

#endif // DATABASEMANAGER_H
