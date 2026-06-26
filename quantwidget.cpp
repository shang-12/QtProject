#include "quantwidget.h"
#include <QHBoxLayout>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QApplication>
#include <QFileDialog>
#include <QLabel>

QuantWidget::QuantWidget(QWidget *parent) : QWidget(parent) {
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 1. 模型选择区域
    QHBoxLayout *modelLayout = new QHBoxLayout;
    modelLayout->addWidget(new QLabel("模型文件："));
    m_editModel = new QLineEdit;
    m_btnSelect = new QPushButton("选择模型文件");
    modelLayout->addWidget(m_editModel);
    modelLayout->addWidget(m_btnSelect);
    mainLayout->addLayout(modelLayout);

    // 2. 校准数据集区域
    QHBoxLayout *calibLayout = new QHBoxLayout;
    calibLayout->addWidget(new QLabel("校准数据集："));
    m_editCalibData = new QLineEdit;
    m_btnSelectCalib = new QPushButton("选择文件夹");
    calibLayout->addWidget(m_editCalibData);
    calibLayout->addWidget(m_btnSelectCalib);
    mainLayout->addLayout(calibLayout);

    // 3. 输出路径区域
    QHBoxLayout *outputLayout = new QHBoxLayout;
    outputLayout->addWidget(new QLabel("输出路径："));
    m_editOutput = new QLineEdit;
    m_btnSelectOutput = new QPushButton("选择文件夹");
    outputLayout->addWidget(m_editOutput);
    outputLayout->addWidget(m_btnSelectOutput);
    mainLayout->addLayout(outputLayout);

    // 4. 量化参数区域
    QHBoxLayout *paramLayout = new QHBoxLayout;
    paramLayout->addWidget(new QLabel("批量大小："));
    m_editBatchSize = new QLineEdit("8"); // 默认批量大小8
    m_editBatchSize->setMaximumWidth(80);
    m_checkStatic = new QCheckBox("启用静态量化");
    m_checkStatic->setChecked(true); // 默认开启静态量化
    paramLayout->addWidget(m_editBatchSize);
    paramLayout->addStretch();
    paramLayout->addWidget(m_checkStatic);
    mainLayout->addLayout(paramLayout);

    // 5. 控制按钮和日志区域
    m_btnStart = new QPushButton("INT8量化");
    m_textLog = new QTextEdit;
    m_textLog->setReadOnly(true);
    mainLayout->addWidget(m_btnStart);
    mainLayout->addWidget(m_textLog);

    // 进程初始化
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &QuantWidget::readLog);
    connect(m_process, &QProcess::readyReadStandardError, this, &QuantWidget::readLog);
    connect(m_process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError error){
        m_textLog->append("错误：启动失败，请检查Python环境和scripts文件夹位置！");
    });

    // 信号槽连接
    connect(m_btnSelect, &QPushButton::clicked, this, &QuantWidget::selectModel);
    connect(m_btnSelectCalib, &QPushButton::clicked, this, &QuantWidget::selectCalibData);
    connect(m_btnSelectOutput, &QPushButton::clicked, this, &QuantWidget::selectOutputPath);
    connect(m_btnStart, &QPushButton::clicked, this, &QuantWidget::startQuant);
}

void QuantWidget::selectModel() {
    m_editModel->setText(QFileDialog::getOpenFileName(this, "选择模型", "./", "模型 (*.pth *.pt)"));
}

void QuantWidget::selectCalibData() {
    m_editCalibData->setText(QFileDialog::getExistingDirectory(this, "选择文件夹", "./"));
}

void QuantWidget::selectOutputPath() {
    m_editOutput->setText(QFileDialog::getExistingDirectory(this, "选择文件夹", "./"));
}

void QuantWidget::startQuant() {
    // 1. 校验必填参数
    QString modelPath = m_editModel->text();
    QString calibPath = m_editCalibData->text();
    QString outputPath = m_editOutput->text();
    QString batchSize = m_editBatchSize->text();

    if(modelPath.isEmpty()) {
        QMessageBox::warning(this,"提示","请选择模型文件！");
        return;
    }
    if(calibPath.isEmpty()) {
        QMessageBox::warning(this,"提示","请选择校准数据集文件夹！");
        return;
    }
    if(outputPath.isEmpty()) {
        QMessageBox::warning(this,"提示","请选择量化模型输出路径！");
        return;
    }
    bool ok;
    int batch = batchSize.toInt(&ok);
    if(!ok || batch <= 0) {
        QMessageBox::warning(this,"提示","批量大小必须为正整数！");
        return;
    }

    // 2. 清空日志并输出启动信息
    m_textLog->clear();
    m_textLog->append("正在启动量化...");
    QString scriptPath = QCoreApplication::applicationDirPath() + "/yolov5/quantize.py";
    m_textLog->append("脚本路径：" + scriptPath);
    m_textLog->append("模型路径：" + modelPath);
    m_textLog->append("校准数据集：" + calibPath);
    m_textLog->append("输出路径：" + outputPath);
    m_textLog->append("批量大小：" + batchSize);

    QString staticStatus = m_checkStatic->isChecked() ? "开启" : "关闭";
    m_textLog->append("静态量化：" + staticStatus);

    // 3. 构建Python命令和参数
    QString pythonCmd = "D:/python3.9/python.exe";
#ifdef Q_OS_WIN
    pythonCmd = "D:/python3.9/python.exe";
#else
    pythonCmd = "python3";
#endif

    QStringList args;
    args << scriptPath
         << "--model" << modelPath
         << "--calib_data" << calibPath
         << "--output" << outputPath
         << "--batch_size" << batchSize;

    // 4. 启动Python进程
    m_process->start(pythonCmd, args);
}

void QuantWidget::readLog() {
    QByteArray out = m_process->readAllStandardOutput();
    QByteArray err = m_process->readAllStandardError();
    if(!out.isEmpty()) m_textLog->append(QString::fromLocal8Bit(out));
    if(!err.isEmpty()) m_textLog->append(QString::fromLocal8Bit(err));
}