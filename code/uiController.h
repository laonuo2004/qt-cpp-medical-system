#ifndef UICONTROLLER_H
#define UICONTROLLER_H

#include <QString>
#include <QCryptographicHash> // 用于密码加密
#include <QVariant> // 用于数据库操作
#include <QRandomGenerator> // 用于生成随机验证码

#include "databasemanager.h" // 包含数据库管理类

// 定义用户身份枚举 (与数据库中的角色对应)
enum class UserRole {
    Admin = 0,      // 管理员
    Doctor = 1,     // 医生
    Patient = 2     // 患者
};
Q_DECLARE_METATYPE(UserRole) // 这一行非常重要，允许QVariant存储UserRole
class UiController : public QObject
{
    Q_OBJECT
public:
    explicit UiController(QObject *parent = nullptr);
    // 单例模式获取实例
    static UiController& instance();
    // --- 1. 认证和用户管理后端 ---

    /**
     * @brief 用户登录验证
     *
     * 功能：验证用户邮箱和密码，根据用户角色发出相应的登录成功信号
     * 简化操作：前端只需调用此方法，无需关心角色判断逻辑
     * 注意事项：邮箱和密码均不能为空，密码会自动进行哈希比对
     *
     * @param email 用户邮箱地址，作为登录凭据
     * @param password 用户密码明文，系统会自动加密比对
     * @return 无返回值，通过信号通知结果：
     *         loginSuccessAdmin/loginSuccessDoctor/loginSuccessPatient - 登录成功
     *         loginFailed(QString reason) - 登录失败，包含具体原因
     */
    Q_INVOKABLE void login(const QString &email, const QString &password);

    /**
     * @brief 用户注册
     *
     * 功能：创建新用户账户，支持管理员、医生、患者三种角色
     * 简化操作：自动处理密码加密、邮箱唯一性检查、密码复杂度验证
     * 注意事项：密码必须满足复杂度要求（8位以上，包含大小写字母、数字、特殊字符）
     *
     * @param username 用户名，显示名称
     * @param email 邮箱地址，必须唯一，用于登录
     * @param password 密码明文，系统会自动加密存储
     * @param role 用户角色枚举值（Admin/Doctor/Patient）
     * @return 无返回值，通过信号通知结果：
     *         registrationSuccess() - 注册成功
     *         registrationFailed(QString reason) - 注册失败，包含具体原因
     */
    Q_INVOKABLE void registerUser(const QString &username, const QString &email, const QString &password, UserRole role);

    /**
     * @brief 忘记密码申请
     *
     * 功能：为已注册用户发送密码重置验证码
     * 简化操作：自动验证邮箱是否已注册，生成6位数字验证码
     * 注意事项：验证码目前存储在内存中，应用重启会丢失
     *
     * @param email 已注册的邮箱地址
     * @return 无返回值，通过信号通知结果：
     *         forgotPasswordRequestSuccess(QString email) - 发送成功
     *         forgotPasswordRequestFailed(QString reason) - 发送失败
     */
    Q_INVOKABLE void forgotPassword(const QString &email);

    /**
     * @brief 重置密码
     *
     * 功能：使用验证码重置用户密码
     * 简化操作：自动验证验证码正确性和密码复杂度
     * 注意事项：验证码使用后会自动清除，新密码同样需要满足复杂度要求
     *
     * @param email 用户邮箱
     * @param verificationCode 6位数字验证码
     * @param newPassword 新密码明文
     * @return 无返回值，通过信号通知结果：
     *         passwordResetSuccess() - 重置成功
     *         passwordResetFailed(QString reason) - 重置失败
     */
    Q_INVOKABLE void resetPassword(const QString &email, const QString &verificationCode, const QString &newPassword);

    // 内部辅助方法，不对外暴露
    bool isEmailRegistered(const QString &email);
    bool isPasswordComplex(const QString &password);
    // --- 2. 个人信息后端 ---

    /**
     * @brief 获取患者详细信息
     *
     * 功能：获取患者的完整个人信息，包括基础信息和扩展信息
     * 简化操作：自动合并users表和patients表的数据，一次调用获取完整信息
     * 注意事项：如果患者尚未完善详细信息，部分字段可能为空
     *
     * @param userId 用户ID（users表中的user_id）
     * @return QVariantMap 包含患者信息的映射表，主要字段包括：
     *         username, email, full_name, sex, id_card_no, birth_date,
     *         age, phone_no, address, sos_name, sos_phone_no
     *         返回空映射表表示用户不存在或数据库连接失败
     */
    Q_INVOKABLE QVariantMap getPatientInfo(int userId);

    /**
     * @brief 更新患者信息
     *
     * 功能：更新患者的个人信息，支持部分字段更新
     * 简化操作：自动处理users表和patients表的关联更新，使用数据库事务确保一致性
     * 注意事项：username字段会更新到users表，其他字段更新到patients表
     *
     * @param userId 用户ID
     * @param details 要更新的信息映射表，可以只包含需要更新的字段
     * @return bool true-更新成功，false-更新失败
     *         失败时会自动回滚所有更改，通过infoUpdateFailed信号通知错误原因
     */
    Q_INVOKABLE bool updatePatientInfo(int userId, const QVariantMap &details);

    /**
     * @brief 获取医生详细信息
     *
     * 功能：获取医生的完整个人信息和职业信息
     * 简化操作：自动合并users表和doctors表的数据
     * 注意事项：需要使用doctor_id而不是user_id作为参数
     *
     * @param doctorId 医生工号（doctors表中的doctor_id）
     * @return QVariantMap 包含医生信息的映射表，主要字段包括：
     *         username, email, full_name, sex, age, department, title,
     *         phone_no, doc_start, doc_finish, registration_fee, patient_limit, photo_url
     */
    Q_INVOKABLE QVariantMap getDoctorInfo(const QString &doctorId);

    /**
     * @brief 更新医生信息
     *
     * 功能：更新医生的个人信息和职业信息
     * 简化操作：自动处理users表和doctors表的关联更新，使用数据库事务
     * 注意事项：会先通过doctor_id查找对应的user_id，然后更新相关表
     *
     * @param doctorId 医生工号
     * @param details 要更新的信息映射表
     * @return bool true-更新成功，false-更新失败
     */
    Q_INVOKABLE bool updateDoctorInfo(const QString &doctorId, const QVariantMap &details);

    // --- 3. 挂号预约后端 ---

    /**
     * @brief 获取可预约医生列表
     *
     * 功能：获取指定科室或全部科室的医生信息
     * 简化操作：自动查询医生的完整信息，包括科室、职称、费用等
     * 注意事项：返回结果按科室和姓名排序，便于前端展示
     *
     * @param department 科室名称，为空则查询所有医生
     * @return QVariantList 医生信息列表，每个元素包含：
     *         doctor_id, full_name, sex, age, department, title, phone_no,
     *         doc_start, doc_finish, registration_fee, patient_limit, photo_url, email
     *         同时发出doctorListReady信号通知前端
     */
    Q_INVOKABLE QVariantList getAvailableDoctors(const QString &department = QString());

    /**
     * @brief 获取医生指定日期的排班信息
     *
     * 功能：查看医生在特定日期的预约情况
     * 简化操作：自动关联患者信息，显示已预约的时间段
     * 注意事项：主要用于医生端查看自己的排班，或患者选择预约时间
     *
     * @param doctorId 医生工号
     * @param date 查询日期
     * @return QVariantList 排班信息列表，包含：
     *         appointment_time, status, patient_name, patient_phone
     *         按预约时间排序，同时发出doctorScheduleReady信号
     */
    Q_INVOKABLE QVariantList getDoctorScheduleForDate(const QString &doctorId, const QDate &date);

    /**
     * @brief 创建预约
     *
     * 功能：为患者创建医生预约，包含时间冲突检查
     * 简化操作：自动检查时间槽可用性，使用事务确保数据一致性
     * 注意事项：会自动设置预约状态为'scheduled'，支付状态为'unpaid'
     *           预约失败会自动回滚，通过信号通知具体失败原因
     *
     * @param patientId 患者ID
     * @param doctorId 医生工号
     * @param appointmentTime 预约时间（精确到分钟）
     * @return bool true-创建成功，false-创建失败
     *         成功时发出appointmentCreated(int appointmentId)信号
     *         失败时发出appointmentCreationFailed(QString reason)信号
     */
    Q_INVOKABLE bool createAppointment(int patientId, const QString &doctorId, const QDateTime &appointmentTime);

    /**
     * @brief 获取患者预约列表
     *
     * 功能：查询患者的预约记录，可按日期筛选
     * 简化操作：自动关联医生信息，提供完整的预约详情
     * 注意事项：date参数为空时查询所有预约，按时间倒序排列
     *
     * @param patientId 患者ID
     * @param date 查询日期，为空则查询所有预约
     * @return QVariantList 预约列表，包含：
     *         appointment_id, appointment_date, appointment_time, status, payment_status,
     *         doctor_id, doctor_name, department, title, registration_fee
     */
    Q_INVOKABLE QVariantList getPatientAppointments(int patientId, const QDate &date = QDate());

    /**
     * @brief 取消预约
     *
     * 功能：将预约状态设置为'cancelled'
     * 简化操作：直接传入预约ID即可取消，无需其他验证
     * 注意事项：取消后状态不可逆，需要重新预约
     *
     * @param appointmentId 预约ID
     * @return bool true-取消成功，false-取消失败
     */
    Q_INVOKABLE bool cancelAppointment(int appointmentId);

    /**
     * @brief 更新预约状态
     *
     * 功能：修改预约状态（scheduled/completed/cancelled）
     * 简化操作：自动验证状态值有效性
     * 注意事项：主要用于医生端，将预约标记为已完成或已取消
     *
     * @param appointmentId 预约ID
     * @param status 新状态（scheduled/completed/cancelled）
     * @return bool true-更新成功，false-更新失败
     */
    Q_INVOKABLE bool updateAppointmentStatus(int appointmentId, const QString &status);

    // --- 4. 病历/医嘱后端 ---

    /**
     * @brief 获取患者病历记录
     *
     * 功能：查询患者的完整病历历史，包括诊断和处方信息
     * 简化操作：自动关联医生、预约、处方信息，按时间倒序排列
     * 注意事项：返回结果包含处方详情的聚合信息
     *
     * @param patientId 患者ID
     * @return QVariantList 病历列表，包含诊断记录、医生信息、处方汇总等
     */
    Q_INVOKABLE QVariantList getPatientMedicalHistory(int patientId);

    /**
     * @brief 创建病历记录
     *
     * 功能：为完成的预约创建病历记录，自动更新预约状态
     * 简化操作：使用事务确保预约状态和病历记录的一致性
     * 注意事项：一个预约只能有一个病历记录，重复创建会失败
     *
     * @param appointmentId 预约ID
     * @param diagnosisNotes 诊断记录内容
     * @return bool 创建是否成功，同时发出medicalRecordCreated信号
     */
    Q_INVOKABLE bool createMedicalRecord(int appointmentId, const QString &diagnosisNotes);

    /**
     * @brief 添加处方
     *
     * 功能：为现有病历记录添加处方信息
     * 简化操作：一个病历可以有多个处方，支持多次调用
     *
     * @param recordId 病历记录ID
     * @param prescriptionDetails 处方详情
     * @return bool 添加是否成功
     */
    Q_INVOKABLE bool addPrescription(int recordId, const QString &prescriptionDetails);

    /**
     * @brief 获取患者处方列表
     *
     * 功能：查询患者的所有处方记录，关联诊断和医生信息
     *
     * @param patientId 患者ID
     * @return QVariantList 处方列表，按开具时间倒序排列
     */
    Q_INVOKABLE QVariantList getPatientPrescriptions(int patientId);

    /**
     * @brief 创建住院记录
     *
     * 功能：为病历记录创建住院信息
     * 注意事项：一个病历只能有一个住院记录
     *
     * @param recordId 病历记录ID
     * @param doctorId 主治医生ID
     * @param wardNo 病房号
     * @param bedNo 床号
     * @return bool 创建是否成功
     */
    Q_INVOKABLE bool createHospitalization(int recordId, const QString &doctorId, const QString &wardNo, const QString &bedNo);

    // --- 5. 预约提醒后端 ---

    /**
     * @brief 获取即将到来的预约
     *
     * 功能：查询用户未来24小时内的预约提醒
     * 简化操作：自动判断用户角色（患者或医生），返回相应的预约信息
     * 注意事项：主要用于登录时的预约提醒功能
     *
     * @param userId 用户ID（可以是患者或医生的user_id）
     * @return QVariantList 即将到来的预约列表
     */
    Q_INVOKABLE QVariantList getUpcomingAppointments(int userId);

    // --- 6. 医患沟通后端 ---

    /**
     * @brief 发送消息
     *
     * 功能：在两个用户之间发送聊天消息
     * 简化操作：自动记录发送时间，通知接收方有新消息
     * 注意事项：消息内容会自动去除前后空格
     *
     * @param senderId 发送者用户ID
     * @param receiverId 接收者用户ID
     * @param content 消息内容
     * @return bool 发送是否成功
     */
    Q_INVOKABLE bool sendMessage(int senderId, int receiverId, const QString &content);

    /**
     * @brief 获取聊天记录
     *
     * 功能：获取两个用户之间的聊天历史记录
     * 简化操作：自动查询双向消息，按时间倒序排列
     * 注意事项：默认返回最近50条消息
     *
     * @param user1Id 用户1的ID
     * @param user2Id 用户2的ID
     * @return QVariantList 聊天记录列表
     */
    Q_INVOKABLE QVariantList getChatHistory(int user1Id, int user2Id);

    /**
     * @brief 获取最近联系人
     *
     * 功能：获取用户最近聊天的联系人列表
     * 简化操作：按最后消息时间排序，最多返回20个联系人
     *
     * @param userId 用户ID
     * @return QVariantList 联系人列表
     */
    Q_INVOKABLE QVariantList getRecentContacts(int userId);

    // --- 7. 考勤管理后端 ---

    /**
     * @brief 医生签到
     *
     * 功能：记录医生的签到时间
     * 简化操作：自动处理重复签到（更新签到时间）和首次签到（创建新记录）
     *
     * @param doctorId 医生工号
     * @return bool 签到是否成功
     */
    Q_INVOKABLE bool checkIn(const QString &doctorId);

    /**
     * @brief 医生签退
     *
     * 功能：记录医生的签退时间
     * 注意事项：需要先有签到记录才能签退
     *
     * @param doctorId 医生工号
     * @return bool 签退是否成功
     */
    Q_INVOKABLE bool checkOut(const QString &doctorId);

    /**
     * @brief 获取考勤历史
     *
     * 功能：查询医生指定时间段的考勤记录
     *
     * @param doctorId 医生工号
     * @param startDate 开始日期
     * @param endDate 结束日期
     * @return QVariantList 考勤记录列表
     */
    Q_INVOKABLE QVariantList getAttendanceHistory(const QString &doctorId, const QDate &startDate, const QDate &endDate);

    /**
     * @brief 提交请假申请
     *
     * 功能：创建医生请假申请记录
     * 简化操作：自动验证请假类型和日期有效性，限制请假事由长度
     *
     * @param doctorId 医生工号
     * @param requestType 请假类型（"因公"或"因私"）
     * @param startDate 请假开始日期
     * @param endDate 请假结束日期
     * @param reason 请假事由（最多60字）
     * @return bool 提交是否成功
     */
    Q_INVOKABLE bool submitLeaveRequest(const QString &doctorId, const QString &requestType, const QDate &startDate, const QDate &endDate, const QString &reason);

    /**
     * @brief 获取请假申请列表
     *
     * @param doctorId 医生工号
     * @return QVariantList 请假申请列表，按开始日期倒序排列
     */
    Q_INVOKABLE QVariantList getLeaveRequests(const QString &doctorId);

    /**
     * @brief 销假
     *
     * 功能：将请假申请状态设置为"已销假"
     *
     * @param requestId 请假申请ID
     * @return bool 销假是否成功
     */
    Q_INVOKABLE bool cancelLeave(int requestId);

    // --- 8. 药品管理后端 ---

    /**
     * @brief 搜索药品
     *
     * 功能：根据关键词搜索药品信息
     * 简化操作：支持按药品名称和描述搜索，自动限制返回数量
     *
     * @param keyword 搜索关键词，为空时返回前50个药品
     * @return QVariantList 药品列表，最多返回50条记录
     */
    Q_INVOKABLE QVariantList searchDrugs(const QString &keyword);

    /**
     * @brief 获取药品详细信息
     *
     * 功能：根据药品ID获取完整的药品信息
     *
     * @param drugId 药品ID
     * @return QVariantMap 药品详细信息，包括名称、描述、用法、注意事项、价格、图片等
     */
    Q_INVOKABLE QVariantMap getDrugDetails(int drugId);
private:
    // 用于生成和验证密码哈希
    QString hashPassword(const QString &password);

    // 生成随机验证码
    QString generateVerificationCode();

    // 存储邮箱和对应的验证码 (模拟，实际应用中会通过邮件发送并有过期机制)
    QMap<QString, QString> m_verificationCodes; // 仍然需要这个来模拟验证码的临时存储

    // --- 辅助函数 ---
    /**
     * @brief 数据库连接状态检查守卫
     *
     * 功能：统一检查数据库连接状态，避免在每个方法中重复检查
     * 简化操作：一行代码完成数据库状态检查和错误日志记录
     * 注意事项：使用__func__宏可以自动获取调用函数名，便于调试
     *
     * @param where 调用位置标识（通常传入__func__）
     * @return bool true-数据库已连接，false-数据库未连接
     */
    bool ensureDbConnected(const char* where);

    /**
     * @brief 将单行数据转换为QVariantMap
     *
     * 功能：将DatabaseManager::DataRow转换为Qt标准的QVariantMap格式
     * 简化操作：消除手动遍历键值对的重复代码
     * 注意事项：适用于单行查询结果的转换
     *
     * @param row 数据库查询返回的单行数据
     * @return QVariantMap 转换后的键值映射表
     */
    QVariantMap rowToMap(const DatabaseManager::DataRow& row);

    /**
     * @brief 将查询结果集转换为QVariantList
     *
     * 功能：将DatabaseManager::ResultSet转换为Qt标准的QVariantList格式
     * 简化操作：一行代码完成整个结果集的转换，替代手动循环
     * 注意事项：适用于多行查询结果，每行转换为QVariantMap后加入列表
     *
     * @param rs 数据库查询返回的结果集
     * @return QVariantList 转换后的数据列表
     */
    QVariantList resultToList(const DatabaseManager::ResultSet& rs);

    /**
     * @brief 从查询结果中获取计数值
     *
     * 功能：从COUNT查询结果中提取整数值，简化复杂的链式调用
     * 简化操作：替代复杂的front().begin()->second.toInt()模式
     * 注意事项：适用于SELECT COUNT(*) 或类似的单值查询结果
     *
     * @param result 数据库查询返回的结果集
     * @return int 计数值，如果结果为空则返回0
     */
    int getCountFromResult(const DatabaseManager::ResultSet& result);

signals:
    // 认证信号
    void loginSuccessAdmin();
    void loginSuccessDoctor();
    void loginSuccessPatient();
    void loginFailed(const QString &reason);
    void registrationSuccess();
    void registrationFailed(const QString &reason);
    void forgotPasswordRequestSuccess(const QString &email);
    void forgotPasswordRequestFailed(const QString &reason);
    void passwordResetSuccess();
    void passwordResetFailed(const QString &reason);

    // 个人信息信号
    void patientInfoReady(const QVariantMap &info);
    void doctorInfoReady(const QVariantMap &info);
    void infoUpdateSuccess();
    void infoUpdateFailed(const QString &reason);

    // 预约挂号信号
    void doctorListReady(const QVariantList &doctors);
    void doctorScheduleReady(const QVariantList &schedule);
    void appointmentCreated(int appointmentId);
    void appointmentCreationFailed(const QString &reason);
    void appointmentListReady(const QVariantList &appointments);
    void appointmentCancelled(int appointmentId);
    void appointmentOperationFailed(const QString &reason);

    // 病历医嘱信号
    void medicalHistoryReady(const QVariantList &records);
    void medicalRecordCreated(int recordId);
    void prescriptionAdded(int prescriptionId);
    void prescriptionListReady(const QVariantList &prescriptions);
    void hospitalizationCreated(int hospitalizationId);
    void medicalOperationFailed(const QString &reason);

    // 提醒信号
    void upcomingAppointmentsReady(const QVariantList &appointments);

    // 聊天通信信号
    void messageSent(int messageId);
    void chatHistoryReady(const QVariantList &messages);
    void contactListReady(const QVariantList &contacts);
    void newMessageReceived(int senderId, const QString &content);
    void chatOperationFailed(const QString &reason);

    // 考勤管理信号
    void checkInSuccess();
    void checkOutSuccess();
    void attendanceHistoryReady(const QVariantList &records);
    void leaveRequestSubmitted(int requestId);
    void leaveRequestsReady(const QVariantList &requests);
    void leaveCancelled(int requestId);
    void attendanceOperationFailed(const QString &reason);

    // 药品管理信号
    void drugSearchResultReady(const QVariantList &drugs);
    void drugDetailsReady(const QVariantMap &details);
    void drugOperationFailed(const QString &reason);

};

#endif // UICONTROLLER_H

