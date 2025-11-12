#include "settings_manager.h"
#include <QSettings>
#include <QFontDatabase>
#include <QFileInfo>
#include <QtMath>
#include "log.h"

static const char* kSelfName = "TXTReader";
static const char* kRecentFiles = "recent_files";
static const char* kLastChapterIndex = "last_chapter_index";
static const char* kLastScrollRatio = "last_scroll_ratio";
static const char* kChapterRegex = "chapter_regex";
static const char* kDefaultChapterRegex = "第[一二三四五六七八九十百千万两0-9]+章[^\\r\\n]*";
static const char* kViewFont = "view_font";
static const char* kLineSpacing = "line_spacing";
static const char* kLetterSpacing = "letter_spacing";
static const char* kAutoScrollSpeed = "auto_scroll_speed";

static QFont default_font(qreal font_size)
{
    const QStringList preferred_families = {"Microsoft YaHei UI", "Noto Sans CJK SC", "PingFang SC", "WenQuanYi Zen Hei"};

    for (const QString& family : preferred_families)
    {
        if (QFontDatabase::families().contains(family, Qt::CaseInsensitive))
        {
            QFont font(family);
            font.setPointSizeF(font_size);
            return font;
        }
    }

    QFont font("sans-serif");
    font.setPointSizeF(font_size);
    return font;
}

settings_manager::settings_manager(QObject* parent) : QObject(parent) { load_defaults(); }

void settings_manager::load_defaults()
{
    QSettings settings(kSelfName, kSelfName);
    view_font_ = settings.value(kViewFont, default_font(38.0)).value<QFont>();
    line_spacing_ = settings.value(kLineSpacing, 1.5).toReal();
    letter_spacing_ = settings.value(kLetterSpacing, 1.5).toReal();
    auto_scroll_speed_ = settings.value(kAutoScrollSpeed, 60).toInt();
}

void settings_manager::save_progress(const QString& file_path, int chapter_index, double scroll_ratio)
{
    if (file_path.isEmpty() || chapter_index < 0)
    {
        return;
    }

    if (file_path == last_saved_file_path_ && chapter_index == last_saved_chapter_index_ && qAbs(scroll_ratio - last_saved_scroll_ratio_) < 0.001)
    {
        return;
    }

    QSettings settings(kSelfName, kSelfName);
    QString absolute_path = QFileInfo(file_path).absoluteFilePath();
    QByteArray key = absolute_path.toUtf8().toBase64(QByteArray::Base64UrlEncoding);
    settings.beginGroup(QString(key));
    settings.setValue(kLastChapterIndex, chapter_index);
    settings.setValue(kLastScrollRatio, scroll_ratio);
    settings.endGroup();

    last_saved_file_path_ = file_path;
    last_saved_chapter_index_ = chapter_index;
    last_saved_scroll_ratio_ = scroll_ratio;

    LOG_INFO("progress saved {} chapter {} ratio {}", key.toStdString(), chapter_index, scroll_ratio);
}

QPair<int, double> settings_manager::load_progress(const QString& file_path)
{
    QSettings settings(kSelfName, kSelfName);
    QString absolute_path = QFileInfo(file_path).absoluteFilePath();
    QByteArray key = absolute_path.toUtf8().toBase64(QByteArray::Base64UrlEncoding);

    if (settings.childGroups().contains(QString(key)))
    {
        settings.beginGroup(QString(key));
        int chapter_index = settings.value(kLastChapterIndex, 0).toInt();
        double scroll_ratio = settings.value(kLastScrollRatio, 0.0).toDouble();
        settings.endGroup();
        LOG_INFO("loading progress {} chapter {} ratio {}", absolute_path.toStdString(), chapter_index, scroll_ratio);
        return qMakePair(chapter_index, scroll_ratio);
    }
    return qMakePair(0, 0.0);
}

void settings_manager::update_recent_files(const QString& file_path)
{
    QSettings settings(kSelfName, kSelfName);
    QVariantList recent_files = settings.value(kRecentFiles).toList();
    int existing_index = -1;
    for (int i = 0; i < recent_files.size(); ++i)
    {
        if (recent_files[i].toMap().value("filePath").toString() == file_path)
        {
            existing_index = i;
            break;
        }
    }

    if (existing_index != -1)
    {
        recent_files.removeAt(existing_index);
    }

    QVariantMap file_info;
    file_info["filePath"] = file_path;
    file_info["regex"] = get_chapter_regex();

    recent_files.prepend(file_info);

    while (recent_files.size() > 10)
    {
        recent_files.removeLast();
    }

    settings.setValue(kRecentFiles, recent_files);
}

QVariantList settings_manager::get_recent_files() const
{
    QSettings settings(kSelfName, kSelfName);
    return settings.value(kRecentFiles).toList();
}

void settings_manager::remove_recent_file(const QString& file_path)
{
    QSettings settings(kSelfName, kSelfName);
    QVariantList recent_files = settings.value(kRecentFiles).toList();
    for (int i = 0; i < recent_files.size(); ++i)
    {
        if (recent_files[i].toMap().value("filePath").toString() == file_path)
        {
            recent_files.removeAt(i);
            break;
        }
    }
    settings.setValue(kRecentFiles, recent_files);
}

void settings_manager::clear_recent_files()
{
    QSettings settings(kSelfName, kSelfName);
    settings.remove(kRecentFiles);
}

void settings_manager::set_chapter_regex(const QString& regex)
{
    QSettings settings(kSelfName, kSelfName);
    settings.setValue(kChapterRegex, regex);
    emit settings_changed();
}

QString settings_manager::get_chapter_regex() const
{
    QSettings settings(kSelfName, kSelfName);
    return settings.value(kChapterRegex, kDefaultChapterRegex).toString();
}

QFont settings_manager::get_font() const { return view_font_; }

void settings_manager::set_font(const QFont& font)
{
    if (view_font_ != font)
    {
        view_font_ = font;
        QSettings settings(kSelfName, kSelfName);
        settings.setValue(kViewFont, view_font_);
        emit font_changed(view_font_);
    }
}

qreal settings_manager::get_line_spacing() const { return line_spacing_; }

void settings_manager::set_line_spacing(qreal spacing)
{
    spacing = qMax(0.5, spacing);
    if (line_spacing_ != spacing)
    {
        line_spacing_ = spacing;
        QSettings settings(kSelfName, kSelfName);
        settings.setValue(kLineSpacing, line_spacing_);
        emit spacing_changed(line_spacing_, letter_spacing_);
    }
}

qreal settings_manager::get_letter_spacing() const { return letter_spacing_; }

void settings_manager::set_letter_spacing(qreal spacing)
{
    spacing = qMax(0.0, spacing);
    if (letter_spacing_ != spacing)
    {
        letter_spacing_ = spacing;
        QSettings settings(kSelfName, kSelfName);
        settings.setValue(kLetterSpacing, letter_spacing_);
        emit spacing_changed(line_spacing_, letter_spacing_);
    }
}

int settings_manager::get_auto_scroll_speed() const { return auto_scroll_speed_; }

void settings_manager::set_auto_scroll_speed(int speed)
{
    speed = qMax(3, speed);
    if (auto_scroll_speed_ != speed)
    {
        auto_scroll_speed_ = speed;
        QSettings settings(kSelfName, kSelfName);
        settings.setValue(kAutoScrollSpeed, auto_scroll_speed_);
        emit settings_changed();
    }
}
