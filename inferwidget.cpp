#include "inferwidget.h"
#include "utils.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QJsonParseError>
#include <QSqlError>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDoubleSpinBox>
#include <QPainter>

InferWidget::InferWidget(QWidget *parent) : QWidget(parent) {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 1. 模型选择行
    QHBoxLayout *modelLayout = new QHBoxLayout;
    m_editModel = new QLineEdit;
    m_editModel->setPlaceholderText("选择模型 .pt/.onnx");
    m_btnSelectModel = new QPushButton("选择模型");
    modelLayout->addWidget(new QLabel("模型："));
    modelLayout->addWidget(m_editModel);
    modelLayout->addWidget(m_btnSelectModel);

    // 2. 数据集YAML选择行
    QHBoxLayout *yamlLayout = new QHBoxLayout;
    m_editYaml = new QLineEdit;
    m_editYaml->setPlaceholderText("数据集yaml文件路径");
    m_btnSelectYaml = new QPushButton("选择YAML");
    yamlLayout->addWidget(new QLabel("数据集YAML："));
    yamlLayout->addWidget(m_editYaml);
    yamlLayout->addWidget(m_btnSelectYaml);

    // 3. 置信度、IOU阈值设置行
    QHBoxLayout *thresLayout = new QHBoxLayout;
    m_spinConfThres = new QDoubleSpinBox;
    m_spinConfThres->setRange(0.01, 1.0);
    m_spinConfThres->setSingleStep(0.05);
    m_spinConfThres->setValue(0.25); // 默认0.25
    m_spinIouThres = new QDoubleSpinBox;
    m_spinIouThres->setRange(0.01, 1.0);
    m_spinIouThres->setSingleStep(0.05);
    m_spinIouThres->setValue(0.45); // 默认0.45
    thresLayout->addWidget(new QLabel("置信度阈值："));
    thresLayout->addWidget(m_spinConfThres);
    thresLayout->addSpacing(20);
    thresLayout->addWidget(new QLabel("IOU阈值："));
    thresLayout->addWidget(m_spinIouThres);

    // 4. 图片选择行
    QHBoxLayout *imgLayout = new QHBoxLayout;
    m_editImage = new QLineEdit;
    m_editImage->setPlaceholderText("选择推理图片");
    m_btnSelectImg = new QPushButton("选择图片");
    imgLayout->addWidget(new QLabel("图片："));
    imgLayout->addWidget(m_editImage);
    imgLayout->addWidget(m_btnSelectImg);

    // 5. 功能按钮行
    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_btnStart = new QPushButton("开始推理");
    m_btnQueryHistory = new QPushButton("查询历史");
    m_btnClearHistory = new QPushButton("清空历史");
    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnQueryHistory);
    btnLayout->addWidget(m_btnClearHistory);

    QHBoxLayout *imgLogLayout = new QHBoxLayout;

    // 图片展示
    m_imgShow = new QLabel;
    m_imgShow->setMinimumSize(320, 320);
    m_imgShow->setStyleSheet("border:1px solid black;");
    // 日志框
    m_textLog = new QTextEdit;
    m_textLog->setReadOnly(true);
    m_textLog->setMinimumWidth(300);  // 日志最小宽度
    // 添加拉伸系数：图片占4份，日志占1份
    imgLogLayout->addWidget(m_imgShow, 3);
    imgLogLayout->addWidget(m_textLog, 2);

    // 历史表格
    m_tableHistory = new QTableWidget;
    m_tableHistory->setColumnCount(9);
    m_tableHistory->setHorizontalHeaderLabels({
        "ID", "图片路径", "类别", "置信度", "X", "Y", "w", "h", "推理时间"
    });
    m_tableHistory->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableHistory->setMinimumHeight(200);

    // 组装主布局
    mainLayout->addLayout(modelLayout);
    mainLayout->addLayout(yamlLayout);
    mainLayout->addLayout(thresLayout);
    mainLayout->addLayout(imgLayout);
    mainLayout->addLayout(btnLayout);
    mainLayout->addLayout(imgLogLayout);
    mainLayout->addWidget(m_tableHistory);
    setLayout(mainLayout);


    // 进程初始化
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &InferWidget::readLog);
    connect(m_process, &QProcess::readyReadStandardError, this, &InferWidget::readLog);
    connect(m_process, &QProcess::errorOccurred, this, [=](){
        m_textLog->append("进程启动失败：检查Python、脚本路径！");
    });

    // 信号槽绑定
    connect(m_btnSelectImg, &QPushButton::clicked, this, &InferWidget::selectImage);
    connect(m_btnSelectModel, &QPushButton::clicked, this, &InferWidget::selectModelFile);
    connect(m_btnSelectYaml, &QPushButton::clicked, this, &InferWidget::selectYamlFile);
    connect(m_btnStart, &QPushButton::clicked, this, &InferWidget::startInfer);
    connect(m_btnQueryHistory, &QPushButton::clicked, this, &InferWidget::queryHistory);
    connect(m_btnClearHistory, &QPushButton::clicked, this, &InferWidget::clearHistory);

    initDatabase();
    queryHistory();
}

InferWidget::~InferWidget() {
    if(m_db.isOpen()) m_db.close();
}

void InferWidget::initDatabase() {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("infer_results.db");
    if(!m_db.open()) {
        m_textLog->append("数据库打开失败：" + m_db.lastError().text());
        return;
    }
    QSqlQuery query;
    QString sqlCreate = R"(
        CREATE TABLE IF NOT EXISTS infer_results (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            img_path TEXT NOT NULL,
            class_name TEXT NOT NULL,
            confidence REAL NOT NULL,
            box_x INTEGER NOT NULL,
            box_y INTEGER NOT NULL,
            box_width INTEGER NOT NULL,
            box_height INTEGER NOT NULL,
            infer_time DATETIME NOT NULL
        )
    )";
    if(query.exec(sqlCreate)) m_textLog->append("数据库初始化完成");
    else m_textLog->append("建表失败：" + query.lastError().text());
}

void InferWidget::selectImage() {
    m_editImage->setText(Utils::selectImage(this));
    m_detectBoxes.clear();
    m_classNames.clear();
    m_confidences.clear();
    QString imgPath = m_editImage->text().trimmed();
    m_originImage.load(imgPath);
    if(m_originImage.isNull()) {
        QMessageBox::warning(this, "错误", "图片加载失败！");
        return;
    }
    m_imgShow->setPixmap(QPixmap::fromImage(m_originImage.scaled(m_imgShow->size(), Qt::KeepAspectRatio)));
    update();
}

void InferWidget::selectModelFile() {
    m_editModel->setText(Utils::selectModel(this));
}

// 新增：选择yaml文件
void InferWidget::selectYamlFile() {
    m_editYaml->setText(Utils::selectYamlFile(this));
}

void InferWidget::startInfer() {
    QString modelPath = m_editModel->text().trimmed();
    QString imgPath = m_editImage->text().trimmed();
    QString yamlPath = m_editYaml->text().trimmed();
    double confThres = m_spinConfThres->value();
    double iouThres = m_spinIouThres->value();

    // 参数校验
    if(modelPath.isEmpty() || imgPath.isEmpty() || yamlPath.isEmpty()) {
        QMessageBox::warning(this, "参数缺失", "请选择模型、图片、数据集YAML文件！");
        return;
    }

    m_textLog->clear();
    m_detectBoxes.clear();
    m_classNames.clear();
    m_confidences.clear();

    m_textLog->append("开始执行推理...");

    // 脚本路径
    QString scriptPath = QCoreApplication::applicationDirPath() + "/yolov5/inference.py";
    QString pythonCmd;
#ifdef Q_OS_WIN
    pythonCmd = "D:/python3.9/python.exe";
#else
    pythonCmd = "python3";
#endif

    // 组装参数：传递model、img、data、conf-thres、iou-thres
    QStringList args;
    args << scriptPath
         << "--model" << modelPath
         << "--img" << imgPath
         << "--data" << yamlPath
         << "--conf_thres" << QString::number(confThres)
         << "--iou_thres" << QString::number(iouThres);

    m_process->start(pythonCmd, args);
    m_textLog->append("Python脚本：" + scriptPath);
    m_textLog->append("置信度阈值：" + QString::number(confThres));
    m_textLog->append("IOU阈值：" + QString::number(iouThres));
}

void InferWidget::readLog() {
    QByteArray out = m_process->readAllStandardOutput();
    QByteArray err = m_process->readAllStandardError();
    if(!out.isEmpty()) {
        QString log = QString::fromLocal8Bit(out);
        m_textLog->append(log);
        if(log.contains("{")) parseInferResult(log);
    }
    if(!err.isEmpty()) {
        m_textLog->append(QString::fromLocal8Bit(err));
    }
}

void InferWidget::parseInferResult(const QString& jsonStr) {
    int jsonStart = jsonStr.indexOf("{");
    int jsonEnd = jsonStr.lastIndexOf("}") + 1;
    if(jsonStart == -1 || jsonEnd <= jsonStart) return;

    QString jsonText = jsonStr.mid(jsonStart, jsonEnd - jsonStart);
    QJsonParseError jsonErr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &jsonErr);
    if(jsonErr.error != QJsonParseError::NoError) {
        m_textLog->append("JSON解析失败：" + jsonErr.errorString());
        return;
    }

    QJsonObject root = doc.object();
    QString resultImgPath = root["result_img_path"].toString();
    if(!resultImgPath.isEmpty())
    {
        QImage resImg;
        if(resImg.load(resultImgPath))
        {
            // 替换显示为推理标注图
            m_imgShow->setPixmap(QPixmap::fromImage(resImg.scaled(m_imgShow->size(), Qt::KeepAspectRatio)));
            m_textLog->append("推理结果图已加载：" + resultImgPath);
        }
        else
        {
            m_textLog->append("推理结果图片加载失败：" + resultImgPath);
        }
    }

    QJsonArray boxesArr = root["boxes"].toArray();
    QJsonArray clsArr = root["classes"].toArray();
    QJsonArray confArr = root["confidences"].toArray();

    m_detectBoxes.clear();
    m_classNames.clear();
    m_confidences.clear();

    for(int i = 0; i < boxesArr.size(); i++) {
        QJsonObject boxObj = boxesArr[i].toObject();
        QRectF normRect(boxObj["cx"].toDouble(), boxObj["cy"].toDouble(), boxObj["w"].toDouble(), boxObj["h"].toDouble());
        QRect labelRect = Utils::convertToLabelRect(normRect, m_originImage.size(), m_imgShow->size());
        m_detectBoxes.append(labelRect);
        m_classNames.append(clsArr[i].toString());
        m_confidences.append(confArr[i].toDouble());
    }

    saveInferResultToDB(m_editImage->text(), m_detectBoxes, m_classNames, m_confidences);
    update();
    queryHistory();
}

void InferWidget::saveInferResultToDB(const QString& imgPath, const QVector<QRect>& boxes, const QVector<QString>& classes, const QVector<double>& confs) {
    if(!m_db.isOpen()) {
        m_textLog->append("数据库未打开，无法保存");
        return;
    }
    QSqlQuery query;
    QString insertSql = "INSERT INTO infer_results (img_path, class_name, confidence, box_x, box_y, box_width, box_height, infer_time) VALUES (?,?,?,?,?,?,?,?)";
    query.prepare(insertSql);
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    for(int i = 0; i < boxes.size(); i++) {
        query.addBindValue(imgPath);
        query.addBindValue(classes[i]);
        query.addBindValue(confs[i]);
        query.addBindValue(boxes[i].x());
        query.addBindValue(boxes[i].y());
        query.addBindValue(boxes[i].width());
        query.addBindValue(boxes[i].height());
        query.addBindValue(now);
        query.exec();
    }
    m_textLog->append("推理结果已存入数据库");
}

void InferWidget::queryHistory() {
    m_tableHistory->setRowCount(0);
    QSqlQuery q("SELECT * FROM infer_results ORDER BY id DESC");
    int row = 0;
    while(q.next()) {
        m_tableHistory->insertRow(row);
        m_tableHistory->setItem(row, 0, new QTableWidgetItem(q.value(0).toString()));
        m_tableHistory->setItem(row, 1, new QTableWidgetItem(q.value(1).toString()));
        m_tableHistory->setItem(row, 2, new QTableWidgetItem(q.value(2).toString()));
        m_tableHistory->setItem(row, 3, new QTableWidgetItem(QString::number(q.value(3).toDouble(), 'f', 2)));
        m_tableHistory->setItem(row, 4, new QTableWidgetItem(q.value(4).toString()));
        m_tableHistory->setItem(row, 5, new QTableWidgetItem(q.value(5).toString()));
        m_tableHistory->setItem(row, 6, new QTableWidgetItem(q.value(6).toString()));
        m_tableHistory->setItem(row, 7, new QTableWidgetItem(q.value(7).toString()));
        m_tableHistory->setItem(row, 8, new QTableWidgetItem(q.value(8).toString()));
        row++;
    }
    m_textLog->append(QString("共加载 %1 条历史记录").arg(row));
}

void InferWidget::clearHistory() {
    if(QMessageBox::question(this, "确认", "确定清空全部推理历史记录？") != QMessageBox::Yes)
        return;
    QSqlQuery q;
    q.exec("DELETE FROM infer_results");
    q.exec("VACUUM");
    m_tableHistory->setRowCount(0);
    m_textLog->append("已清空所有历史数据");
}

void InferWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    // if(m_originImage.isNull() || m_detectBoxes.isEmpty()) return;

    // QPainter painter(this);
    // painter.setRenderHint(QPainter::Antialiasing);
    // QRect labelArea = m_imgShow->rect();
    // QSize scaledImg = m_originImage.size().scaled(labelArea.size(), Qt::KeepAspectRatio);
    // int xOff = m_imgShow->x() + (labelArea.width() - scaledImg.width()) / 2;
    // int yOff = m_imgShow->y() + (labelArea.height() - scaledImg.height()) / 2;

    // for(int i = 0; i < m_detectBoxes.size(); i++) {
    //     QRect box = m_detectBoxes[i];
    //     box.translate(xOff, yOff);
    //     painter.setPen(QPen(Qt::red, 2));
    //     painter.drawRect(box);

    //     QString labelTxt = QString("%1 %.2f").arg(m_classNames[i]).arg(m_confidences[i]);
    //     painter.setBrush(QColor(255,0,0,180));
    //     painter.setPen(Qt::white);
    //     painter.drawText(QRect(box.x(), box.y()-20, box.width(), 20), Qt::AlignVCenter, labelTxt);
    // }
}