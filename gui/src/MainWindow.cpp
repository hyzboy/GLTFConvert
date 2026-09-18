#include "MainWindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QRadioButton>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QScrollBar>

MainWindow::MainWindow(const QStringList &initial_files, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("ULRE 模型转换工具 (GLTFConvert Qt)"));
    resize(1000, 720);
    setAcceptDrops(true);

    SetupUI();
    SetupConnections();
    CheckTexConvEnvironment();

    for (const QString &path : initial_files)
    {
        AddFilePath(path);
    }
}

MainWindow::~MainWindow()
{
    if (m_workThread && m_workThread->isRunning())
    {
        if (m_worker)
        {
            m_worker->Cancel();
        }
        m_workThread->quit();
        m_workThread->wait(3000);
    }
}

void MainWindow::SetupUI()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // 主分割器：上下分割（上半部分为配置与文件，下半部分为日志）
    auto *mainSplitter = new QSplitter(Qt::Vertical, this);

    // ---- 上半部分面板 ----
    auto *topWidget = new QWidget(mainSplitter);
    auto *topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(10);

    // 左侧：文件管理区
    auto *fileGroupBox = new QGroupBox(QStringLiteral("待转换模型列表 (支持拖拽 .gltf / .glb 文件)"), topWidget);
    auto *fileLayout = new QVBoxLayout(fileGroupBox);

    m_fileTable = new QTableWidget(0, 4, fileGroupBox);
    m_fileTable->setHorizontalHeaderLabels({
        QStringLiteral("文件名"),
        QStringLiteral("大小"),
        QStringLiteral("状态"),
        QStringLiteral("完整路径")
    });
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setAlternatingRowColors(true);
    fileLayout->addWidget(m_fileTable);

    auto *fileBtnLayout = new QHBoxLayout();
    m_btnAddFiles = new QPushButton(QStringLiteral("添加模型文件..."), fileGroupBox);
    m_btnAddFolder = new QPushButton(QStringLiteral("扫描文件夹..."), fileGroupBox);
    m_btnRemove = new QPushButton(QStringLiteral("移除选中"), fileGroupBox);
    m_btnClear = new QPushButton(QStringLiteral("清空列表"), fileGroupBox);

    fileBtnLayout->addWidget(m_btnAddFiles);
    fileBtnLayout->addWidget(m_btnAddFolder);
    fileBtnLayout->addWidget(m_btnRemove);
    fileBtnLayout->addWidget(m_btnClear);
    fileBtnLayout->addStretch();
    fileLayout->addLayout(fileBtnLayout);

    topLayout->addWidget(fileGroupBox, 6);

    // 右侧：配置选项区
    auto *rightPanel = new QWidget(topWidget);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    // 输出目录分组
    auto *outputGroup = new QGroupBox(QStringLiteral("输出目录设置"), rightPanel);
    auto *outputLayout = new QVBoxLayout(outputGroup);

    m_rbSameDir = new QRadioButton(QStringLiteral("与输入模型保存在同一目录"), outputGroup);
    m_rbSameDir->setChecked(true);
    m_rbCustomDir = new QRadioButton(QStringLiteral("统一输出到指定目录:"), outputGroup);

    auto *customDirLayout = new QHBoxLayout();
    m_editOutputDir = new QLineEdit(outputGroup);
    m_editOutputDir->setEnabled(false);
    m_btnBrowseOutput = new QPushButton(QStringLiteral("浏览..."), outputGroup);
    m_btnBrowseOutput->setEnabled(false);
    customDirLayout->addWidget(m_editOutputDir);
    customDirLayout->addWidget(m_btnBrowseOutput);

    outputLayout->addWidget(m_rbSameDir);
    outputLayout->addWidget(m_rbCustomDir);
    outputLayout->addLayout(customDirLayout);
    rightLayout->addWidget(outputGroup);

    // 转换参数分组
    auto *paramsGroup = new QGroupBox(QStringLiteral("几何与网格转换参数"), rightPanel);
    auto *paramsLayout = new QGridLayout(paramsGroup);

    paramsLayout->addWidget(new QLabel(QStringLiteral("法线压缩格式:"), paramsGroup), 0, 0);
    m_comboNormalFormat = new QComboBox(paramsGroup);
    m_comboNormalFormat->addItem(QStringLiteral("八面体 UNorm8 (V2UN8，默认推荐)"), 0);
    m_comboNormalFormat->addItem(QStringLiteral("半精度浮点 (V2HF)"), 1);
    m_comboNormalFormat->addItem(QStringLiteral("标准三维浮点 (V3F)"), 2);
    paramsLayout->addWidget(m_comboNormalFormat, 0, 1);

    m_chkMakeMeshlet = new QCheckBox(QStringLiteral("构建 Meshlet 数据 (GPU-Driven 渲染必备)"), paramsGroup);
    m_chkMakeMeshlet->setChecked(true);
    paramsLayout->addWidget(m_chkMakeMeshlet, 1, 0, 1, 2);

    m_chkWithTangent = new QCheckBox(QStringLiteral("导出法线切线 (Tangent)"), paramsGroup);
    paramsLayout->addWidget(m_chkWithTangent, 2, 0, 1, 2);

    m_chkAllowU8Indices = new QCheckBox(QStringLiteral("允许 uint8 索引缓冲区"), paramsGroup);
    paramsLayout->addWidget(m_chkAllowU8Indices, 3, 0, 1, 2);

    m_chkExportImages = new QCheckBox(QStringLiteral("导出并调用 TexConv 转换纹理"), paramsGroup);
    m_chkExportImages->setChecked(true);
    paramsLayout->addWidget(m_chkExportImages, 4, 0, 1, 2);

    m_chkImagesOnly = new QCheckBox(QStringLiteral("仅转换纹理 (跳过网格生成)"), paramsGroup);
    paramsLayout->addWidget(m_chkImagesOnly, 5, 0, 1, 2);

    rightLayout->addWidget(paramsGroup);

    // TexConv 状态卡片
    auto *texConvGroup = new QGroupBox(QStringLiteral("TexConv 纹理工具状态"), rightPanel);
    auto *texConvLayout = new QVBoxLayout(texConvGroup);
    m_lblTexConvStatus = new QLabel(QStringLiteral("正在检测..."), texConvGroup);
    m_lblTexConvStatus->setWordWrap(true);
    texConvLayout->addWidget(m_lblTexConvStatus);
    rightLayout->addWidget(texConvGroup);

    rightLayout->addStretch();
    topLayout->addWidget(rightPanel, 4);

    mainSplitter->addWidget(topWidget);

    // ---- 下半部分：日志面板 ----
    auto *bottomWidget = new QWidget(mainSplitter);
    auto *bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);

    auto *logHeaderLayout = new QHBoxLayout();
    logHeaderLayout->addWidget(new QLabel(QStringLiteral("转换输出日志:"), bottomWidget));
    logHeaderLayout->addStretch();
    m_btnClearLog = new QPushButton(QStringLiteral("清空日志"), bottomWidget);
    logHeaderLayout->addWidget(m_btnClearLog);
    bottomLayout->addLayout(logHeaderLayout);

    m_logEdit = new QPlainTextEdit(bottomWidget);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(2000);
    bottomLayout->addWidget(m_logEdit);

    mainSplitter->addWidget(bottomWidget);
    mainSplitter->setStretchFactor(0, 6);
    mainSplitter->setStretchFactor(1, 4);

    mainLayout->addWidget(mainSplitter);

    // ---- 控制与进度底栏 ----
    auto *bottomControlLayout = new QHBoxLayout();

    auto *progressLayout = new QVBoxLayout();
    m_lblStatus = new QLabel(QStringLiteral("就绪"), this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    progressLayout->addWidget(m_lblStatus);
    progressLayout->addWidget(m_progressBar);

    bottomControlLayout->addLayout(progressLayout, 1);

    m_btnStart = new QPushButton(QStringLiteral("开始转换"), this);
    m_btnStart->setMinimumSize(120, 42);
    m_btnStart->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));

    m_btnCancel = new QPushButton(QStringLiteral("取消"), this);
    m_btnCancel->setMinimumSize(80, 42);
    m_btnCancel->setEnabled(false);

    bottomControlLayout->addWidget(m_btnStart);
    bottomControlLayout->addWidget(m_btnCancel);

    mainLayout->addLayout(bottomControlLayout);
}

void MainWindow::SetupConnections()
{
    connect(m_btnAddFiles, &QPushButton::clicked, this, &MainWindow::OnAddFiles);
    connect(m_btnAddFolder, &QPushButton::clicked, this, &MainWindow::OnAddFolder);
    connect(m_btnRemove, &QPushButton::clicked, this, &MainWindow::OnRemoveSelected);
    connect(m_btnClear, &QPushButton::clicked, this, &MainWindow::OnClearFiles);

    connect(m_rbSameDir, &QRadioButton::toggled, this, &MainWindow::OnOutputModeToggled);
    connect(m_rbCustomDir, &QRadioButton::toggled, this, &MainWindow::OnOutputModeToggled);
    connect(m_btnBrowseOutput, &QPushButton::clicked, this, &MainWindow::OnBrowseOutputDir);

    connect(m_chkImagesOnly, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
        {
            m_chkExportImages->setChecked(true);
            m_chkExportImages->setEnabled(false);
            m_chkMakeMeshlet->setEnabled(false);
            m_chkWithTangent->setEnabled(false);
            m_chkAllowU8Indices->setEnabled(false);
            m_comboNormalFormat->setEnabled(false);
        }
        else
        {
            m_chkExportImages->setEnabled(true);
            m_chkMakeMeshlet->setEnabled(true);
            m_chkWithTangent->setEnabled(true);
            m_chkAllowU8Indices->setEnabled(true);
            m_comboNormalFormat->setEnabled(true);
        }
    });

    connect(m_btnClearLog, &QPushButton::clicked, this, &MainWindow::OnClearLog);
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::OnStartConvert);
    connect(m_btnCancel, &QPushButton::clicked, this, &MainWindow::OnCancelConvert);
}

void MainWindow::CheckTexConvEnvironment()
{
    bool available = false;
    char pathBuf[1024] = {0};
    GLTFConvert_GetTexConvInfo(&available, pathBuf, sizeof(pathBuf));

    if (available)
    {
        QString desc = QStringLiteral("<span style='color:#55ff55;'><b>已就绪</b></span><br>");
        desc += QStringLiteral("路径: %1").arg(QString::fromUtf8(pathBuf));
        m_lblTexConvStatus->setText(desc);
    }
    else
    {
        QString desc = QStringLiteral("<span style='color:#ffaa00;'><b>未检测到 TexConv</b></span><br>");
        desc += QStringLiteral("未找到 TexConv 可执行文件，纹理转换将跳过。<br>请在 <code>GLTFConvert.ini</code> 中配置 <code>TexConvDir</code>。");
        m_lblTexConvStatus->setText(desc);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls)
    {
        QString localPath = url.toLocalFile();
        if (localPath.isEmpty())
            continue;

        QFileInfo fi(localPath);
        if (fi.isDir())
        {
            QDir dir(localPath);
            QStringList filters = { QStringLiteral("*.gltf"), QStringLiteral("*.glb") };
            QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
            for (const auto &subFi : files)
            {
                AddFilePath(subFi.absoluteFilePath());
            }
        }
        else
        {
            AddFilePath(localPath);
        }
    }
}

void MainWindow::AddFilePath(const QString &path)
{
    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();
    if (ext != QStringLiteral("gltf") && ext != QStringLiteral("glb"))
    {
        return;
    }

    if (m_fileList.contains(path))
    {
        return;
    }

    m_fileList.append(path);
    int row = m_fileTable->rowCount();
    m_fileTable->insertRow(row);

    qint64 bytes = fi.size();
    QString sizeStr;
    if (bytes >= 1024 * 1024)
    {
        sizeStr = QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + QStringLiteral(" MB");
    }
    else
    {
        sizeStr = QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    }

    auto *itemFile = new QTableWidgetItem(fi.fileName());
    auto *itemSize = new QTableWidgetItem(sizeStr);
    auto *itemStatus = new QTableWidgetItem(QStringLiteral("等待中"));
    auto *itemPath = new QTableWidgetItem(fi.absoluteFilePath());

    itemFile->setToolTip(fi.absoluteFilePath());
    itemPath->setToolTip(fi.absoluteFilePath());

    m_fileTable->setItem(row, 0, itemFile);
    m_fileTable->setItem(row, 1, itemSize);
    m_fileTable->setItem(row, 2, itemStatus);
    m_fileTable->setItem(row, 3, itemPath);
}

void MainWindow::OnAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择 glTF 模型文件"),
        QString(),
        QStringLiteral("glTF 模型 (*.gltf *.glb);;所有文件 (*.*)")
    );

    for (const QString &file : files)
    {
        AddFilePath(file);
    }
}

void MainWindow::OnAddFolder()
{
    QString dirPath = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择包含 glTF 模型的文件夹")
    );

    if (dirPath.isEmpty())
        return;

    QDir dir(dirPath);
    QStringList filters = { QStringLiteral("*.gltf"), QStringLiteral("*.glb") };
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
    for (const auto &fi : files)
    {
        AddFilePath(fi.absoluteFilePath());
    }
}

void MainWindow::OnRemoveSelected()
{
    QList<QTableWidgetItem *> selected = m_fileTable->selectedItems();
    QSet<int> rows;
    for (auto *it : selected)
    {
        rows.insert(it->row());
    }

    QList<int> sortedRows = rows.values();
    std::sort(sortedRows.begin(), sortedRows.end(), std::greater<int>());

    for (int r : sortedRows)
    {
        m_fileTable->removeRow(r);
        m_fileList.removeAt(r);
    }
}

void MainWindow::OnClearFiles()
{
    m_fileTable->setRowCount(0);
    m_fileList.clear();
}

void MainWindow::OnOutputModeToggled()
{
    bool custom = m_rbCustomDir->isChecked();
    m_editOutputDir->setEnabled(custom);
    m_btnBrowseOutput->setEnabled(custom);
}

void MainWindow::OnBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择统一输出目录")
    );
    if (!dir.isEmpty())
    {
        m_editOutputDir->setText(dir);
    }
}

void MainWindow::OnClearLog()
{
    m_logEdit->clear();
}

void MainWindow::SetControlsEnabled(bool enabled)
{
    m_fileTable->setEnabled(enabled);
    m_btnAddFiles->setEnabled(enabled);
    m_btnAddFolder->setEnabled(enabled);
    m_btnRemove->setEnabled(enabled);
    m_btnClear->setEnabled(enabled);

    m_rbSameDir->setEnabled(enabled);
    m_rbCustomDir->setEnabled(enabled);
    if (m_rbCustomDir->isChecked())
    {
        m_editOutputDir->setEnabled(enabled);
        m_btnBrowseOutput->setEnabled(enabled);
    }

    m_comboNormalFormat->setEnabled(enabled && !m_chkImagesOnly->isChecked());
    m_chkMakeMeshlet->setEnabled(enabled && !m_chkImagesOnly->isChecked());
    m_chkWithTangent->setEnabled(enabled && !m_chkImagesOnly->isChecked());
    m_chkAllowU8Indices->setEnabled(enabled && !m_chkImagesOnly->isChecked());
    m_chkExportImages->setEnabled(enabled && !m_chkImagesOnly->isChecked());
    m_chkImagesOnly->setEnabled(enabled);

    m_btnStart->setEnabled(enabled);
    m_btnCancel->setEnabled(!enabled);
}

void MainWindow::AppendLog(int level, const QString &msg)
{
    QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz"));
    QString color = QStringLiteral("#c8c8c8");
    QString prefix = QStringLiteral("INFO");

    switch (level)
    {
    case GLTF_LOG_VERBOSE:
        color = QStringLiteral("#777777");
        prefix = QStringLiteral("VERB");
        break;
    case GLTF_LOG_DEBUG:
        color = QStringLiteral("#888888");
        prefix = QStringLiteral("DEBUG");
        break;
    case GLTF_LOG_INFO:
        color = QStringLiteral("#d0d0d0");
        prefix = QStringLiteral("INFO");
        break;
    case GLTF_LOG_WARN:
        color = QStringLiteral("#ffaa00");
        prefix = QStringLiteral("WARN");
        break;
    case GLTF_LOG_ERROR:
        color = QStringLiteral("#ff5555");
        prefix = QStringLiteral("ERROR");
        break;
    }

    QString html = QStringLiteral("<span style='color:#777;'>[%1]</span> <span style='color:%2;'>[%3] %4</span>")
        .arg(timestamp, color, prefix, msg.toHtmlEscaped());

    m_logEdit->appendHtml(html);
    m_logEdit->verticalScrollBar()->setValue(m_logEdit->verticalScrollBar()->maximum());
}

void MainWindow::OnStartConvert()
{
    if (m_fileList.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加待转换的 glTF / glb 模型文件！"));
        return;
    }

    QString customOutDir;
    if (m_rbCustomDir->isChecked())
    {
        customOutDir = m_editOutputDir->text().trimmed();
        if (customOutDir.isEmpty() || !QDir(customOutDir).exists())
        {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请指定有效的统一输出目录！"));
            return;
        }
    }

    QList<TaskItem> tasks;
    for (int i = 0; i < m_fileList.size(); ++i)
    {
        TaskItem item;
        item.input_path = m_fileList[i];
        item.output_dir = customOutDir;
        tasks.append(item);

        if (auto *st = m_fileTable->item(i, 2))
        {
            st->setText(QStringLiteral("等待中"));
            st->setForeground(QBrush(QColor(200, 200, 200)));
        }
    }

    UIConvertOptions options;
    options.normal_format = m_comboNormalFormat->currentData().toInt();
    options.with_tangent = m_chkWithTangent->isChecked();
    options.allow_u8_indices = m_chkAllowU8Indices->isChecked();
    options.make_meshlet = m_chkMakeMeshlet->isChecked();
    options.export_images = m_chkExportImages->isChecked();
    options.images_only = m_chkImagesOnly->isChecked();

    SetControlsEnabled(false);
    m_progressBar->setValue(0);
    m_lblStatus->setText(QStringLiteral("正在启动转换..."));

    m_workThread = new QThread(this);
    m_worker = new ConvertWorker();
    m_worker->SetTasks(tasks, options);
    m_worker->moveToThread(m_workThread);

    connect(m_workThread, &QThread::started, m_worker, &ConvertWorker::Run);

    connect(m_worker, &ConvertWorker::TaskStarted, this, &MainWindow::OnTaskStarted);
    connect(m_worker, &ConvertWorker::TaskProgress, this, &MainWindow::OnTaskProgress);
    connect(m_worker, &ConvertWorker::TaskLog, this, &MainWindow::OnTaskLog);
    connect(m_worker, &ConvertWorker::TaskFinished, this, &MainWindow::OnTaskFinished);
    connect(m_worker, &ConvertWorker::AllFinished, this, &MainWindow::OnAllFinished);

    connect(m_worker, &ConvertWorker::AllFinished, m_workThread, &QThread::quit);
    connect(m_workThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_workThread, &QThread::finished, m_workThread, &QObject::deleteLater);
    connect(m_workThread, &QThread::finished, this, [this]() {
        m_workThread = nullptr;
        m_worker = nullptr;
    });

    m_workThread->start();
}

void MainWindow::OnCancelConvert()
{
    if (m_worker)
    {
        m_worker->Cancel();
        m_lblStatus->setText(QStringLiteral("正在取消任务..."));
    }
}

void MainWindow::OnTaskStarted(int taskIndex, int totalTasks, const QString &filePath)
{
    QFileInfo fi(filePath);
    m_lblStatus->setText(QStringLiteral("[%1/%2] 正在处理: %3").arg(taskIndex + 1).arg(totalTasks).arg(fi.fileName()));
    if (auto *st = m_fileTable->item(taskIndex, 2))
    {
        st->setText(QStringLiteral("转换中..."));
        st->setForeground(QBrush(QColor(85, 170, 255)));
    }
}

void MainWindow::OnTaskProgress(int taskIndex, float percentage, const QString &stepName)
{
    int total = m_fileList.size();
    if (total <= 0) return;

    float overall = ((float)taskIndex + (percentage / 100.0f)) / (float)total * 100.0f;
    m_progressBar->setValue(static_cast<int>(overall));

    if (!stepName.isEmpty())
    {
        QFileInfo fi(m_fileList[taskIndex]);
        m_lblStatus->setText(QStringLiteral("[%1/%2] %3: %4 (%5%)")
            .arg(taskIndex + 1)
            .arg(total)
            .arg(fi.fileName())
            .arg(stepName)
            .arg(static_cast<int>(percentage)));
    }
}

void MainWindow::OnTaskLog(int level, const QString &message)
{
    AppendLog(level, message);
}

void MainWindow::OnTaskFinished(int taskIndex, bool success)
{
    if (auto *st = m_fileTable->item(taskIndex, 2))
    {
        if (success)
        {
            st->setText(QStringLiteral("完成"));
            st->setForeground(QBrush(QColor(85, 255, 85)));
        }
        else
        {
            st->setText(QStringLiteral("失败"));
            st->setForeground(QBrush(QColor(255, 85, 85)));
        }
    }
}

void MainWindow::OnAllFinished(int successCount, int failCount)
{
    m_progressBar->setValue(100);
    SetControlsEnabled(true);

    QString summary = QStringLiteral("转换结束: 成功 %1 个，失败 %2 个。").arg(successCount).arg(failCount);
    m_lblStatus->setText(summary);
    AppendLog(GLTF_LOG_INFO, summary);

    if (failCount > 0)
    {
        QMessageBox::warning(this, QStringLiteral("转换完成"), summary + QStringLiteral("\n请查看下方日志获取错误详情。"));
    }
    else
    {
        QMessageBox::information(this, QStringLiteral("转换完成"), summary);
    }
}
