#ifndef TXTREADER_ENCODING_DETECTOR_H
#define TXTREADER_ENCODING_DETECTOR_H

#include <cstddef>
#include <string>

#include <QString>

namespace txtreader
{
namespace encoding
{

struct EncodingResult
{
    std::string encoding;
    bool success;
    std::string error_msg;
};

class EncodingDetector
{
   public:
    EncodingDetector();
    ~EncodingDetector();

    EncodingResult detect(const QString& file_path);
    EncodingResult detect(const char* data, size_t size);
    EncodingResult detect(const std::string& content);

    static std::string default_encoding();

    void reset();

   private:
    void* ud_;
};

}    // namespace encoding
}    // namespace txtreader

#endif    // TXTREADER_ENCODING_DETECTOR_H
