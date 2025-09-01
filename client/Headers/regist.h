#ifndef REGIST_H
#define REGIST_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class Regist; }
QT_END_NAMESPACE

class Regist : public QWidget
{
    Q_OBJECT
public:
    explicit Regist(QWidget *parent = nullptr);
    ~Regist();

    void preset(const QString &role, const QString &user, const QString &pass);

signals:
    void requestBackToLogin();

private slots:
    void on_btnRegister_clicked();
    void on_btnBackLogin_clicked();

private:
    QString selectedRole() const;
    bool sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg = nullptr);

private:
    Ui::Regist *ui;
    bool submitting_ = false;   // 防抖：注册中的状态
};

#endif // REGIST_H
