#ifndef TXTREADER_MAIN_WINDOW_H
#define TXTREADER_MAIN_WINDOW_H

#include <QMainWindow>
#include <QList>
#include <QPair>
#include <QFont>
#include <QColor>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QVariantMap>
#include "tray_icon.h"
#include "app_state.h"
#include "settings_manager.h"

class QListWidget;
class QSplitter;
class QListWidgetItem;
class QTimer;
class QAction;
class QElapsedTimer;
class QToolBar;
class novel_view;
class QLabel;
class QLineEdit;

class main_window : public QMainWindow
{
    Q_OBJECT

   public:
    explicit main_window(app_state* app_state, settings_manager* settings_manager, QWidget* parent = nullptr);
    ~main_window() override;

    void populate_recent_files_menu();
    void set_status_message(const QString& chapter_text, const QString& progress_text);
    void show_transient_status_message(const QString& message, int timeout = 0);
    void clear_novel_view();
    void append_chapter_to_view(int chapter_index, const QString& content);
    void prepend_chapter_to_view(int chapter_index, const QString& content);
    void restore_scroll_position(double ratio);

    void perform_local_search(const QString& keyword);
    void jump_to_match(int match_index);
    void clear_local_search();

    int first_displayed_chapter_index() const;
    int last_displayed_chapter_index() const;
    bool is_chapter_displayed(int chapter_index) const;
    QPair<int, double> get_current_progress() const;

   protected:
    void paintEvent(QPaintEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

   signals:
    void open_file_triggered();
    void open_recent_file_triggered(const QVariantMap& file_info);
    void clear_recent_files_triggered();
    void chapter_selected(int index);
    void regex_dialog_triggered();
    void application_quit_triggered();
    void save_progress_requested();

    void request_load_previous_chapter();
    void request_load_next_chapter();

    void font_selected(const QFont& font);
    void font_size_increase_triggered();
    void font_size_decrease_triggered();
    void line_spacing_increase_triggered();
    void line_spacing_decrease_triggered();
    void letter_spacing_increase_triggered();
    void letter_spacing_decrease_triggered();
    void auto_scroll_speed_increase_triggered();
    void auto_scroll_speed_decrease_triggered();

    void search_triggered(const QString& keyword);
    void find_next_result_triggered();
    void find_previous_result_triggered();

   private slots:
    void on_chapter_list_item_clicked(QListWidgetItem* item);
    void on_open_recent_file_action();
    void perform_auto_scroll();
    void on_auto_scroll_click();
    void update_background_gradient();
    void on_color_action();
    void change_to_next_color_scheme();
    void switch_to_next_background();
    void on_select_font_dialog();

    void on_search_return_pressed();

    void on_app_state_chapter_list_cleared();
    void on_app_state_chapter_found(const QString& title);
    void on_app_state_current_chapter_index_changed(int index);
    void on_settings_font_changed(const QFont& font);
    void on_settings_spacing_changed(qreal line_spacing, qreal letter_spacing);
    void on_app_state_search_results_changed();

    void on_view_scrolled();
    void on_load_next_chapter_action();
    void on_load_previous_chapter_action();

   private:
    void setup_ui();
    void setup_connections();
    void setup_static_backgrounds();
    void setup_color_schemes();
    void apply_font_and_spacing();
    void setup_shortcuts();
    void ensure_chapter_is_visible(int chapter_index);
    void update_progress_status();
    void update_search_status();

   private:
    app_state* app_state_;
    settings_manager* settings_manager_;

    QList<QColor> static_backgrounds_;
    qsizetype current_static_bg_index_ = 0;
    QColor last_used_static_color_;
    QList<QColor> color_schemes_;
    int scheme_index_ = 0;
    QColor current_color_;
    QColor target_color_;

    QElapsedTimer* transition_start_time_;
    QTimer* color_change_timer_;
    bool is_dynamic_background_ = false;
    QTimer* background_animation_timer_;
    QTimer* auto_scroll_timer_;
    QTimer* auto_save_timer_;

    QListWidget* chapter_list_;
    novel_view* novel_view_;
    QSplitter* splitter_;
    QToolBar* main_tool_bar_;
    QAction* open_file_action_;
    QAction* color_action_;
    QAction* switch_background_action_;
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

    QLineEdit* search_input_;
    QAction* find_next_action_;
    QAction* find_prev_action_;

    bool auto_scroll_ = false;

    tray_icon* tray_icon_ = nullptr;
    QLabel* status_chapter_label_ = nullptr;
    QLabel* status_progress_label_ = nullptr;
    QLabel* status_search_label_ = nullptr;
};

#endif
