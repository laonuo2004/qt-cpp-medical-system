#include "uicontroller.h"
#include <QDebug>
#include <QDateTime> // 用于验证码的过期时间（可选，但推荐）

// --- 辅助函数实现 ---
UiController& UiController::instance()
{
    static UiController instance;
    return instance;
}
bool UiController::ensureDbConnected(const char* where)
{
    if (!DatabaseManager::instance().isConnected())
    {
        qCritical() << "数据库未连接，位置：" << where;
        return false;
    }
    return true;
}

QVariantMap UiController::rowToMap(const DatabaseManager::DataRow& row)
{
    QVariantMap map;
    for (const auto& pair : row)
    {
        map[pair.first] = pair.second;
    }
    return map;
}

QVariantList UiController::resultToList(const DatabaseManager::ResultSet& rs)
{
    QVariantList list;
    for (const auto& row : rs)
    {
        list.append(rowToMap(row));
    }
    return list;
}

int UiController::getCountFromResult(const DatabaseManager::ResultSet& result)
{
    if (result.empty())
    {
        return 0;
    }
    return result.front().begin()->second.toInt();
}

UiController::UiController(QObject *parent) : QObject(parent)
{
    // 确保数据库连接已初始化并创建了表
    if (!ensureDbConnected(__func__))
    {
        qCritical() << "数据库连接失败！";
        // 可以发出一个全局错误信号或者在UI上显示错误
    }
}
// --- 1. 认证和用户管理后端 ---
// 登录验证方法
void UiController::login(const QString &email, const QString &password)
{
    if (email.isEmpty() || password.isEmpty()) {
        emit loginFailed("邮箱或密码不能为空。");
        return;
    }

    if (!ensureDbConnected(__func__)) {
        emit loginFailed("数据库未连接。");
        return;
    }

    QString sql = QString("SELECT username, password_hash, role FROM users WHERE email = '%1'").arg(email);
    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    if (result.empty())
    {
        emit loginFailed("邮箱未注册，请先注册。");
        return;
    }

    // 找到了用户
    DatabaseManager::DataRow userRow = result[0];
    QString storedPasswordHash = userRow["password_hash"].toString();
    QString roleString = userRow["role"].toString(); // 从数据库获取角色字符串

    if (storedPasswordHash == hashPassword(password))
    {
        // 登录成功
        qDebug() << "用户" << email << "登录成功，角色为" << roleString;
        if (roleString == "admin")
        {
            emit loginSuccessAdmin();
        }
        else if (roleString == "doctor")
        {
            emit loginSuccessDoctor();
        }
        else if (roleString == "patient")
        {
            emit loginSuccessPatient();
        }
        else
        {
            emit loginFailed("未知用户角色。");
        }
    }
    else
    {
        emit loginFailed("密码不正确。");
    }
}

// 注册方法
void UiController::registerUser(const QString &username, const QString &email, const QString &password, UserRole role)
{
    if (username.isEmpty() || email.isEmpty() || password.isEmpty()) {
        emit registrationFailed("用户名、邮箱或密码不能为空。");
        return;
    }

    if (!ensureDbConnected(__func__)) {
        emit registrationFailed("数据库未连接。");
        return;
    }

    if (isEmailRegistered(email)) {
        emit registrationFailed("该邮箱已被注册。");
        return;
    }

    if (!isPasswordComplex(password)) {
        emit registrationFailed("密码不符合复杂度要求：至少包含8位，大小写字母，数字和特殊字符。");
        return;
    }

    DatabaseManager::DataRow userData;
    userData["username"] = username;
    userData["email"] = email;
    userData["password_hash"] = hashPassword(password);

    // 将枚举转换为字符串存储
    QString roleString;
    switch (role)
    {
        case UserRole::Admin:
            roleString = "admin";
            break;
        case UserRole::Doctor:
            roleString = "doctor";
            break;
        case UserRole::Patient:
            roleString = "patient";
            break;
    }
    userData["role"] = roleString;

    if (DatabaseManager::instance().insert("users", userData))
    {
        qDebug() << "用户" << email << "注册成功";
        emit registrationSuccess();
    }
    else
    {
        qCritical() << "用户注册失败：" << DatabaseManager::instance().lastError();
        emit registrationFailed("注册失败：" + DatabaseManager::instance().lastError());
    }
}

// 检查邮箱是否已注册
bool UiController::isEmailRegistered(const QString &email)
{
    if (!ensureDbConnected(__func__))
    {
        qWarning() << "邮箱检查时数据库未连接";
        return false; // 如果数据库未连接，暂时认为未注册
    }

    QString sql = QString("SELECT COUNT(*) FROM users WHERE email = '%1'").arg(email);
    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    if (result.empty())
    {
        qWarning() << "邮箱注册检查查询返回空结果";
        return false;
    }

    // 获取第一行第一列的值（COUNT(*)）
    return result[0].begin()->second.toInt() > 0;
}

// 检查密码复杂度
bool UiController::isPasswordComplex(const QString &password)
{
    // 至少8位
    if (password.length() < 8) return false;

    bool hasLower = false;
    bool hasUpper = false;
    bool hasDigit = false;
    bool hasSpecial = false;

    // 检查大小写字母、数字、特殊字符
    for (QChar c : password) {
        if (c.isLower()) hasLower = true;
        else if (c.isUpper()) hasUpper = true;
        else if (c.isDigit()) hasDigit = true;
        else if (!c.isLetterOrNumber()) hasSpecial = true; // 认为非字母数字即为特殊字符
    }

    return hasLower && hasUpper && hasDigit && hasSpecial;
}

// 找回密码请求
void UiController::forgotPassword(const QString &email)
{
    if (email.isEmpty()) {
        emit forgotPasswordRequestFailed("邮箱不能为空。");
        return;
    }

    if (!ensureDbConnected(__func__)) {
        emit forgotPasswordRequestFailed("数据库未连接。");
        return;
    }

    if (!isEmailRegistered(email)) {
        emit forgotPasswordRequestFailed("该邮箱未注册。");
        return;
    }

    // 模拟发送验证码到邮箱
    QString verificationCode = generateVerificationCode();
    // 存储验证码，可以考虑存储一个时间戳，用于判断验证码是否过期
    m_verificationCodes.insert(email, verificationCode);

    qDebug() << "为" << email << "生成验证码：" << verificationCode;
    // 实际应用中，这里会调用邮件发送服务
    emit forgotPasswordRequestSuccess(email);
}

// 验证验证码并重置密码
void UiController::resetPassword(const QString &email, const QString &verificationCode, const QString &newPassword)
{
    if (email.isEmpty() || verificationCode.isEmpty() || newPassword.isEmpty()) {
        emit passwordResetFailed("邮箱、验证码或新密码不能为空。");
        return;
    }

    if (!ensureDbConnected(__func__)) {
        emit passwordResetFailed("数据库未连接。");
        return;
    }

    if (!m_verificationCodes.contains(email) || m_verificationCodes.value(email) != verificationCode) {
        emit passwordResetFailed("验证码不正确或已过期。");
        return;
    }

    if (!isPasswordComplex(newPassword)) {
        emit passwordResetFailed("新密码不符合复杂度要求：至少包含8位，大小写字母，数字和特殊字符。");
        return;
    }

    DatabaseManager::DataRow updateData;
    updateData["password_hash"] = hashPassword(newPassword);
    QString whereClause = QString("email = '%1'").arg(email);

    if (DatabaseManager::instance().update("users", updateData, whereClause))
    {
        m_verificationCodes.remove(email); // 移除已使用的验证码
        qDebug() << "用户" << email << "密码重置成功";
        emit passwordResetSuccess();
    }
    else
    {
        qCritical() << "密码重置失败：" << DatabaseManager::instance().lastError();
        emit passwordResetFailed("密码重置失败：" + DatabaseManager::instance().lastError());
    }
}

// 用于生成和验证密码哈希
QString UiController::hashPassword(const QString &password)
{
    // 同样，生产环境中请使用更安全的哈希算法和盐
    QByteArray passwordBytes = password.toUtf8();
    return QString(QCryptographicHash::hash(passwordBytes, QCryptographicHash::Sha256).toHex());
}

// 生成随机验证码
QString UiController::generateVerificationCode()
{
    const QString possibleCharacters("0123456789"); // 验证码通常是数字
    const int codeLength = 6;
    QString randomString;
    for (int i = 0; i < codeLength; ++i) {
        int index = QRandomGenerator::global()->bounded(possibleCharacters.length());
        randomString.append(possibleCharacters.at(index));
    }
    return randomString;
}
// --- 2. 个人信息后端 ---
QVariantMap UiController::getPatientInfo(int userId)
{
    if (!ensureDbConnected(__func__))
    {
        return {};
    }

    // 使用 LEFT JOIN 联合查询，确保即使用户还没有详细信息也能查出用户名
    QString sql = QString(
        "SELECT u.username, u.email, p.* "
        "FROM users u "
        "LEFT JOIN patients p ON u.user_id = p.user_id "
        "WHERE u.user_id = %1"
    ).arg(userId);

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    if (result.empty())
    {
        qWarning() << "未找到用户ID为" << userId << "的用户";
        return {}; // 没有找到用户
    }

    return rowToMap(result.front());
}

bool UiController::updatePatientInfo(int userId, const QVariantMap &details)
{
    if (!ensureDbConnected(__func__)) {
        return false;
    }

    // 开启事务，确保数据一致性
    if (!DatabaseManager::instance().beginTransaction()) {
        qCritical() << "开启事务失败:" << DatabaseManager::instance().lastError();
        return false;
    }

    // 1. 更新 users 表中的姓名 (username)
    if (details.contains("username"))
    {
        DatabaseManager::DataRow userData;
        userData["username"] = details["username"];
        QString userWhereClause = QString("user_id = %1").arg(userId);
        if (!DatabaseManager::instance().update("users", userData, userWhereClause))
        {
            qCritical() << "更新用户名失败:" << DatabaseManager::instance().lastError();
            DatabaseManager::instance().rollbackTransaction();
            return false;
        }
    }

    // 2. 更新或插入 patients 表
    DatabaseManager::DataRow patientData;
    // 遍历传入的数据，填充到 DataRow 中
    for(auto it = details.constBegin(); it != details.constEnd(); ++it)
    {
        // "username" 字段不属于 patients 表，跳过
        if (it.key() != "username")
        {
            patientData[it.key()] = it.value();
        }
    }

    // 检查 patients 中是否已存在该用户的记录
    QString checkSql = QString("SELECT COUNT(*) as count FROM patients WHERE user_id = %1").arg(userId);
    DatabaseManager::ResultSet result = DatabaseManager::instance().query(checkSql);
    bool detailsExist = !result.empty() && result.front()["count"].toInt() > 0;

    bool success;
    if (detailsExist)
    {
        // 记录存在，执行 UPDATE
        QString patientWhereClause = QString("user_id = %1").arg(userId);
        success = DatabaseManager::instance().update("patients", patientData, patientWhereClause);
    }
    else
    {
        // 记录不存在，执行 INSERT
        patientData["user_id"] = userId; // 必须插入 user_id
        success = DatabaseManager::instance().insert("patients", patientData);
    }

    if (!success)
    {
        qCritical() << "更新患者信息失败:" << DatabaseManager::instance().lastError();
        DatabaseManager::instance().rollbackTransaction();
        return false;
    }

    // 所有操作成功，提交事务
    qInfo() << "用户" << userId << "信息更新成功.";
    return DatabaseManager::instance().commitTransaction();
}

QVariantMap UiController::getDoctorInfo(const QString &doctorId)
{
    if (!ensureDbConnected(__func__))
    {
        return {};
    }

    QString sql = QString(
        "SELECT u.username, u.email, d.* "
        "FROM users u "
        "JOIN doctors d ON u.user_id = d.user_id "
        "WHERE d.doctor_id = '%1'"
    ).arg(doctorId);

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    if (result.empty())
    {
        qWarning() << "未找到医生工号为:" << doctorId;
        return {};
    }

    return rowToMap(result.front());
}

bool UiController::updateDoctorInfo(const QString &doctorId, const QVariantMap &details)
{
    if (!ensureDbConnected(__func__))
    {
        return false;
    }

    // 开启事务
    if (!DatabaseManager::instance().beginTransaction())
    {
        qCritical() << "开启事务失败:" << DatabaseManager::instance().lastError();
        return false;
    }

    // 首先获取医生的 user_id
    QString getUserSql = QString("SELECT user_id FROM doctors WHERE doctor_id = '%1'").arg(doctorId);
    DatabaseManager::ResultSet userResult = DatabaseManager::instance().query(getUserSql);

    if (userResult.empty())
    {
        qCritical() << "Doctor not found:" << doctorId;
        DatabaseManager::instance().rollbackTransaction();
        return false;
    }

    int userId = userResult.front()["user_id"].toInt();

    // 1. 更新 users 表中的姓名 (username)
    if (details.contains("username"))
    {
        DatabaseManager::DataRow userData;
        userData["username"] = details["username"];
        QString userWhereClause = QString("user_id = %1").arg(userId);
        if (!DatabaseManager::instance().update("users", userData, userWhereClause))
        {
            qCritical() << "更新用户名失败:" << DatabaseManager::instance().lastError();
            DatabaseManager::instance().rollbackTransaction();
            return false;
        }
    }

    // 2. 更新 doctors 表
    DatabaseManager::DataRow doctorData;
    for(auto it = details.constBegin(); it != details.constEnd(); ++it)
    {
        // "username" 字段不属于 doctors 表，跳过
        if (it.key() != "username")
        {
            doctorData[it.key()] = it.value();
        }
    }

    if (!doctorData.empty())
    {
        QString doctorWhereClause = QString("doctor_id = '%1'").arg(doctorId);
        if (!DatabaseManager::instance().update("doctors", doctorData, doctorWhereClause))
        {
            qCritical() << "更新医生信息失败:" << DatabaseManager::instance().lastError();
            DatabaseManager::instance().rollbackTransaction();
            return false;
        }
    }

    // 提交事务
    qInfo() << "Doctor info for" << doctorId << "信息更新成功.";
    return DatabaseManager::instance().commitTransaction();
}

    // --- 3. 挂号预约后端 ---

QVariantList UiController::getAvailableDoctors(const QString &department)
{
    if (!ensureDbConnected(__func__))
    {
        emit doctorListReady(QVariantList());
        return QVariantList();
    }

    auto result = DatabaseManager::instance().getDoctorsByDepartment(department);
    auto doctorList = resultToList(result);

    emit doctorListReady(doctorList);
    return doctorList;
}

QVariantList UiController::getDoctorScheduleForDate(const QString &doctorId, const QDate &date)
{
    if (!ensureDbConnected(__func__))
    {
        emit doctorScheduleReady(QVariantList());
        return QVariantList();
    }

    auto result = DatabaseManager::instance().getDoctorSchedule(doctorId, date);
    auto scheduleList = resultToList(result);

    emit doctorScheduleReady(scheduleList);
    return scheduleList;
}

/**
 * @brief 创建预约
 *
 * @param patientId 患者ID
 * @param doctorId 医生ID
 * @param appointmentTime 预约时间
 * @return bool 是否创建成功
 */
bool UiController::createAppointment(int patientId, const QString &doctorId, const QDateTime &appointmentTime)
{
    if (!ensureDbConnected(__func__))
    {
        emit appointmentCreationFailed("数据库未连接");
        return false;
    }

    // 关键逻辑：预约前必须检查时间槽可用性，避免重复预约
    if (!DatabaseManager::instance().isTimeSlotAvailable(doctorId, appointmentTime))
    {
        emit appointmentCreationFailed("该时间段已被预约或医生不存在");
        return false;
    }

    // 使用事务确保预约创建的原子性，避免部分数据写入的情况
    if (!DatabaseManager::instance().beginTransaction())
    {
        emit appointmentCreationFailed("事务开启失败");
        return false;
    }

    // 创建预约记录
    DatabaseManager::DataRow appointmentData;
    appointmentData["patient_id"] = patientId;
    appointmentData["doctor_id"] = doctorId;
    appointmentData["appointment_date"] = appointmentTime.date();
    appointmentData["appointment_time"] = appointmentTime;
    appointmentData["status"] = "scheduled";
    appointmentData["payment_status"] = "unpaid";

    if (!DatabaseManager::instance().insert("appointments", appointmentData))
    {
        DatabaseManager::instance().rollbackTransaction();
        emit appointmentCreationFailed("预约创建失败: " + DatabaseManager::instance().lastError());
        return false;
    }

    // 获取刚创建的预约ID
    QString getIdSql = "SELECT last_insert_rowid() as appointment_id";
    DatabaseManager::ResultSet idResult = DatabaseManager::instance().query(getIdSql);

    if (idResult.empty())
    {
        DatabaseManager::instance().rollbackTransaction();
        emit appointmentCreationFailed("无法获取预约ID");
        return false;
    }

    int appointmentId = idResult.front()["appointment_id"].toInt();

    // 提交事务
    if (DatabaseManager::instance().commitTransaction())
    {
        qInfo() << "预约创建成功，预约ID为:" << appointmentId;
        emit appointmentCreated(appointmentId);
        return true;
    }
    else
    {
        emit appointmentCreationFailed("事务提交失败");
        return false;
    }
}

QVariantList UiController::getPatientAppointments(int patientId, const QDate &date)
{
    if (!ensureDbConnected(__func__))
    {
        emit appointmentListReady(QVariantList());
        return QVariantList();
    }

    auto result = DatabaseManager::instance().getPatientAppointments(patientId, date);
    auto appointmentList = resultToList(result);

    emit appointmentListReady(appointmentList);
    return appointmentList;
}

bool UiController::cancelAppointment(int appointmentId)
{
    if (!ensureDbConnected(__func__))
    {
        emit appointmentOperationFailed("数据库未连接");
        return false;
    }

    DatabaseManager::DataRow updateData;
    updateData["status"] = "cancelled";
    QString whereClause = QString("appointment_id = %1").arg(appointmentId);

    if (DatabaseManager::instance().update("appointments", updateData, whereClause))
    {
        qInfo() << "Appointment" << appointmentId << "取消成功";
        emit appointmentCancelled(appointmentId);
        return true;
    }
    else
    {
        emit appointmentOperationFailed("预约取消失败: " + DatabaseManager::instance().lastError());
        return false;
    }
}

bool UiController::updateAppointmentStatus(int appointmentId, const QString &status)
{
    if (!ensureDbConnected(__func__))
    {
        emit appointmentOperationFailed("数据库未连接");
        return false;
    }

    // 验证状态值
    QStringList validStatuses = {"scheduled", "completed", "cancelled"};
    if (!validStatuses.contains(status))
    {
        emit appointmentOperationFailed("无效的预约状态");
        return false;
    }

    DatabaseManager::DataRow updateData;
    updateData["status"] = status;
    QString whereClause = QString("appointment_id = %1").arg(appointmentId);

    if (DatabaseManager::instance().update("appointments", updateData, whereClause))
    {
        qInfo() << "Appointment" << appointmentId << "状态已更新为:" << status;
        return true;
    }
    else
    {
        emit appointmentOperationFailed("预约状态更新失败: " + DatabaseManager::instance().lastError());
        return false;
    }
}

    // --- 4. 病历/医嘱后端 ---

QVariantList UiController::getPatientMedicalHistory(int patientId)
{
    if (!ensureDbConnected(__func__))
    {
        emit medicalHistoryReady(QVariantList());
        return QVariantList();
    }

    auto result = DatabaseManager::instance().getPatientMedicalRecords(patientId);
    auto recordList = resultToList(result);

    emit medicalHistoryReady(recordList);
    return recordList;
}

/**
 * @brief 创建病历记录
 *
 * @param appointmentId 预约ID
 * @param diagnosisNotes 诊断记录
 * @return bool 是否创建成功
 */
bool UiController::createMedicalRecord(int appointmentId, const QString &diagnosisNotes)
{
    if (!ensureDbConnected(__func__))
    {
        emit medicalOperationFailed("数据库未连接");
        return false;
    }

    // 关键验证：确保预约存在，防止为不存在的预约创建病历
    QString checkSql = QString("SELECT COUNT(*) FROM appointments WHERE appointment_id = %1").arg(appointmentId);
    DatabaseManager::ResultSet checkResult = DatabaseManager::instance().query(checkSql);

    if (getCountFromResult(checkResult) == 0)
    {
        emit medicalOperationFailed("预约记录不存在");
        return false;
    }

    // 业务规则：一个预约只能有一个病历记录，防止重复创建
    QString existsSql = QString("SELECT COUNT(*) FROM medical_records WHERE appointment_id = %1").arg(appointmentId);
    DatabaseManager::ResultSet existsResult = DatabaseManager::instance().query(existsSql);

    if (getCountFromResult(existsResult) > 0)
    {
        emit medicalOperationFailed("该预约已有病历记录");
        return false;
    }

    // 开启事务
    if (!DatabaseManager::instance().beginTransaction())
    {
        emit medicalOperationFailed("事务开启失败");
        return false;
    }

    // 创建病历记录
    DatabaseManager::DataRow recordData;
    recordData["appointment_id"] = appointmentId;
    recordData["diagnosis_notes"] = diagnosisNotes;
    recordData["diagnosis_date"] = QDate::currentDate();

    if (!DatabaseManager::instance().insert("medical_records", recordData))
    {
        DatabaseManager::instance().rollbackTransaction();
        emit medicalOperationFailed("病历创建失败: " + DatabaseManager::instance().lastError());
        return false;
    }

    // 获取刚创建的病历ID
    QString getIdSql = "SELECT last_insert_rowid() as record_id";
    DatabaseManager::ResultSet idResult = DatabaseManager::instance().query(getIdSql);

    if (idResult.empty())
    {
        DatabaseManager::instance().rollbackTransaction();
        emit medicalOperationFailed("无法获取病历ID");
        return false;
    }

    int recordId = idResult.front()["record_id"].toInt();

    // 重要：创建病历后必须更新预约状态，确保业务流程的完整性
    DatabaseManager::DataRow appointmentUpdate;
    appointmentUpdate["status"] = "completed";
    QString whereClause = QString("appointment_id = %1").arg(appointmentId);

    if (!DatabaseManager::instance().update("appointments", appointmentUpdate, whereClause))
    {
        DatabaseManager::instance().rollbackTransaction(); // 任何步骤失败都要回滚
        emit medicalOperationFailed("预约状态更新失败");
        return false;
    }

    // 提交事务
    if (DatabaseManager::instance().commitTransaction())
    {
        qInfo() << "病历创建成功，病历ID为:" << recordId;
        emit medicalRecordCreated(recordId);
        return true;
    }
    else
    {
        emit medicalOperationFailed("事务提交失败");
        return false;
    }
}

bool UiController::addPrescription(int recordId, const QString &prescriptionDetails)
{
    if (!ensureDbConnected(__func__))
    {
        emit medicalOperationFailed("数据库未连接");
        return false;
    }

    // 检查病历记录是否存在
    QString checkSql = QString("SELECT COUNT(*) FROM medical_records WHERE record_id = %1").arg(recordId);
    DatabaseManager::ResultSet checkResult = DatabaseManager::instance().query(checkSql);

    if (getCountFromResult(checkResult) == 0)
    {
        emit medicalOperationFailed("病历记录不存在");
        return false;
    }

    // 创建处方记录
    DatabaseManager::DataRow prescriptionData;
    prescriptionData["record_id"] = recordId;
    prescriptionData["details"] = prescriptionDetails;

    if (DatabaseManager::instance().insert("prescriptions", prescriptionData))
    {
        // 获取刚创建的处方ID
        QString getIdSql = "SELECT last_insert_rowid() as prescription_id";
        DatabaseManager::ResultSet idResult = DatabaseManager::instance().query(getIdSql);

        if (!idResult.empty())
        {
            int prescriptionId = idResult.front()["prescription_id"].toInt();
            qInfo() << "处方添加成功，处方ID为:" << prescriptionId;
            emit prescriptionAdded(prescriptionId);
            return true;
        }
    }

    emit medicalOperationFailed("处方添加失败: " + DatabaseManager::instance().lastError());
    return false;
}

QVariantList UiController::getPatientPrescriptions(int patientId)
{
    if (!ensureDbConnected(__func__))
    {
        emit medicalOperationFailed("数据库未连接");
        return {};
    }

    QString sql = QString(R"(
        SELECT pr.prescription_id, pr.details, pr.issued_at,
               mr.diagnosis_date, mr.diagnosis_notes,
               d.doctor_id, d.full_name as doctor_name, d.department
        FROM prescriptions pr
        JOIN medical_records mr ON pr.record_id = mr.record_id
        JOIN appointments a ON mr.appointment_id = a.appointment_id
        JOIN doctors d ON a.doctor_id = d.doctor_id
        WHERE a.patient_id = %1
        ORDER BY pr.issued_at DESC
    )").arg(patientId);

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    auto prescriptionList = resultToList(result);

    emit prescriptionListReady(prescriptionList);
    return prescriptionList;
}

bool UiController::createHospitalization(int recordId, const QString &doctorId, const QString &wardNo, const QString &bedNo)
{
    if (!ensureDbConnected(__func__))
    {
        emit medicalOperationFailed("数据库未连接");
        return false;
    }

    // 检查病历记录是否存在
    QString checkSql = QString("SELECT COUNT(*) FROM medical_records WHERE record_id = %1").arg(recordId);
    DatabaseManager::ResultSet checkResult = DatabaseManager::instance().query(checkSql);

    if (getCountFromResult(checkResult) == 0)
    {
        emit medicalOperationFailed("病历记录不存在");
        return false;
    }

    // 检查是否已有住院记录
    QString existsSql = QString("SELECT COUNT(*) FROM hospitalizations WHERE record_id = %1").arg(recordId);
    DatabaseManager::ResultSet existsResult = DatabaseManager::instance().query(existsSql);

    if (getCountFromResult(existsResult) > 0)
    {
        emit medicalOperationFailed("该病历已有住院记录");
        return false;
    }

    // 创建住院记录
    DatabaseManager::DataRow hospitalizationData;
    hospitalizationData["record_id"] = recordId;
    hospitalizationData["doctor_id"] = doctorId;
    hospitalizationData["ward_no"] = wardNo;
    hospitalizationData["bed_no"] = bedNo;
    hospitalizationData["admission_date"] = QDate::currentDate();

    if (DatabaseManager::instance().insert("hospitalizations", hospitalizationData))
    {
        // 获取刚创建的住院ID
        QString getIdSql = "SELECT last_insert_rowid() as hospitalization_id";
        DatabaseManager::ResultSet idResult = DatabaseManager::instance().query(getIdSql);

        if (!idResult.empty())
        {
            int hospitalizationId = idResult.front()["hospitalization_id"].toInt();
            qInfo() << "住院记录创建成功，住院ID为:" << hospitalizationId;
            emit hospitalizationCreated(hospitalizationId);
            return true;
        }
    }

    emit medicalOperationFailed("住院记录创建失败: " + DatabaseManager::instance().lastError());
    return false;
}

    // --- 5. 预约提醒后端 ---

QVariantList UiController::getUpcomingAppointments(int userId)
{
    if (!ensureDbConnected(__func__))
    {
        return {};
    }

    // 获取未来24小时内的预约
    QDateTime currentTime = QDateTime::currentDateTime();
    QDateTime tomorrowTime = currentTime.addDays(1);

    QString sql = QString(R"(
        SELECT a.appointment_id, a.appointment_time, a.status,
               d.doctor_id, d.full_name as doctor_name, d.department,
               p.full_name as patient_name
        FROM appointments a
        JOIN doctors d ON a.doctor_id = d.doctor_id
        JOIN patients p ON a.patient_id = p.patient_id
        WHERE (a.patient_id = (SELECT patient_id FROM patients WHERE user_id = %1)
               OR a.doctor_id = (SELECT doctor_id FROM doctors WHERE user_id = %1))
        AND a.appointment_time BETWEEN '%2' AND '%3'
        AND a.status = 'scheduled'
        ORDER BY a.appointment_time
    )").arg(userId)
       .arg(currentTime.toString("yyyy-MM-dd hh:mm:ss"))
       .arg(tomorrowTime.toString("yyyy-MM-dd hh:mm:ss"));

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    auto appointmentList = resultToList(result);

    emit upcomingAppointmentsReady(appointmentList);
    return appointmentList;
}

    // --- 6. 医患沟通后端 ---

bool UiController::sendMessage(int senderId, int receiverId, const QString &content)
{
    if (!ensureDbConnected(__func__))
    {
        emit chatOperationFailed("数据库未连接");
        return false;
    }

    // 数据验证：自动去除前后空格，避免发送空消息
    if (content.trimmed().isEmpty())
    {
        emit chatOperationFailed("消息内容不能为空");
        return false;
    }

    // 创建消息记录
    DatabaseManager::DataRow messageData;
    messageData["sender_id"] = senderId;
    messageData["receiver_id"] = receiverId;
    messageData["content"] = content.trimmed();

    if (DatabaseManager::instance().insert("chat_messages", messageData))
    {
        // 获取刚创建的消息ID
        QString getIdSql = "SELECT last_insert_rowid() as message_id";
        DatabaseManager::ResultSet idResult = DatabaseManager::instance().query(getIdSql);

        if (!idResult.empty())
        {
            int messageId = idResult.front()["message_id"].toInt();
            qInfo() << "消息发送成功，消息ID为:" << messageId;
            emit messageSent(messageId);
            emit newMessageReceived(senderId, content); // 通知接收方有新消息
            return true;
        }
    }

    emit chatOperationFailed("消息发送失败: " + DatabaseManager::instance().lastError());
    return false;
}

QVariantList UiController::getChatHistory(int user1Id, int user2Id)
{
    auto result = DatabaseManager::instance().getChatHistory(user1Id, user2Id);
    auto messageList = resultToList(result);

    emit chatHistoryReady(messageList);
    return messageList;
}

QVariantList UiController::getRecentContacts(int userId)
{
    if (!ensureDbConnected(__func__))
    {
        emit chatOperationFailed("数据库未连接");
        return {};
    }

    QString sql = QString(R"(
        SELECT DISTINCT
               CASE
                   WHEN cm.sender_id = %1 THEN cm.receiver_id
                   ELSE cm.sender_id
               END as contact_id,
               u.username as contact_name,
               u.role as contact_role,
               MAX(cm.sent_at) as last_message_time
        FROM chat_messages cm
        JOIN users u ON (CASE
                            WHEN cm.sender_id = %1 THEN cm.receiver_id
                            ELSE cm.sender_id
                        END) = u.user_id
        WHERE cm.sender_id = %1 OR cm.receiver_id = %1
        GROUP BY contact_id
        ORDER BY last_message_time DESC
        LIMIT 20
    )").arg(userId);

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    auto contactList = resultToList(result);

    emit contactListReady(contactList);
    return contactList;
}

    // --- 7. 考勤管理后端 ---

bool UiController::checkIn(const QString &doctorId)
{
    if (!ensureDbConnected(__func__))
    {
        emit attendanceOperationFailed("数据库未连接");
        return false;
    }

    QDate today = QDate::currentDate();
    QTime currentTime = QTime::currentTime();

    // 检查今天是否已经签到
    QString checkSql = QString(R"(
        SELECT COUNT(*) FROM attendance
        WHERE doctor_id = '%1' AND date = '%2'
    )").arg(doctorId).arg(today.toString("yyyy-MM-dd"));

    DatabaseManager::ResultSet checkResult = DatabaseManager::instance().query(checkSql);

    if (getCountFromResult(checkResult) > 0)
    {
        // 智能处理：如果当天已有记录，则更新签到时间（支持重复签到）
        DatabaseManager::DataRow updateData;
        updateData["check_in_time"] = currentTime;
        QString whereClause = QString("doctor_id = '%1' AND date = '%2'").arg(doctorId).arg(today.toString("yyyy-MM-dd"));

        if (DatabaseManager::instance().update("attendance", updateData, whereClause))
        {
            qInfo() << "医生" << doctorId << "签到成功，时间为" << currentTime.toString();
            emit checkInSuccess();
            return true;
        }
    }
    else
    {
        // 首次签到：创建新的考勤记录
        DatabaseManager::DataRow attendanceData;
        attendanceData["doctor_id"] = doctorId;
        attendanceData["date"] = today;
        attendanceData["check_in_time"] = currentTime;

        if (DatabaseManager::instance().insert("attendance", attendanceData))
        {
            qInfo() << "医生" << doctorId << "签到成功，时间为" << currentTime.toString();
            emit checkInSuccess();
            return true;
        }
    }

    emit attendanceOperationFailed("签到失败: " + DatabaseManager::instance().lastError());
    return false;
}

bool UiController::checkOut(const QString &doctorId)
{
    if (!ensureDbConnected(__func__))
    {
        emit attendanceOperationFailed("数据库未连接");
        return false;
    }

    QDate today = QDate::currentDate();
    QTime currentTime = QTime::currentTime();

    // 更新签退时间
    DatabaseManager::DataRow updateData;
    updateData["check_out_time"] = currentTime;
    QString whereClause = QString("doctor_id = '%1' AND date = '%2'").arg(doctorId).arg(today.toString("yyyy-MM-dd"));

    if (DatabaseManager::instance().update("attendance", updateData, whereClause))
    {
        qInfo() << "医生" << doctorId << "签退成功，时间为" << currentTime.toString();
        emit checkOutSuccess();
        return true;
    }
    else
    {
        emit attendanceOperationFailed("签退失败: " + DatabaseManager::instance().lastError());
        return false;
    }
}

QVariantList UiController::getAttendanceHistory(const QString &doctorId, const QDate &startDate, const QDate &endDate)
{
    if (!ensureDbConnected(__func__))
    {
        emit attendanceOperationFailed("数据库未连接");
        return {};
    }

    QString sql = QString(R"(
        SELECT * FROM attendance
        WHERE doctor_id = '%1'
        AND date BETWEEN '%2' AND '%3'
        ORDER BY date DESC
    )").arg(doctorId)
       .arg(startDate.toString("yyyy-MM-dd"))
       .arg(endDate.toString("yyyy-MM-dd"));

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    auto attendanceList = resultToList(result);

    emit attendanceHistoryReady(attendanceList);
    return attendanceList;
}

bool UiController::submitLeaveRequest(const QString &doctorId, const QString &requestType, const QDate &startDate, const QDate &endDate, const QString &reason)
{
    if (!ensureDbConnected(__func__))
    {
        emit attendanceOperationFailed("数据库未连接");
        return false;
    }

    // 验证请假类型
    if (requestType != "因公" && requestType != "因私")
    {
        emit attendanceOperationFailed("无效的请假类型");
        return false;
    }

    // 验证日期
    if (startDate > endDate)
    {
        emit attendanceOperationFailed("开始日期不能晚于结束日期");
        return false;
    }

    // 创建请假申请
    DatabaseManager::DataRow requestData;
    requestData["doctor_id"] = doctorId;
    requestData["request_type"] = requestType;
    requestData["start_date"] = startDate;
    requestData["end_date"] = endDate;
    // 限制60字（按Unicode字符计）
    QVector<uint> ucs4 = reason.toUcs4();
    if (ucs4.size() > 60) ucs4.resize(60);
    requestData["reason"] = QString::fromUcs4(ucs4.constData(), ucs4.size());
    requestData["status"] = "未销假";

    if (DatabaseManager::instance().insert("leave_requests", requestData))
    {
        // 获取刚创建的请假申请ID
        QString getIdSql = "SELECT last_insert_rowid() as request_id";
        DatabaseManager::ResultSet idResult = DatabaseManager::instance().query(getIdSql);

        if (!idResult.empty())
        {
            int requestId = idResult.front()["request_id"].toInt();
            qInfo() << "请假申请提交成功，申请ID为:" << requestId;
            emit leaveRequestSubmitted(requestId);
            return true;
        }
    }

    emit attendanceOperationFailed("请假申请提交失败: " + DatabaseManager::instance().lastError());
    return false;
}

QVariantList UiController::getLeaveRequests(const QString &doctorId)
{
    if (!ensureDbConnected(__func__))
    {
        emit attendanceOperationFailed("数据库未连接");
        return {};
    }

    QString sql = QString(R"(
        SELECT * FROM leave_requests
        WHERE doctor_id = '%1'
        ORDER BY start_date DESC
    )").arg(doctorId);

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    auto requestList = resultToList(result);

    emit leaveRequestsReady(requestList);
    return requestList;
}

bool UiController::cancelLeave(int requestId)
{
    if (!ensureDbConnected(__func__))
    {
        emit attendanceOperationFailed("数据库未连接");
        return false;
    }

    DatabaseManager::DataRow updateData;
    updateData["status"] = "已销假";
    QString whereClause = QString("request_id = %1").arg(requestId);

    if (DatabaseManager::instance().update("leave_requests", updateData, whereClause))
    {
        qInfo() << "请假申请" << requestId << "取消成功";
        emit leaveCancelled(requestId);
        return true;
    }
    else
    {
        emit attendanceOperationFailed("销假失败: " + DatabaseManager::instance().lastError());
        return false;
    }
}

    // --- 8. 药品管理后端 ---

QVariantList UiController::searchDrugs(const QString &keyword)
{
    if (!ensureDbConnected(__func__))
    {
        emit drugOperationFailed("数据库未连接");
        return {};
    }

    QString sql;
    if (keyword.isEmpty())
    {
        sql = "SELECT * FROM drugs ORDER BY drug_name LIMIT 50";
    }
    else
    {
        sql = QString(R"(
            SELECT * FROM drugs
            WHERE drug_name LIKE '%%1%'
               OR description LIKE '%%1%'
            ORDER BY drug_name
            LIMIT 50
        )").arg(keyword);
    }

    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    auto drugList = resultToList(result);

    emit drugSearchResultReady(drugList);
    return drugList;
}

QVariantMap UiController::getDrugDetails(int drugId)
{
    if (!ensureDbConnected(__func__))
    {
        emit drugOperationFailed("数据库未连接");
        return {};
    }

    QString sql = QString("SELECT * FROM drugs WHERE drug_id = %1").arg(drugId);
    DatabaseManager::ResultSet result = DatabaseManager::instance().query(sql);

    if (result.empty())
    {
        emit drugOperationFailed("药品不存在");
        return {};
    }

    auto drugDetails = rowToMap(result.front());

    emit drugDetailsReady(drugDetails);
    return drugDetails;
}
