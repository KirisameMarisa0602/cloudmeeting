#pragma once
#include <QtWidgets>

class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    QComboBox* comboRole;     // 工厂/专家
    QLineEdit* editAccount;   // 账号
    QLineEdit* editUsername;  // 用户名称/昵称
    QLineEdit* editPassword;  // 密码

    explicit RegisterDialog(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("注册"));
        setMinimumSize(420, 300);

        comboRole = new QComboBox(this);
        comboRole->addItems(QStringList() << "工厂" << "专家");

        editAccount  = new QLineEdit(this);  editAccount->setPlaceholderText("账号（用于登录，唯一）");
        editUsername = new QLineEdit(this);  editUsername->setPlaceholderText("用户名称/昵称（用于展示）");
        editPassword = new QLineEdit(this);  editPassword->setPlaceholderText("密码"); editPassword->setEchoMode(QLineEdit::Password);

        auto form = new QFormLayout;
        form->addRow("身份：",   comboRole);
        form->addRow("账号：",   editAccount);
        form->addRow("用户名称：", editUsername);
        form->addRow("密码：",   editPassword);

        QDialogButtonBox* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        auto lay = new QVBoxLayout(this);
        lay->addLayout(form);
        lay->addWidget(btns);

        connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
};
