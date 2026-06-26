#ifndef QUANTWIDGET_H
#define QUANTWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QProcess>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

class QuantWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QuantWidget(QWidget *parent = nullptr);

private slots:
    void selectModel();
    void startQuant();
    void readLog();
    void selectCalibData();  // 选择校准数据集
    void selectOutputPath(); // 选择输出路径

private:
    QLineEdit *m_editModel;       // 原始模型路径
    QLineEdit *m_editCalibData;   // 校准数据集路径
    QLineEdit *m_editOutput;      // 量化模型输出路径
    QLineEdit *m_editBatchSize;   // 批量大小
    QCheckBox *m_checkStatic;     // 是否静态量化
    QPushButton *m_btnSelect;     // 选择模型按钮
    QPushButton *m_btnSelectCalib;// 选择校准集按钮
    QPushButton *m_btnSelectOutput;// 选择输出路径按钮
    QPushButton *m_btnStart;      // 启动量化按钮
    QTextEdit *m_textLog;         // 日志输出框
    QProcess *m_process;          // 进程对象
};

#endif // QUANTWIDGET_H