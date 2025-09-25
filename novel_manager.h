#ifndef TXTREADER_NOVEL_MANAGER_H
#define TXTREADER_NOVEL_MANAGER_H

#include <QObject>
#include <QString>
#include <vector>
#include <string>

struct chapter_info
{
    std::string title;
    qint64 offset;
};

class novel_manager : public QObject
{
    Q_OBJECT

   public:
    explicit novel_manager(QObject* parent = nullptr);
    ~novel_manager() override;

   signals:
    void chapter_found(const QString& title);
    void parsing_finished(size_t total_chapters);
    void chapter_content_ready(int chapter_index, const QString& content);

   public slots:
    void load_file(const QString& file_path);
    void fetch_chapter_content(int chapter_index);

   private:
    void parse_chapters_async();

   private:
    QString file_path_;
    std::vector<chapter_info> chapters_;
    std::string detected_encoding_;
};

#endif    // NOVEL_MANAGER_H
