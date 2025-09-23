#ifndef TXTREADER_MAIN_WINDOW_H
#define TXTREADER_MAIN_WINDOW_H

#include <QMainWindow>
#include <QList>

class QListWidget;
class QSplitter;
class novel_manager;
class QListWidgetItem;
class QTimer;
class QAction;
class QToolBar;
class NovelView;

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

    void load_previous_chapter(int currentFirstIndex);
    void load_next_chapter(int currentLastIndex);

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
    void load_chapter(int chapterIndex);
    void reset_auto_scroll_speed();
    void update_font_size(int new_size);

   private:
    int gradient_offset_ = 0;
    bool is_dynamic_background_ = false;
    QTimer* background_animation_timer_;
    int hue_;
    QTimer* auto_scroll_timer_;
    QListWidget* chapter_list_;
    NovelView* novel_view_;
    QSplitter* splitter_;
    QToolBar* main_tool_bar_;

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
    int font_size_ = 38;
    qreal line_spacing_ = 1.5;
    qreal letter_spacing_ = 1.5;

    novel_manager* novel_manager_;
    bool is_loading_content_ = false;
};

#endif
