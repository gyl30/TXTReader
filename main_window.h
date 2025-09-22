#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QList>
#include <QPushButton>
#include <QSlider>
#include <QTextBrowser>

class QListWidget;
class QPlainTextEdit;
class QSplitter;
class novel_manager;
class QListWidgetItem;
class QScrollBar;

class main_window : public QMainWindow
{
    Q_OBJECT

   public:
    explicit main_window(QWidget* parent = nullptr);
    ~main_window();

   protected:
    void paintEvent(QPaintEvent* event) override;

   private slots:
    void open_file_dialog();
    void toggle_chapter_list_visibility();
    void on_chapter_list_item_clicked(QListWidgetItem* item);

    void on_chapter_found(const QString& title);
    void on_parsing_finished(size_t total_chapters);

    void on_scroll_value_changed(int value);
    void update_progress_status();

    void perform_auto_scroll();
    void auto_scroll_click();

    void increase_auto_speed();
    void decrease_auto_speed();
    void increase_font_size();
    void decrease_font_size();
    void update_background_gradient();
    void on_color_action();
    void increase_line_spacing();
    void decrease_line_spacing();
    void increase_letter_spacing();
    void decrease_letter_spacing();

   private:
    void setup_ui();
    void setup_connections();

    void update_text_style();
    void load_chapters_around(int center_index);
    void append_next_chapter();
    void rebuild_document_for_prepend();

    void reset_auto_scroll_speed();
    void update_font_size(int new_size);

   private:
    // UI 和 动画相关
    int gradient_offset_ = 0;
    bool is_dynamic_background_ = false;
    QTimer* background_animation_timer_;
    int hue_;
    QTimer* auto_scroll_timer_;
    QListWidget* chapter_list_;
    QTextBrowser* text_display_;
    QSplitter* splitter_;
    QScrollBar* scroll_bar_;
    QToolBar* main_tool_bar_;

    // 动作 Action
    QAction* open_file_action_;
    QAction* color_action_;
    QAction* toggle_list_action_;
    QAction* add_font_action_;
    QAction* del_font_action_;
    QAction* scroll_action_;
    QAction* add_speed_;
    QAction* del_speed_;
    QAction* add_line_spacing_action_;
    QAction* del_line_spacing_action_;
    QAction* add_letter_spacing_action_;
    QAction* del_letter_spacing_action_;
    bool auto_scroll_ = false;
    int speed_ = 30;
    int font_size_ = 24;
    qreal line_spacing_ = 5.0;
    qreal letter_spacing_ = 1.5;
    novel_manager* novel_manager_;
    QList<int> displayed_chapter_indices_;
    bool is_loading_content_ = false;
};

#endif    // MAIN_WINDOW_H
