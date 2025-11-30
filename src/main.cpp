#include "BackupEngine.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QProgressBar>
#include <QSpinBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>
#include <QtGlobal>

#include <algorithm>
#include <optional>

class MainWindow : public QMainWindow
{
public:
    MainWindow(QWidget *parent = nullptr);

private:
    void setupBackupTab(QWidget *parent);
    void setupRestoreTab(QWidget *parent);
    void setupSettingsTab(QWidget *parent);

    void handleBackupClicked();
    void runBackup(const BackupOptions &options, bool scheduled = false);
    void onBackupFinished();

    void handleRestoreClicked();
    void onRestoreFinished();

    void handleVerifyClicked();
    void onVerifyFinished();

    void chooseBackupSource();
    void chooseBackupDestination();
    void chooseRestoreBackupFile();
    void chooseRestoreBackupDirectory();
    void chooseRestoreDestination();

    void appendBackupLog(const QString &message);
    void appendRestoreLog(const QString &message);
    void appendLog(QTextEdit *target, const QString &message);
    void updateBackupProgress(int current, int total);
    void updateRestoreProgress(int current, int total);

    std::optional<BackupOptions> collectBackupOptions(QString &error) const;
    std::optional<RestoreOptions> collectRestoreOptions(QString &error) const;
    std::optional<VerifyOptions> collectVerifyOptions(QString &error) const;

    void setBackupUiEnabled(bool enabled);
    void setRestoreUiEnabled(bool enabled);

    void saveScheduleSettings();
    void handleScheduleTimeout();
    void toggleScheduling(bool enabled);
    void updateNextRunLabel();
    QDateTime computeNextRun(const QDateTime &from) const;

    QLineEdit *backupSourceEdit = nullptr;
    QLineEdit *backupDestinationEdit = nullptr;
    QLineEdit *backupNameEdit = nullptr;
    QCheckBox *compressCheck = nullptr;
    QCheckBox *encryptCheck = nullptr;
    QCheckBox *packageCheck = nullptr;
    QCheckBox *metadataCheck = nullptr;
    QCheckBox *specialCheck = nullptr;
    QCheckBox *verifyCheck = nullptr;
    QLineEdit *passwordEdit = nullptr;
    QTextEdit *backupLog = nullptr;
    QProgressBar *backupProgress = nullptr;
    QPushButton *backupButton = nullptr;

    QLineEdit *restoreBackupEdit = nullptr;
    QLineEdit *restoreDestinationEdit = nullptr;
    QLineEdit *restorePasswordEdit = nullptr;
    QTextEdit *restoreLog = nullptr;
    QProgressBar *restoreProgress = nullptr;
    QPushButton *restoreButton = nullptr;
    QPushButton *verifyButton = nullptr;

    QCheckBox *scheduleEnableCheck = nullptr;
    QGroupBox *scheduleGroup = nullptr;
    QComboBox *scheduleFrequencyCombo = nullptr;
    QTimeEdit *scheduleTimeEdit = nullptr;
    QSpinBox *scheduleRetentionSpin = nullptr;
    QLabel *nextRunLabel = nullptr;
    QTimer *scheduleTimer = nullptr;

    QFutureWatcher<BackupResult> backupWatcher;
    QFutureWatcher<RestoreResult> restoreWatcher;
    QFutureWatcher<VerifyResult> verifyWatcher;

    BackupOptions scheduledOptions;
    bool hasScheduledOptions = false;
    QDateTime nextScheduleRun;
    int scheduleAnchorWeekday = 1;
    int scheduleAnchorDay = 1;
    QString scheduledFrequency;
    QTime scheduledTime;
    bool backupTriggeredBySchedule = false;
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("文件备份软件");
    setMinimumSize(900, 600);

    QTabWidget *tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    QWidget *backupTab = new QWidget();
    setupBackupTab(backupTab);
    tabWidget->addTab(backupTab, QStringLiteral("备份"));

    QWidget *restoreTab = new QWidget();
    setupRestoreTab(restoreTab);
    tabWidget->addTab(restoreTab, QStringLiteral("恢复"));

    QWidget *settingsTab = new QWidget();
    setupSettingsTab(settingsTab);
    tabWidget->addTab(settingsTab, QStringLiteral("设置"));

    connect(&backupWatcher, &QFutureWatcher<BackupResult>::finished, this, &MainWindow::onBackupFinished);
    connect(&restoreWatcher, &QFutureWatcher<RestoreResult>::finished, this, &MainWindow::onRestoreFinished);
    connect(&verifyWatcher, &QFutureWatcher<VerifyResult>::finished, this, &MainWindow::onVerifyFinished);

    scheduleTimer = new QTimer(this);
    scheduleTimer->setInterval(60000);
    connect(scheduleTimer, &QTimer::timeout, this, &MainWindow::handleScheduleTimeout);
}

void MainWindow::setupBackupTab(QWidget *parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(parent);

    QGridLayout *grid = new QGridLayout();
    backupSourceEdit = new QLineEdit(parent);
    QPushButton *sourceButton = new QPushButton(QStringLiteral("选择..."), parent);
    connect(sourceButton, &QPushButton::clicked, this, &MainWindow::chooseBackupSource);
    grid->addWidget(new QLabel(QStringLiteral("源路径:"), parent), 0, 0);
    grid->addWidget(backupSourceEdit, 0, 1);
    grid->addWidget(sourceButton, 0, 2);

    backupDestinationEdit = new QLineEdit(parent);
    QPushButton *destinationButton = new QPushButton(QStringLiteral("选择..."), parent);
    connect(destinationButton, &QPushButton::clicked, this, &MainWindow::chooseBackupDestination);
    grid->addWidget(new QLabel(QStringLiteral("目标路径:"), parent), 1, 0);
    grid->addWidget(backupDestinationEdit, 1, 1);
    grid->addWidget(destinationButton, 1, 2);

    backupNameEdit = new QLineEdit(parent);
    grid->addWidget(new QLabel(QStringLiteral("备份名称:"), parent), 2, 0);
    grid->addWidget(backupNameEdit, 2, 1, 1, 2);

    mainLayout->addLayout(grid);

    QGroupBox *optionsGroup = new QGroupBox(QStringLiteral("备份选项"), parent);
    QGridLayout *optionsLayout = new QGridLayout(optionsGroup);
    compressCheck = new QCheckBox(QStringLiteral("启用压缩"), optionsGroup);
    encryptCheck = new QCheckBox(QStringLiteral("启用加密"), optionsGroup);
    packageCheck = new QCheckBox(QStringLiteral("打包为单文件"), optionsGroup);
    metadataCheck = new QCheckBox(QStringLiteral("保留元数据"), optionsGroup);
    metadataCheck->setChecked(true);
    specialCheck = new QCheckBox(QStringLiteral("包含特殊文件"), optionsGroup);
    specialCheck->setChecked(true);
    verifyCheck = new QCheckBox(QStringLiteral("备份后校验"), optionsGroup);
    verifyCheck->setChecked(true);
    passwordEdit = new QLineEdit(optionsGroup);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(QStringLiteral("输入加密密码"));

    optionsLayout->addWidget(compressCheck, 0, 0);
    optionsLayout->addWidget(encryptCheck, 0, 1);
    optionsLayout->addWidget(packageCheck, 0, 2);
    optionsLayout->addWidget(metadataCheck, 1, 0);
    optionsLayout->addWidget(specialCheck, 1, 1);
    optionsLayout->addWidget(verifyCheck, 1, 2);
    optionsLayout->addWidget(new QLabel(QStringLiteral("加密密码:"), optionsGroup), 2, 0);
    optionsLayout->addWidget(passwordEdit, 2, 1, 1, 2);

    mainLayout->addWidget(optionsGroup);

    backupButton = new QPushButton(QStringLiteral("开始备份"), parent);
    connect(backupButton, &QPushButton::clicked, this, &MainWindow::handleBackupClicked);
    mainLayout->addWidget(backupButton);

    backupProgress = new QProgressBar(parent);
    backupProgress->setRange(0, 100);
    backupProgress->setValue(0);
    mainLayout->addWidget(backupProgress);

    backupLog = new QTextEdit(parent);
    backupLog->setReadOnly(true);
    backupLog->setPlaceholderText(QStringLiteral("备份日志输出..."));
    mainLayout->addWidget(backupLog);
}

void MainWindow::setupRestoreTab(QWidget *parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(parent);

    QGridLayout *grid = new QGridLayout();
    restoreBackupEdit = new QLineEdit(parent);

    QPushButton *backupSelectButton = new QPushButton(QStringLiteral("选择..."), parent);
    QMenu *selectMenu = new QMenu(backupSelectButton);
    selectMenu->addAction(QStringLiteral("备份文件"), this, &MainWindow::chooseRestoreBackupFile);
    selectMenu->addAction(QStringLiteral("备份目录"), this, &MainWindow::chooseRestoreBackupDirectory);
    backupSelectButton->setMenu(selectMenu);

    grid->addWidget(new QLabel(QStringLiteral("备份路径:"), parent), 0, 0);
    grid->addWidget(restoreBackupEdit, 0, 1);
    grid->addWidget(backupSelectButton, 0, 2);

    restoreDestinationEdit = new QLineEdit(parent);
    QPushButton *restoreDestinationButton = new QPushButton(QStringLiteral("选择..."), parent);
    connect(restoreDestinationButton, &QPushButton::clicked, this, &MainWindow::chooseRestoreDestination);
    grid->addWidget(new QLabel(QStringLiteral("恢复到:"), parent), 1, 0);
    grid->addWidget(restoreDestinationEdit, 1, 1);
    grid->addWidget(restoreDestinationButton, 1, 2);

    restorePasswordEdit = new QLineEdit(parent);
    restorePasswordEdit->setEchoMode(QLineEdit::Password);
    restorePasswordEdit->setPlaceholderText(QStringLiteral("若备份已加密，请输入密码"));
    grid->addWidget(new QLabel(QStringLiteral("解密密码:"), parent), 2, 0);
    grid->addWidget(restorePasswordEdit, 2, 1, 1, 2);

    mainLayout->addLayout(grid);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    restoreButton = new QPushButton(QStringLiteral("开始恢复"), parent);
    verifyButton = new QPushButton(QStringLiteral("验证备份"), parent);
    buttonLayout->addWidget(restoreButton);
    buttonLayout->addWidget(verifyButton);
    mainLayout->addLayout(buttonLayout);

    connect(restoreButton, &QPushButton::clicked, this, &MainWindow::handleRestoreClicked);
    connect(verifyButton, &QPushButton::clicked, this, &MainWindow::handleVerifyClicked);

    restoreProgress = new QProgressBar(parent);
    restoreProgress->setRange(0, 100);
    restoreProgress->setValue(0);
    mainLayout->addWidget(restoreProgress);

    restoreLog = new QTextEdit(parent);
    restoreLog->setReadOnly(true);
    restoreLog->setPlaceholderText(QStringLiteral("恢复/验证日志..."));
    mainLayout->addWidget(restoreLog);
}

void MainWindow::setupSettingsTab(QWidget *parent)
{
    QVBoxLayout *layout = new QVBoxLayout(parent);
    scheduleEnableCheck = new QCheckBox(QStringLiteral("启用定时备份"), parent);
    layout->addWidget(scheduleEnableCheck);

    scheduleGroup = new QGroupBox(QStringLiteral("定时备份设置"), parent);
    QGridLayout *groupLayout = new QGridLayout(scheduleGroup);

    scheduleFrequencyCombo = new QComboBox(scheduleGroup);
    scheduleFrequencyCombo->addItems({QStringLiteral("每天"), QStringLiteral("每周"), QStringLiteral("每月")});
    groupLayout->addWidget(new QLabel(QStringLiteral("频率:"), scheduleGroup), 0, 0);
    groupLayout->addWidget(scheduleFrequencyCombo, 0, 1);

    scheduleTimeEdit = new QTimeEdit(scheduleGroup);
    scheduleTimeEdit->setDisplayFormat("HH:mm");
    scheduleTimeEdit->setTime(QTime::currentTime());
    groupLayout->addWidget(new QLabel(QStringLiteral("时间:"), scheduleGroup), 1, 0);
    groupLayout->addWidget(scheduleTimeEdit, 1, 1);

    scheduleRetentionSpin = new QSpinBox(scheduleGroup);
    scheduleRetentionSpin->setMinimum(1);
    scheduleRetentionSpin->setMaximum(50);
    scheduleRetentionSpin->setValue(5);
    groupLayout->addWidget(new QLabel(QStringLiteral("保留备份数量:"), scheduleGroup), 2, 0);
    groupLayout->addWidget(scheduleRetentionSpin, 2, 1);

    QPushButton *saveButton = new QPushButton(QStringLiteral("保存定时设置"), scheduleGroup);
    groupLayout->addWidget(saveButton, 3, 0, 1, 2);

    nextRunLabel = new QLabel(QStringLiteral("下一次执行时间: 未计划"), scheduleGroup);
    groupLayout->addWidget(nextRunLabel, 4, 0, 1, 2);

    layout->addWidget(scheduleGroup);
    layout->addStretch();

    connect(scheduleEnableCheck, &QCheckBox::toggled, this, &MainWindow::toggleScheduling);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveScheduleSettings);

    scheduleGroup->setEnabled(false);
}

void MainWindow::handleBackupClicked()
{
    QString error;
    auto options = collectBackupOptions(error);
    if (!options)
    {
        QMessageBox::warning(this, QStringLiteral("无法开始备份"), error);
        return;
    }
    runBackup(*options);
}

void MainWindow::runBackup(const BackupOptions &options, bool scheduled)
{
    if (backupWatcher.isRunning())
    {
        if (!scheduled)
        {
            QMessageBox::information(this, QStringLiteral("备份进行中"), QStringLiteral("当前已有备份任务在执行"));
        }
        return;
    }

    backupTriggeredBySchedule = scheduled;
    setBackupUiEnabled(false);
    backupProgress->setValue(0);
    appendBackupLog(scheduled ? QStringLiteral("开始执行定时备份任务...") : QStringLiteral("开始备份..."));

    QPointer<MainWindow> guard(this);
    BackupEngineCallbacks callbacks;
    callbacks.log = [guard](const QString &message) {
        if (!guard)
        {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, message]() {
            if (guard)
            {
                guard->appendBackupLog(message);
            }
        });
    };
    callbacks.progress = [guard](int current, int total) {
        if (!guard)
        {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, current, total]() {
            if (guard)
            {
                guard->updateBackupProgress(current, total);
            }
        });
    };

    auto future = QtConcurrent::run([options, callbacks]() {
        BackupEngine engine;
        return engine.performBackup(options, callbacks);
    });
    backupWatcher.setFuture(future);
}

void MainWindow::onBackupFinished()
{
    setBackupUiEnabled(true);
    const bool scheduledRun = backupTriggeredBySchedule;
    backupTriggeredBySchedule = false;
    const BackupResult result = backupWatcher.result();
    if (result.success)
    {
        appendBackupLog(QStringLiteral("备份完成: %1").arg(result.location));
    }
    else
    {
        appendBackupLog(QStringLiteral("备份失败: %1").arg(result.error));
        if (!scheduledRun)
        {
            QMessageBox::warning(this, QStringLiteral("备份失败"), result.error);
        }
    }
    backupProgress->setValue(result.success ? 100 : 0);
}

void MainWindow::handleRestoreClicked()
{
    if (restoreWatcher.isRunning() || verifyWatcher.isRunning())
    {
        QMessageBox::information(this, QStringLiteral("恢复进行中"), QStringLiteral("已有恢复任务在执行"));
        return;
    }
    QString error;
    auto options = collectRestoreOptions(error);
    if (!options)
    {
        QMessageBox::warning(this, QStringLiteral("无法开始恢复"), error);
        return;
    }

    setRestoreUiEnabled(false);
    restoreProgress->setValue(0);
    appendRestoreLog(QStringLiteral("开始恢复..."));

    QPointer<MainWindow> guard(this);
    BackupEngineCallbacks callbacks;
    callbacks.log = [guard](const QString &message) {
        if (!guard)
        {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, message]() {
            if (guard)
            {
                guard->appendRestoreLog(message);
            }
        });
    };
    callbacks.progress = [guard](int current, int total) {
        if (!guard)
        {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, current, total]() {
            if (guard)
            {
                guard->updateRestoreProgress(current, total);
            }
        });
    };

    auto future = QtConcurrent::run([options = *options, callbacks]() {
        BackupEngine engine;
        return engine.restore(options, callbacks);
    });
    restoreWatcher.setFuture(future);
}

void MainWindow::onRestoreFinished()
{
    setRestoreUiEnabled(true);
    const RestoreResult result = restoreWatcher.result();
    if (result.success)
    {
        appendRestoreLog(QStringLiteral("恢复完成: %1").arg(result.targetPath));
        QMessageBox::information(this, QStringLiteral("恢复完成"), QStringLiteral("数据已恢复"));
    }
    else
    {
        appendRestoreLog(QStringLiteral("恢复失败: %1").arg(result.error));
        QMessageBox::warning(this, QStringLiteral("恢复失败"), result.error);
    }
    restoreProgress->setValue(result.success ? 100 : 0);
}

void MainWindow::handleVerifyClicked()
{
    if (verifyWatcher.isRunning() || restoreWatcher.isRunning())
    {
        QMessageBox::information(this, QStringLiteral("验证进行中"), QStringLiteral("已有验证任务在执行"));
        return;
    }
    QString error;
    auto options = collectVerifyOptions(error);
    if (!options)
    {
        QMessageBox::warning(this, QStringLiteral("无法验证"), error);
        return;
    }

    setRestoreUiEnabled(false);
    restoreProgress->setValue(0);
    appendRestoreLog(QStringLiteral("开始验证备份..."));

    QPointer<MainWindow> guard(this);
    BackupEngineCallbacks callbacks;
    callbacks.log = [guard](const QString &message) {
        if (!guard)
        {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, message]() {
            if (guard)
            {
                guard->appendRestoreLog(message);
            }
        });
    };
    callbacks.progress = [guard](int current, int total) {
        if (!guard)
        {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, current, total]() {
            if (guard)
            {
                guard->updateRestoreProgress(current, total);
            }
        });
    };

    auto future = QtConcurrent::run([options = *options, callbacks]() {
        BackupEngine engine;
        return engine.verify(options, callbacks);
    });
    verifyWatcher.setFuture(future);
}

void MainWindow::onVerifyFinished()
{
    setRestoreUiEnabled(true);
    const VerifyResult result = verifyWatcher.result();
    if (result.success)
    {
        appendRestoreLog(QStringLiteral("验证完成，数据一致"));
        QMessageBox::information(this, QStringLiteral("验证完成"), QStringLiteral("备份数据校验通过"));
    }
    else
    {
        appendRestoreLog(QStringLiteral("验证失败: %1").arg(result.error));
        QMessageBox::warning(this, QStringLiteral("验证失败"),
                             result.error.isEmpty() ? QStringLiteral("发现数据不一致") : result.error);
    }
    restoreProgress->setValue(result.success ? 100 : 0);
}

void MainWindow::chooseBackupSource()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择源目录"));
    if (!directory.isEmpty())
    {
        backupSourceEdit->setText(directory);
    }
}

void MainWindow::chooseBackupDestination()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择备份保存目录"));
    if (!directory.isEmpty())
    {
        backupDestinationEdit->setText(directory);
    }
}

void MainWindow::chooseRestoreBackupFile()
{
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("选择备份文件"), QString(), QStringLiteral("备份文件 (*.fbk);;所有文件 (*)"));
    if (!file.isEmpty())
    {
        restoreBackupEdit->setText(file);
    }
}

void MainWindow::chooseRestoreBackupDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择备份目录"));
    if (!directory.isEmpty())
    {
        restoreBackupEdit->setText(directory);
    }
}

void MainWindow::chooseRestoreDestination()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择恢复目标目录"));
    if (!directory.isEmpty())
    {
        restoreDestinationEdit->setText(directory);
    }
}

void MainWindow::appendBackupLog(const QString &message)
{
    appendLog(backupLog, message);
}

void MainWindow::appendRestoreLog(const QString &message)
{
    appendLog(restoreLog, message);
}

void MainWindow::appendLog(QTextEdit *target, const QString &message)
{
    if (!target)
    {
        return;
    }
    const QString prefix = QTime::currentTime().toString("HH:mm:ss");
    target->append(QStringLiteral("[%1] %2").arg(prefix, message));
}

void MainWindow::updateBackupProgress(int current, int total)
{
    if (!backupProgress)
    {
        return;
    }
    const int percent = total <= 0 ? 0 : qBound(0, static_cast<int>((current * 100.0) / total), 100);
    backupProgress->setValue(percent);
}

void MainWindow::updateRestoreProgress(int current, int total)
{
    if (!restoreProgress)
    {
        return;
    }
    const int percent = total <= 0 ? 0 : qBound(0, static_cast<int>((current * 100.0) / total), 100);
    restoreProgress->setValue(percent);
}

std::optional<BackupOptions> MainWindow::collectBackupOptions(QString &error) const
{
    BackupOptions options;
    options.sourcePath = backupSourceEdit->text().trimmed();
    options.destinationPath = backupDestinationEdit->text().trimmed();
    options.backupName = backupNameEdit->text().trimmed();
    options.compress = compressCheck->isChecked();
    options.encrypt = encryptCheck->isChecked();
    options.package = packageCheck->isChecked();
    options.preserveMetadata = metadataCheck->isChecked();
    options.includeSpecialFiles = specialCheck->isChecked();
    options.verify = verifyCheck->isChecked();
    options.password = passwordEdit->text();

    if (options.sourcePath.isEmpty())
    {
        error = QStringLiteral("请填写需要备份的源路径");
        return std::nullopt;
    }
    if (options.destinationPath.isEmpty())
    {
        error = QStringLiteral("请填写备份目标路径");
        return std::nullopt;
    }
    if (options.encrypt && options.password.isEmpty())
    {
        error = QStringLiteral("启用加密时必须设置密码");
        return std::nullopt;
    }
    return options;
}

std::optional<RestoreOptions> MainWindow::collectRestoreOptions(QString &error) const
{
    RestoreOptions options;
    options.backupPath = restoreBackupEdit->text().trimmed();
    options.restoreDestination = restoreDestinationEdit->text().trimmed();
    options.password = restorePasswordEdit->text();

    if (options.backupPath.isEmpty())
    {
        error = QStringLiteral("请输入备份路径或文件");
        return std::nullopt;
    }
    if (options.restoreDestination.isEmpty())
    {
        error = QStringLiteral("请输入恢复目标路径");
        return std::nullopt;
    }
    return options;
}

std::optional<VerifyOptions> MainWindow::collectVerifyOptions(QString &error) const
{
    VerifyOptions options;
    options.backupPath = restoreBackupEdit->text().trimmed();
    options.password = restorePasswordEdit->text();
    if (options.backupPath.isEmpty())
    {
        error = QStringLiteral("请输入要验证的备份路径");
        return std::nullopt;
    }
    return options;
}

void MainWindow::setBackupUiEnabled(bool enabled)
{
    if (backupButton)
    {
        backupButton->setEnabled(enabled);
    }
}

void MainWindow::setRestoreUiEnabled(bool enabled)
{
    if (restoreButton)
    {
        restoreButton->setEnabled(enabled);
    }
    if (verifyButton)
    {
        verifyButton->setEnabled(enabled);
    }
}

void MainWindow::saveScheduleSettings()
{
    QString error;
    auto options = collectBackupOptions(error);
    if (!options)
    {
        QMessageBox::warning(this, QStringLiteral("参数无效"), QStringLiteral("请先在备份页面填写完整信息: %1").arg(error));
        return;
    }

    options->retentionCount = scheduleRetentionSpin->value();
    scheduledOptions = *options;
    hasScheduledOptions = true;
    scheduledFrequency = scheduleFrequencyCombo->currentText();
    scheduledTime = scheduleTimeEdit->time();
    scheduleAnchorWeekday = QDate::currentDate().dayOfWeek();
    scheduleAnchorDay = QDate::currentDate().day();
    nextScheduleRun = computeNextRun(QDateTime::currentDateTime());
    updateNextRunLabel();

    if (scheduleEnableCheck->isChecked() && !scheduleTimer->isActive())
    {
        scheduleTimer->start();
    }

    appendBackupLog(QStringLiteral("已保存定时备份设置。%1")
                        .arg(nextScheduleRun.isValid()
                                 ? QStringLiteral("下一次执行时间: %1").arg(nextScheduleRun.toString("yyyy-MM-dd HH:mm"))
                                 : QStringLiteral("等待计划时间计算")));
}

void MainWindow::handleScheduleTimeout()
{
    if (!scheduleEnableCheck->isChecked() || !hasScheduledOptions || scheduledFrequency.isEmpty())
    {
        return;
    }
    if (!nextScheduleRun.isValid())
    {
        nextScheduleRun = computeNextRun(QDateTime::currentDateTime());
        updateNextRunLabel();
        return;
    }
    if (QDateTime::currentDateTime() < nextScheduleRun || backupWatcher.isRunning())
    {
        return;
    }

    appendBackupLog(QStringLiteral("到达计划时间，启动定时备份"));
    runBackup(scheduledOptions, true);
    nextScheduleRun = computeNextRun(QDateTime::currentDateTime().addSecs(60));
    updateNextRunLabel();
}

void MainWindow::toggleScheduling(bool enabled)
{
    if (scheduleGroup)
    {
        scheduleGroup->setEnabled(enabled);
    }
    if (!enabled)
    {
        scheduleTimer->stop();
        nextScheduleRun = {};
        updateNextRunLabel();
    }
    else if (hasScheduledOptions)
    {
        if (!scheduleTimer->isActive())
        {
            scheduleTimer->start();
        }
        if (!nextScheduleRun.isValid())
        {
            nextScheduleRun = computeNextRun(QDateTime::currentDateTime());
        }
        updateNextRunLabel();
    }
}

void MainWindow::updateNextRunLabel()
{
    if (!nextRunLabel)
    {
        return;
    }
    if (scheduleEnableCheck->isChecked() && nextScheduleRun.isValid())
    {
        nextRunLabel->setText(QStringLiteral("下一次执行时间: %1").arg(nextScheduleRun.toString("yyyy-MM-dd HH:mm")));
    }
    else
    {
        nextRunLabel->setText(QStringLiteral("下一次执行时间: 未计划"));
    }
}

QDateTime MainWindow::computeNextRun(const QDateTime &from) const
{
    if (scheduledFrequency.isEmpty())
    {
        return {};
    }

    const QTime runTime = scheduledTime.isValid() ? scheduledTime : QTime::currentTime();
    QDateTime candidate(QDate(from.date().year(), from.date().month(), from.date().day()), runTime);

    if (scheduledFrequency == QStringLiteral("每天"))
    {
        if (candidate <= from)
        {
            candidate = candidate.addDays(1);
        }
        return candidate;
    }

    if (scheduledFrequency == QStringLiteral("每周"))
    {
        int anchor = scheduleAnchorWeekday;
        if (anchor < 1 || anchor > 7)
        {
            anchor = from.date().dayOfWeek();
        }
        QDate targetDate = from.date();
        int diff = anchor - targetDate.dayOfWeek();
        if (diff < 0 || (diff == 0 && candidate <= from))
        {
            diff += 7;
        }
        targetDate = targetDate.addDays(diff);
        return QDateTime(targetDate, runTime);
    }

    // 每月
    int anchorDay = scheduleAnchorDay <= 0 ? from.date().day() : scheduleAnchorDay;
    QDate baseDate(from.date().year(), from.date().month(), std::min(anchorDay, from.date().daysInMonth()));
    QDateTime monthlyCandidate(baseDate, runTime);
    if (monthlyCandidate <= from)
    {
        QDate nextMonth = baseDate.addMonths(1);
        baseDate = QDate(nextMonth.year(), nextMonth.month(), std::min(anchorDay, nextMonth.daysInMonth()));
        monthlyCandidate = QDateTime(baseDate, runTime);
    }
    return monthlyCandidate;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
