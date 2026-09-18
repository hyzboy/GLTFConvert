#pragma once

#include <QMainWindow>
#include <QThread>
#include <QList>
#include <QStringList>

class QTableWidget;
class QPushButton;
class QProgressBar;
class QLabel;
class QPlainTextEdit;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QGroupBox;

#include "ConvertWorker.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QStringList &initial_files = QStringList(), QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private Q_SLOTS:
    void OnAddFiles();
    void OnAddFolder();
    void OnRemoveSelected();
    void OnClearFiles();
    void OnBrowseOutputDir();
    void OnOutputModeToggled();
    void OnStartConvert();
    void OnCancelConvert();
    void OnClearLog();

    // Worker signals
    void OnTaskStarted(int taskIndex, int totalTasks, const QString &filePath);
    void OnTaskProgress(int taskIndex, float percentage, const QString &stepName);
    void OnTaskLog(int level, const QString &message);
    void OnTaskFinished(int taskIndex, bool success);
    void OnAllFinished(int successCount, int failCount);

private:
    void SetupUI();
    void SetupConnections();
    void CheckTexConvEnvironment();
    void AddFilePath(const QString &path);
    void SetControlsEnabled(bool enabled);
    void AppendLog(int level, const QString &msg);

    // File table
    QTableWidget *m_fileTable = nullptr;
    QPushButton *m_btnAddFiles = nullptr;
    QPushButton *m_btnAddFolder = nullptr;
    QPushButton *m_btnRemove = nullptr;
    QPushButton *m_btnClear = nullptr;

    // Output options
    QRadioButton *m_rbSameDir = nullptr;
    QRadioButton *m_rbCustomDir = nullptr;
    QLineEdit *m_editOutputDir = nullptr;
    QPushButton *m_btnBrowseOutput = nullptr;

    // Convert options
    QComboBox *m_comboNormalFormat = nullptr;
    QCheckBox *m_chkMakeMeshlet = nullptr;
    QCheckBox *m_chkWithTangent = nullptr;
    QCheckBox *m_chkAllowU8Indices = nullptr;
    QCheckBox *m_chkExportImages = nullptr;
    QCheckBox *m_chkImagesOnly = nullptr;

    // TexConv info
    QLabel *m_lblTexConvStatus = nullptr;

    // Progress and control
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_lblStatus = nullptr;
    QPushButton *m_btnStart = nullptr;
    QPushButton *m_btnCancel = nullptr;

    // Log
    QPlainTextEdit *m_logEdit = nullptr;
    QPushButton *m_btnClearLog = nullptr;

    // Threading
    QThread *m_workThread = nullptr;
    ConvertWorker *m_worker = nullptr;

    QStringList m_fileList;
};
