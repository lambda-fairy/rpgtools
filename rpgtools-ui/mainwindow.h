#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QProcess>
#include <QString>

class QDir;
class QPushButton;
class QProgressDialog;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    explicit MainWindow(QDir &tooldir);

private slots:
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processCanceled();
    void mapdumpClicked();
    void rpgconvClicked();
    void xyzClicked();

private:
    void startProcess(const QString &name, const QStringList &args);

    QPushButton *mapdumpBtn;
    QPushButton *rpgconvBtn;
    QPushButton *xyzBtn;

    QString mapdumpPath;
    QString rpgconvPath;
    QString xyzPath;

    QProcess *process;
    QProgressDialog *progress;
    bool processWasCanceled;
};

#endif // MAINWINDOW_H
