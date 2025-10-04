#ifndef TXTREADER_MAIN_WINDOW_H
#define TXTREADER_MAIN_WINDOW_H

#include <QMainWindow>
#include <QList>
#include <QPair>
#include <QFont>
#include <QColor>

class QListWidget;
class QSplitter;
class novel_manager;
class QListWidgetItem;
class QTimer;
class QAction;
class QElapsedTimer;
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
    void request_load_file(const QString& file_path, const QString& chapter_regex);
    void request_chapter_content(int chapter_index);

   private slots:
    void open_file_dialog();
    void toggle_chapter_list_visibility();
    void on_chapter_list_item_clicked(QListWidgetItem* item);
    void select_font_dialog();
    void open_regex_dialog();

    void populate_recent_files_menu();
    void open_recent_file();
    void clear_recent_files();
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
    void change_to_next_color_scheme();
    void increase_line_spacing();
    void decrease_line_spacing();
    void increase_letter_spacing();
    void decrease_letter_spacing();

   private:
    void setup_ui();
    void setup_connections();
    void load_chapter(int chapter_index);
    void setup_color_schemes();
    void reset_auto_scroll_speed();
    void apply_font_and_spacing();
    void ensure_chapter_is_visible(int chapter_index);
    void update_recent_files(const QString& file_path);
    void load_new_file(const QString& file_path);
    void save_progress();
    void load_progress(const QString& file_path);

    QString get_current_regex();
    void setup_shortcuts();

   private:
    QString current_regex_;
    QList<QColor> color_schemes_;
    int scheme_index_ = 0;
    QColor current_color_;
    QColor target_color_;

    QElapsedTimer* transition_start_time_;
    QTimer* color_change_timer_;
    bool is_dynamic_background_ = false;
    QTimer* background_animation_timer_;
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
    QAction* select_font_action_;
    QAction* recent_files_action_;
    QMenu* recent_files_menu_;

    bool auto_scroll_ = false;
    int speed_ = 60;
    QFont view_font_;
    qreal line_spacing_ = 1.5;
    qreal letter_spacing_ = 1.5;
    novel_manager* novel_manager_;
    QThread* worker_thread_;
    size_t total_chapters_ = 0;
    double scroll_ratio_to_restore_ = 0.0;
    int chapter_index_to_restore_ = -1;
    bool is_loading_content_ = false;
    int initial_chapter_to_load_ = -1;
    int current_chapter_index_ = -1;
};

#endif    // TXTREADER_MAIN_WINDOW_H
