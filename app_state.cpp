#include "app_state.h"

app_state::app_state(QObject* parent) : QObject(parent) {}

const QString& app_state::file_path() const { return file_path_; }

const QList<chapter_info>& app_state::chapters() const { return chapters_; }

size_t app_state::total_chapters() const { return total_chapters_; }

int app_state::current_chapter_index() const { return current_chapter_index_; }

const QString& app_state::search_keyword() const { return search_keyword_; }

const QList<int>& app_state::search_results() const { return search_results_; }

int app_state::current_search_result_index() const { return current_search_result_index_; }

int app_state::total_search_results() const { return search_results_.size(); }

void app_state::clear()
{
    file_path_.clear();
    chapters_.clear();
    total_chapters_ = 0;
    current_chapter_index_ = -1;
    clear_search_results();
    emit chapter_list_cleared();
}

void app_state::set_file_path(const QString& file_path)
{
    if (file_path_ != file_path)
    {
        file_path_ = file_path;
        emit file_path_changed(file_path_);
    }
}

void app_state::add_chapter(const QString& title, qint64 offset)
{
    chapters_.append({title.toStdString(), offset});
    emit chapter_found(title);
}

void app_state::set_parsing_finished(size_t total_chapters)
{
    total_chapters_ = total_chapters;
    emit parsing_finished(total_chapters_);
}

void app_state::set_current_chapter_index(int index)
{
    if (current_chapter_index_ != index)
    {
        current_chapter_index_ = index;
        emit current_chapter_index_changed(index);
    }
}

void app_state::set_search_results(const QString& keyword, const QList<int>& results)
{
    search_keyword_ = keyword;
    search_results_ = results;
    current_search_result_index_ = -1;
    emit search_results_changed();
}

void app_state::clear_search_results()
{
    search_keyword_.clear();
    search_results_.clear();
    current_search_result_index_ = -1;
    emit search_results_changed();
}

void app_state::set_current_search_result_index(int index)
{
    if (current_search_result_index_ != index)
    {
        current_search_result_index_ = index;
        emit search_results_changed();
    }
}
