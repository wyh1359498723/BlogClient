#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QPushButton>
#include <QLabel>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void saveSettings();
    void testConnection();

private:
    void loadSettings();

    QLineEdit *m_apiUrlEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QLineEdit *m_userNameEdit;
    QPushButton *m_testButton;
}; 