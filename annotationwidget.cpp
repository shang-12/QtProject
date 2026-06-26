#include "annotationwidget.h"
#include <QPainter>
#include <QFileInfo>
#include <QMessageBox>
#include <QInputDialog>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QFileDialog>
#include <QTextStream>
#include <QShortcut>

// ==================== AnnotationCommand 实现 ====================
// 通用命令构造：创建/修改坐标/修改类别
AnnotationCommand::AnnotationCommand(ImageLabel* label, OpType type, int index,
                                     const QRect& oldRect, const QRect& newRect,
                                     int oldCls, int newCls, QUndoCommand* parent)
    : QUndoCommand(parent), m_label(label), m_type(type), m_index(index),
    m_oldRect(oldRect), m_newRect(newRect), m_oldCls(oldCls), m_newCls(newCls)
{}

// 删除框专用构造：保存删除时的索引+完整框数据
AnnotationCommand::AnnotationCommand(ImageLabel* label, int delIndex, const AnnotationBox& delBox, QUndoCommand* parent)
    : QUndoCommand(parent), m_label(label), m_type(DeleteBox), m_index(delIndex), m_deletedBox(delBox)
{}

void AnnotationCommand::undo() {
    switch (m_type) {
    case CreateBox:
        // 撤销创建：删除末尾新增的框
        m_label->deleteBoxInternal(m_index);
        break;
    case DeleteBox:
        // 撤销删除：将框插回原始删除位置（修复越界核心）
        m_label->insertBoxInternal(m_index, m_deletedBox);
        break;
    case ModifyBox:
        m_label->modifyBoxInternal(m_index, m_oldRect);
        break;
    case ChangeClass:
        m_label->changeClassInternal(m_index, m_oldCls);
        break;
    }
}

void AnnotationCommand::redo() {
    switch (m_type) {
    case CreateBox:
        m_label->createBoxInternal(AnnotationBox(m_newRect, m_newCls));
        break;
    case DeleteBox:
        m_label->deleteBoxInternal(m_index);
        break;
    case ModifyBox:
        m_label->modifyBoxInternal(m_index, m_newRect);
        break;
    case ChangeClass:
        m_label->changeClassInternal(m_index, m_newCls);
        break;
    }
}

// ==================== ImageLabel 实现 ====================
ImageLabel::ImageLabel(QWidget *parent) : QLabel(parent) {
    setStyleSheet("border:1px solid black;");
    setAlignment(Qt::AlignCenter);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    m_undoStack = new QUndoStack(this);
}

void ImageLabel::updateClassMap(const QList<QPair<QString, int>>& clsMap)
{
    m_classNameMap = clsMap;
    update();
}

void ImageLabel::setImage(const QImage &img) {
    m_image = img;
    setPixmap(QPixmap::fromImage(img.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    m_selectedBoxIdx = -1;
    m_undoStack->clear(); // 切换图片清空撤销栈，避免旧命令越界
}

void ImageLabel::clearBoxes() {
    m_boxes.clear();
    m_selectedBoxIdx = -1;
    m_undoStack->clear();
    update();
}

// 内部：追加新框
void ImageLabel::createBoxInternal(const AnnotationBox& box) {
    m_boxes.append(box);
    m_selectedBoxIdx = m_boxes.size() - 1;
    update();
}

// 内部：删除指定索引框（带边界判断）
void ImageLabel::deleteBoxInternal(int index) {
    if (index < 0 || index >= m_boxes.size()) return;
    m_boxes.removeAt(index);
    m_selectedBoxIdx = -1;
    update();
}

// 新增：撤销删除时，把框插回原下标
void ImageLabel::insertBoxInternal(int index, const AnnotationBox& box) {
    if (index < 0 || index > m_boxes.size()) return;
    m_boxes.insert(index, box);
    m_selectedBoxIdx = index;
    update();
}

// 内部：修改框坐标
void ImageLabel::modifyBoxInternal(int index, const QRect& rect) {
    if (index < 0 || index >= m_boxes.size()) return;
    m_boxes[index].rect = rect;
    update();
}

// 内部：修改框类别
void ImageLabel::changeClassInternal(int index, int clsId) {
    if (index < 0 || index >= m_boxes.size()) return;
    m_boxes[index].clsId = clsId;
    update();
}

ImageLabel::ResizeHandle ImageLabel::hitTest(const QPoint& pos, const QRect& rect) {
    const int handleSize = 8;
    QRect tl(rect.topLeft(), QSize(handleSize, handleSize));
    QRect tr(rect.topRight() - QPoint(handleSize, 0), QSize(handleSize, handleSize));
    QRect bl(rect.bottomLeft() - QPoint(0, handleSize), QSize(handleSize, handleSize));
    QRect br(rect.bottomRight() - QPoint(handleSize, handleSize), QSize(handleSize, handleSize));

    if (tl.contains(pos)) return TopLeft;
    if (tr.contains(pos)) return TopRight;
    if (bl.contains(pos)) return BottomLeft;
    if (br.contains(pos)) return BottomRight;
    if (rect.contains(pos)) return Move;
    return None;
}

void ImageLabel::drawClassText(QPainter& painter, const AnnotationBox& box) {
    QString clsName = "unknown";
    for (QPair<QString, int> pair : m_classNameMap) {
        if(pair.second == box.clsId){
            clsName = pair.first;
            break;
        }
    }
    QPoint textPos = box.rect.topLeft() + QPoint(5, 15);
    painter.setPen(Qt::white);
    painter.setBrush(QBrush(QColor(0, 0, 0, 180)));
    painter.drawRect(textPos.x() - 2, textPos.y() - 12, 80, 18);
    painter.drawText(textPos, clsName);
}

void ImageLabel::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        bool hitBox = false;
        for (int i = m_boxes.size() - 1; i >= 0; --i) {
            ResizeHandle handle = hitTest(event->pos(), m_boxes[i].rect);
            if (handle != None) {
                m_selectedBoxIdx = i;
                m_resizeHandle = handle;
                m_dragStartPos = event->pos();
                m_dragStartRect = m_boxes[i].rect;
                hitBox = true;
                break;
            }
        }
        if (!hitBox) {
            m_selectedBoxIdx = -1;
            m_start = event->pos();
            m_isDrawing = true;
            m_currentBox = QRect();
        }
    }
    QLabel::mousePressEvent(event);
}

void ImageLabel::mouseMoveEvent(QMouseEvent *event) {
    if (m_isDrawing) {
        m_currentBox = QRect(m_start, event->pos()).normalized();
        update();
    } else if (m_selectedBoxIdx >= 0 && m_resizeHandle != None) {
        QRect newRect = m_dragStartRect;
        QPoint delta = event->pos() - m_dragStartPos;
        switch (m_resizeHandle) {
        case TopLeft: newRect.setTopLeft(m_dragStartRect.topLeft() + delta); break;
        case TopRight: newRect.setTopRight(m_dragStartRect.topRight() + delta); break;
        case BottomLeft: newRect.setBottomLeft(m_dragStartRect.bottomLeft() + delta); break;
        case BottomRight: newRect.setBottomRight(m_dragStartRect.bottomRight() + delta); break;
        case Move: newRect.translate(delta); break;
        default: break;
        }
        m_boxes[m_selectedBoxIdx].rect = newRect.normalized();
        update();
    }
    QLabel::mouseMoveEvent(event);
}

void ImageLabel::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // 完成绘制：推入创建命令
        if (m_isDrawing) {
            if (!m_currentBox.isEmpty()) {
                m_undoStack->push(new AnnotationCommand(this,
                                                        AnnotationCommand::CreateBox,
                                                        m_boxes.size(),
                                                        QRect(),
                                                        m_currentBox,
                                                        0,
                                                        0));
            }
            m_isDrawing = false;
            update();
        }
        // 完成移动/缩放：推入修改坐标命令
        else if (m_resizeHandle != None && m_selectedBoxIdx >= 0) {
            QRect oldRect = m_dragStartRect;
            QRect newRect = m_boxes[m_selectedBoxIdx].rect;
            if (oldRect != newRect) {
                m_undoStack->push(new AnnotationCommand(this,
                                                        AnnotationCommand::ModifyBox,
                                                        m_selectedBoxIdx,
                                                        oldRect,
                                                        newRect));
            }
            m_resizeHandle = None;
        }
    }
    QLabel::mouseReleaseEvent(event);
}

void ImageLabel::paintEvent(QPaintEvent *event) {
    QLabel::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    for (int i = 0; i < m_boxes.size(); ++i) {
        auto& box = m_boxes[i];
        if (i == m_selectedBoxIdx) {
            painter.setPen(QPen(Qt::blue, 2));
            painter.drawRect(box.rect);
            const int s = 8;
            painter.setBrush(Qt::blue);
            painter.drawRect(QRect(box.rect.topLeft(), QSize(s,s)));
            painter.drawRect(QRect(box.rect.topRight()-QPoint(s,0), QSize(s,s)));
            painter.drawRect(QRect(box.rect.bottomLeft()-QPoint(0,s), QSize(s,s)));
            painter.drawRect(QRect(box.rect.bottomRight()-QPoint(s,s), QSize(s,s)));
        } else {
            painter.setPen(QPen(Qt::red, 2));
            painter.drawRect(box.rect);
        }
        drawClassText(painter, box);
    }

    if (m_isDrawing && !m_currentBox.isEmpty()) {
        painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
        painter.drawRect(m_currentBox);
    }
}

void ImageLabel::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete && m_selectedBoxIdx >= 0) {
        deleteSelectedBox();
    }
    QLabel::keyPressEvent(event);
}

void ImageLabel::contextMenuEvent(QContextMenuEvent *event) {
    if (m_selectedBoxIdx >= 0) {
        QMenu menu(this);
        QAction* del = menu.addAction("删除此标注框");
        connect(del, &QAction::triggered, this, &ImageLabel::deleteSelectedBox);
        menu.exec(event->globalPos());
    }
}

// 修改框类别
void ImageLabel::setBoxClass(int boxIndex, int clsId) {
    if (boxIndex <0 || boxIndex >= m_boxes.size()) return;
    int oldCls = m_boxes[boxIndex].clsId;
    if (oldCls != clsId) {
        m_undoStack->push(new AnnotationCommand(this,
                                                AnnotationCommand::ChangeClass,
                                                boxIndex,
                                                QRect(), QRect(),
                                                oldCls, clsId));
    }
}

// 删除选中框：推送专用删除命令（保存完整框数据）
void ImageLabel::deleteSelectedBox() {
    if (m_selectedBoxIdx <0 || m_selectedBoxIdx >= m_boxes.size()) return;
    AnnotationBox targetBox = m_boxes[m_selectedBoxIdx];
    m_undoStack->push(new AnnotationCommand(this, m_selectedBoxIdx, targetBox));
}

void ImageLabel::updateSelectedBoxClass(int clsId) {
    setBoxClass(m_selectedBoxIdx, clsId);
}

// ==================== AnnotationWidget 实现 ====================
AnnotationWidget::AnnotationWidget(QWidget *parent) : QWidget(parent) {
    // 初始化类别
    m_classList = {
        {"Brokenend", Brokenend},
        {"Brokenyarn", Brokenyarn},
        {"Brokenpick", Brokenpick},
        {"Weft_curling", Weft_curling},
        {"Fuzzyball", Fuzzyball},
        {"Cutselvage", Cutselvage},
        {"Crease", Crease},
        {"Warpball", Warpball},
        {"Knots", Knots},
        {"Contamination", Contamination},
        {"Nep", Nep},
        {"Weftcrack", Weftcrack}
    };

    QVBoxLayout *layout = new QVBoxLayout(this);
    QHBoxLayout *topLayout = new QHBoxLayout;

    // 按钮初始化
    m_btnOpen = new QPushButton("打开图片");
    m_btnSave = new QPushButton("保存标注");
    m_btnAddClass = new QPushButton("新建类别");
    m_btnDelClass = new QPushButton("删除类别");
    m_btnUndo = new QPushButton("撤销(Ctrl+Z)");
    m_btnRedo = new QPushButton("重做(Ctrl+Y)");
    m_cboClass = new QComboBox;
    updateClassComboBox();

    // 布局
    topLayout->addWidget(m_btnOpen);
    topLayout->addWidget(m_cboClass);
    topLayout->addWidget(m_btnAddClass);
    topLayout->addWidget(m_btnDelClass);
    topLayout->addWidget(m_btnUndo);
    topLayout->addWidget(m_btnRedo);
    topLayout->addWidget(m_btnSave);
    topLayout->addStretch();

    m_imgLabel = new ImageLabel();
    m_imgLabel->setMinimumSize(800, 500);
    layout->addLayout(topLayout);
    layout->addWidget(m_imgLabel);
    syncClassMapToLabel();

    // 信号槽
    connect(m_btnOpen, &QPushButton::clicked, this, &AnnotationWidget::openImage);
    connect(m_btnSave, &QPushButton::clicked, this, &AnnotationWidget::saveLabel);
    connect(m_btnAddClass, &QPushButton::clicked, this, &AnnotationWidget::addNewClass);
    connect(m_btnDelClass, &QPushButton::clicked, this, &AnnotationWidget::deleteSelectedClass);
    connect(m_cboClass, &QComboBox::currentIndexChanged, this, &AnnotationWidget::onClassChanged);
    connect(m_btnUndo, &QPushButton::clicked, this, &AnnotationWidget::onUndo);
    connect(m_btnRedo, &QPushButton::clicked, this, &AnnotationWidget::onRedo);

    // 快捷键
    new QShortcut(QKeySequence::Undo, this, SLOT(onUndo()));
    new QShortcut(QKeySequence::Redo, this, SLOT(onRedo()));

    // 绑定撤销栈状态，自动禁用/启用按钮
    connect(m_imgLabel->undoStack(), &QUndoStack::canUndoChanged, this, &AnnotationWidget::updateUndoRedoButtons);
    connect(m_imgLabel->undoStack(), &QUndoStack::canRedoChanged, this, &AnnotationWidget::updateUndoRedoButtons);
    updateUndoRedoButtons();
}

void AnnotationWidget::updateUndoRedoButtons() {
    m_btnUndo->setEnabled(m_imgLabel->undoStack()->canUndo());
    m_btnRedo->setEnabled(m_imgLabel->undoStack()->canRedo());
}

int AnnotationWidget::getCurrentClassId() {
    return m_cboClass->currentData().toInt();
}

void AnnotationWidget::updateClassComboBox() {
    m_cboClass->clear();
    for (auto& p : m_classList) m_cboClass->addItem(p.first, p.second);
}

void AnnotationWidget::addNewClass() {
    bool ok;
    QString name = QInputDialog::getText(this, "新建类别", "名称：", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        for (auto& p : m_classList) if (p.first == name) {
                QMessageBox::warning(this, "", "名称重复");
                return;
            }
        m_classList.append({name, m_nextClassId++});
        updateClassComboBox();
        syncClassMapToLabel();
    }
}

// 同步类别列表到ImageLabel绘制映射
void AnnotationWidget::syncClassMapToLabel()
{
    m_imgLabel->updateClassMap(m_classList);
}

void AnnotationWidget::deleteSelectedClass() {
    int idx = m_cboClass->currentIndex();
    if (idx <0) return;
    int id = m_cboClass->currentData().toInt();
    if (id <6) { QMessageBox::warning(this,"","默认类别不可删"); return; }
    m_classList.removeAt(idx);
    updateClassComboBox();
    syncClassMapToLabel();
}

void AnnotationWidget::onClassChanged(int) {
    int idx = m_imgLabel->getSelectedBoxIndex();
    if (idx >=0) m_imgLabel->updateSelectedBoxClass(getCurrentClassId());
}

void AnnotationWidget::openImage() {
    QString path = QFileDialog::getOpenFileName(this, "打开图片", "", "图片(*.png *.jpg *.jpeg)");
    if (path.isEmpty()) return;
    m_imgPath = path;
    m_currentImg.load(path);
    m_imgLabel->setImage(m_currentImg);
    m_imgLabel->clearBoxes();
}

void AnnotationWidget::saveLabel() {
    if (m_currentImg.isNull() || m_imgLabel->getBoxes().isEmpty()) {
        QMessageBox::warning(this, "", "请先标注！");
        return;
    }
    QString path = QFileInfo(m_imgPath).absolutePath() + "/" + QFileInfo(m_imgPath).baseName() + ".txt";
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) return;
    QTextStream out(&file);
    QSize sz = m_currentImg.size();
    for (auto& box : m_imgLabel->getBoxes()) {
        double x = box.rect.x() * 1.0 / sz.width();
        double y = box.rect.y() * 1.0 / sz.height();
        double w = box.rect.width() * 1.0 / sz.width();
        double h = box.rect.height() * 1.0 / sz.height();
        out << box.clsId << " " << x+w/2 << " " << y+h/2 << " " << w << " " << h << "\n";
    }
    file.close();
    QMessageBox::information(this, "", "保存成功！");
}