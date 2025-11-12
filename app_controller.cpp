#include "app_controller.h"
#include <QThread>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>
#include <QApplication>
#include "log.h"

app_controller::app_controller(QObject* parent) : QObject(parent)
{
    app_state_ = new app_state(this);
    settings_manager_ = new settings_manager(this);
    main_window_ = new main_window(app_state_, settings_manager_);

    worker_thread_ = new QThread(this);
    novel_manager_ = new novel_manager();
    novel_manager_->moveToThread(worker_thread_);

    setup_connections();

    worker_thread_->start();
}

app_controller::~app_controller()
{
    worker_thread_->quit();
    worker_thread_->wait();
}

void app_controller::run() { main_window_->show(); }

void app_controller::setup_connections()
{
    connect(main_window_, &main_window::open_file_triggered, this, &app_controller::on_open_file_triggered);
    connect(main_window_, &main_window::open_recent_file_triggered, this, &app_controller::on_open_recent_file_triggered);
    connect(main_window_, &main_window::clear_recent_files_triggered, this, &app_controller::on_clear_recent_files_triggered);
    connect(main_window_, &main_window::chapter_selected, this, &app_controller::on_chapter_selected);
    connect(main_window_, &main_window::regex_dialog_triggered, this, &app_controller::on_regex_dialog_triggered);
    connect(main_window_, &main_window::application_quit_triggered, this, &app_controller::on_application_quit);

    connect(main_window_, &main_window::font_selected, this, &app_controller::on_font_selected);
    connect(main_window_, &main_window::font_size_increase_triggered, this, &app_controller::on_font_size_increase_triggered);
    connect(main_window_, &main_window::font_size_decrease_triggered, this, &app_controller::on_font_size_decrease_triggered);
    connect(main_window_, &main_window::line_spacing_increase_triggered, this, &app_controller::on_line_spacing_increase_triggered);
    connect(main_window_, &main_window::line_spacing_decrease_triggered, this, &app_controller::on_line_spacing_decrease_triggered);
    connect(main_window_, &main_window::letter_spacing_increase_triggered, this, &app_controller::on_letter_spacing_increase_triggered);
    connect(main_window_, &main_window::letter_spacing_decrease_triggered, this, &app_controller::on_letter_spacing_decrease_triggered);

    connect(main_window_, &main_window::auto_scroll_speed_increase_triggered, this, &app_controller::on_auto_scroll_speed_increase_triggered);
    connect(main_window_, &main_window::auto_scroll_speed_decrease_triggered, this, &app_controller::on_auto_scroll_speed_decrease_triggered);

    connect(main_window_, &main_window::save_progress_requested, this, &app_controller::on_save_progress_requested);

    connect(main_window_, &main_window::request_load_next_chapter, this, &app_controller::on_load_next_chapter);
    connect(main_window_, &main_window::request_load_previous_chapter, this, &app_controller::on_load_previous_chapter);

    connect(novel_manager_, &novel_manager::chapter_found, app_state_, &app_state::add_chapter);
    connect(novel_manager_, &novel_manager::parsing_finished, this, &app_controller::on_parsing_finished);
    connect(novel_manager_, &novel_manager::chapter_content_ready, this, &app_controller::on_chapter_content_ready);

    connect(worker_thread_, &QThread::finished, novel_manager_, &QObject::deleteLater);
}

void app_controller::on_open_file_triggered()
{
    QString file_path = QFileDialog::getOpenFileName(main_window_, "打开小说", "", "Text Files (*.txt)");
    if (!file_path.isEmpty())
    {
        load_new_file(file_path, settings_manager_->get_chapter_regex());
    }
}

void app_controller::on_open_recent_file_triggered(const QVariantMap& file_info)
{
    QString file_path = file_info.value("filePath").toString();
    QString regex = file_info.value("regex").toString();

    if (!QFile::exists(file_path))
    {
        QMessageBox::warning(main_window_, "文件未找到", QString("无法找到文件：\n%1\n\n该记录将被移除。").arg(file_path));
        settings_manager_->remove_recent_file(file_path);
        main_window_->populate_recent_files_menu();
        return;
    }
    load_new_file(file_path, regex);
}

void app_controller::on_clear_recent_files_triggered()
{
    settings_manager_->clear_recent_files();
    LOG_INFO("recent files list cleared.");
}

void app_controller::load_new_file(const QString& file_path, const QString& regex)
{
    if (file_path.isEmpty())
    {
        return;
    }
    on_save_progress_requested();
    app_state_->clear();
    app_state_->set_file_path(file_path);
    main_window_->set_status_message(" 正在解析章节...", "");

    settings_manager_->set_chapter_regex(regex);
    settings_manager_->update_recent_files(file_path);

    QMetaObject::invokeMethod(novel_manager_, "load_file", Qt::QueuedConnection, Q_ARG(QString, file_path), Q_ARG(QString, regex));
}

void app_controller::on_parsing_finished(size_t total_chapters)
{
    app_state_->set_parsing_finished(total_chapters);
    main_window_->show_transient_status_message(QString("找到 %1 个章节。").arg(total_chapters), 3000);
    if (total_chapters == 0)
    {
        main_window_->set_status_message(" 未找到章节", "进度: 0.00%");
    }
    const QString current_file_path = app_state_->file_path();
    if (!current_file_path.isEmpty())
    {
        QPair<int, double> progress = settings_manager_->load_progress(current_file_path);
        int chapter_index_to_load = progress.first;
        if (chapter_index_to_load >= 0 && static_cast<size_t>(chapter_index_to_load) < total_chapters)
        {
            chapter_index_to_restore_ = chapter_index_to_load;
            scroll_ratio_to_restore_ = progress.second;
            load_chapter(chapter_index_to_load);
        }
        else
        {
            load_chapter(0);
        }
    }
    else if (total_chapters > 0)
    {
        load_chapter(0);
    }
}

void app_controller::load_chapter(int chapter_index)
{
    on_save_progress_requested();
    if (chapter_index < 0 || static_cast<size_t>(chapter_index) >= app_state_->total_chapters())
    {
        return;
    }

    app_state_->set_current_chapter_index(chapter_index);
    is_loading_content_ = true;
    main_window_->clear_novel_view();
    initial_chapter_to_load_ = chapter_index;
    QMetaObject::invokeMethod(novel_manager_, "fetch_chapter_content", Qt::QueuedConnection, Q_ARG(int, chapter_index));
}

void app_controller::on_chapter_selected(int index)
{
    if (index >= 0 && index != app_state_->current_chapter_index())
    {
        load_chapter(index);
    }
}

void app_controller::on_chapter_content_ready(int chapter_index, const QString& content)
{
    if (content.isEmpty())
    {
        is_loading_content_ = false;
        return;
    }
    if (chapter_index == initial_chapter_to_load_)
    {
        main_window_->append_chapter_to_view(chapter_index, content);
        if (chapter_index + 1 < app_state_->total_chapters())
        {
            QMetaObject::invokeMethod(novel_manager_, "fetch_chapter_content", Qt::QueuedConnection, Q_ARG(int, chapter_index + 1));
        }
        initial_chapter_to_load_ = -1;
    }
    else if (chapter_index < main_window_->first_displayed_chapter_index())
    {
        main_window_->prepend_chapter_to_view(chapter_index, content);
    }
    else
    {
        main_window_->append_chapter_to_view(chapter_index, content);
    }
    is_loading_content_ = false;

    if (chapter_index == chapter_index_to_restore_)
    {
        main_window_->restore_scroll_position(scroll_ratio_to_restore_);
        chapter_index_to_restore_ = -1;
        scroll_ratio_to_restore_ = 0.0;
    }
}

void app_controller::on_load_previous_chapter()
{
    if (is_loading_content_)
    {
        return;
    }
    int first_index = main_window_->first_displayed_chapter_index();
    if (first_index <= 0)
    {
        return;
    }
    int prev_index = first_index - 1;
    if (main_window_->is_chapter_displayed(prev_index))
    {
        return;
    }
    is_loading_content_ = true;
    QMetaObject::invokeMethod(novel_manager_, "fetch_chapter_content", Qt::QueuedConnection, Q_ARG(int, prev_index));
}

void app_controller::on_load_next_chapter()
{
    if (is_loading_content_)
    {
        return;
    }
    int last_index = main_window_->last_displayed_chapter_index();
    if (static_cast<size_t>(last_index) >= app_state_->total_chapters() - 1)
    {
        return;
    }
    int next_index = last_index + 1;
    if (main_window_->is_chapter_displayed(next_index))
    {
        return;
    }
    is_loading_content_ = true;
    QMetaObject::invokeMethod(novel_manager_, "fetch_chapter_content", Qt::QueuedConnection, Q_ARG(int, next_index));
}

void app_controller::on_regex_dialog_triggered()
{
    QString old_regex = settings_manager_->get_chapter_regex();
    bool ok;
    QString new_regex = QInputDialog::getText(main_window_, "设置章节正则表达式", "正则表达式:", QLineEdit::Normal, old_regex, &ok);
    if (!ok || new_regex.isEmpty() || new_regex == old_regex)
    {
        return;
    }
    LOG_INFO("chapter regex changed from {} to {}", old_regex.toStdString(), new_regex.toStdString());
    const QString current_file_path = app_state_->file_path();
    if (!current_file_path.isEmpty())
    {
        LOG_INFO("reloading file {} with new regex", current_file_path.toStdString());
        load_new_file(current_file_path, new_regex);
    }
    else
    {
        settings_manager_->set_chapter_regex(new_regex);
    }
}

void app_controller::on_font_selected(const QFont& font) { settings_manager_->set_font(font); }

void app_controller::on_font_size_increase_triggered()
{
    QFont font = settings_manager_->get_font();
    font.setPointSizeF(font.pointSizeF() + 2.0);
    settings_manager_->set_font(font);
}

void app_controller::on_font_size_decrease_triggered()
{
    QFont font = settings_manager_->get_font();
    font.setPointSizeF(qMax(8.0, font.pointSizeF() - 2.0));
    settings_manager_->set_font(font);
}

void app_controller::on_line_spacing_increase_triggered()
{
    qreal spacing = settings_manager_->get_line_spacing();
    settings_manager_->set_line_spacing(spacing + 0.1);
}

void app_controller::on_line_spacing_decrease_triggered()
{
    qreal spacing = settings_manager_->get_line_spacing();
    settings_manager_->set_line_spacing(spacing - 0.1);
}

void app_controller::on_letter_spacing_increase_triggered()
{
    qreal spacing = settings_manager_->get_letter_spacing();
    settings_manager_->set_letter_spacing(spacing + 0.5);
}

void app_controller::on_letter_spacing_decrease_triggered()
{
    qreal spacing = settings_manager_->get_letter_spacing();
    settings_manager_->set_letter_spacing(spacing - 0.5);
}

void app_controller::on_auto_scroll_speed_increase_triggered()
{
    int speed = settings_manager_->get_auto_scroll_speed();
    settings_manager_->set_auto_scroll_speed(speed - 1);
}

void app_controller::on_auto_scroll_speed_decrease_triggered()
{
    int speed = settings_manager_->get_auto_scroll_speed();
    settings_manager_->set_auto_scroll_speed(speed + 1);
}

void app_controller::on_save_progress_requested()
{
    const QString file_path = app_state_->file_path();
    if (file_path.isEmpty() || app_state_->total_chapters() == 0)
    {
        return;
    }
    QPair<int, double> progress = main_window_->get_current_progress();
    if (progress.first >= 0)
    {
        settings_manager_->save_progress(file_path, progress.first, progress.second);
    }
}

void app_controller::on_application_quit()
{
    on_save_progress_requested();
    main_window_->hide();
    QApplication::quit();
}
