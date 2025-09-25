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
class novel_view;
class QThread;

class main_window : public QMainWindow
{
    Q_OBJECT

   public:
    explicit main_window(QWidget* parent = nullptr);
    ~main_window() override;

   protected:
    void paintEvent(QPaintEvent* event) override;

   signals:
    void request_load_file(const QString& file_path);
    void request_chapter_content(int chapter_index);

   private slots:
    void open_file_dialog();
    void toggle_chapter_list_visibility();
    void on_chapter_list_item_clicked(QListWidgetItem* item);

    void on_chapter_found(const QString& title);
    void on_parsing_finished(size_t total_chapters);
    void on_chapter_content_ready(int chapter_index, const QString& content);
    void load_previous_chapter();
    void load_next_chapter();

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
    void load_chapter(int chapter_index);
    void reset_auto_scroll_speed();

   private:
    int gradient_offset_ = 0;
    bool is_dynamic_background_ = false;
    QTimer* background_animation_timer_;
    int hue_;
    QTimer* auto_scroll_timer_;
    QListWidget* chapter_list_;
    novel_view* novel_view_;
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
    qreal font_size_ = 38.0;
    qreal line_spacing_ = 1.5;
    qreal letter_spacing_ = 1.5;
    novel_manager* novel_manager_;
    QThread* worker_thread_;
    size_t total_chapters_ = 0;
    bool is_loading_content_ = false;
    int initial_chapter_to_load_ = -1;
};

#endif    // TXTREADER_MAIN_WINDOW_H
