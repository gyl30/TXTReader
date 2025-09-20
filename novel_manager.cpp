#include <QFile>
#include <QThread>
#include <QDebug>
#include <string>
#include <boost/regex.hpp>
#include "novel_manager.h"

novel_manager::novel_manager(QObject* parent) : QObject(parent), worker_thread_(new QThread(this))
{
    moveToThread(worker_thread_);
    connect(worker_thread_, &QThread::started, this, &novel_manager::parse_chapters_async);
    worker_thread_->start();
}

novel_manager::~novel_manager()
{
    worker_thread_->quit();
    worker_thread_->wait();
}

void novel_manager::load_file(const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "无法打开文件:" << file_path;
        return;
    }

    file_content_ = file.readAll();
    chapters_.clear();

    QMetaObject::invokeMethod(this, "parse_chapters_async", Qt::QueuedConnection);
}

void novel_manager::parse_chapters_async()
{
    if (file_content_.isEmpty())
    {
        emit parsing_finished(0);
        return;
    }

    std::string content(file_content_.constData(), file_content_.size());

    boost::regex chapter_pattern("第[一二三四五六七八九十百千万两0-9]+章[^\\r\\n]*");
    boost::sregex_iterator begin(content.begin(), content.end(), chapter_pattern);
    boost::sregex_iterator end;

    for (auto it = begin; it != end; ++it)
    {
        const boost::smatch& match = *it;
        chapter_info chapter = {match.str(), static_cast<qint64>(match.position())};
        chapters_.push_back(chapter);
        emit chapter_found(QString::fromStdString(chapter.title));
    }

    emit parsing_finished(chapters_.size());
}

QString novel_manager::get_chapter_content(int chapter_index)
{
    if (chapter_index < 0 || chapter_index >= chapters_.size())
    {
        return {};
    }

    const auto& current_chapter = chapters_[chapter_index];
    qint64 start_pos = current_chapter.offset;
    qint64 end_pos = file_content_.size();

    if (chapter_index + 1 < chapters_.size())
    {
        end_pos = chapters_[chapter_index + 1].offset;
    }

    qint64 length = end_pos - start_pos;
    QByteArray chapter_data = file_content_.mid(start_pos, length);
    return QString::fromStdString(chapter_data.toStdString());
}
