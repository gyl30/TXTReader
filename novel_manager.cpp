#include "novel_manager.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QRegularExpression>

#include <boost/locale.hpp>
#include <boost/regex.hpp>
#include <uchardet/uchardet.h>

#include <algorithm>

#include "log.h"
#include "scoped_exit.h"
#include "src/encoding/encoding_detector.h"
#include "src/parser/chapter_parser.h"

novel_manager::novel_manager(QObject* parent)
    : QObject(parent), encoding_detector_(new txtreader::encoding::EncodingDetector()), chapter_parser_(new txtreader::parser::ChapterParser())
{
}

novel_manager::~novel_manager()
{
    delete encoding_detector_;
    delete chapter_parser_;
    LOG_INFO("novel_manager destroyed");
}

void novel_manager::load_file(const QString& file_path, const QString& chapter_regex)
{
    file_path_ = file_path;
    chapters_.clear();

    auto result = encoding_detector_->detect(file_path);
    if (result.success)
    {
        detected_encoding_ = result.encoding;
    }
    else
    {
        detected_encoding_ = txtreader::encoding::EncodingDetector::default_encoding();
        LOG_WARN("encoding detection failed, using default", detected_encoding_);
    }

    parse_chapters_async(chapter_regex);
}

void novel_manager::parse_chapters_async(const QString& chapter_regex)
{
    if (file_path_.isEmpty())
    {
        LOG_ERROR("parse chapters failed file path is empty");
        emit parsing_finished(0);
        return;
    }

    if (detected_encoding_.empty())
    {
        detected_encoding_ = "UTF-8";
        LOG_WARN("detected encoding is empty, fallback to utf-8", file_path_.toStdString());
    }

    LOG_INFO("parse chapters encoding", file_path_.toStdString(), detected_encoding_.c_str());

    txtreader::parser::ParserConfig config;
    config.chapter_regex = chapter_regex.toStdString();
    config.source_encoding = detected_encoding_;
    config.min_chapter_distance = 100;
    config.treat_as_single_chapter = true;
    chapter_parser_->configure(config);

    auto result = chapter_parser_->parse(file_path_);

    if (!result.success)
    {
        LOG_ERROR("parse chapters failed", result.error_msg);
        emit parsing_finished(0);
        return;
    }

    LOG_INFO("parse chapters chapters count", result.chapters.size(), result.parse_failed, result.parse_success, result.duplicate_removed);

    chapters_.clear();
    for (const auto& chap : result.chapters)
    {
        chapters_.push_back({chap.title, chap.offset});
        emit chapter_found(QString::fromStdString(chap.title), chap.offset);
    }

    if (chapters_.empty())
    {
        LOG_INFO("no chapters found treating the entire file as a single chapter");
        QString title = QFileInfo(file_path_).fileName();
        chapter_info full_text_chapter;
        full_text_chapter.title = title.toStdString();
        full_text_chapter.offset = 0;
        chapters_.push_back(full_text_chapter);
        emit chapter_found(title, 0);
    }

    emit parsing_finished(chapters_.size());
}

void novel_manager::fetch_chapter_content(int chapter_index)
{
    if (static_cast<size_t>(chapter_index) >= chapters_.size() || file_path_.isEmpty())
    {
        emit chapter_content_ready(chapter_index, {});
        return;
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR("get chapter content open file failed {}", file_path_.toStdString());
        emit chapter_content_ready(chapter_index, "[无法打开文件]");
        return;
    }
    DEFER(file.close());

    const auto& current_chapter = chapters_[chapter_index];
    qint64 start_pos = current_chapter.offset;
    qint64 end_pos = file.size();
    if (static_cast<size_t>(chapter_index) + 1 < chapters_.size())
    {
        end_pos = chapters_[chapter_index + 1].offset;
    }
    qint64 length = end_pos - start_pos;

    LOG_INFO("fetching chapter index {} title {} start pos {} end pos {} size {}", chapter_index, current_chapter.title, start_pos, end_pos, length);

    if (length <= 0)
    {
        emit chapter_content_ready(chapter_index, {});
        return;
    }

    file.seek(start_pos);
    QByteArray chapter_data = file.read(length);
    QString content;
    try
    {
        std::string raw_bytes(chapter_data.constData(), chapter_data.size());
        std::string utf8_content = boost::locale::conv::to_utf<char>(raw_bytes, detected_encoding_);
        content = QString::fromStdString(utf8_content);
    }
    catch (const boost::locale::conv::conversion_error& e)
    {
        LOG_ERROR("chapter content encoding conversion failed encoding {} {}", detected_encoding_, e.what());
        content = "[内容转换失败]";
    }

    emit chapter_content_ready(chapter_index, content);
}

void novel_manager::search_file(const QString& keyword)
{
    if (keyword.isEmpty() || file_path_.isEmpty() || chapters_.empty())
    {
        emit search_finished({});
        return;
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit search_finished({});
        return;
    }

    uchar* mapped_data = file.map(0, file.size());
    if (mapped_data == nullptr)
    {
        emit search_finished({});
        return;
    }
    DEFER(file.unmap(mapped_data));

    std::string encoded_keyword;
    try
    {
        encoded_keyword = boost::locale::conv::from_utf(keyword.toStdString(), detected_encoding_);
    }
    catch (const boost::locale::conv::conversion_error& e)
    {
        LOG_ERROR("Search keyword encoding failed: {}", e.what());
        emit search_finished({});
        return;
    }

    QList<int> results;
    std::string_view content(reinterpret_cast<const char*>(mapped_data), file.size());
    size_t pos = 0;

    while ((pos = content.find(encoded_keyword, pos)) != std::string_view::npos)
    {
        qint64 match_offset = static_cast<qint64>(pos);

        auto it = std::upper_bound(
            chapters_.begin(), chapters_.end(), match_offset, [](qint64 offset, const chapter_info& chap) { return offset < chap.offset; });

        if (it != chapters_.begin())
        {
            --it;
            auto chapter_index = std::distance(chapters_.begin(), it);
            results.append(static_cast<int>(chapter_index));
        }
        else
        {
            results.append(0);
        }
        pos += encoded_keyword.length();
    }

    emit search_finished(results);
}
