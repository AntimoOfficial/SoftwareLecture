#include "BackupEngine.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileSystemWatcher>
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
    void showWelcomeDialog();
    void chooseScheduleSource();
    void chooseScheduleDestination();

    // 实时备份相关
    void setupRealtimeTab(QWidget *parent);
    void chooseRealtimeSource();
    void chooseRealtimeDestination();
    void toggleRealtimeBackup(bool enabled);
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void triggerRealtimeBackup();
    void addPathToWatcher(const QString &path);
    void appendRealtimeLog(const QString &message);

    QLineEdit *backupSourceEdit = nullptr;
    QLineEdit *backupDestinationEdit = nullptr;
    QLineEdit *backupNameEdit = nullptr;
    QCheckBox *compressCheck = nullptr;
    QComboBox *compressMethodCombo = nullptr;
    QCheckBox *encryptCheck = nullptr;
    QComboBox *encryptMethodCombo = nullptr;
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
    // 定时备份选项
    QLineEdit *scheduleSourceEdit = nullptr;
    QLineEdit *scheduleDestinationEdit = nullptr;
    QCheckBox *scheduleCompressCheck = nullptr;
    QComboBox *scheduleCompressMethodCombo = nullptr;
    QCheckBox *scheduleEncryptCheck = nullptr;
    QComboBox *scheduleEncryptMethodCombo = nullptr;
    QLineEdit *schedulePasswordEdit = nullptr;
    QCheckBox *schedulePackageCheck = nullptr;
    QCheckBox *scheduleMetadataCheck = nullptr;
    QCheckBox *scheduleSpecialCheck = nullptr;
    QCheckBox *scheduleVerifyCheck = nullptr;

    // 实时备份相关控件
    QLineEdit *realtimeSourceEdit = nullptr;
    QLineEdit *realtimeDestinationEdit = nullptr;
    QCheckBox *realtimeEnableCheck = nullptr;
    QCheckBox *realtimeCompressCheck = nullptr;
    QComboBox *realtimeCompressMethodCombo = nullptr;
    QCheckBox *realtimeEncryptCheck = nullptr;
    QComboBox *realtimeEncryptMethodCombo = nullptr;
    QLineEdit *realtimePasswordEdit = nullptr;
    QCheckBox *realtimePackageCheck = nullptr;
    QCheckBox *realtimeMetadataCheck = nullptr;
    QCheckBox *realtimeSpecialCheck = nullptr;
    QCheckBox *realtimeVerifyCheck = nullptr;
    QSpinBox *realtimeDelaySpin = nullptr;
    QSpinBox *realtimeRetentionSpin = nullptr;
    QLabel *realtimeStatusLabel = nullptr;
    QTextEdit *realtimeLog = nullptr;
    QFileSystemWatcher *fileWatcher = nullptr;
    QTimer *realtimeDebounceTimer = nullptr;
    bool realtimeBackupRunning = false;
    int realtimeBackupCount = 0;

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
    setMinimumSize(900, 650);

    QTabWidget *tabWidget = new QTabWidget(this);
    setCentralWidget(tabWidget);

    QWidget *backupTab = new QWidget();
    setupBackupTab(backupTab);
    tabWidget->addTab(backupTab, QStringLiteral("备份"));

    QWidget *restoreTab = new QWidget();
    setupRestoreTab(restoreTab);
    tabWidget->addTab(restoreTab, QStringLiteral("恢复"));

    QWidget *realtimeTab = new QWidget();
    setupRealtimeTab(realtimeTab);
    tabWidget->addTab(realtimeTab, QStringLiteral("实时备份"));

    QWidget *settingsTab = new QWidget();
    setupSettingsTab(settingsTab);
    tabWidget->addTab(settingsTab, QStringLiteral("定时备份"));

    connect(&backupWatcher, &QFutureWatcher<BackupResult>::finished, this, &MainWindow::onBackupFinished);
    connect(&restoreWatcher, &QFutureWatcher<RestoreResult>::finished, this, &MainWindow::onRestoreFinished);
    connect(&verifyWatcher, &QFutureWatcher<VerifyResult>::finished, this, &MainWindow::onVerifyFinished);

    scheduleTimer = new QTimer(this);
    scheduleTimer->setInterval(60000);
    connect(scheduleTimer, &QTimer::timeout, this, &MainWindow::handleScheduleTimeout);

    // 显示欢迎提示（延迟显示，等窗口显示后）
    QTimer::singleShot(500, this, [this]() {
        showWelcomeDialog();
    });
}

void MainWindow::showWelcomeDialog()
{
    QMessageBox welcome(this);
    welcome.setWindowTitle(QStringLiteral("欢迎使用文件备份软件"));
    welcome.setIcon(QMessageBox::Information);
    welcome.setText(QStringLiteral("<h3>欢迎使用文件备份软件！</h3>"));
    welcome.setInformativeText(QStringLiteral(
        "<p><b>快速开始：</b></p>"
        "<p>1. 在「备份」页面选择要备份的文件夹</p>"
        "<p>2. 选择备份保存的位置</p>"
        "<p>3. 点击「开始备份」按钮</p>"
        "<hr>"
        "<p><b>小贴士：</b></p>"
        "<p>• 鼠标悬停在选项上可查看详细说明</p>"
        "<p>• 新手建议保持默认选项即可</p>"
        "<p>• 如需加密，请务必记住密码！</p>"
    ));
    welcome.setStandardButtons(QMessageBox::Ok);
    welcome.button(QMessageBox::Ok)->setText(QStringLiteral("开始使用"));
    welcome.exec();
}

void MainWindow::setupBackupTab(QWidget *parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(parent);

    // 添加快速入门提示
    QLabel *tipLabel = new QLabel(QStringLiteral(
        "<b>快速入门：</b>1. 选择要备份的文件夹 → 2. 选择保存位置 → 3. 点击「开始备份」"), parent);
    tipLabel->setStyleSheet("QLabel { background-color: #e8f4fd; padding: 8px; border-radius: 4px; color: #1565c0; }");
    tipLabel->setWordWrap(true);
    mainLayout->addWidget(tipLabel);

    QGridLayout *grid = new QGridLayout();
    backupSourceEdit = new QLineEdit(parent);
    backupSourceEdit->setPlaceholderText(QStringLiteral("请选择需要备份的文件夹"));
    backupSourceEdit->setToolTip(QStringLiteral("选择你想要备份的文件或文件夹"));
    QPushButton *sourceButton = new QPushButton(QStringLiteral("选择..."), parent);
    sourceButton->setToolTip(QStringLiteral("点击选择需要备份的文件夹"));
    connect(sourceButton, &QPushButton::clicked, this, &MainWindow::chooseBackupSource);
    grid->addWidget(new QLabel(QStringLiteral("源路径:"), parent), 0, 0);
    grid->addWidget(backupSourceEdit, 0, 1);
    grid->addWidget(sourceButton, 0, 2);

    backupDestinationEdit = new QLineEdit(parent);
    backupDestinationEdit->setPlaceholderText(QStringLiteral("请选择备份文件的保存位置"));
    backupDestinationEdit->setToolTip(QStringLiteral("备份数据将保存到这个位置"));
    QPushButton *destinationButton = new QPushButton(QStringLiteral("选择..."), parent);
    destinationButton->setToolTip(QStringLiteral("点击选择备份保存位置"));
    connect(destinationButton, &QPushButton::clicked, this, &MainWindow::chooseBackupDestination);
    grid->addWidget(new QLabel(QStringLiteral("目标路径:"), parent), 1, 0);
    grid->addWidget(backupDestinationEdit, 1, 1);
    grid->addWidget(destinationButton, 1, 2);

    backupNameEdit = new QLineEdit(parent);
    backupNameEdit->setPlaceholderText(QStringLiteral("可选，留空则使用源文件夹名称"));
    backupNameEdit->setToolTip(QStringLiteral("为备份起一个名字，方便以后识别（可选）"));
    grid->addWidget(new QLabel(QStringLiteral("备份名称:"), parent), 2, 0);
    grid->addWidget(backupNameEdit, 2, 1, 1, 2);

    mainLayout->addLayout(grid);

    QGroupBox *optionsGroup = new QGroupBox(QStringLiteral("备份选项（新手可保持默认）"), parent);
    QGridLayout *optionsLayout = new QGridLayout(optionsGroup);

    // 先创建所有控件
    compressCheck = new QCheckBox(QStringLiteral("启用压缩"), optionsGroup);
    compressCheck->setToolTip(QStringLiteral("压缩可以减小备份文件大小，但会增加备份时间"));
    compressMethodCombo = new QComboBox(optionsGroup);
    compressMethodCombo->addItem(QStringLiteral("Huffman 编码"), static_cast<int>(CompressionMethod::Huffman));
    compressMethodCombo->addItem(QStringLiteral("RLE 游程编码"), static_cast<int>(CompressionMethod::RLE));
    compressMethodCombo->addItem(QStringLiteral("Zlib 压缩"), static_cast<int>(CompressionMethod::Zlib));
    compressMethodCombo->setToolTip(QStringLiteral("选择压缩算法：Huffman适合文本，RLE适合重复数据，Zlib压缩率最高"));
    compressMethodCombo->setEnabled(false);

    encryptCheck = new QCheckBox(QStringLiteral("启用加密"), optionsGroup);
    encryptCheck->setToolTip(QStringLiteral("加密后需要密码才能恢复，请务必记住密码！"));
    encryptMethodCombo = new QComboBox(optionsGroup);
    encryptMethodCombo->addItem(QStringLiteral("XOR 异或加密"), static_cast<int>(EncryptionMethod::XOR));
    encryptMethodCombo->addItem(QStringLiteral("RC4 流加密"), static_cast<int>(EncryptionMethod::RC4));
    encryptMethodCombo->addItem(QStringLiteral("AES-256 加密"), static_cast<int>(EncryptionMethod::AES256));
    encryptMethodCombo->setToolTip(QStringLiteral("选择加密算法：XOR最快但安全性低，RC4较安全，AES-256安全性最高"));
    encryptMethodCombo->setEnabled(false);

    packageCheck = new QCheckBox(QStringLiteral("打包为单文件"), optionsGroup);
    packageCheck->setToolTip(QStringLiteral("将备份打包成一个 .fbk 文件，方便通过U盘或网盘传输"));
    metadataCheck = new QCheckBox(QStringLiteral("保留元数据"), optionsGroup);
    metadataCheck->setToolTip(QStringLiteral("保存文件的权限、所有者、修改时间等信息"));
    metadataCheck->setChecked(true);
    specialCheck = new QCheckBox(QStringLiteral("包含特殊文件"), optionsGroup);
    specialCheck->setToolTip(QStringLiteral("包含符号链接、管道等特殊文件（普通用户通常不需要关心）"));
    specialCheck->setChecked(true);
    verifyCheck = new QCheckBox(QStringLiteral("备份后校验"), optionsGroup);
    verifyCheck->setToolTip(QStringLiteral("备份完成后自动验证数据完整性，推荐保持开启"));
    verifyCheck->setChecked(true);
    passwordEdit = new QLineEdit(optionsGroup);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(QStringLiteral("启用加密时必须设置密码"));
    passwordEdit->setToolTip(QStringLiteral("设置加密密码，请务必记住！忘记密码将无法恢复数据"));
    passwordEdit->setEnabled(false);

    // 连接信号（在所有控件创建后）
    connect(compressCheck, &QCheckBox::toggled, compressMethodCombo, &QComboBox::setEnabled);
    connect(encryptCheck, &QCheckBox::toggled, encryptMethodCombo, &QComboBox::setEnabled);
    connect(encryptCheck, &QCheckBox::toggled, passwordEdit, &QLineEdit::setEnabled);

    // 布局
    optionsLayout->addWidget(compressCheck, 0, 0);
    optionsLayout->addWidget(compressMethodCombo, 0, 1);
    optionsLayout->addWidget(packageCheck, 0, 2);
    optionsLayout->addWidget(encryptCheck, 1, 0);
    optionsLayout->addWidget(encryptMethodCombo, 1, 1);
    optionsLayout->addWidget(verifyCheck, 1, 2);
    optionsLayout->addWidget(metadataCheck, 2, 0);
    optionsLayout->addWidget(specialCheck, 2, 1);
    optionsLayout->addWidget(new QLabel(QStringLiteral("加密密码:"), optionsGroup), 3, 0);
    optionsLayout->addWidget(passwordEdit, 3, 1, 1, 2);

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

    // 添加快速入门提示
    QLabel *tipLabel = new QLabel(QStringLiteral(
        "<b>快速入门：</b>1. 选择备份文件（.fbk）或备份文件夹 → 2. 选择恢复位置 → 3. 点击「开始恢复」"), parent);
    tipLabel->setStyleSheet("QLabel { background-color: #e8f5e9; padding: 8px; border-radius: 4px; color: #2e7d32; }");
    tipLabel->setWordWrap(true);
    mainLayout->addWidget(tipLabel);

    QGridLayout *grid = new QGridLayout();
    restoreBackupEdit = new QLineEdit(parent);
    restoreBackupEdit->setPlaceholderText(QStringLiteral("选择 .fbk 备份文件或备份文件夹"));
    restoreBackupEdit->setToolTip(QStringLiteral("选择之前创建的备份（.fbk 文件或包含 manifest.json 的文件夹）"));

    QPushButton *backupSelectButton = new QPushButton(QStringLiteral("选择..."), parent);
    backupSelectButton->setToolTip(QStringLiteral("点击选择备份文件或备份目录"));
    QMenu *selectMenu = new QMenu(backupSelectButton);
    selectMenu->addAction(QStringLiteral("备份文件 (.fbk)"), this, &MainWindow::chooseRestoreBackupFile);
    selectMenu->addAction(QStringLiteral("备份目录"), this, &MainWindow::chooseRestoreBackupDirectory);
    backupSelectButton->setMenu(selectMenu);

    grid->addWidget(new QLabel(QStringLiteral("备份路径:"), parent), 0, 0);
    grid->addWidget(restoreBackupEdit, 0, 1);
    grid->addWidget(backupSelectButton, 0, 2);

    restoreDestinationEdit = new QLineEdit(parent);
    restoreDestinationEdit->setPlaceholderText(QStringLiteral("选择要恢复到的目标位置"));
    restoreDestinationEdit->setToolTip(QStringLiteral("文件将恢复到这个文件夹中"));
    QPushButton *restoreDestinationButton = new QPushButton(QStringLiteral("选择..."), parent);
    restoreDestinationButton->setToolTip(QStringLiteral("点击选择恢复目标位置"));
    connect(restoreDestinationButton, &QPushButton::clicked, this, &MainWindow::chooseRestoreDestination);
    grid->addWidget(new QLabel(QStringLiteral("恢复到:"), parent), 1, 0);
    grid->addWidget(restoreDestinationEdit, 1, 1);
    grid->addWidget(restoreDestinationButton, 1, 2);

    restorePasswordEdit = new QLineEdit(parent);
    restorePasswordEdit->setEchoMode(QLineEdit::Password);
    restorePasswordEdit->setPlaceholderText(QStringLiteral("如果备份未加密，可留空"));
    restorePasswordEdit->setToolTip(QStringLiteral("输入备份时设置的加密密码（如果备份未加密则留空）"));
    grid->addWidget(new QLabel(QStringLiteral("解密密码:"), parent), 2, 0);
    grid->addWidget(restorePasswordEdit, 2, 1, 1, 2);

    mainLayout->addLayout(grid);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    restoreButton = new QPushButton(QStringLiteral("开始恢复"), parent);
    restoreButton->setToolTip(QStringLiteral("将备份中的文件恢复到指定位置"));
    verifyButton = new QPushButton(QStringLiteral("验证备份"), parent);
    verifyButton->setToolTip(QStringLiteral("检查备份文件是否完整，不会修改任何文件"));
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

    // 添加说明提示
    QLabel *tipLabel = new QLabel(QStringLiteral(
        "<b>定时备份说明：</b>设置好备份路径和选项后，启用定时备份，软件会在设定时间自动执行备份。"), parent);
    tipLabel->setStyleSheet("QLabel { background-color: #fff3e0; padding: 8px; border-radius: 4px; color: #e65100; }");
    tipLabel->setWordWrap(true);
    layout->addWidget(tipLabel);

    // 路径设置
    QGroupBox *pathGroup = new QGroupBox(QStringLiteral("备份路径"), parent);
    QGridLayout *pathLayout = new QGridLayout(pathGroup);

    scheduleSourceEdit = new QLineEdit(pathGroup);
    scheduleSourceEdit->setPlaceholderText(QStringLiteral("选择需要备份的文件夹"));
    QPushButton *sourceButton = new QPushButton(QStringLiteral("选择..."), pathGroup);
    connect(sourceButton, &QPushButton::clicked, this, &MainWindow::chooseScheduleSource);
    pathLayout->addWidget(new QLabel(QStringLiteral("源路径:"), pathGroup), 0, 0);
    pathLayout->addWidget(scheduleSourceEdit, 0, 1);
    pathLayout->addWidget(sourceButton, 0, 2);

    scheduleDestinationEdit = new QLineEdit(pathGroup);
    scheduleDestinationEdit->setPlaceholderText(QStringLiteral("选择备份保存位置"));
    QPushButton *destButton = new QPushButton(QStringLiteral("选择..."), pathGroup);
    connect(destButton, &QPushButton::clicked, this, &MainWindow::chooseScheduleDestination);
    pathLayout->addWidget(new QLabel(QStringLiteral("目标路径:"), pathGroup), 1, 0);
    pathLayout->addWidget(scheduleDestinationEdit, 1, 1);
    pathLayout->addWidget(destButton, 1, 2);

    layout->addWidget(pathGroup);

    // 备份选项
    QGroupBox *optionsGroup = new QGroupBox(QStringLiteral("备份选项"), parent);
    QGridLayout *optionsLayout = new QGridLayout(optionsGroup);

    scheduleCompressCheck = new QCheckBox(QStringLiteral("启用压缩"), optionsGroup);
    scheduleCompressMethodCombo = new QComboBox(optionsGroup);
    scheduleCompressMethodCombo->addItem(QStringLiteral("Huffman 编码"), static_cast<int>(CompressionMethod::Huffman));
    scheduleCompressMethodCombo->addItem(QStringLiteral("RLE 游程编码"), static_cast<int>(CompressionMethod::RLE));
    scheduleCompressMethodCombo->addItem(QStringLiteral("Zlib 压缩"), static_cast<int>(CompressionMethod::Zlib));
    scheduleCompressMethodCombo->setCurrentIndex(2);
    scheduleCompressMethodCombo->setEnabled(false);
    connect(scheduleCompressCheck, &QCheckBox::toggled, scheduleCompressMethodCombo, &QComboBox::setEnabled);

    scheduleEncryptCheck = new QCheckBox(QStringLiteral("启用加密"), optionsGroup);
    scheduleEncryptMethodCombo = new QComboBox(optionsGroup);
    scheduleEncryptMethodCombo->addItem(QStringLiteral("XOR 异或加密"), static_cast<int>(EncryptionMethod::XOR));
    scheduleEncryptMethodCombo->addItem(QStringLiteral("RC4 流加密"), static_cast<int>(EncryptionMethod::RC4));
    scheduleEncryptMethodCombo->addItem(QStringLiteral("AES-256 加密"), static_cast<int>(EncryptionMethod::AES256));
    scheduleEncryptMethodCombo->setCurrentIndex(2);
    scheduleEncryptMethodCombo->setEnabled(false);

    schedulePasswordEdit = new QLineEdit(optionsGroup);
    schedulePasswordEdit->setEchoMode(QLineEdit::Password);
    schedulePasswordEdit->setPlaceholderText(QStringLiteral("启用加密时设置密码"));
    schedulePasswordEdit->setEnabled(false);
    connect(scheduleEncryptCheck, &QCheckBox::toggled, scheduleEncryptMethodCombo, &QComboBox::setEnabled);
    connect(scheduleEncryptCheck, &QCheckBox::toggled, schedulePasswordEdit, &QLineEdit::setEnabled);

    schedulePackageCheck = new QCheckBox(QStringLiteral("打包为单文件"), optionsGroup);
    scheduleMetadataCheck = new QCheckBox(QStringLiteral("保留元数据"), optionsGroup);
    scheduleMetadataCheck->setChecked(true);
    scheduleSpecialCheck = new QCheckBox(QStringLiteral("包含特殊文件"), optionsGroup);
    scheduleSpecialCheck->setChecked(true);
    scheduleVerifyCheck = new QCheckBox(QStringLiteral("备份后校验"), optionsGroup);
    scheduleVerifyCheck->setChecked(true);

    optionsLayout->addWidget(scheduleCompressCheck, 0, 0);
    optionsLayout->addWidget(scheduleCompressMethodCombo, 0, 1);
    optionsLayout->addWidget(schedulePackageCheck, 0, 2);
    optionsLayout->addWidget(scheduleEncryptCheck, 1, 0);
    optionsLayout->addWidget(scheduleEncryptMethodCombo, 1, 1);
    optionsLayout->addWidget(scheduleVerifyCheck, 1, 2);
    optionsLayout->addWidget(scheduleMetadataCheck, 2, 0);
    optionsLayout->addWidget(scheduleSpecialCheck, 2, 1);
    optionsLayout->addWidget(new QLabel(QStringLiteral("加密密码:"), optionsGroup), 3, 0);
    optionsLayout->addWidget(schedulePasswordEdit, 3, 1, 1, 2);

    layout->addWidget(optionsGroup);

    // 定时设置
    scheduleGroup = new QGroupBox(QStringLiteral("定时设置"), parent);
    QGridLayout *groupLayout = new QGridLayout(scheduleGroup);

    scheduleFrequencyCombo = new QComboBox(scheduleGroup);
    scheduleFrequencyCombo->addItems({QStringLiteral("每天"), QStringLiteral("每周"), QStringLiteral("每月")});
    groupLayout->addWidget(new QLabel(QStringLiteral("频率:"), scheduleGroup), 0, 0);
    groupLayout->addWidget(scheduleFrequencyCombo, 0, 1);

    scheduleTimeEdit = new QTimeEdit(scheduleGroup);
    scheduleTimeEdit->setDisplayFormat("HH:mm");
    scheduleTimeEdit->setTime(QTime(2, 0)); // 默认凌晨2点
    groupLayout->addWidget(new QLabel(QStringLiteral("时间:"), scheduleGroup), 1, 0);
    groupLayout->addWidget(scheduleTimeEdit, 1, 1);

    scheduleRetentionSpin = new QSpinBox(scheduleGroup);
    scheduleRetentionSpin->setRange(1, 50);
    scheduleRetentionSpin->setValue(5);
    groupLayout->addWidget(new QLabel(QStringLiteral("保留数量:"), scheduleGroup), 2, 0);
    groupLayout->addWidget(scheduleRetentionSpin, 2, 1);
    groupLayout->addWidget(new QLabel(QStringLiteral("（自动删除旧备份）"), scheduleGroup), 2, 2);

    layout->addWidget(scheduleGroup);

    // 启用开关和状态
    QHBoxLayout *controlLayout = new QHBoxLayout();
    scheduleEnableCheck = new QCheckBox(QStringLiteral("启用定时备份"), parent);
    scheduleEnableCheck->setStyleSheet("QCheckBox { font-size: 14px; font-weight: bold; }");

    QPushButton *saveButton = new QPushButton(QStringLiteral("保存设置"), parent);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveScheduleSettings);

    controlLayout->addWidget(scheduleEnableCheck);
    controlLayout->addStretch();
    controlLayout->addWidget(saveButton);
    layout->addLayout(controlLayout);

    nextRunLabel = new QLabel(QStringLiteral("下一次执行时间: 未计划"), parent);
    nextRunLabel->setStyleSheet("QLabel { color: #1976d2; font-weight: bold; }");
    layout->addWidget(nextRunLabel);

    // 添加注意事项
    QLabel *noteLabel = new QLabel(QStringLiteral(
        "<b>注意：</b>定时备份仅在软件运行时生效。关闭软件后定时任务将暂停。"), parent);
    noteLabel->setStyleSheet("QLabel { background-color: #ffebee; padding: 8px; border-radius: 4px; color: #c62828; }");
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    layout->addStretch();

    connect(scheduleEnableCheck, &QCheckBox::toggled, this, &MainWindow::toggleScheduling);
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

void MainWindow::chooseScheduleSource()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择定时备份源目录"));
    if (!directory.isEmpty())
    {
        scheduleSourceEdit->setText(directory);
    }
}

void MainWindow::chooseScheduleDestination()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("选择定时备份保存目录"));
    if (!directory.isEmpty())
    {
        scheduleDestinationEdit->setText(directory);
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
    options.compressionMethod = static_cast<CompressionMethod>(compressMethodCombo->currentData().toInt());
    options.encryptionMethod = static_cast<EncryptionMethod>(encryptMethodCombo->currentData().toInt());
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
    // 从定时备份页面收集选项
    QString sourcePath = scheduleSourceEdit->text().trimmed();
    QString destPath = scheduleDestinationEdit->text().trimmed();

    if (sourcePath.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("参数无效"), QStringLiteral("请选择需要备份的源路径"));
        return;
    }
    if (destPath.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("参数无效"), QStringLiteral("请选择备份保存位置"));
        return;
    }
    if (scheduleEncryptCheck->isChecked() && schedulePasswordEdit->text().isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("参数无效"), QStringLiteral("启用加密时必须设置密码"));
        return;
    }

    BackupOptions options;
    options.sourcePath = sourcePath;
    options.destinationPath = destPath;
    options.compress = scheduleCompressCheck->isChecked();
    options.compressionMethod = static_cast<CompressionMethod>(scheduleCompressMethodCombo->currentData().toInt());
    options.encrypt = scheduleEncryptCheck->isChecked();
    options.encryptionMethod = static_cast<EncryptionMethod>(scheduleEncryptMethodCombo->currentData().toInt());
    options.password = schedulePasswordEdit->text();
    options.package = schedulePackageCheck->isChecked();
    options.preserveMetadata = scheduleMetadataCheck->isChecked();
    options.includeSpecialFiles = scheduleSpecialCheck->isChecked();
    options.verify = scheduleVerifyCheck->isChecked();
    options.retentionCount = scheduleRetentionSpin->value();

    scheduledOptions = options;
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

// ==================== 实时备份功能实现 ====================

void MainWindow::setupRealtimeTab(QWidget *parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(parent);

    // 添加说明提示
    QLabel *tipLabel = new QLabel(QStringLiteral(
        "<b>实时备份说明：</b>开启后，当监控的文件夹中有文件发生变化时，会自动进行备份。"
        "适合需要实时保护的重要工作文件夹。"), parent);
    tipLabel->setStyleSheet("QLabel { background-color: #e3f2fd; padding: 8px; border-radius: 4px; color: #1565c0; }");
    tipLabel->setWordWrap(true);
    mainLayout->addWidget(tipLabel);

    // 路径设置
    QGridLayout *pathGrid = new QGridLayout();

    realtimeSourceEdit = new QLineEdit(parent);
    realtimeSourceEdit->setPlaceholderText(QStringLiteral("选择需要监控的文件夹"));
    realtimeSourceEdit->setToolTip(QStringLiteral("当这个文件夹中的文件发生变化时，会自动触发备份"));
    QPushButton *sourceButton = new QPushButton(QStringLiteral("选择..."), parent);
    connect(sourceButton, &QPushButton::clicked, this, &MainWindow::chooseRealtimeSource);
    pathGrid->addWidget(new QLabel(QStringLiteral("监控路径:"), parent), 0, 0);
    pathGrid->addWidget(realtimeSourceEdit, 0, 1);
    pathGrid->addWidget(sourceButton, 0, 2);

    realtimeDestinationEdit = new QLineEdit(parent);
    realtimeDestinationEdit->setPlaceholderText(QStringLiteral("选择备份保存位置"));
    realtimeDestinationEdit->setToolTip(QStringLiteral("自动备份将保存到这个位置"));
    QPushButton *destButton = new QPushButton(QStringLiteral("选择..."), parent);
    connect(destButton, &QPushButton::clicked, this, &MainWindow::chooseRealtimeDestination);
    pathGrid->addWidget(new QLabel(QStringLiteral("备份位置:"), parent), 1, 0);
    pathGrid->addWidget(realtimeDestinationEdit, 1, 1);
    pathGrid->addWidget(destButton, 1, 2);

    mainLayout->addLayout(pathGrid);

    // 备份选项
    QGroupBox *optionsGroup = new QGroupBox(QStringLiteral("备份选项"), parent);
    QGridLayout *optionsLayout = new QGridLayout(optionsGroup);

    realtimeCompressCheck = new QCheckBox(QStringLiteral("启用压缩"), optionsGroup);
    realtimeCompressMethodCombo = new QComboBox(optionsGroup);
    realtimeCompressMethodCombo->addItem(QStringLiteral("Huffman 编码"), static_cast<int>(CompressionMethod::Huffman));
    realtimeCompressMethodCombo->addItem(QStringLiteral("RLE 游程编码"), static_cast<int>(CompressionMethod::RLE));
    realtimeCompressMethodCombo->addItem(QStringLiteral("Zlib 压缩"), static_cast<int>(CompressionMethod::Zlib));
    realtimeCompressMethodCombo->setCurrentIndex(2); // 默认 Zlib
    realtimeCompressMethodCombo->setEnabled(false);
    connect(realtimeCompressCheck, &QCheckBox::toggled, realtimeCompressMethodCombo, &QComboBox::setEnabled);

    realtimeEncryptCheck = new QCheckBox(QStringLiteral("启用加密"), optionsGroup);
    realtimeEncryptMethodCombo = new QComboBox(optionsGroup);
    realtimeEncryptMethodCombo->addItem(QStringLiteral("XOR 异或加密"), static_cast<int>(EncryptionMethod::XOR));
    realtimeEncryptMethodCombo->addItem(QStringLiteral("RC4 流加密"), static_cast<int>(EncryptionMethod::RC4));
    realtimeEncryptMethodCombo->addItem(QStringLiteral("AES-256 加密"), static_cast<int>(EncryptionMethod::AES256));
    realtimeEncryptMethodCombo->setCurrentIndex(2); // 默认 AES-256
    realtimeEncryptMethodCombo->setEnabled(false);

    realtimePasswordEdit = new QLineEdit(optionsGroup);
    realtimePasswordEdit->setEchoMode(QLineEdit::Password);
    realtimePasswordEdit->setPlaceholderText(QStringLiteral("启用加密时设置密码"));
    realtimePasswordEdit->setEnabled(false);
    connect(realtimeEncryptCheck, &QCheckBox::toggled, realtimeEncryptMethodCombo, &QComboBox::setEnabled);
    connect(realtimeEncryptCheck, &QCheckBox::toggled, realtimePasswordEdit, &QLineEdit::setEnabled);

    realtimePackageCheck = new QCheckBox(QStringLiteral("打包为单文件"), optionsGroup);
    realtimePackageCheck->setToolTip(QStringLiteral("将备份打包成一个 .fbk 文件"));
    realtimeMetadataCheck = new QCheckBox(QStringLiteral("保留元数据"), optionsGroup);
    realtimeMetadataCheck->setToolTip(QStringLiteral("保存文件的权限、所有者、修改时间等信息"));
    realtimeMetadataCheck->setChecked(true);
    realtimeSpecialCheck = new QCheckBox(QStringLiteral("包含特殊文件"), optionsGroup);
    realtimeSpecialCheck->setToolTip(QStringLiteral("包含符号链接、管道等特殊文件"));
    realtimeSpecialCheck->setChecked(true);
    realtimeVerifyCheck = new QCheckBox(QStringLiteral("备份后校验"), optionsGroup);
    realtimeVerifyCheck->setToolTip(QStringLiteral("备份完成后自动验证数据完整性"));
    realtimeVerifyCheck->setChecked(false); // 实时备份默认不校验以提高速度

    optionsLayout->addWidget(realtimeCompressCheck, 0, 0);
    optionsLayout->addWidget(realtimeCompressMethodCombo, 0, 1);
    optionsLayout->addWidget(realtimePackageCheck, 0, 2);
    optionsLayout->addWidget(realtimeEncryptCheck, 1, 0);
    optionsLayout->addWidget(realtimeEncryptMethodCombo, 1, 1);
    optionsLayout->addWidget(realtimeVerifyCheck, 1, 2);
    optionsLayout->addWidget(realtimeMetadataCheck, 2, 0);
    optionsLayout->addWidget(realtimeSpecialCheck, 2, 1);
    optionsLayout->addWidget(new QLabel(QStringLiteral("加密密码:"), optionsGroup), 3, 0);
    optionsLayout->addWidget(realtimePasswordEdit, 3, 1, 1, 2);

    mainLayout->addWidget(optionsGroup);

    // 高级设置
    QGroupBox *advancedGroup = new QGroupBox(QStringLiteral("高级设置"), parent);
    QGridLayout *advancedLayout = new QGridLayout(advancedGroup);

    realtimeDelaySpin = new QSpinBox(advancedGroup);
    realtimeDelaySpin->setRange(1, 300);
    realtimeDelaySpin->setValue(5);
    realtimeDelaySpin->setSuffix(QStringLiteral(" 秒"));
    realtimeDelaySpin->setToolTip(QStringLiteral("文件变化后等待多少秒再执行备份，避免频繁备份"));
    advancedLayout->addWidget(new QLabel(QStringLiteral("延迟时间:"), advancedGroup), 0, 0);
    advancedLayout->addWidget(realtimeDelaySpin, 0, 1);
    advancedLayout->addWidget(new QLabel(QStringLiteral("（文件变化后等待的时间）"), advancedGroup), 0, 2);

    realtimeRetentionSpin = new QSpinBox(advancedGroup);
    realtimeRetentionSpin->setRange(1, 100);
    realtimeRetentionSpin->setValue(10);
    realtimeRetentionSpin->setToolTip(QStringLiteral("保留最近多少次备份，超出的旧备份会被自动删除"));
    advancedLayout->addWidget(new QLabel(QStringLiteral("保留数量:"), advancedGroup), 1, 0);
    advancedLayout->addWidget(realtimeRetentionSpin, 1, 1);
    advancedLayout->addWidget(new QLabel(QStringLiteral("（保留最近的备份数）"), advancedGroup), 1, 2);

    mainLayout->addWidget(advancedGroup);

    // 启用开关和状态
    QHBoxLayout *controlLayout = new QHBoxLayout();
    realtimeEnableCheck = new QCheckBox(QStringLiteral("启用实时备份"), parent);
    realtimeEnableCheck->setStyleSheet("QCheckBox { font-size: 14px; font-weight: bold; }");
    connect(realtimeEnableCheck, &QCheckBox::toggled, this, &MainWindow::toggleRealtimeBackup);

    realtimeStatusLabel = new QLabel(QStringLiteral("状态: 未启用"), parent);
    realtimeStatusLabel->setStyleSheet("QLabel { color: #757575; }");

    controlLayout->addWidget(realtimeEnableCheck);
    controlLayout->addStretch();
    controlLayout->addWidget(realtimeStatusLabel);
    mainLayout->addLayout(controlLayout);

    // 日志
    realtimeLog = new QTextEdit(parent);
    realtimeLog->setReadOnly(true);
    realtimeLog->setMaximumHeight(150);
    realtimeLog->setPlaceholderText(QStringLiteral("实时备份日志..."));
    mainLayout->addWidget(realtimeLog);

    mainLayout->addStretch();

    // 初始化文件监控器和防抖动计时器
    fileWatcher = new QFileSystemWatcher(this);
    connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);
    connect(fileWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onDirectoryChanged);

    realtimeDebounceTimer = new QTimer(this);
    realtimeDebounceTimer->setSingleShot(true);
    connect(realtimeDebounceTimer, &QTimer::timeout, this, &MainWindow::triggerRealtimeBackup);
}

void MainWindow::chooseRealtimeSource()
{
    QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择监控文件夹"));
    if (!path.isEmpty())
    {
        realtimeSourceEdit->setText(path);
        // 如果已启用，重新添加监控
        if (realtimeEnableCheck->isChecked())
        {
            toggleRealtimeBackup(false);
            toggleRealtimeBackup(true);
        }
    }
}

void MainWindow::chooseRealtimeDestination()
{
    QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择备份保存位置"));
    if (!path.isEmpty())
    {
        realtimeDestinationEdit->setText(path);
    }
}

void MainWindow::toggleRealtimeBackup(bool enabled)
{
    if (!fileWatcher)
    {
        return;
    }

    // 清除现有监控
    QStringList watchedFiles = fileWatcher->files();
    QStringList watchedDirs = fileWatcher->directories();
    if (!watchedFiles.isEmpty())
    {
        fileWatcher->removePaths(watchedFiles);
    }
    if (!watchedDirs.isEmpty())
    {
        fileWatcher->removePaths(watchedDirs);
    }

    if (enabled)
    {
        QString sourcePath = realtimeSourceEdit->text().trimmed();
        QString destPath = realtimeDestinationEdit->text().trimmed();

        if (sourcePath.isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请先选择监控路径"));
            realtimeEnableCheck->setChecked(false);
            return;
        }
        if (destPath.isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("请先选择备份位置"));
            realtimeEnableCheck->setChecked(false);
            return;
        }
        if (realtimeEncryptCheck->isChecked() && realtimePasswordEdit->text().isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("启用加密时必须设置密码"));
            realtimeEnableCheck->setChecked(false);
            return;
        }

        // 添加目录及其子目录到监控
        addPathToWatcher(sourcePath);

        realtimeStatusLabel->setText(QStringLiteral("状态: <span style='color: #4caf50;'>监控中</span>"));
        realtimeStatusLabel->setStyleSheet("");
        appendRealtimeLog(QStringLiteral("开始监控: %1").arg(sourcePath));
        appendRealtimeLog(QStringLiteral("备份将保存到: %1").arg(destPath));
        appendRealtimeLog(QStringLiteral("延迟时间: %1 秒，保留数量: %2").arg(realtimeDelaySpin->value()).arg(realtimeRetentionSpin->value()));
    }
    else
    {
        realtimeStatusLabel->setText(QStringLiteral("状态: 未启用"));
        realtimeStatusLabel->setStyleSheet("QLabel { color: #757575; }");
        if (realtimeDebounceTimer->isActive())
        {
            realtimeDebounceTimer->stop();
        }
        appendRealtimeLog(QStringLiteral("已停止监控"));
    }
}

void MainWindow::addPathToWatcher(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists())
    {
        return;
    }

    if (info.isDir())
    {
        fileWatcher->addPath(path);
        QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            QString subPath = it.next();
            QFileInfo subInfo(subPath);
            if (subInfo.isDir())
            {
                fileWatcher->addPath(subPath);
            }
            else if (subInfo.isFile())
            {
                fileWatcher->addPath(subPath);
            }
        }
    }
    else
    {
        fileWatcher->addPath(path);
    }
}

void MainWindow::onFileChanged(const QString &path)
{
    if (!realtimeEnableCheck->isChecked())
    {
        return;
    }

    appendRealtimeLog(QStringLiteral("检测到文件变化: %1").arg(QFileInfo(path).fileName()));

    // 文件可能被删除后重建，需要重新添加监控
    if (QFileInfo::exists(path))
    {
        fileWatcher->addPath(path);
    }

    // 重置防抖动计时器
    int delayMs = realtimeDelaySpin->value() * 1000;
    realtimeDebounceTimer->start(delayMs);
    realtimeStatusLabel->setText(QStringLiteral("状态: <span style='color: #ff9800;'>等待备份... (%1秒)</span>").arg(realtimeDelaySpin->value()));
}

void MainWindow::onDirectoryChanged(const QString &path)
{
    if (!realtimeEnableCheck->isChecked())
    {
        return;
    }

    appendRealtimeLog(QStringLiteral("检测到目录变化: %1").arg(QFileInfo(path).fileName()));

    // 重新扫描目录，添加新文件/目录到监控
    addPathToWatcher(path);

    // 重置防抖动计时器
    int delayMs = realtimeDelaySpin->value() * 1000;
    realtimeDebounceTimer->start(delayMs);
    realtimeStatusLabel->setText(QStringLiteral("状态: <span style='color: #ff9800;'>等待备份... (%1秒)</span>").arg(realtimeDelaySpin->value()));
}

void MainWindow::triggerRealtimeBackup()
{
    if (realtimeBackupRunning)
    {
        appendRealtimeLog(QStringLiteral("上次备份仍在进行中，跳过本次备份"));
        return;
    }

    QString sourcePath = realtimeSourceEdit->text().trimmed();
    QString destPath = realtimeDestinationEdit->text().trimmed();

    if (sourcePath.isEmpty() || destPath.isEmpty())
    {
        return;
    }

    realtimeBackupRunning = true;
    realtimeStatusLabel->setText(QStringLiteral("状态: <span style='color: #2196f3;'>正在备份...</span>"));

    BackupOptions options;
    options.sourcePath = sourcePath;
    options.destinationPath = destPath;
    options.backupName = QStringLiteral("realtime_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    options.compress = realtimeCompressCheck->isChecked();
    options.compressionMethod = static_cast<CompressionMethod>(realtimeCompressMethodCombo->currentData().toInt());
    options.encrypt = realtimeEncryptCheck->isChecked();
    options.encryptionMethod = static_cast<EncryptionMethod>(realtimeEncryptMethodCombo->currentData().toInt());
    options.password = realtimePasswordEdit->text();
    options.package = realtimePackageCheck->isChecked();
    options.preserveMetadata = realtimeMetadataCheck->isChecked();
    options.includeSpecialFiles = realtimeSpecialCheck->isChecked();
    options.verify = realtimeVerifyCheck->isChecked();
    options.retentionCount = realtimeRetentionSpin->value();

    appendRealtimeLog(QStringLiteral("开始执行备份..."));

    // 在后台线程执行备份
    QPointer<MainWindow> self = this;
    auto future = QtConcurrent::run([options, self]() -> BackupResult {
        BackupEngine engine;
        BackupEngineCallbacks callbacks;
        callbacks.log = [self](const QString &msg) {
            if (self)
            {
                QMetaObject::invokeMethod(self, [self, msg]() {
                    self->appendRealtimeLog(msg);
                }, Qt::QueuedConnection);
            }
        };
        return engine.performBackup(options, callbacks);
    });

    // 使用 QFutureWatcher 监控完成
    auto *watcher = new QFutureWatcher<BackupResult>(this);
    connect(watcher, &QFutureWatcher<BackupResult>::finished, this, [this, watcher]() {
        BackupResult result = watcher->result();
        realtimeBackupRunning = false;

        if (result.success)
        {
            ++realtimeBackupCount;
            appendRealtimeLog(QStringLiteral("备份完成！已完成 %1 次实时备份").arg(realtimeBackupCount));
            realtimeStatusLabel->setText(QStringLiteral("状态: <span style='color: #4caf50;'>监控中 (已备份 %1 次)</span>").arg(realtimeBackupCount));
        }
        else
        {
            appendRealtimeLog(QStringLiteral("备份失败: %1").arg(result.error));
            realtimeStatusLabel->setText(QStringLiteral("状态: <span style='color: #f44336;'>备份失败</span>"));
        }

        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void MainWindow::appendRealtimeLog(const QString &message)
{
    if (!realtimeLog)
    {
        return;
    }
    const QString prefix = QTime::currentTime().toString("HH:mm:ss");
    realtimeLog->append(QStringLiteral("[%1] %2").arg(prefix, message));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
