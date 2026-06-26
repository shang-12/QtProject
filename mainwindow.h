#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include "annotationwidget.h"
#include "trainwidget.h"
#include "quantwidget.h"
#include "inferwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    QTabWidget *m_tabWidget;
    AnnotationWidget *m_annoWidget;
    TrainWidget *m_trainWidget;
    QuantWidget *m_quantWidget;
    InferWidget *m_inferWidget;
};

#endif // MAINWINDOW_H