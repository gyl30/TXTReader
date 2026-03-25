#ifndef TXTREADER_CHAPTER_PARSER_H
#define TXTREADER_CHAPTER_PARSER_H

#include <cstddef>
#include <memory>

#include <QString>

#include "chapter_info.h"

namespace txtreader
{
namespace parser
{

class ChapterParser
{
   public:
    ChapterParser();
    ~ChapterParser();

    void configure(const ParserConfig& config);

    ParseResult parse(const QString& file_path);
    ParseResult parse(const char* data, size_t size);
    ParseResult parse(const std::string& content);

    static QString extract_chapter_head(const QString& title);
    static QString default_chapter_regex();

   private:
    ParseResult parse_internal(const char* data, size_t size);
    bool compile_regex();
    std::string effective_encoding() const;

    ParserConfig config_;
    void* regex_ = nullptr;
    bool compiled_ = false;
};

}    // namespace parser
}    // namespace txtreader

#endif    // TXTREADER_CHAPTER_PARSER_H
