#ifndef TXTREADER_SETTINGS_MANAGER_H
#define TXTREADER_SETTINGS_MANAGER_H

#include <QObject>
#include <QFont>
#include <QColor>
#include <QVariantList>
#include <QPair>

class settings_manager : public QObject
{
    Q_OBJECT

   public:
    explicit settings_manager(QObject* parent = nullptr);

    void save_progress(const QString& file_path, int chapter_index, double scroll_ratio);
    QPair<int, double> load_progress(const QString& file_path);

    void update_recent_files(const QString& file_path);
    QVariantList get_recent_files() const;
    void remove_recent_file(const QString& file_path);
    void clear_recent_files();

    void set_chapter_regex(const QString& regex);
    QString get_chapter_regex() const;

    QFont get_font() const;
    void set_font(const QFont& font);

    qreal get_line_spacing() const;
    void set_line_spacing(qreal spacing);

    qreal get_letter_spacing() const;
    void set_letter_spacing(qreal spacing);

    int get_auto_scroll_speed() const;
    void set_auto_scroll_speed(int speed);

   signals:
    void settings_changed();
    void font_changed(const QFont& font);
    void spacing_changed(qreal line_spacing, qreal letter_spacing);

   private:
    void load_defaults();

    QFont view_font_;
    qreal line_spacing_;
    qreal letter_spacing_;
    int auto_scroll_speed_;

    QString last_saved_file_path_;
    int last_saved_chapter_index_ = -1;
    double last_saved_scroll_ratio_ = -1.0;
};

#endif    // TXTREADER_SETTINGS_MANAGER_H
