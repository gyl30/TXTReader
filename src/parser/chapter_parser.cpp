#include "chapter_parser.h"

#include <cstddef>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <boost/locale.hpp>
#include <boost/regex.hpp>

#include "../../log.h"
#include "../../scoped_exit.h"

namespace txtreader
{
namespace parser
{

ChapterParser::ChapterParser() : regex_(nullptr), compiled_(false) {}

ChapterParser::~ChapterParser()
{
    if (regex_ != nullptr)
    {
        delete static_cast<boost::regex*>(regex_);
    }
}

void ChapterParser::configure(const ParserConfig& config)
{
    config_ = config;
    compiled_ = false;
}

QString ChapterParser::default_chapter_regex() { return QStringLiteral("第[0-9一二三四五六七八九十百千万两]+章"); }

QString ChapterParser::extract_chapter_head(const QString& title)
{
    static const QRegularExpression re(QStringLiteral("^第[0-9一二三四五六七八九十百千万两]+章"));

    auto match = re.match(title);
    if (match.hasMatch())
    {
        return match.captured(0);
    }

    QString clean_title = title;
    static const QRegularExpression space_re(QStringLiteral("\\s"));
    clean_title.remove(space_re);
    return clean_title;
}

bool ChapterParser::compile_regex()
{
    if (regex_ != nullptr)
    {
        delete static_cast<boost::regex*>(regex_);
        regex_ = nullptr;
    }

    if (config_.chapter_regex.empty())
    {
        LOG_ERROR("chapter parser empty regex");
        return false;
    }

    std::string effective_encoding = config_.source_encoding;
    if (effective_encoding == "ASCII" || effective_encoding.empty())
    {
        effective_encoding = "UTF-8";
    }

    try
    {
        std::string encoding_str;
        try
        {
            encoding_str = boost::locale::conv::from_utf(config_.chapter_regex, effective_encoding, boost::locale::conv::method_type::stop);
        }
        catch (const boost::locale::conv::conversion_error& e)
        {
            LOG_ERROR("chapter parser regex encoding conversion failed", e.what());
            return false;
        }

        regex_ = new boost::regex();
        static_cast<boost::regex*>(regex_)->assign(encoding_str);
        compiled_ = true;
        return true;
    }
    catch (const boost::regex_error& e)
    {
        LOG_ERROR("chapter parser regex creation failed", e.what());
        compiled_ = false;
        return false;
    }
}

std::string ChapterParser::effective_encoding() const
{
    std::string enc = config_.source_encoding;
    if (enc == "ASCII" || enc.empty())
    {
        return "UTF-8";
    }
    return enc;
}

ParseResult ChapterParser::parse(const QString& file_path)
{
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG_ERROR("chapter parser open file failed", file.errorString().toStdString());
        return {};
    }

    qint64 file_size = file.size();
    if (file_size == 0)
    {
        return {};
    }

    uchar* mapped_data = file.map(0, file_size);
    if (mapped_data == nullptr)
    {
        LOG_ERROR("chapter parser file map failed");
        return {};
    }

    DEFER(file.unmap(mapped_data));

    return parse_internal(reinterpret_cast<const char*>(mapped_data), static_cast<size_t>(file_size));
}

ParseResult ChapterParser::parse(const char* data, size_t size)
{
    if (data == nullptr || size == 0)
    {
        return {};
    }
    return parse_internal(data, size);
}

ParseResult ChapterParser::parse(const std::string& content) { return parse(content.data(), content.size()); }

ParseResult ChapterParser::parse_internal(const char* data, size_t size)
{
    ParseResult result;

    if (!compiled_ && !compile_regex())
    {
        result.success = false;
        result.error_msg = "regex compilation failed";
        return result;
    }

    const std::string_view content(data, size);
    const boost::regex& pattern = *static_cast<boost::regex*>(regex_);

    const boost::cregex_iterator begin(content.data(), content.data() + content.size(), pattern);
    const boost::cregex_iterator end;

    std::vector<ChapterInfo> chapters;

    for (auto it = begin; it != end; ++it)
    {
        result.total_matches++;
        const boost::cmatch& match = *it;
        auto current_offset = static_cast<qint64>(match.position());

        std::string utf8_title;
        bool title_converted = false;

        try
        {
            utf8_title = boost::locale::conv::to_utf<char>(match.str(), effective_encoding());
            title_converted = true;
        }
        catch (const boost::locale::conv::conversion_error&)
        {
            result.parse_failed++;
            utf8_title = "[标题转换失败]";
        }

        if (title_converted && !chapters.empty())
        {
            const auto& last_chapter = chapters.back();
            qint64 distance = current_offset - last_chapter.offset;

            if (distance < config_.min_chapter_distance)
            {
                const QString q_curr_title = QString::fromStdString(utf8_title);
                const QString q_last_title = QString::fromStdString(last_chapter.title);
                const QString head_curr = extract_chapter_head(q_curr_title);
                const QString head_last = extract_chapter_head(q_last_title);

                if (head_curr == head_last)
                {
                    result.duplicate_removed++;
                    continue;
                }
            }
        }

        if (title_converted)
        {
            result.parse_success++;
        }

        chapters.emplace_back(utf8_title, current_offset);
    }

    if (chapters.empty() && size > 0 && config_.treat_as_single_chapter)
    {
        chapters.emplace_back("[全文]", 0);
    }

    result.chapters = std::move(chapters);
    return result;
}

}    // namespace parser
}    // namespace txtreader
