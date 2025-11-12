#ifndef TXTREADER_APP_STATE_H
#define TXTREADER_APP_STATE_H

#include <QObject>
#include <QString>
#include <QList>
#include "novel_manager.h"

class app_state : public QObject
{
    Q_OBJECT

   public:
    explicit app_state(QObject* parent = nullptr);

    const QString& file_path() const;
    const QList<chapter_info>& chapters() const;
    size_t total_chapters() const;
    int current_chapter_index() const;

   public slots:
    void clear();
    void set_file_path(const QString& file_path);
    void add_chapter(const QString& title, qint64 offset);
    void set_parsing_finished(size_t total_chapters);
    void set_current_chapter_index(int index);

   signals:
    void file_path_changed(const QString& file_path);
    void chapter_list_cleared();
    void chapter_found(const QString& title);
    void parsing_finished(size_t total_chapters);
    void current_chapter_index_changed(int index);

   private:
    QString file_path_;
    QList<chapter_info> chapters_;
    size_t total_chapters_ = 0;
    int current_chapter_index_ = -1;
};

#endif    // TXTREADER_APP_STATE_H
