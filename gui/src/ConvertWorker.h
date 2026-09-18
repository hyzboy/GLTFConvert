#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <atomic>
#include "GLTFConvertCore.h"

struct TaskItem
{
    QString input_path;
    QString output_dir;
};

struct UIConvertOptions
{
    int normal_format = 0;      // 0: V2UN8, 1: V2HF, 2: V3F
    bool with_tangent = false;
    bool allow_u8_indices = false;
    bool make_meshlet = true;
    bool export_images = true;
    bool images_only = false;
    QString custom_config_file;
};

class ConvertWorker : public QObject
{
    Q_OBJECT

public:
    explicit ConvertWorker(QObject *parent = nullptr);
    ~ConvertWorker() override = default;

    void SetTasks(const QList<TaskItem> &tasks, const UIConvertOptions &options);
    void Cancel();

public Q_SLOTS:
    void Run();

Q_SIGNALS:
    void TaskStarted(int taskIndex, int totalTasks, const QString &filePath);
    void TaskProgress(int taskIndex, float percentage, const QString &stepName);
    void TaskLog(int level, const QString &message);
    void TaskFinished(int taskIndex, bool success);
    void AllFinished(int successCount, int failCount);

private:
    static void ProgressCallbackThunk(float percentage, const char *step_name, void *user_data);
    static void LogCallbackThunk(GLTFLogLevel level, const char *msg, void *user_data);

    void HandleProgress(float percentage, const QString &step_name);
    void HandleLog(int level, const QString &msg);

    QList<TaskItem> m_tasks;
    UIConvertOptions m_options;
    std::atomic<bool> m_cancelRequested{false};
    int m_currentTaskIndex = 0;
};
