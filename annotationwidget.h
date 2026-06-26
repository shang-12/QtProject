#ifndef ANNOTATIONWIDGET_H
#define ANNOTATIONWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QMouseEvent>
#include <QImage>
#include <QVector>
#include <QMenu>
#include <QUndoStack>
#include <QUndoCommand>
#include <QMap>
#include <QPair>

// 标注框结构体：包含矩形、类别ID
struct AnnotationBox {
    QRect rect;
    int clsId = 0;
    AnnotationBox() = default;
    AnnotationBox(const QRect& r, int c) : rect(r), clsId(c) {}
};

class ImageLabel;

// 自定义撤销/重做命令
class AnnotationCommand : public QUndoCommand
{
public:
    // 操作类型
    enum OpType { CreateBox, DeleteBox, ModifyBox, ChangeClass };

    // 创建框 / 修改坐标 / 修改类别 构造
    AnnotationCommand(ImageLabel* label, OpType type, int index,
                      const QRect& oldRect = QRect(), const QRect& newRect = QRect(),
                      int oldCls = -1, int newCls = -1, QUndoCommand* parent = nullptr);

    // 删除框专用构造：保存被删除的完整框+原索引
    AnnotationCommand(ImageLabel* label, int delIndex, const AnnotationBox& delBox, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    ImageLabel* m_label;
    OpType m_type;
    int m_index;            // 操作目标索引
    QRect m_oldRect;        // 修改前坐标
    QRect m_newRect;        // 修改后坐标
    int m_oldCls;           // 修改前类别
    int m_newCls;           // 修改后类别
    AnnotationBox m_deletedBox; // 删除操作：保存完整框数据
};

class ImageLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ImageLabel(QWidget *parent = nullptr);
    void setImage(const QImage& img);
    void clearBoxes();
    QVector<AnnotationBox> getBoxes() const { return m_boxes; }

    // 标注框编辑相关
    void setBoxClass(int boxIndex, int clsId);
    void deleteSelectedBox();
    void updateSelectedBoxClass(int clsId);
    int getSelectedBoxIndex() const { return m_selectedBoxIdx; }
    void updateClassMap(const QList<QPair<QString, int>>& clsMap);
    // 撤销/重做接口
    void undo() { if(m_undoStack->canUndo()) m_undoStack->undo(); }
    void redo() { if(m_undoStack->canRedo()) m_undoStack->redo(); }
    QUndoStack* undoStack() const { return m_undoStack; }

    // 供命令类调用的内部操作（全部增加边界判断）
    void createBoxInternal(const AnnotationBox& box);
    void deleteBoxInternal(int index);
    void insertBoxInternal(int index, const AnnotationBox& box); // 撤销删除：插入回原位置
    void modifyBoxInternal(int index, const QRect& rect);
    void changeClassInternal(int index, int clsId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    enum ResizeHandle { None, TopLeft, TopRight, BottomLeft, BottomRight, Move };
    ResizeHandle hitTest(const QPoint& pos, const QRect& rect);

    QImage m_image;
    QPoint m_start;
    QRect m_currentBox;
    QVector<AnnotationBox> m_boxes;

    bool m_isDrawing = false;
    int m_selectedBoxIdx = -1;
    ResizeHandle m_resizeHandle = None;
    QPoint m_dragStartPos;
    QRect m_dragStartRect;
    QList<QPair<QString, int>> m_classNameMap;
    // 撤销/重做栈
    QUndoStack* m_undoStack;

    void drawClassText(QPainter& painter, const AnnotationBox& box);
};

class AnnotationWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AnnotationWidget(QWidget *parent = nullptr);
    enum ClassId {
        Brokenend = 0,
        Brokenyarn = 1,
        Brokenpick = 2,
        Weft_curling = 3,
        Fuzzyball = 4,
        Cutselvage = 5,
        Crease = 6,
        Warpball = 7,
        Knots = 8,
        Contamination = 9,
        Nep = 10,
        Weftcrack = 11
    };

private slots:
    void openImage();
    void saveLabel();
    void addNewClass();
    void deleteSelectedClass();
    void onClassChanged(int index);
    void onUndo() { m_imgLabel->undo(); }
    void onRedo() { m_imgLabel->redo(); }
    void updateUndoRedoButtons();

private:
    int getCurrentClassId();
    void updateClassComboBox();
    void syncClassMapToLabel();

    ImageLabel *m_imgLabel;
    QPushButton *m_btnOpen;
    QPushButton *m_btnSave;
    QPushButton *m_btnAddClass;
    QPushButton *m_btnDelClass;
    QPushButton *m_btnUndo;
    QPushButton *m_btnRedo;
    QComboBox *m_cboClass;
    QImage m_currentImg;
    QString m_imgPath;

    QList<QPair<QString, int>> m_classList;
    int m_nextClassId = 12;
};

#endif // ANNOTATIONWIDGET_H