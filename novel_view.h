#ifndef TXTREADER_NOVEL_VIEW_H
#define TXTREADER_NOVEL_VIEW_H

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QList>
#include <QMenu>
#include <QMouseEvent>
#include <QTextLayout>
#include <memory>

struct paragraph_layout
{
    std::shared_ptr<QTextLayout> text_layout;
    qreal height = 0.0;
    qreal y = 0.0;
};

struct chapter_layout
{
    int chapter_index;
    QList<paragraph_layout> paragraphs;
    qreal height = 0.0;
    qreal y = 0.0;
};

struct text_position
{
    int chapter_layout_index = -1;
    int paragraph_index = -1;
    int char_index = -1;
};

class novel_view : public QAbstractScrollArea
{
    Q_OBJECT

   public:
    explicit novel_view(QWidget* parent = nullptr);

    void set_font_style(QFont font, qreal line_spacing, qreal letter_spacing);
    void clear_content();
    void prepend_chapter_content(int chapter_index, const QString& content);
    void append_chapter_content(int chapter_index, const QString& content);
    void relayout_and_redraw();

    [[nodiscard]] bool is_chapter_displayed(int chapter_index) const;
    [[nodiscard]] int first_displayed_chapter_index() const;
    [[nodiscard]] int last_displayed_chapter_index() const;
    [[nodiscard]] QPair<int, double> current_progress() const;
   signals:
    void need_previous_chapter(int current_first_index);
    void need_next_chapter(int current_last_index);

   public slots:
    void copy_selection();

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

   private:
    void layout_chapter(chapter_layout& chapter, const QString& content);
    void relayout_all_chapters();
    void update_scrollbar();
    [[nodiscard]] text_position map_point_to_text_position(const QPoint& point) const;
    [[nodiscard]] QString selected_text() const;
    void clear_selection();

   private slots:
    void on_scroll_value_changed(int value);

   private:
    QList<chapter_layout> chapter_layouts_;
    qreal total_height_ = 0.0;

    QFont font_;
    qreal line_spacing_ = 1.5;
    qreal letter_spacing_ = 1.5;
    qreal paragraph_spacing_ = 10.0;

    bool is_selecting_ = false;
    text_position selection_start_;
    text_position selection_end_;
};

#endif    // TXTREADER_NOVEL_VIEW_H
