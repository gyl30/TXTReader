#include "encoding_detector.h"

#include <cstddef>

#include <QFile>
#include <uchardet/uchardet.h>

#include "../../log.h"

namespace txtreader
{
namespace encoding
{

EncodingDetector::EncodingDetector() { ud_ = reinterpret_cast<void*>(uchardet_new()); }

EncodingDetector::~EncodingDetector()
{
    if (ud_ != nullptr)
    {
        uchardet_delete(reinterpret_cast<uchardet_t>(ud_));
    }
}

EncodingResult EncodingDetector::detect(const QString& file_path)
{
    QFile file(file_path);
    if (file.open(QIODevice::ReadOnly) == false)
    {
        LOG_ERROR("encoding detector open file failed", file_path.toStdString(), file.errorString().toStdString());
        return {"", false, file.errorString().toStdString()};
    }

    char buf[4096];
    const qint64 len = file.read(buf, sizeof(buf));
    file.close();

    return detect(buf, static_cast<size_t>(len));
}

EncodingResult EncodingDetector::detect(const char* data, size_t size)
{
    if (ud_ == nullptr)
    {
        return {"", false, "uchardet handle is null"};
    }

    if (data == nullptr || size == 0)
    {
        return {"", false, "invalid data"};
    }

    uchardet_t handle = reinterpret_cast<uchardet_t>(ud_);
    if (uchardet_handle_data(handle, data, size) != 0)
    {
        LOG_ERROR("encoding detector uchardet handle data failed");
        return {"", false, "uchardet handle data failed"};
    }

    uchardet_data_end(handle);

    const char* charset = uchardet_get_charset(handle);
    if (charset != nullptr && (*charset != 0))
    {
        std::string encoding = QString::fromLatin1(charset).trimmed().toStdString();
        reset();
        return {encoding, true, ""};
    }

    LOG_WARN("encoding detector detect failed fallback to utf-8");
    reset();
    return {default_encoding(), true, "fallback to default"};
}

EncodingResult EncodingDetector::detect(const std::string& content) { return detect(content.data(), content.size()); }

std::string EncodingDetector::default_encoding() { return "UTF-8"; }

void EncodingDetector::reset()
{
    if (ud_ != nullptr)
    {
        uchardet_reset(reinterpret_cast<uchardet_t>(ud_));
    }
}

}    // namespace encoding
}    // namespace txtreader
