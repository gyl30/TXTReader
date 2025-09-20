#include <QToolBar>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextBlock>
#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#include "main_window.h"
#include "novel_manager.h"

static const int kViewportSafeMargin = 5;
main_window::main_window(QWidget *parent) : QMainWindow(parent), novel_manager_(new novel_manager()), is_updating_content_(false)
{
    setup_ui();
    setup_connections();

    text_display_->setStyleSheet(
        "background-color: #FDF6E3;"
        "color: #586E75;"
        "font-family: 'Microsoft YaHei', 'SimSun';"
        "font-size: 14pt;"
        "border: none;"
        "padding: 10px;");
}

main_window::~main_window() { delete novel_manager_; }

void main_window::setup_ui()
{
    main_tool_bar_ = new QToolBar("主工具栏", this);
    addToolBar(main_tool_bar_);

    open_file_action_ = new QAction("打开", this);
    main_tool_bar_->addAction(open_file_action_);

    toggle_list_action_ = new QAction("目录", this);
    toggle_list_action_->setStatusTip("显示/隐藏章节列表");
    main_tool_bar_->addAction(toggle_list_action_);

    scroll_action_ = new QAction("自动阅读", this);
    main_tool_bar_->addAction(scroll_action_);

    add_speed_ = new QAction("++", this);
    del_speed_ = new QAction("--", this);
    splitter_ = new QSplitter(Qt::Horizontal, this);
    auto_scroll_timer_ = new QTimer(this);

    setCentralWidget(splitter_);

    chapter_list_ = new QListWidget(splitter_);
    text_display_ = new QPlainTextEdit(splitter_);
    text_display_->setReadOnly(true);
    scroll_bar_ = text_display_->verticalScrollBar();
    text_display_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_display_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    splitter_->addWidget(chapter_list_);
    splitter_->addWidget(text_display_);
    splitter_->setSizes({250, 800});

    setWindowTitle("TXT 小说阅读器");
    resize(1024, 768);
}

void main_window::reset_auto_scroll_speed()
{
    auto_scroll_timer_->stop();
    speed_ = std::max(speed_, 30);
    qDebug() << "speed " << speed_ * 15;
    auto_scroll_timer_->setInterval(speed_ * 15);
    auto_scroll_timer_->start();
}

void main_window::setup_connections()
{
    connect(add_speed_,
            &QAction::triggered,
            [this]()
            {
                speed_++;
                reset_auto_scroll_speed();
            });
    connect(del_speed_,
            &QAction::triggered,
            [this]()
            {
                speed_--;
                reset_auto_scroll_speed();
            });

    connect(auto_scroll_timer_, &QTimer::timeout, this, &main_window::perform_auto_scroll);
    connect(open_file_action_, &QAction::triggered, this, &main_window::open_file_dialog);
    connect(scroll_action_, &QAction::triggered, this, &main_window::auto_scroll_click);
    connect(toggle_list_action_, &QAction::triggered, this, &main_window::toggle_chapter_list_visibility);
    connect(text_display_->verticalScrollBar(), &QScrollBar::valueChanged, this, &main_window::update_state_on_scroll);
    connect(chapter_list_, &QListWidget::itemClicked, this, &main_window::on_chapter_list_item_clicked);
    connect(novel_manager_, &novel_manager::chapter_found, this, &main_window::on_chapter_found, Qt::QueuedConnection);
    connect(novel_manager_, &novel_manager::parsing_finished, this, &main_window::on_parsing_finished, Qt::QueuedConnection);
}

void main_window::toggle_auto_scroll()
{
    if (auto_scroll_timer_->isActive())
    {
        auto_scroll_timer_->stop();
        statusBar()->showMessage("自动滚动已停止", 2000);
    }
    else
    {
        statusBar()->showMessage("自动滚动已开始", 2000);
    }
}

void main_window::perform_auto_scroll()
{
    QScrollBar *scroll_bar = text_display_->verticalScrollBar();
    if (scroll_bar->value() >= scroll_bar->maximum())
    {
        auto_scroll_timer_->stop();
        statusBar()->showMessage("已滚动到底部，自动停止", 3000);
    }
    else
    {
        scroll_bar->setValue(scroll_bar->value() + 1);
    }
}

void main_window::auto_scroll_click()
{
    if (auto_scroll_)
    {
        main_tool_bar_->removeAction(del_speed_);
        main_tool_bar_->removeAction(add_speed_);
        scroll_action_->setText("开启自动阅读");
        auto_scroll_timer_->stop();
    }
    else
    {
        main_tool_bar_->addAction(add_speed_);
        main_tool_bar_->addAction(del_speed_);
        scroll_action_->setText("关闭自动阅读");
        reset_auto_scroll_speed();
    }
    auto_scroll_ = !auto_scroll_;
}

void main_window::toggle_chapter_list_visibility()
{
    if (chapter_list_->isVisible())
    {
        chapter_list_->hide();
    }
    else
    {
        chapter_list_->show();
    }
}

void main_window::open_file_dialog()
{
    QString file_path = QFileDialog::getOpenFileName(this, "选择小说文件", "", "Text Files (*.txt)");
    if (!file_path.isEmpty())
    {
        setWindowTitle(QFileInfo(file_path).fileName() + " - TXT 小说阅读器");
        chapter_list_->clear();
        text_display_->clear();
        loaded_chapter_indices_.clear();
        statusBar()->showMessage("正在解析章节，请稍候...");
        novel_manager_->load_file(file_path);
    }
}

void main_window::on_chapter_found(const QString &title) { chapter_list_->addItem(title); }

void main_window::on_parsing_finished(int total_chapters)
{
    statusBar()->showMessage(QString("解析完成，共 %1 章。").arg(total_chapters), 5000);
    if (total_chapters > 0)
    {
        load_chapter_to_display(0, false, false);
    }
}

void main_window::on_chapter_list_item_clicked(QListWidgetItem *item)
{
    if ((item == nullptr) || is_updating_content_)
    {
        return;
    }
    auto row = chapter_list_->row(item);
    if (row == 0)
    {
        load_chapter_to_display(row, false, false);
        return;
    }
    qDebug() << "chapter_list click";
    load_chapter_to_display(row - 1, false, false);
    load_chapter_to_display(row, true, true);
}

void main_window::update_progress_status()
{
    int total_blocks = text_display_->document()->blockCount();
    if (total_blocks <= 1)
    {
        statusBar()->showMessage("进度: 0%");
        return;
    }
    QPoint center_point = text_display_->viewport()->rect().center();
    QTextCursor center_cursor = text_display_->cursorForPosition(center_point);

    int current_block = center_cursor.blockNumber();

    double progress_ratio = static_cast<double>(current_block) / (total_blocks - 1);
    int percentage = qBound(0, static_cast<int>(progress_ratio * 100), 100);

    statusBar()->showMessage(QString("进度: %1%").arg(percentage));

    const QRect viewport_rect = text_display_->viewport()->rect();

    QPoint top_point = viewport_rect.topLeft() + QPoint(kViewportSafeMargin, kViewportSafeMargin);
    QPoint bottom_point = viewport_rect.bottomLeft() + QPoint(kViewportSafeMargin, -kViewportSafeMargin);

    QTextCursor top_cursor = text_display_->cursorForPosition(top_point);
    int top_block = top_cursor.blockNumber();

    QTextCursor bottom_cursor = text_display_->cursorForPosition(bottom_point);
    int bottom_block = bottom_cursor.blockNumber();

    qDebug() << "1top_block " << top_block << " bottom_block " << bottom_block << " current_block " << current_block << " total_blocks "
             << total_blocks;
    // load next
    if (bottom_block > total_blocks - 100)
    {
        int last_index = loaded_chapter_indices_.last();
        if (last_index + 1 >= chapter_list_->count())
        {
            return;
        }
        QTextCursor cursor_before_load = text_display_->cursorForPosition(QPoint(10, 10));
        load_chapter_to_display(last_index + 1, true, false);
        QTextBlock block_to_restore = text_display_->document()->findBlockByNumber(cursor_before_load.blockNumber());
        if (block_to_restore.isValid())
        {
            QTextCursor restore_cursor(block_to_restore);
            text_display_->setTextCursor(restore_cursor);
        }
    }
    // load prev
    if (top_block < 100)
    {
        int first_index = loaded_chapter_indices_.first();
        if (first_index <= 0)
        {
            return;
        }
        int old_scroll_value = scroll_bar_->value();
        int old_max_value = scroll_bar_->maximum();
        insert_chapter_to_display(first_index - 1);
        int new_max_value = scroll_bar_->maximum();
        scroll_bar_->setValue(old_scroll_value + (new_max_value - old_max_value));
    }
}

void main_window::load_chapter_to_display(int load_index, bool append, bool fouce)
{
    if (is_updating_content_)
    {
        return;
    }
    if (load_index >= chapter_list_->count())
    {
        return;
    }
    is_updating_content_ = true;

    statusBar()->showMessage("正在加载章节...", 2000);

    QString title = chapter_list_->item(load_index)->text();
    QString content = novel_manager_->get_chapter_content(load_index);

    qDebug() << "1load " << title << (append ? " append " : " insert ") << (fouce ? " fouce " : " no fouce");
    if (append)
    {
        text_display_->appendPlainText(content);
    }
    else
    {
        text_display_->clear();
        loaded_chapter_indices_.clear();
        text_display_->setPlainText(content);
    }
    if (fouce)
    {
        auto find = text_display_->find(title);
        assert(find);
        if (find)
        {
            text_display_->centerCursor();
            QTextCursor cursor = text_display_->textCursor();
            cursor.clearSelection();
            text_display_->setTextCursor(cursor);
        }
    }
    loaded_chapter_indices_.append(load_index);
    is_updating_content_ = false;
}

void main_window::insert_chapter_to_display(int load_index)
{
    QString title = chapter_list_->item(load_index)->text();
    qDebug() << "2load " << title << " insert " << "no fouce";
    QString content = novel_manager_->get_chapter_content(load_index);
    QTextCursor cursor(text_display_->document());
    cursor.movePosition(QTextCursor::Start);
    cursor.insertText(content);
    loaded_chapter_indices_.prepend(load_index);
}

void main_window::update_state_on_scroll() { update_progress_status(); }
