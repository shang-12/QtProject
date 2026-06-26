#include "utils.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

QString Utils::selectFolder(QWidget *parent) {
    return QFileDialog::getExistingDirectory(parent, "选择文件夹", "./");
}

QString Utils::selectImage(QWidget *parent) {
    return QFileDialog::getOpenFileName(parent, "选择图片", "./", "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.tiff *.webp)");
}

QString Utils::selectModel(QWidget *parent) {
    return QFileDialog::getOpenFileName(
        parent,
        "选择模型",
        "./",
        "模型文件 (*.pt *.pth *.onnx)"
        );
}

QString Utils::selectYamlFile(QWidget *parent) {
    return QFileDialog::getOpenFileName(
        parent,
        "选择数据集配置yaml",
        "./",
        "YAML配置 (*.yaml *.yml)"
        );
}

// 修改为返回QRectF（浮点型，适配YOLO格式）
QRectF Utils::normalizeRect(const QRect& rect, const QSize& imgSize) {
    // 计算图片在Label中的缩放比例
    QSize scaledSize = imgSize.scaled(QSize(800,500), Qt::KeepAspectRatio);
    double scaleX = (double)imgSize.width() / scaledSize.width();
    double scaleY = (double)imgSize.height() / scaledSize.height();

    // 还原到原始图片坐标并归一化
    double x = (rect.x() * scaleX) / imgSize.width();
    double y = (rect.y() * scaleY) / imgSize.height();
    double w = (rect.width() * scaleX) / imgSize.width();
    double h = (rect.height() * scaleY) / imgSize.height();

    // YOLO格式：中心坐标 + 宽高（归一化）
    double cx = x + w/2;
    double cy = y + h/2;
    return QRectF(cx, cy, w, h);
}

void Utils::saveYoloLabel(const QString& savePath, const QVector<QRect>& boxes, int clsId, const QSize& imgSize) {
    QFile file(savePath);
    if(!file.open(QFile::WriteOnly | QFile::Text)) return;

    QTextStream out(&file);
    for(auto& box : boxes) {
        QRectF r = normalizeRect(box, imgSize);
        out << clsId << " "
            << r.x() << " "
            << r.y() << " "
            << r.width() << " "
            << r.height() << "\n";
    }
    file.close();
}

// 新增：将归一化坐标转换为Label显示坐标
QRect Utils::convertToLabelRect(const QRectF& normRect, const QSize& imgSize, const QSize& labelSize) {
    QSize scaledImg = imgSize.scaled(labelSize, Qt::KeepAspectRatio);
    double scaleX = scaledImg.width() / (double)imgSize.width();
    double scaleY = scaledImg.height() / (double)imgSize.height();

    // 还原为原始像素坐标
    double cx = normRect.x() * imgSize.width();
    double cy = normRect.y() * imgSize.height();
    double w = normRect.width() * imgSize.width();
    double h = normRect.height() * imgSize.height();

    // 转换为Label显示坐标
    int x = (cx - w/2) * scaleX;
    int y = (cy - h/2) * scaleY;
    int width = w * scaleX;
    int height = h * scaleY;

    return QRect(x, y, width, height);
}