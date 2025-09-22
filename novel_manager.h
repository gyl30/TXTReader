#ifndef NOVEL_MANAGER_H
#define NOVEL_MANAGER_H

#include <QObject>
#include <QString>
#include <vector>

class QThread;

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
    ~novel_manager();

    void load_file(const QString& file_path);
    QString get_chapter_content(int chapter_index);
    [[nodiscard]] size_t get_total_chapters() const { return chapters_.size(); }

   signals:
    void chapter_found(const QString& title);
    void parsing_finished(size_t total_chapters);

   private slots:
    void parse_chapters_async();

   private:
    QThread* worker_thread_;
    QString file_path_;
    std::vector<chapter_info> chapters_;
};

#endif    // NOVEL_MANAGER_H
