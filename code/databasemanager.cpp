#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlField>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QDate>

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
        // 初始化数据库连接
        bool success = initConnection();
        emit connectionStatusChanged(success);

        if (success) {
            // 初始化数据库表结构
            if (!initDatabase()) {
                qWarning() << "数据库表结构初始化失败:" << lastError();
            }
        } else {
            qCritical() << "数据库连接初始化失败:" << lastError();
        }
    }
// 单例实例初始化
DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

    DatabaseManager::~DatabaseManager()
    {
        if (m_db.isOpen()) {
            m_db.close();
            emit connectionStatusChanged(false);
            qDebug() << "数据库连接已关闭";
        }
    }

    bool DatabaseManager::isConnected() const
    {
        return m_db.isOpen();
    }

    QString DatabaseManager::lastError() const
    {
        if (m_db.lastError().type() != QSqlError::NoError) {
            return m_db.lastError().text();
        }
        return m_lastError;
    }

    bool DatabaseManager::initDatabase()
    {
        if (!isConnected()) {
            m_lastError = "数据库未连接";
            return false;
        }

        return createTables();
    }

    bool DatabaseManager::execute(const QString &sql)
    {
        if (!isConnected()) {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return false;
        }

        QSqlQuery query(m_db);
        if (!query.exec(sql)) {
            m_lastError = query.lastError().text();
            emit errorOccurred(m_lastError);
            qWarning() << "SQL执行失败:" << sql << "错误:" << m_lastError;
            return false;
        }

        return true;
    }

    DatabaseManager::ResultSet DatabaseManager::query(const QString &sql)
    {
        ResultSet result;
        if (!isConnected()) {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return result;
        }

        QSqlQuery query(m_db);
        if (!query.exec(sql)) {
            m_lastError = query.lastError().text();
            emit errorOccurred(m_lastError);
            qWarning() << "查询失败:" << sql << "错误:" << m_lastError;
            return result;
        }

        // 解析查询结果
        QSqlRecord record = query.record();
        int columnCount = record.count();

        while (query.next()) {
            DataRow row;
            for (int i = 0; i < columnCount; ++i) {
                QString columnName = record.fieldName(i);
                QVariant value = query.value(i);
                row[columnName] = value;
            }
            result.push_back(row);
        }

        return result;
    }

    bool DatabaseManager::insert(const QString &tableName, const DataRow &data)
    {
        if (data.empty()) {
            m_lastError = "插入数据不能为空";
            emit errorOccurred(m_lastError);
            return false;
        }

        // 构建INSERT语句
        QString sql = QString("INSERT INTO %1 (").arg(tableName);
        QString valuesClause = "VALUES (";

        auto it = data.begin();
        while (it != data.end()) {
            sql += it->first;
            valuesClause += ":" + it->first;

            ++it;
            if (it != data.end()) {
                sql += ", ";
                valuesClause += ", ";
            }
        }

        sql += ") " + valuesClause + ")";

        // 执行插入
        QSqlQuery query(m_db);
        query.prepare(sql);

        for (const auto& pair : data) {
            query.bindValue(":" + pair.first, pair.second);
        }

        if (!query.exec()) {
            m_lastError = query.lastError().text();
            emit errorOccurred(m_lastError);
            qWarning() << "插入失败:" << sql << "错误:" << m_lastError;
            return false;
        }

        return true;
    }

    bool DatabaseManager::update(const QString &tableName, const DataRow &data, const QString &whereClause)
    {
        if (data.empty()) {
            m_lastError = "更新数据不能为空";
            emit errorOccurred(m_lastError);
            return false;
        }

        if (whereClause.isEmpty()) {
            m_lastError = "更新操作必须指定WHERE条件";
            emit errorOccurred(m_lastError);
            return false;
        }

        // 构建UPDATE语句
        QString sql = QString("UPDATE %1 SET ").arg(tableName);

        auto it = data.begin();
        while (it != data.end()) {
            sql += it->first + " = :" + it->first;

            ++it;
            if (it != data.end()) {
                sql += ", ";
            }
        }

        sql += " WHERE " + whereClause;

        // 执行更新
        QSqlQuery query(m_db);
        query.prepare(sql);

        for (const auto& pair : data) {
            query.bindValue(":" + pair.first, pair.second);
        }

        if (!query.exec()) {
            m_lastError = query.lastError().text();
            emit errorOccurred(m_lastError);
            qWarning() << "更新失败:" << sql << "错误:" << m_lastError;
            return false;
        }

        return true;
    }

    bool DatabaseManager::remove(const QString &tableName, const QString &whereClause)
    {
        if (whereClause.isEmpty()) {
            m_lastError = "删除操作必须指定WHERE条件";
            emit errorOccurred(m_lastError);
            return false;
        }

        QString sql = QString("DELETE FROM %1 WHERE %2").arg(tableName).arg(whereClause);
        return execute(sql);
    }

    bool DatabaseManager::beginTransaction()
    {
        if (!isConnected()) {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return false;
        }

        return m_db.transaction();
    }

    bool DatabaseManager::commitTransaction()
    {
        if (!isConnected()) {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return false;
        }

        return m_db.commit();
    }

    bool DatabaseManager::rollbackTransaction()
    {
        if (!isConnected()) {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return false;
        }

        return m_db.rollback();
    }

    QSqlDatabase DatabaseManager::database() const
    {
        return m_db;
    }

    bool DatabaseManager::initConnection(const QString &dbName)
    {
        // 检查是否已存在相同名称的连接
        if (QSqlDatabase::contains("medical_system_connection")) {
            m_db = QSqlDatabase::database("medical_system_connection");
        } else {
            // 创建新的数据库连接
            m_db = QSqlDatabase::addDatabase("QSQLITE", "medical_system_connection");
        }

        // 设置数据库文件路径（放在应用程序目录下）
        QString dbPath = QDir(QCoreApplication::applicationDirPath()).filePath(dbName);
        m_db.setDatabaseName(dbPath);

        // 打开数据库
        if (!m_db.open()) {
            m_lastError = m_db.lastError().text();
            emit errorOccurred("数据库打开失败: " + m_lastError);
            return false;
        }

        qDebug() << "数据库连接成功，路径:" << dbPath;
        return true;
    }

    bool DatabaseManager::createTables()
    {
        // 开始事务批量创建表
        if (!beginTransaction())
        {
            return false;
        }

        // 1. 用户表（存储所有角色用户）
        bool success = execute(R"(
            CREATE TABLE IF NOT EXISTS users (
                user_id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                password_hash TEXT NOT NULL,
                role TEXT NOT NULL CHECK(role IN ('admin', 'doctor', 'patient')),
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )");

        // 2. 管理员表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS admins (
                    admin_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id INTEGER NOT NULL UNIQUE,
                    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
                )
            )");
        }

        // 3. 患者表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS patients (
                    patient_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    user_id INTEGER NOT NULL UNIQUE,
                    full_name TEXT,
                    sex TEXT CHECK(sex IN ('男', '女')),
                    id_card_no TEXT UNIQUE,
                    birth_date DATE,
                    age INTEGER,
                    phone_no TEXT UNIQUE,
                    address TEXT,
                    sos_name TEXT,
                    sos_phone_no TEXT UNIQUE,
                    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
                )
            )");
        }

        // 4. 医生表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS doctors (
                    doctor_id TEXT PRIMARY KEY,
                    user_id INTEGER NOT NULL UNIQUE,
                    full_name TEXT,
                    sex TEXT CHECK(sex IN ('男', '女')),
                    age INTEGER,
                    department TEXT,
                    title TEXT,
                    phone_no TEXT UNIQUE,
                    doc_start TIME,
                    doc_finish TIME,
                    registration_fee TEXT,
                    patient_limit INTEGER,
                    photo_url TEXT,
                    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE CASCADE
                )
            )");
        }

        // 5. 预约表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS appointments (
                    appointment_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    patient_id INTEGER NOT NULL,
                    doctor_id TEXT NOT NULL,
                    appointment_date DATE NOT NULL,
                    appointment_time DATETIME NOT NULL,
                    status TEXT NOT NULL DEFAULT 'scheduled' CHECK(status IN ('scheduled', 'completed', 'cancelled')),
                    payment_status TEXT NOT NULL DEFAULT 'unpaid' CHECK(payment_status IN ('unpaid', 'paid')),
                    FOREIGN KEY (patient_id) REFERENCES patients(patient_id) ON DELETE CASCADE,
                    FOREIGN KEY (doctor_id) REFERENCES doctors(doctor_id) ON DELETE CASCADE
                )
            )");
        }

        // 6. 病历表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS medical_records (
                    record_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    appointment_id INTEGER NOT NULL UNIQUE,
                    diagnosis_notes TEXT,
                    diagnosis_date DATE,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (appointment_id) REFERENCES appointments(appointment_id) ON DELETE CASCADE
                )
            )");
        }

        // 7. 住院表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS hospitalizations (
                    hospitalization_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    record_id INTEGER NOT NULL UNIQUE,
                    doctor_id TEXT NOT NULL,
                    ward_no TEXT,
                    bed_no TEXT,
                    admission_date DATE,
                    discharge_date DATE,
                    FOREIGN KEY (record_id) REFERENCES medical_records(record_id) ON DELETE CASCADE,
                    FOREIGN KEY (doctor_id) REFERENCES doctors(doctor_id) ON DELETE CASCADE
                )
            )");
        }

        // 8. 处方表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS prescriptions (
                    prescription_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    record_id INTEGER NOT NULL,
                    details TEXT,
                    issued_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (record_id) REFERENCES medical_records(record_id) ON DELETE CASCADE
                )
            )");
        }

        // 9. 药品表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS drugs (
                    drug_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    drug_name TEXT NOT NULL UNIQUE,
                    description TEXT,
                    usage TEXT,
                    precautions TEXT,
                    drug_price REAL,
                    image_url TEXT
                )
            )");
        }

        // 10. 聊天消息表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS chat_messages (
                    message_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    sender_id INTEGER NOT NULL,
                    receiver_id INTEGER NOT NULL,
                    content TEXT NOT NULL,
                    sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (sender_id) REFERENCES users(user_id) ON DELETE CASCADE,
                    FOREIGN KEY (receiver_id) REFERENCES users(user_id) ON DELETE CASCADE
                )
            )");
        }

        // 11. 病历模板表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS case_templates (
                    template_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    created_by_doctor_id TEXT NOT NULL,
                    template_name TEXT NOT NULL,
                    default_diagnosis TEXT,
                    default_prescription TEXT,
                    FOREIGN KEY (created_by_doctor_id) REFERENCES doctors(doctor_id) ON DELETE CASCADE
                )
            )");
        }

        // 12. 考勤表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS attendance (
                    attendance_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    doctor_id TEXT NOT NULL,
                    date DATE NOT NULL,
                    check_in_time TIME,
                    check_out_time TIME,
                    FOREIGN KEY (doctor_id) REFERENCES doctors(doctor_id) ON DELETE CASCADE,
                    UNIQUE(doctor_id, date)
                )
            )");
        }

        // 13. 请假申请表
        if (success)
        {
            success = execute(R"(
                CREATE TABLE IF NOT EXISTS leave_requests (
                    request_id INTEGER PRIMARY KEY AUTOINCREMENT,
                    doctor_id TEXT NOT NULL,
                    request_type TEXT NOT NULL CHECK(request_type IN ('因公', '因私')),
                    start_date DATE NOT NULL,
                    end_date DATE NOT NULL,
                    reason TEXT,
                    status TEXT NOT NULL DEFAULT '未销假' CHECK(status IN ('已销假', '未销假')),
                    FOREIGN KEY (doctor_id) REFERENCES doctors(doctor_id) ON DELETE CASCADE
                )
            )");
        }

        // 根据事务结果提交或回滚
        if (success)
        {
            success = commitTransaction();
            qDebug() << "数据库表结构创建/验证成功";
        }
        else
        {
            rollbackTransaction();
            qWarning() << "数据库表结构创建失败";
        }

        return success;
    }

    // --- 高级查询接口实现 ---

    DatabaseManager::ResultSet DatabaseManager::getDoctorsByDepartment(const QString& department)
    {
        if (!isConnected())
        {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return {};
        }

        QString sql;
        if (department.isEmpty())
        {
            // 查询所有医生
            sql = R"(
                SELECT d.doctor_id, d.full_name, d.sex, d.age, d.department, d.title,
                       d.phone_no, d.doc_start, d.doc_finish, d.registration_fee,
                       d.patient_limit, d.photo_url, u.email
                FROM doctors d
                JOIN users u ON d.user_id = u.user_id
                ORDER BY d.department, d.full_name
            )";
        }
        else
        {
            sql = QString(R"(
                SELECT d.doctor_id, d.full_name, d.sex, d.age, d.department, d.title,
                       d.phone_no, d.doc_start, d.doc_finish, d.registration_fee,
                       d.patient_limit, d.photo_url, u.email
                FROM doctors d
                JOIN users u ON d.user_id = u.user_id
                WHERE d.department = '%1'
                ORDER BY d.full_name
            )").arg(department);
        }

        return query(sql);
    }

    DatabaseManager::ResultSet DatabaseManager::getPatientAppointments(int patientId, const QDate& date)
    {
        if (!isConnected())
        {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return {};
        }

        QString sql = QString(R"(
            SELECT a.appointment_id, a.appointment_date, a.appointment_time,
                   a.status, a.payment_status,
                   d.doctor_id, d.full_name as doctor_name, d.department, d.title,
                   d.registration_fee
            FROM appointments a
            JOIN doctors d ON a.doctor_id = d.doctor_id
            WHERE a.patient_id = %1
        )").arg(patientId);

        if (date.isValid())
        {
            sql += QString(" AND a.appointment_date = '%1'").arg(date.toString("yyyy-MM-dd"));
        }

        sql += " ORDER BY a.appointment_time DESC";

        return query(sql);
    }

    DatabaseManager::ResultSet DatabaseManager::getDoctorSchedule(const QString& doctorId, const QDate& date)
    {
        if (!isConnected())
        {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return {};
        }

        QString sql = QString(R"(
            SELECT a.appointment_time, a.status,
                   p.full_name as patient_name, p.phone_no as patient_phone
            FROM appointments a
            JOIN patients p ON a.patient_id = p.patient_id
            WHERE a.doctor_id = '%1' AND a.appointment_date = '%2'
            ORDER BY a.appointment_time
        )").arg(doctorId).arg(date.toString("yyyy-MM-dd"));

        return query(sql);
    }

    bool DatabaseManager::isTimeSlotAvailable(const QString& doctorId, const QDateTime& dateTime)
    {
        if (!isConnected())
        {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return false;
        }

        // 检查医生是否存在
        QString doctorCheckSql = QString("SELECT COUNT(*) FROM doctors WHERE doctor_id = '%1'").arg(doctorId);
        ResultSet doctorResult = query(doctorCheckSql);
        if (doctorResult.empty() || doctorResult[0].begin()->second.toInt() == 0)
        {
            m_lastError = "医生不存在";
            return false;
        }

        // 检查时间槽是否已被预约
        QString sql = QString(R"(
            SELECT COUNT(*)
            FROM appointments
            WHERE doctor_id = '%1'
            AND appointment_time = '%2'
            AND status != 'cancelled'
        )").arg(doctorId).arg(dateTime.toString("yyyy-MM-dd hh:mm:ss"));

        ResultSet result = query(sql);
        if (result.empty())
        {
            return false;
        }

        return result[0].begin()->second.toInt() == 0;
    }

    DatabaseManager::ResultSet DatabaseManager::getPatientMedicalRecords(int patientId)
    {
        if (!isConnected())
        {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return {};
        }

        QString sql = QString(R"(
            SELECT mr.record_id, mr.diagnosis_notes, mr.diagnosis_date, mr.created_at,
                   a.appointment_time,
                   d.doctor_id, d.full_name as doctor_name, d.department, d.title,
                   GROUP_CONCAT(pr.details, '; ') as prescriptions
            FROM medical_records mr
            JOIN appointments a ON mr.appointment_id = a.appointment_id
            JOIN doctors d ON a.doctor_id = d.doctor_id
            LEFT JOIN prescriptions pr ON mr.record_id = pr.record_id
            WHERE a.patient_id = %1
            GROUP BY mr.record_id
            ORDER BY mr.created_at DESC
        )").arg(patientId);

        return query(sql);
    }

    DatabaseManager::ResultSet DatabaseManager::getChatHistory(int user1Id, int user2Id, int limit)
    {
        if (!isConnected())
        {
            m_lastError = "数据库未连接";
            emit errorOccurred(m_lastError);
            return {};
        }

        QString sql = QString(R"(
            SELECT cm.message_id, cm.sender_id, cm.receiver_id, cm.content, cm.sent_at,
                   u1.username as sender_name, u2.username as receiver_name
            FROM chat_messages cm
            JOIN users u1 ON cm.sender_id = u1.user_id
            JOIN users u2 ON cm.receiver_id = u2.user_id
            WHERE (cm.sender_id = %1 AND cm.receiver_id = %2)
               OR (cm.sender_id = %2 AND cm.receiver_id = %1)
            ORDER BY cm.sent_at DESC
            LIMIT %3
        )").arg(user1Id).arg(user2Id).arg(limit);

        return query(sql);
    }
