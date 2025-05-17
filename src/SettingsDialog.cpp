#include "SettingsDialog.h"
#include "api/WordPressAPI.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("博客API设置"));
    setMinimumWidth(400);

    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout;
    
    // API URL
    m_apiUrlEdit = new QLineEdit(this);
    formLayout->addRow(tr("WordPress API URL:"), m_apiUrlEdit);
    
    // 用户名
    m_usernameEdit = new QLineEdit(this);
    formLayout->addRow(tr("用户名:"), m_usernameEdit);
    
    // 密码
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(tr("密码:"), m_passwordEdit);
    
    // 显示名称
    m_userNameEdit = new QLineEdit(this);
    formLayout->addRow(tr("显示名称:"), m_userNameEdit);
    
    // 创建API设置组
    QGroupBox *apiGroupBox = new QGroupBox(tr("API设置"), this);
    apiGroupBox->setLayout(formLayout);
    
    // 创建按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_testButton = new QPushButton(tr("测试连接"), this);
    buttonBox->addButton(m_testButton, QDialogButtonBox::ActionRole);
    
    // 连接信号和槽
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::saveSettings);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(m_testButton, &QPushButton::clicked, this, &SettingsDialog::testConnection);
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(apiGroupBox);
    mainLayout->addWidget(buttonBox);
    
    setLayout(mainLayout);
    
    // 加载现有设置
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::loadSettings()
{
    QSettings settings;
    
    // 加载API设置
    m_apiUrlEdit->setText(settings.value("api/url").toString());
    m_usernameEdit->setText(settings.value("api/username").toString());
    m_passwordEdit->setText(settings.value("api/password").toString());
    m_userNameEdit->setText(settings.value("user/name").toString());
}

void SettingsDialog::saveSettings()
{
    QSettings settings;
    
    // 保存API设置
    settings.setValue("api/url", m_apiUrlEdit->text());
    settings.setValue("api/username", m_usernameEdit->text());
    settings.setValue("api/password", m_passwordEdit->text());
    settings.setValue("user/name", m_userNameEdit->text());
    
    // 确保立即保存设置到磁盘
    settings.sync();
    
    // 显示保存成功提示
    QMessageBox::information(this, tr("设置已保存"), 
        tr("API设置已成功保存。现在可以使用远程功能了。"),
        QMessageBox::Ok);
    
    accept();
}

void SettingsDialog::testConnection()
{
    // 禁用测试按钮，避免重复点击
    m_testButton->setEnabled(false);
    
    // 获取设置值
    QString apiUrl = m_apiUrlEdit->text();
    QString username = m_usernameEdit->text();
    QString password = m_passwordEdit->text();
    
    // 验证输入
    if (apiUrl.isEmpty() || username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"), 
            tr("请输入所有必填字段。"),
            QMessageBox::Ok);
        m_testButton->setEnabled(true);
        return;
    }
    
    // 设置API信息
    WordPressAPI::instance().setApiUrl(apiUrl);
    WordPressAPI::instance().setCredentials(username, password);
    
    // 连接API错误信号
    connect(&WordPressAPI::instance(), &WordPressAPI::error, this, [this](const QString& error) {
        QMessageBox::critical(this, tr("连接失败"), 
            tr("API连接测试失败：%1").arg(error),
            QMessageBox::Ok);
        m_testButton->setEnabled(true);
        
        // 断开连接，避免多次触发
        disconnect(&WordPressAPI::instance(), &WordPressAPI::error, this, nullptr);
    });
    
    // 连接成功信号
    connect(&WordPressAPI::instance(), &WordPressAPI::postsReceived, this, [this](const QList<Post>&) {
        QMessageBox::information(this, tr("连接成功"), 
            tr("WordPress API连接测试成功！\n\n请点击\"确定\"按钮保存这些设置。"),
            QMessageBox::Ok);
        m_testButton->setEnabled(true);
        
        // 断开连接，避免多次触发
        disconnect(&WordPressAPI::instance(), &WordPressAPI::postsReceived, this, nullptr);
    });
    
    // 测试连接 - 尝试获取文章列表
    WordPressAPI::instance().fetchPosts();
} 