#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QRect>
#include <QSize>
#include <QWidget>

class Utils
{
public:
    static QString selectFolder(QWidget *parent);
    static QString selectImage(QWidget *parent);
    static QString selectModel(QWidget *parent);
    static QString selectYamlFile(QWidget *parent);
    static QRectF normalizeRect(const QRect& rect, const QSize& imgSize); // 修改为QRectF
    static void saveYoloLabel(const QString& savePath, const QVector<QRect>& boxes, int clsId, const QSize& imgSize);
    static QRect convertToLabelRect(const QRectF& normRect, const QSize& imgSize, const QSize& labelSize);
};


#endif // UTILS_H