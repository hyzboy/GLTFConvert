#include "ConvertWorker.h"
#include <QByteArray>

ConvertWorker::ConvertWorker(QObject *parent)
    : QObject(parent)
{
}

void ConvertWorker::SetTasks(const QList<TaskItem> &tasks, const UIConvertOptions &options)
{
    m_tasks = tasks;
    m_options = options;
    m_cancelRequested = false;
}

void ConvertWorker::Cancel()
{
    m_cancelRequested = true;
}

void ConvertWorker::ProgressCallbackThunk(float percentage, const char *step_name, void *user_data)
{
    auto *worker = static_cast<ConvertWorker *>(user_data);
    if (worker)
    {
        worker->HandleProgress(percentage, QString::fromUtf8(step_name ? step_name : ""));
    }
}

void ConvertWorker::LogCallbackThunk(GLTFLogLevel level, const char *msg, void *user_data)
{
    auto *worker = static_cast<ConvertWorker *>(user_data);
    if (worker)
    {
        worker->HandleLog(static_cast<int>(level), QString::fromUtf8(msg ? msg : ""));
    }
}

void ConvertWorker::HandleProgress(float percentage, const QString &step_name)
{
    Q_EMIT TaskProgress(m_currentTaskIndex, percentage, step_name);
}

void ConvertWorker::HandleLog(int level, const QString &msg)
{
    Q_EMIT TaskLog(level, msg);
}

void ConvertWorker::Run()
{
    int total = m_tasks.size();
    int successCount = 0;
    int failCount = 0;

    QByteArray configUtf8 = m_options.custom_config_file.toUtf8();
    const char *configPath = configUtf8.isEmpty() ? nullptr : configUtf8.constData();

    for (int i = 0; i < total; ++i)
    {
        if (m_cancelRequested)
        {
            Q_EMIT TaskLog(static_cast<int>(GLTF_LOG_WARN), QStringLiteral("用户取消了转换任务。"));
            break;
        }

        m_currentTaskIndex = i;
        const TaskItem &item = m_tasks[i];

        Q_EMIT TaskStarted(i, total, item.input_path);

        QByteArray inputUtf8 = item.input_path.toUtf8();
        QByteArray outputUtf8 = item.output_dir.toUtf8();

        GLTFConvertOptions opts;
        GLTFConvert_InitDefaultOptions(&opts);
        opts.input_path = inputUtf8.constData();
        opts.output_dir = outputUtf8.isEmpty() ? nullptr : outputUtf8.constData();
        opts.config_file = configPath;
        opts.normal_format = static_cast<GLTFNormalFormat>(m_options.normal_format);
        opts.export_tangent = m_options.with_tangent;
        opts.allow_u8_indices = m_options.allow_u8_indices;
        opts.build_meshlets = m_options.make_meshlet;
        opts.export_images = m_options.export_images;
        opts.images_only = m_options.images_only;

        opts.progress_cb = ProgressCallbackThunk;
        opts.log_cb = LogCallbackThunk;
        opts.user_data = this;

        char errBuf[1024] = {0};
        bool ok = GLTFConvert_Process(&opts, errBuf, sizeof(errBuf));
        if (ok)
        {
            successCount++;
        }
        else
        {
            failCount++;
            if (errBuf[0] != '\0')
            {
                Q_EMIT TaskLog(static_cast<int>(GLTF_LOG_ERROR), QString::fromUtf8(errBuf));
            }
        }

        Q_EMIT TaskFinished(i, ok);
    }

    Q_EMIT AllFinished(successCount, failCount);
}
