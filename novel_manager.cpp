#include <QDebug>
#include <QFile>
#include <QThread>
#include <boost/locale.hpp>
#include <boost/regex.hpp>
#include <uchardet/uchardet.h>
#include "log.h"
#include "scoped_exit.h"
#include "novel_manager.h"

static std::string detect_file_encoding(const QString& file_path)
{
    uchardet_t ud = uchardet_new();
    DEFER(uchardet_delete(ud));
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR("open file failed {} {}", file_path.toStdString(), file.errorString().toStdString());
        return "";
    }

    char buf[4096];
    const qint64 len = file.read(buf, sizeof(buf));
    file.close();

    if (len > 0)
    {
        if (uchardet_handle_data(ud, buf, len) != 0)
        {
            LOG_ERROR("uchardet data failed {} {}", file_path.toStdString(), file.errorString().toStdString());
        }
    }
    uchardet_data_end(ud);

    const char* charset = uchardet_get_charset(ud);
    if (charset != nullptr && (*charset != 0))
    {
        return QString::fromLatin1(charset).trimmed().toStdString();
    }
    LOG_ERROR("uchardet get charset failed {}", file_path.toStdString());
    return "";
}

novel_manager::novel_manager(QObject* parent) : QObject(parent), worker_thread_(new QThread(this))
{
    boost::locale::generator gen;
    std::locale::global(gen(""));
    this->moveToThread(worker_thread_);
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
    detected_encoding_ = detect_file_encoding(file_path);
    QMetaObject::invokeMethod(this, "parse_chapters_async", Qt::QueuedConnection);
}

void novel_manager::parse_chapters_async()
{
    if (file_path_.isEmpty())
    {
        LOG_ERROR("parse chapters failed file path is empty");
        emit parsing_finished(0);
        return;
    }
    LOG_INFO("parse chapters {} encoding {}", file_path_.toStdString(), detected_encoding_.c_str());
    ;

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR("open file failed {} {}", file_path_.toStdString(), file.errorString().toStdString());
        emit parsing_finished(0);
        return;
    }
    DEFER(file.close());

    qint64 file_size = file.size();
    if (file_size == 0)
    {
        emit parsing_finished(0);
        return;
    }
    LOG_INFO("parse chapters {} file size {}", file_path_.toStdString(), file_size);
    uchar* mapped_data = file.map(0, file_size);
    if (mapped_data == nullptr)
    {
        LOG_ERROR("file map failed {}", file_path_.toStdString());
        emit parsing_finished(0);
        return;
    }
    DEFER(file.unmap(mapped_data));

    std::string utf8_str = "第[一二三四五六七八九十百千万两0-9]+章[^\\r\\n]*";
    std::string encoding_str;
    try
    {
        encoding_str = boost::locale::conv::from_utf(utf8_str, detected_encoding_);
    }
    catch (const boost::locale::conv::conversion_error& e)
    {
        LOG_ERROR("regular expression encoding conversion failed {} encoding {} {}", utf8_str, detected_encoding_, e.what());
        emit parsing_finished(0);
        return;
    }
    LOG_INFO("regular expression encoding {} encoding {} {}", utf8_str, detected_encoding_, encoding_str);

    boost::regex chapter_pattern;
    try
    {
        chapter_pattern.assign(encoding_str);
    }
    catch (const boost::regex_error& e)
    {
        LOG_ERROR("regular expression creation failed {} {} {}", encoding_str, detected_encoding_, e.what());
        emit parsing_finished(0);
        return;
    }

    std::string_view content(reinterpret_cast<const char*>(mapped_data), file_size);
    boost::cregex_iterator begin(content.data(), content.data() + content.size(), chapter_pattern);
    boost::cregex_iterator end;

    uint32_t chapter_count = 0;
    uint32_t chapter_parse_failed = 0;
    uint32_t chapter_parse_success = 0;
    for (auto it = begin; it != end; ++it)
    {
        chapter_count++;
        const boost::cmatch& match = *it;
        try
        {
            chapter_parse_success++;
            chapter_info chapter = {match.str(), static_cast<qint64>(match.position())};
            std::string utf8_title = boost::locale::conv::to_utf<char>(chapter.title, detected_encoding_);
            chapter.title = utf8_title;
            chapters_.push_back(chapter);
            emit chapter_found(QString::fromStdString(utf8_title));
        }
        catch (const boost::locale::conv::conversion_error&)
        {
            chapter_parse_failed++;
            emit chapter_found("[标题转换失败]");
        }
    }
    LOG_INFO("parse chapters {} encoding {} chapters count {} failed {} success {}",
             file_path_.toStdString(),
             detected_encoding_,
             chapter_count,
             chapter_parse_failed,
             chapter_parse_success);

    emit parsing_finished(chapters_.size());
}

QString novel_manager::get_chapter_content(size_t chapter_index)
{
    if (chapter_index >= chapters_.size() || file_path_.isEmpty())
    {
        return {};
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR("get chapter content open file failed {}", file_path_.toStdString());
        return "[无法打开文件]";
    }
    DEFER(file.close());

    const auto& current_chapter = chapters_[chapter_index];
    qint64 start_pos = current_chapter.offset;
    qint64 end_pos = file.size();
    if (chapter_index + 1 < chapters_.size())
    {
        end_pos = chapters_[chapter_index + 1].offset;
    }
    qint64 length = end_pos - start_pos;
    LOG_INFO("chapter index {} title {} start pos {} end pos {} size {}", chapter_index, current_chapter.title, start_pos, end_pos, length);
    if (length <= 0)
    {
        return {};
    }

    file.seek(start_pos);
    QByteArray chapter_data = file.read(length);
    try
    {
        std::string raw_bytes(chapter_data.constData(), chapter_data.size());
        std::string utf8_content = boost::locale::conv::to_utf<char>(raw_bytes, detected_encoding_);
        return QString::fromStdString(utf8_content);
    }
    catch (const boost::locale::conv::conversion_error& e)
    {
        LOG_ERROR("chapter content encoding conversion failed encoding {} {}", detected_encoding_, e.what());
        return "[内容转换失败]";
    }
    return {};
}
