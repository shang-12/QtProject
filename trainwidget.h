#ifndef TRAINWIDGET_H
#define TRAINWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QProcess>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QFile>
#include <QtCharts>
#include <QChart>
#include <QLineSeries>
#include <QChartView>
#include <QValueAxis>

class TrainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TrainWidget(QWidget *parent = nullptr);
    ~TrainWidget();

private slots:
    //void selectTrainDataset();
    //void selectValDataset();
    void startTrain();
    void readLog();
    void initDatabase();
    void saveTrainResult();
    //void plotMetrics();

private:
    QString generateYoloDatasetYaml(int nc, const QString& trainPath, const QString& valPath);

    // 数据集控件
    QLineEdit *m_editTrainDataset;
    QLineEdit *m_editValDataset;
    QPushButton *m_btnSelectTrain;
    QPushButton *m_btnSelectVal;

    // YOLOv5超参（移除lr0、weight_decay，新增imgsize）
    QLineEdit *m_editNc;
    QLineEdit *m_editEpochs;
    QLineEdit *m_editBatchSize;
    QLineEdit *m_editImgSize;   // 新增图片尺寸

    // 基础控件
    QPushButton *m_btnStart;
    QTextEdit *m_textLog;
    QTextEdit *m_textMetrics;
    QProcess *m_process;

    // QChart *m_chart;
    // QLineSeries *m_mapSeries;
    // QLineSeries *m_lossSeries;
    // QChartView *m_chartView;

    QMap<QString, double> m_numMetrics;
    QString m_modelPath;
    // QVector<double> m_epochMap;
    // QVector<double> m_epochTotalLoss;
};

#endif // TRAINWIDGET_H