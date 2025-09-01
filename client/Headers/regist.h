#ifndef REGIST_H
#define REGIST_H

#include <QWidget>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
namespace Ui { class Regist; }
QT_END_NAMESPACE

class Regist : public QWidget
{
    Q_OBJECT
public:
    explicit Regist(QWidget *parent = nullptr);
    ~Regist();

    // 从登录页带入默认值
    void preset(const QString& role, const QString& user, const QString& pass);

signals:
    // 让登录页知道应该恢复显示
    void requestBackToLogin();

private slots:
    void on_btnRegister_clicked();
    void on_btnBackLogin_clicked();

private:
    QString selectedRole() const; // expert | factory | ""
    bool sendRequest(const QJsonObject& obj, QJsonObject& reply, QString* errMsg = nullptr);

private:
    Ui::Regist *ui;
};

#endif // REGIST_H
