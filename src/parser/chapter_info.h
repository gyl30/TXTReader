#ifndef TXTREADER_CHAPTER_INFO_H
#define TXTREADER_CHAPTER_INFO_H

#include <cstdint>
#include <string>
#include <vector>

#include <QString>

namespace txtreader
{
namespace parser
{

struct ChapterInfo
{
    std::string title;
    qint64 offset;
    bool valid;

    ChapterInfo() : offset(0), valid(true) {}

    ChapterInfo(const std::string& t, qint64 o) : title(t), offset(o), valid(true) {}

    static ChapterInfo from_string(const QString& title_str, qint64 off) { return ChapterInfo(title_str.toStdString(), off); }
};

struct ParserConfig
{
    std::string chapter_regex;
    std::string source_encoding;
    int min_chapter_distance = 100;
    bool treat_as_single_chapter = false;

    ParserConfig() = default;

    ParserConfig(const std::string& regex, const std::string& encoding) : chapter_regex(regex), source_encoding(encoding) {}
};

struct ParseResult
{
    std::vector<ChapterInfo> chapters;
    uint32_t total_matches = 0;
    uint32_t parse_failed = 0;
    uint32_t parse_success = 0;
    uint32_t duplicate_removed = 0;
    bool success;
    std::string error_msg;

    ParseResult() : success(true) {}
};

}    // namespace parser
}    // namespace txtreader

#endif    // TXTREADER_CHAPTER_INFO_H
