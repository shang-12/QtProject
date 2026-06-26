#ifndef INFERWIDGET_H
#define INFERWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QProcess>
#include <QLineEdit>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QTableWidget>
#include <QHeaderView>
#include <QPainter>
#include <QDoubleSpinBox> // 浮点阈值输入框
#include <QVector>
#include <QDateTime>

class InferWidget : public QWidget
{
    Q_OBJECT
public:
    explicit InferWidget(QWidget *parent = nullptr);
    ~InferWidget();

private slots:
    void selectImage();
    void selectModelFile();
    // 新增：选择yaml数据集
    void selectYamlFile();
    void startInfer();
    void readLog();
    void parseInferResult(const QString& jsonStr);
    void queryHistory();
    void clearHistory();

private:
    void initDatabase();
    void saveInferResultToDB(const QString& imgPath,
                             const QVector<QRect>& boxes,
                             const QVector<QString>& classes,
                             const QVector<double>& confs);
    void paintEvent(QPaintEvent *event) override;

    // 原有控件
    QLineEdit *m_editImage;
    QPushButton *m_btnSelectImg;
    QPushButton *m_btnStart;
    QLabel *m_imgShow;
    QTextEdit *m_textLog;
    QProcess *m_process;
    QImage m_originImage;
    QVector<QRect> m_detectBoxes;
    QVector<QString> m_classNames;
    QVector<double> m_confidences;
    QSqlDatabase m_db;

    // 模型选择
    QLineEdit *m_editModel;
    QPushButton *m_btnSelectModel;

    // 【新增】数据集yaml配置
    QLineEdit *m_editYaml;
    QPushButton *m_btnSelectYaml;

    // 【新增】置信度、IOU阈值输入
    QDoubleSpinBox *m_spinConfThres;
    QDoubleSpinBox *m_spinIouThres;

    // 历史表格
    QTableWidget *m_tableHistory;
    QPushButton *m_btnQueryHistory;
    QPushButton *m_btnClearHistory;
};

#endif // INFERWIDGET_H