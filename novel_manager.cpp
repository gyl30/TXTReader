#include "novel_manager.h"
#include <QDebug>
#include <QFile>
#include <QThread>
#include <boost/regex.hpp>
#include <string_view>

novel_manager::novel_manager(QObject* parent) : QObject(parent), worker_thread_(new QThread(this))
{
    this->moveToThread(worker_thread_);
    connect(this, &novel_manager::parse_chapters_async, this, &novel_manager::parse_chapters_async, Qt::QueuedConnection);
    worker_thread_->start();
}

novel_manager::~novel_manager()
{
    worker_thread_->quit();
    worker_thread_->wait();
}

void novel_manager::load_file(const QString& file_path)
{
    file_path_ = file_path;
    chapters_.clear();

    QMetaObject::invokeMethod(this, "parse_chapters_async", Qt::QueuedConnection);
}

void novel_manager::parse_chapters_async()
{
    if (file_path_.isEmpty())
    {
        emit parsing_finished(0);
        return;
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "无法打开文件:" << file_path_;
        emit parsing_finished(0);
        return;
    }

    qint64 file_size = file.size();
    if (file_size == 0)
    {
        file.close();
        emit parsing_finished(0);
        return;
    }
    uchar* mapped_data = file.map(0, file_size);

    if (mapped_data == nullptr)
    {
        qWarning() << "无法将文件映射到内存:" << file_path_;
        file.close();
        emit parsing_finished(0);
        return;
    }

    std::string_view content(reinterpret_cast<const char*>(mapped_data), file_size);

    boost::regex chapter_pattern("第[一二三四五六七八九十百千万两0-9]+章[^\\r\\n]*");
    boost::cregex_iterator begin(content.data(), content.data() + content.size(), chapter_pattern);
    boost::cregex_iterator end;

    for (auto it = begin; it != end; ++it)
    {
        const boost::cmatch& match = *it;
        chapter_info chapter = {match.str(), static_cast<qint64>(match.position())};
        chapters_.push_back(chapter);
        emit chapter_found(QString::fromStdString(chapter.title));
    }

    file.unmap(mapped_data);
    file.close();

    emit parsing_finished(chapters_.size());
}

QString novel_manager::get_chapter_content(size_t chapter_index)
{
    if (chapter_index < 0 || chapter_index >= chapters_.size() || file_path_.isEmpty())
    {
        return {};
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "获取章节内容时无法打开文件:" << file_path_;
        return {};
    }

    const auto& current_chapter = chapters_[chapter_index];
    qint64 start_pos = current_chapter.offset;
    qint64 end_pos = file.size();

    if (chapter_index + 1 < chapters_.size())
    {
        end_pos = chapters_[chapter_index + 1].offset;
    }

    qint64 length = end_pos - start_pos;
    if (length <= 0)
    {
        return {};
    }

    file.seek(start_pos);
    QByteArray chapter_data = file.read(length);
    return QString::fromUtf8(chapter_data);
}
