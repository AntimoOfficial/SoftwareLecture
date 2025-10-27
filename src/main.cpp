#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QFileDialog>
#include <QComboBox>
#include <QTimeEdit>
#include <QSpinBox>

class MainWindow : public QMainWindow
{
public:
    MainWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("文件备份软件");
        setMinimumSize(600, 400);

        QTabWidget *tabWidget = new QTabWidget(this);
        setCentralWidget(tabWidget);

        QWidget *backupTab = new QWidget();
        setupBackupTab(backupTab);
        tabWidget->addTab(backupTab, "备份");

        QWidget *restoreTab = new QWidget();
        setupRestoreTab(restoreTab);
        tabWidget->addTab(restoreTab, "恢复");

        QWidget *settingsTab = new QWidget();
        setupSettingsTab(settingsTab);
        tabWidget->addTab(settingsTab, "设置");
    }

private:
    void setupBackupTab(QWidget *parent)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(parent);

        // Source and Destination
        QGridLayout *pathLayout = new QGridLayout();
        pathLayout->addWidget(new QLabel("源:", parent), 0, 0);
        pathLayout->addWidget(new QLineEdit(parent), 0, 1);
        pathLayout->addWidget(new QPushButton("选择...", parent), 0, 2);
        pathLayout->addWidget(new QLabel("目标:", parent), 1, 0);
        pathLayout->addWidget(new QLineEdit(parent), 1, 1);
        pathLayout->addWidget(new QPushButton("选择...", parent), 1, 2);
        mainLayout->addLayout(pathLayout);

        // Options
        QGroupBox *optionsGroup = new QGroupBox("备份选项", parent);
        QHBoxLayout *optionsLayout = new QHBoxLayout();
        optionsLayout->addWidget(new QCheckBox("压缩备份", parent));
        optionsLayout->addWidget(new QCheckBox("加密备份", parent));
        optionsLayout->addWidget(new QCheckBox("打包为单文件", parent));
        optionsLayout->addWidget(new QCheckBox("保存文件元数据", parent));
        optionsGroup->setLayout(optionsLayout);
        mainLayout->addWidget(optionsGroup);

        // Actions
        mainLayout->addStretch();
        mainLayout->addWidget(new QPushButton("开始备份", parent));

        // Progress and Log
        mainLayout->addWidget(new QProgressBar(parent));
        mainLayout->addWidget(new QTextEdit(parent));
    }

    void setupRestoreTab(QWidget *parent)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(parent);

        // Backup Source and Restore Destination
        QGridLayout *pathLayout = new QGridLayout();
        pathLayout->addWidget(new QLabel("备份文件:", parent), 0, 0);
        pathLayout->addWidget(new QLineEdit(parent), 0, 1);
        pathLayout->addWidget(new QPushButton("选择...", parent), 0, 2);
        pathLayout->addWidget(new QLabel("恢复到:", parent), 1, 0);
        pathLayout->addWidget(new QLineEdit(parent), 1, 1);
        pathLayout->addWidget(new QPushButton("选择...", parent), 1, 2);
        mainLayout->addLayout(pathLayout);

        // Options
        QGroupBox *optionsGroup = new QGroupBox("恢复选项", parent);
        QGridLayout *optionsLayout = new QGridLayout(optionsGroup);
        optionsLayout->addWidget(new QLabel("解密密码:", parent), 0, 0);
        optionsLayout->addWidget(new QLineEdit(parent), 0, 1);
        mainLayout->addWidget(optionsGroup);

        // Actions
        mainLayout->addStretch();
        mainLayout->addWidget(new QPushButton("开始恢复", parent));

        // Progress and Log
        mainLayout->addWidget(new QProgressBar(parent));
        mainLayout->addWidget(new QTextEdit(parent));
    }

    void setupSettingsTab(QWidget *parent)
    {
        QVBoxLayout *mainLayout = new QVBoxLayout(parent);

        QCheckBox *enableScheduling = new QCheckBox("启用定时备份", parent);
        mainLayout->addWidget(enableScheduling);

        QGroupBox *scheduleGroup = new QGroupBox("定时备份设置", parent);
        mainLayout->addWidget(scheduleGroup);

        QGridLayout *scheduleLayout = new QGridLayout(scheduleGroup);
        scheduleLayout->addWidget(new QLabel("频率:", parent), 0, 0);
        QComboBox *frequencyCombo = new QComboBox(parent);
        frequencyCombo->addItems({"每天", "每周", "每月"});
        scheduleLayout->addWidget(frequencyCombo, 0, 1);

        scheduleLayout->addWidget(new QLabel("时间:", parent), 1, 0);
        scheduleLayout->addWidget(new QTimeEdit(parent), 1, 1);

        scheduleLayout->addWidget(new QLabel("保留备份数量:", parent), 2, 0);
        scheduleLayout->addWidget(new QSpinBox(parent), 2, 1);

        mainLayout->addStretch();
        mainLayout->addWidget(new QPushButton("保存设置", parent));

        scheduleGroup->setEnabled(false);
        connect(enableScheduling, &QCheckBox::toggled, scheduleGroup, &QGroupBox::setEnabled);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
