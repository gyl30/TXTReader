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

   signals:
    void chapter_found(const QString& title);
    void parsing_finished(size_t total_chapters);

   private slots:
    void parse_chapters_async();

   private:
    QThread* worker_thread_;
    QByteArray file_content_;
    std::vector<chapter_info> chapters_;
};

#endif    // NOVEL_MANAGER_H
