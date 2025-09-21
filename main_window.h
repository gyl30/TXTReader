#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <qabstractspinbox.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <QMainWindow>
#include <QList>

class QListWidget;
class QPlainTextEdit;
class QSplitter;
class novel_manager;
class QListWidgetItem;
class QScrollBar;

class main_window : public QMainWindow
{
    Q_OBJECT

   public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();

   protected:
    void paintEvent(QPaintEvent *event) override;

   private slots:
    void toggle_chapter_list_visibility();
    void open_file_dialog();
    void on_chapter_list_item_clicked(QListWidgetItem *item);

    void on_chapter_found(const QString &title);
    void on_parsing_finished(int total_chapters);

    void update_state_on_scroll();
    void update_progress_status();
    void toggle_auto_scroll();
    void perform_auto_scroll();
    void auto_scroll_click();

    void increase_auto_speed();
    void decrease_auto_speed();
    void increase_font_size();
    void decrease_font_size();
    void update_background_gradient();

   private:
    void setup_ui();
    void setup_connections();

    void load_chapter_to_display(int load_index, bool append, bool fouce);
    void insert_chapter_to_display(int load_index);
    void reset_auto_scroll_speed();
    void update_font_size(int new_size);
    void on_color_action();

   private:
    int gradient_offset_ = 0;
    bool is_dynamic_background_ = false;
    QTimer *background_animation_timer_;
    int hue_;
    QTimer *auto_scroll_timer_;
    QListWidget *chapter_list_;
    QPlainTextEdit *text_display_;
    bool auto_scroll_ = false;
    int speed_ = 80;
    QAction *add_speed_;
    QAction *del_speed_;
    QAction *add_font_action_;
    QAction *del_font_action_;
    QSplitter *splitter_;
    QAction *scroll_action_;
    QScrollBar *scroll_bar_;
    QAction *open_file_action_;
    QAction *color_action_;
    QToolBar *main_tool_bar_;
    QAction *toggle_list_action_;
    novel_manager *novel_manager_;
    QList<int> loaded_chapter_indices_;
    bool is_updating_content_;
};

#endif    // MAIN_WINDOW_H
