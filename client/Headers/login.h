#ifndef LOGIN_H
#define LOGIN_H

#include <QWidget>
#include <QPointer>

QT_BEGIN_NAMESPACE
namespace Ui { class Login; }
QT_END_NAMESPACE

class ClientExpert;
class ClientFactory;
class Regist;

class Login : public QWidget
{
    Q_OBJECT
public:
    explicit Login(QWidget *parent = nullptr);
    ~Login();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btnLogin_clicked();
    void on_btnToReg_clicked();

private:
    bool sendRequest(const QJsonObject &obj, QJsonObject &reply, QString *errMsg = nullptr);
    QString selectedRole() const;

private:
    Ui::Login *ui;
    QPointer<ClientExpert>  expertWin;
    QPointer<ClientFactory> factoryWin;
    QPointer<Regist>        regWin;     // 防止重复打开注册窗口
};

#endif // LOGIN_H
