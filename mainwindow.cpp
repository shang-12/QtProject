#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("织物缺陷检测");
    setMinimumSize(1000, 700);

    // 标签页容器
    m_tabWidget = new QTabWidget(this);
    m_annoWidget = new AnnotationWidget;
    m_trainWidget = new TrainWidget;
    m_quantWidget = new QuantWidget;
    m_inferWidget = new InferWidget;

    // 添加模块
    m_tabWidget->addTab(m_annoWidget, "图像标注");
    m_tabWidget->addTab(m_trainWidget, "模型训练");
    m_tabWidget->addTab(m_quantWidget, "模型量化");
    m_tabWidget->addTab(m_inferWidget, "模型推理");

    setCentralWidget(m_tabWidget);
}

MainWindow::~MainWindow()
{
}