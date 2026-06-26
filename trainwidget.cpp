#include "trainwidget.h"
#include "utils.h"
#include <QHBoxLayout>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QSqlError>
#include <QRegularExpression>
#include <QPainter>
#include <QFileInfo>
#include <QTextStream>

TrainWidget::TrainWidget(QWidget *parent) : QWidget(parent) {
    initDatabase();
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 1. 数据集区域
    QGroupBox *datasetGroup = new QGroupBox("数据集配置");
    QFormLayout *datasetLayout = new QFormLayout(datasetGroup);
    m_editTrainDataset = new QLineEdit;
    m_btnSelectTrain = new QPushButton("选择训练集images文件夹");
    m_editValDataset = new QLineEdit;
    m_btnSelectVal = new QPushButton("选择验证集images文件夹");

    QHBoxLayout *trainLay = new QHBoxLayout;
    trainLay->addWidget(m_editTrainDataset);
    trainLay->addWidget(m_btnSelectTrain);
    QHBoxLayout *valLay = new QHBoxLayout;
    valLay->addWidget(m_editValDataset);
    valLay->addWidget(m_btnSelectVal);

    datasetLayout->addRow("Train图片路径：", trainLay);
    datasetLayout->addRow("Val图片路径：", valLay);
    mainLayout->addWidget(datasetGroup);

    // 2. YOLOv5超参数区域
    QGroupBox *paramGroup = new QGroupBox("YOLOv5训练超参");
    QFormLayout *paramLayout = new QFormLayout(paramGroup);
    m_editNc = new QLineEdit("12");
    m_editEpochs = new QLineEdit("10");
    m_editBatchSize = new QLineEdit("16");
    m_editImgSize = new QLineEdit("320"); // 默认640分辨率

    paramLayout->addRow("类别数量(nc)：", m_editNc);
    paramLayout->addRow("训练轮数epochs：", m_editEpochs);
    paramLayout->addRow("Batch Size：", m_editBatchSize);
    paramLayout->addRow("输入图片尺寸imgsz：", m_editImgSize);
    mainLayout->addWidget(paramGroup);

    // 启动按钮
    m_btnStart = new QPushButton("开始YOLOv5训练");
    mainLayout->addWidget(m_btnStart);

    // 日志区
    QGroupBox *logGroup = new QGroupBox("实时训练日志");
    QVBoxLayout *logLay = new QVBoxLayout(logGroup);
    m_textLog = new QTextEdit;
    m_textLog->setReadOnly(true);
    logLay->addWidget(m_textLog);
    mainLayout->addWidget(logGroup);

    // 指标文本区
    QGroupBox *metricGroup = new QGroupBox("训练最终性能指标");
    QVBoxLayout *metricLay = new QVBoxLayout(metricGroup);
    m_textMetrics = new QTextEdit;
    m_textMetrics->setReadOnly(true);
    metricLay->addWidget(m_textMetrics);
    mainLayout->addWidget(metricGroup);

    // 可视化图表
    // QGroupBox *chartGroup = new QGroupBox("mAP & Total Loss 曲线");
    // QVBoxLayout *chartLay = new QVBoxLayout(chartGroup);
    // m_chart = new QChart();
    // m_chart->setTitle("YOLOv5 训练曲线");
    // m_mapSeries = new QLineSeries();
    // m_lossSeries = new QLineSeries();
    // m_mapSeries->setName("mAP@0.5");
    // m_lossSeries->setName("Total Loss");

    // m_chart->addSeries(m_mapSeries);
    // m_chart->addSeries(m_lossSeries);
    // m_chart->createDefaultAxes();

    // QValueAxis *xAxis = new QValueAxis;
    // xAxis->setTitleText("Epochs");
    // QValueAxis *yAxis = new QValueAxis;
    // yAxis->setTitleText("Value");
    // m_chart->setAxisX(xAxis, m_mapSeries);
    // m_chart->setAxisY(yAxis, m_mapSeries);
    // m_chart->setAxisX(xAxis, m_lossSeries);
    // m_chart->setAxisY(yAxis, m_lossSeries);

    // m_chartView = new QChartView(m_chart);
    // m_chartView->setRenderHint(QPainter::Antialiasing);
    // chartLay->addWidget(m_chartView);
    // mainLayout->addWidget(chartGroup);

    // 信号槽
    connect(m_btnSelectTrain, &QPushButton::clicked, this, [=](){
        m_editTrainDataset->setText(Utils::selectFolder(this));
    });
    connect(m_btnSelectVal, &QPushButton::clicked, this, [=](){
        m_editValDataset->setText(Utils::selectFolder(this));
    });
    connect(m_btnStart, &QPushButton::clicked, this, &TrainWidget::startTrain);

    // 进程初始化
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TrainWidget::readLog);
    connect(m_process, &QProcess::readyReadStandardError, this, &TrainWidget::readLog);
    connect(m_process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError){
        m_textLog->append("进程启动失败：" + m_process->errorString());
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int code, QProcess::ExitStatus){
                m_textLog->append(QString("训练进程结束，退出码：%1").arg(code));
                //plotMetrics();
                saveTrainResult();
            });
}

TrainWidget::~TrainWidget() {
    QSqlDatabase::removeDatabase("yolo_train_db");
    if(m_process->state() == QProcess::Running)
        m_process->kill();
}

// 自动生成YOLO数据集临时yaml文件
QString TrainWidget::generateYoloDatasetYaml(int nc, const QString& trainPath, const QString& valPath)
{
    QString yamlPath = QCoreApplication::applicationDirPath() + "/temp_dataset.yaml";
    QString classes[12] = {"Brokenend", "Brokenyarn", "Brokenpick", "Weft_curling", "Fuzzyball", "Cutselvage", "Crease", "Warpball", "Knots", "Contamination", "Nep", "Weftcrack" };
    QFile file(yamlPath);
    if(!file.open(QFile::WriteOnly | QFile::Text)){
        m_textLog->append(" 生成数据集yaml失败，无法写入文件");
        return "";
    }
    QTextStream out(&file);
    // yolov5 yaml标准格式
    out << "train: " << QDir::toNativeSeparators(trainPath) << "\n";
    out << "val: " << QDir::toNativeSeparators(valPath) << "\n";
    // 类别名称默认填充占位，可自行扩展界面输入names
    out << "names: \n";

    for(int i=0;i<nc;i++) out << " " << i << ": " << classes[i] << "\n";

    file.close();
    m_textLog->append("自动生成数据集yaml：" + yamlPath);
    return yamlPath;
}

// 初始化数据库（YOLOv5指标表）
void TrainWidget::initDatabase()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "yolo_train_db");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/yolo_train_history.db");
    if(!db.open()){
        m_textLog->append("数据库打开失败：" + db.lastError().text());
        return;
    }
    QSqlQuery q(db);
    QString createSql = R"(
        CREATE TABLE IF NOT EXISTS train_records (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            train_path TEXT,
            val_path TEXT,
            nc INTEGER,
            epochs INTEGER,
            batch_size INTEGER,
            imgsize INTEGER,
            mAP05 REAL,
            mAP0595 REAL,
            box_loss REAL,
            obj_loss REAL,
            cls_loss REAL,
            best_model_path TEXT,
            train_time DATETIME
        )
    )";
    if(!q.exec(createSql))
        m_textLog->append("创建数据表失败：" + q.lastError().text());
}

// 启动YOLOv5 train.py
void TrainWidget::startTrain()
{
    // 1. 校验路径
    QString trainDir = m_editTrainDataset->text().trimmed();
    QString valDir = m_editValDataset->text().trimmed();
    if(trainDir.isEmpty() || valDir.isEmpty()){
        QMessageBox::warning(this,"提示","请填写训练集、验证集图片文件夹！");
        return;
    }
    // 2. 校验超参
    bool okNc, okEpo, okBatch, okImg;
    int nc = m_editNc->text().toInt(&okNc);
    int epochs = m_editEpochs->text().toInt(&okEpo);
    int batch = m_editBatchSize->text().toInt(&okBatch);
    int imgsize = m_editImgSize->text().toInt(&okImg);
    if(!okNc || !okEpo || !okBatch || !okImg || nc <=0 || epochs <=0 || batch <=0 || imgsize <= 0){
        QMessageBox::warning(this,"提示","超参数输入非法！数字必须大于0");
        return;
    }
    // 3. 清空历史数据
    m_textLog->clear();
    m_textMetrics->clear();
    // m_epochMap.clear();
    // m_epochTotalLoss.clear();
    // m_mapSeries->clear();
    // m_lossSeries->clear();
    m_numMetrics.clear();
    m_modelPath.clear();

    // 4. 生成临时yaml
    QString yamlPath = generateYoloDatasetYaml(nc, trainDir, valDir);
    if(yamlPath.isEmpty()) return;

    // 5. yolov5 train.py 路径
    QString script = QCoreApplication::applicationDirPath() + "/yolov5/train.py";
    QFileInfo scriptInfo(script);
    if(!scriptInfo.exists()){
        m_textLog->append("找不到YOLOv5训练脚本：" + script);
        return;
    }
    m_textLog->append("YOLOv5脚本路径：" + script);

    // 6. Python解释器兼容
    QString pythonCmd = "python3";
#ifdef Q_OS_WIN
    QProcess checkPy;
    checkPy.start("where python", QStringList());
    checkPy.waitForFinished(1000);
    QString pyOut = QString::fromLocal8Bit(checkPy.readAllStandardOutput()).trimmed();
    if(!pyOut.isEmpty())
        pythonCmd = pyOut.split("\n").first().trimmed();
    else
        pythonCmd = "D:/python3.9/python.exe";
#endif

    // 7. 组装YOLOv5标准启动参数
    QStringList args;
    args << script
         << "--data" << yamlPath
         << "--epochs" << QString::number(epochs)
         << "--batch-size" << QString::number(batch)
         << "--img" << QString::number(imgsize); // 新增图片尺寸

    m_textLog->append("正在启动YOLOv5训练...");
    m_process->start(pythonCmd, args);
    if(!m_process->waitForStarted(1000))
        m_textLog->append("Python进程启动失败：" + m_process->errorString());
}

// 实时读取YOLO控制台输出，解析loss/mAP
void TrainWidget::readLog()
{
    QByteArray outBuf = m_process->readAllStandardOutput();
    QByteArray errBuf = m_process->readAllStandardError();
    if(!outBuf.isEmpty()){
        QString log = QString::fromUtf8(outBuf);
        m_textLog->append(log);

        // ====================== 正则1：捕获每轮Epoch的三类Loss（绘图用） ======================
        // 匹配行：        0/0     0.533G     0.1176   0.006763    0.05804          5        320:
        QRegularExpression epochLossReg(R"(^\s*(\d+)/\d+\s+\d+\.\d+G\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+))");
        QRegularExpressionMatch lossMatch = epochLossReg.match(log);
        if(lossMatch.hasMatch())
        {
            //int epoch = lossMatch.captured(1).toInt();
            double box = lossMatch.captured(2).toDouble();
            double obj = lossMatch.captured(3).toDouble();
            double cls = lossMatch.captured(4).toDouble();
            //double totalLoss = box + obj + cls;

            // 缓存Loss曲线数据
            // m_epochMap.append(0.0); // 暂时占位，等val行更新mAP
            // m_epochTotalLoss.append(totalLoss);

            // 仅保存loss数值，mAP等待验证行更新
            m_numMetrics["box_loss"] = box;
            m_numMetrics["obj_loss"] = obj;
            m_numMetrics["cls_loss"] = cls;
        }

        // ====================== 正则2：捕获验证集all行mAP、P、R（最终精度） ======================
        // 匹配行：                   all         67         68   7.56e-05    0.00455   4.06e-05   4.06e-06
        QRegularExpression mapReg(R"(\s+all\s+\d+\s+\d+\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+))");
        QRegularExpressionMatch mapMatch = mapReg.match(log);
        if(mapMatch.hasMatch())
        {
            double precision = mapMatch.captured(1).toDouble();
            double recall = mapMatch.captured(2).toDouble();
            double map05 = mapMatch.captured(3).toDouble();
            double map0595 = mapMatch.captured(4).toDouble();

            // 存入数值容器
            m_numMetrics["precision"] = precision;
            m_numMetrics["recall"] = recall;
            m_numMetrics["mAP05"] = map05;
            m_numMetrics["mAP0595"] = map0595;

            // 更新最后一轮的mAP用于绘图
            // if(!m_epochMap.isEmpty())
            //     m_epochMap.last() = map05;

            // 填充指标文本框
            QString metricText = QString(R"(
===== YOLOv5 最终训练指标 =====
Precision：%1
Recall：%2
mAP@0.5：%3
mAP@0.5:0.95：%4
Box Loss：%5
Obj Loss：%6
Cls Loss：%7
最优模型路径：%8
            )")
                                     .arg(precision,0,'f',6)
                                     .arg(recall,0,'f',6)
                                     .arg(map05,0,'f',6)
                                     .arg(map0595,0,'f',6)
                                     .arg(m_numMetrics["box_loss"],0,'f',6)
                                     .arg(m_numMetrics["obj_loss"],0,'f',6)
                                     .arg(m_numMetrics["cls_loss"],0,'f',6)
                                     .arg(m_modelPath);
            m_textMetrics->setText(metricText);
        }

        // ====================== 正则3：匹配best.pt模型文件路径 ======================
        QRegularExpression modelReg(R"(Optimizer stripped from (.+best\.pt))");
        QRegularExpressionMatch modelMatch = modelReg.match(log);
        if(modelMatch.hasMatch())
        {
            m_modelPath = QDir::toNativeSeparators(modelMatch.captured(1).trimmed());
        }
    }
    if(!errBuf.isEmpty()){

        QString log = QString::fromUtf8(errBuf);
        m_textLog->append(log);

        // ====================== 正则1：捕获每轮Epoch的三类Loss（绘图用） ======================
        // 匹配行：        0/0     0.533G     0.1176   0.006763    0.05804          5        320:
        QRegularExpression epochLossReg(R"(^\s*(\d+)/\d+\s+\d+\.\d+G\s+(\d+\.\d+)\s+(\d+\.\d+)\s+(\d+\.\d+))");
        QRegularExpressionMatch lossMatch = epochLossReg.match(log);
        if(lossMatch.hasMatch())
        {
            //int epoch = lossMatch.captured(1).toInt();
            double box = lossMatch.captured(2).toDouble();
            double obj = lossMatch.captured(3).toDouble();
            double cls = lossMatch.captured(4).toDouble();
            //double totalLoss = box + obj + cls;

            // 缓存Loss曲线数据
            // m_epochMap.append(0.0); // 暂时占位，等val行更新mAP
            // m_epochTotalLoss.append(totalLoss);

            // 仅保存loss数值，mAP等待验证行更新
            m_numMetrics["box_loss"] = box;
            m_numMetrics["obj_loss"] = obj;
            m_numMetrics["cls_loss"] = cls;
        }

        // ====================== 正则2：捕获验证集all行mAP、P、R（最终精度） ======================
        // 匹配行：                   all         67         68   7.56e-05    0.00455   4.06e-05   4.06e-06
        QRegularExpression mapReg(R"(\s+all\s+\d+\s+\d+\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+))");
        QRegularExpressionMatch mapMatch = mapReg.match(log);
        if(mapMatch.hasMatch())
        {
            double precision = mapMatch.captured(1).toDouble();
            double recall = mapMatch.captured(2).toDouble();
            double map05 = mapMatch.captured(3).toDouble();
            double map0595 = mapMatch.captured(4).toDouble();

            // 存入数值容器
            m_numMetrics["precision"] = precision;
            m_numMetrics["recall"] = recall;
            m_numMetrics["mAP05"] = map05;
            m_numMetrics["mAP0595"] = map0595;

            // 更新最后一轮的mAP用于绘图
            // if(!m_epochMap.isEmpty())
            //     m_epochMap.last() = map05;

            // 填充指标文本框
            QString metricText = QString(R"(
===== YOLOv5 最终训练指标 =====
Precision：%1
Recall：%2
mAP@0.5：%3
mAP@0.5:0.95：%4
Box Loss：%5
Obj Loss：%6
Cls Loss：%7
最优模型路径：%8
            )")
                                     .arg(precision,0,'f',6)
                                     .arg(recall,0,'f',6)
                                     .arg(map05,0,'f',6)
                                     .arg(map0595,0,'f',6)
                                     .arg(m_numMetrics["box_loss"],0,'f',6)
                                     .arg(m_numMetrics["obj_loss"],0,'f',6)
                                     .arg(m_numMetrics["cls_loss"],0,'f',6)
                                     .arg(m_modelPath);
            m_textMetrics->setText(metricText);
        }

        // ====================== 正则3：匹配best.pt模型文件路径 ======================
        QRegularExpression modelReg(R"(Optimizer stripped from (.+best\.pt))");
        QRegularExpressionMatch modelMatch = modelReg.match(log);
        if(modelMatch.hasMatch())
        {
            m_modelPath = QDir::toNativeSeparators(modelMatch.captured(1).trimmed());
        }
    }
}

// 绘制mAP与总Loss曲线
// void TrainWidget::plotMetrics()
// {
//     m_mapSeries->clear();
//     m_lossSeries->clear();
//     for(int i=0; i<m_epochMap.size(); i++){
//         m_mapSeries->append(i+1, m_epochMap[i]);
//         m_lossSeries->append(i+1, m_epochTotalLoss[i]);
//     }
//     m_chart->createDefaultAxes();
//     m_chartView->repaint();
// }

// 将YOLO训练记录存入SQLite数据库
void TrainWidget::saveTrainResult()
{
    if(m_numMetrics.isEmpty() || m_modelPath.isEmpty()){
        m_textLog->append(" 未解析到完整训练指标，跳过入库");
        return;
    }
    QSqlDatabase db = QSqlDatabase::database("yolo_train_db");
    if(!db.isOpen()){
        m_textLog->append(" 数据库未连接，保存失败");
        return;
    }
    QSqlQuery q(db);
    q.prepare(R"(
        INSERT INTO train_records(
            train_path, val_path, nc, epochs, batch_size, imgsize,
            mAP05, mAP0595, box_loss, obj_loss, cls_loss, best_model_path, train_time
        ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)
    )");
    q.addBindValue(m_editTrainDataset->text());
    q.addBindValue(m_editValDataset->text());
    q.addBindValue(m_editNc->text().toInt());
    q.addBindValue(m_editEpochs->text().toInt());
    q.addBindValue(m_editBatchSize->text().toInt());
    q.addBindValue(m_editImgSize->text().toInt()); // 新增imgsize入库
    q.addBindValue(m_numMetrics["mAP05"]);
    q.addBindValue(m_numMetrics["mAP0595"]);
    q.addBindValue(m_numMetrics["box_loss"]);
    q.addBindValue(m_numMetrics["obj_loss"]);
    q.addBindValue(m_numMetrics["cls_loss"]);
    q.addBindValue(m_modelPath);
    q.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    if(q.exec())
        m_textLog->append(QString(" 训练记录入库成功，ID：%1").arg(q.lastInsertId().toInt()));
    else
        m_textLog->append(" 入库失败：" + q.lastError().text());
}