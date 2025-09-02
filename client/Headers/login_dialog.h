#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QProgressBar>
#include <QMessageBox>

class ClientConn;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    struct LoginResult {
        bool success = false;
        QString token;
        QString username;
        QString role;
        QString errorMessage;
    };

    LoginResult getLoginResult() const { return result_; }

public slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onModeChanged();

private:
    void setupUI();
    void setMode(bool isRegisterMode);
    bool performAuth(const QString& op, const QString& username, const QString& password, const QString& role = "");

private:
    QLineEdit* usernameEdit_;
    QLineEdit* passwordEdit_;
    QLineEdit* confirmPasswordEdit_;
    QComboBox* roleCombo_;
    QPushButton* loginBtn_;
    QPushButton* registerBtn_;
    QPushButton* switchModeBtn_;
    QLabel* titleLabel_;
    QLabel* statusLabel_;
    QProgressBar* progressBar_;
    QGroupBox* formGroup_;

    bool isRegisterMode_;
    LoginResult result_;
    ClientConn* conn_;
};