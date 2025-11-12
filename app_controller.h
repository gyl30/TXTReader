#ifndef TXTREADER_APP_CONTROLLER_H
#define TXTREADER_APP_CONTROLLER_H

#include <QObject>
#include "main_window.h"
#include "novel_manager.h"
#include "app_state.h"
#include "settings_manager.h"

class QThread;

class app_controller : public QObject
{
    Q_OBJECT

   public:
    explicit app_controller(QObject* parent = nullptr);
    ~app_controller() override;
    void run();

   private slots:
    void on_open_file_triggered();
    void on_open_recent_file_triggered(const QVariantMap& file_info);
    void on_clear_recent_files_triggered();
    void on_chapter_selected(int index);
    void on_regex_dialog_triggered();
    void on_load_next_chapter();
    void on_load_previous_chapter();

    void on_font_selected(const QFont& font);
    void on_font_size_increase_triggered();
    void on_font_size_decrease_triggered();
    void on_line_spacing_increase_triggered();
    void on_line_spacing_decrease_triggered();
    void on_letter_spacing_increase_triggered();
    void on_letter_spacing_decrease_triggered();
    void on_auto_scroll_speed_increase_triggered();
    void on_auto_scroll_speed_decrease_triggered();

    void on_save_progress_requested();
    void on_application_quit();

    void on_parsing_finished(size_t total_chapters);
    void on_chapter_content_ready(int chapter_index, const QString& content);

   private:
    void setup_connections();
    void load_new_file(const QString& file_path, const QString& regex);
    void load_chapter(int chapter_index);

    main_window* main_window_;
    novel_manager* novel_manager_;
    app_state* app_state_;
    settings_manager* settings_manager_;
    QThread* worker_thread_;

    bool is_loading_content_ = false;
    int initial_chapter_to_load_ = -1;
    int chapter_index_to_restore_ = -1;
    double scroll_ratio_to_restore_ = 0.0;
};

#endif    // TXTREADER_APP_CONTROLLER_H
