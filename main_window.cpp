#include "main_window.h"
#include "novel_manager.h"
#include "splitter.h"
#include <QDebug>
#include <QTextDocumentFragment>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenuBar>
#include <QPainter>
#include <QScrollBar>
#include <QListWidget>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolBar>
#include <algorithm>

static const int kChaptersInView = 5;
static const int kScrollLoadThreshold = 500;

main_window::main_window(QWidget* parent) : QMainWindow(parent), novel_manager_(new novel_manager())
{
    setup_ui();
    setup_connections();
    update_font_size(24);
    hue_ = 180;
    background_animation_timer_ = new QTimer(this);
    connect(background_animation_timer_, &QTimer::timeout, this, &main_window::update_background_gradient);
    // on_color_action();
    this->setStyleSheet(
        "QSplitter, QPlainTextEdit, QListWidget, QToolBar, QStatusBar, QTextBrowser { background-color: transparent; border: none; }");
}

main_window::~main_window() { delete novel_manager_; }

void main_window::setup_ui()
{
    main_tool_bar_ = new QToolBar("主工具栏", this);
    main_tool_bar_->setMovable(false);
    addToolBar(main_tool_bar_);

    open_file_action_ = new QAction("打开", this);
    main_tool_bar_->addAction(open_file_action_);
    color_action_ = new QAction("换色", this);
    main_tool_bar_->addAction(color_action_);

    toggle_list_action_ = new QAction("目录", this);
    toggle_list_action_->setStatusTip("显示/隐藏章节列表");
    main_tool_bar_->addAction(toggle_list_action_);

    add_font_action_ = new QAction("+", this);
    del_font_action_ = new QAction("-", this);
    main_tool_bar_->addAction(add_font_action_);
    main_tool_bar_->addAction(del_font_action_);

    scroll_action_ = new QAction("自动阅读", this);
    main_tool_bar_->addAction(scroll_action_);

    add_speed_ = new QAction("++", this);
    del_speed_ = new QAction("--", this);

    splitter_ = new AnimatedSplitter(Qt::Horizontal);
    auto_scroll_timer_ = new QTimer(this);

    setCentralWidget(splitter_);

    chapter_list_ = new QListWidget(splitter_);
    chapter_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chapter_list_->setStyleSheet(R"(
    QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }
    QScrollBar::handle:vertical { background: rgba(120,120,120,120); border-radius: 7px; min-height: 60px; }
    QScrollBar::handle:vertical:hover { background: rgba(80,80,80,180); }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )");

    text_display_ = new QTextBrowser(splitter_);
    text_display_->setReadOnly(true);
    text_display_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text_display_->verticalScrollBar()->setVisible(false);
    text_display_->horizontalScrollBar()->setVisible(false);

    scroll_bar_ = text_display_->verticalScrollBar();
    splitter_->addWidget(chapter_list_);
    splitter_->addWidget(text_display_);
    splitter_->setSizes({250, 800});

    statusBar()->setSizeGripEnabled(false);
    setWindowTitle("TXT 小说阅读器");
    resize(1024, 768);
}

void main_window::setup_connections()
{
    connect(color_action_, &QAction::triggered, this, &main_window::on_color_action);
    connect(add_speed_, &QAction::triggered, this, &main_window::increase_auto_speed);
    connect(del_speed_, &QAction::triggered, this, &main_window::decrease_auto_speed);
    connect(add_font_action_, &QAction::triggered, this, &main_window::increase_font_size);
    connect(del_font_action_, &QAction::triggered, this, &main_window::decrease_font_size);
    connect(auto_scroll_timer_, &QTimer::timeout, this, &main_window::perform_auto_scroll);
    connect(open_file_action_, &QAction::triggered, this, &main_window::open_file_dialog);
    connect(scroll_action_, &QAction::triggered, this, &main_window::auto_scroll_click);
    connect(toggle_list_action_, &QAction::triggered, this, &main_window::toggle_chapter_list_visibility);
    // 【修改】连接到新的滚动处理槽
    connect(scroll_bar_, &QScrollBar::valueChanged, this, &main_window::on_scroll_value_changed);
    connect(chapter_list_, &QListWidget::itemClicked, this, &main_window::on_chapter_list_item_clicked);
    connect(novel_manager_, &novel_manager::chapter_found, this, &main_window::on_chapter_found, Qt::QueuedConnection);
    connect(novel_manager_, &novel_manager::parsing_finished, this, &main_window::on_parsing_finished, Qt::QueuedConnection);
}

void main_window::open_file_dialog()
{
    QString file_path = QFileDialog::getOpenFileName(this, "选择小说文件", "", "Text Files (*.txt)");
    if (!file_path.isEmpty())
    {
        setWindowTitle(QFileInfo(file_path).fileName() + " - TXT 小说阅读器");
        chapter_list_->clear();
        text_display_->clear();
        displayed_chapter_indices_.clear();
        statusBar()->showMessage("正在解析章节，请稍候...");
        novel_manager_->load_file(file_path);
    }
}

void main_window::on_chapter_found(const QString& title) { chapter_list_->addItem(title); }

void main_window::on_parsing_finished(size_t total_chapters)
{
    statusBar()->showMessage(QString("解析完成，共 %1 章。").arg(total_chapters), 5000);
    if (total_chapters > 0)
    {
        load_chapters_around(0);
    }
}

void main_window::on_chapter_list_item_clicked(QListWidgetItem* item)
{
    if ((item == nullptr) || is_loading_content_)
    {
        return;
    }
    int row = chapter_list_->row(item);
    load_chapters_around(row);
}

void main_window::on_scroll_value_changed(int value)
{
    if (is_loading_content_ || displayed_chapter_indices_.isEmpty())
    {
        return;
    }

    int max_value = scroll_bar_->maximum();

    if (max_value > 0 && value >= max_value - kScrollLoadThreshold)
    {
        append_next_chapter();
    }
    else if (value <= kScrollLoadThreshold)
    {
        rebuild_document_for_prepend();
    }

    update_progress_status();
}

void main_window::load_chapters_around(int center_index)
{
    if (is_loading_content_ || novel_manager_->get_total_chapters() == 0)
    {
        return;
    }

    is_loading_content_ = true;
    statusBar()->showMessage("正在加载章节...", 1000);

    text_display_->clear();
    displayed_chapter_indices_.clear();

    size_t start_index = std::max<int>(0, center_index - 1);
    size_t end_index = std::min<size_t>(novel_manager_->get_total_chapters() - 1, center_index + 1);

    QString full_content;
    for (size_t i = start_index; i <= end_index; ++i)
    {
        QString content = novel_manager_->get_chapter_content(static_cast<int>(i));
        full_content.append(QString("<a name='ch_%1'></a>").arg(i));
        full_content.append(content.toHtmlEscaped().replace("\n", "<br>"));
        displayed_chapter_indices_.append(static_cast<int>(i));
    }

    text_display_->setHtml(full_content);

    text_display_->scrollToAnchor(QString("ch_%1").arg(center_index));

    is_loading_content_ = false;
}

void main_window::append_next_chapter()
{
    if (displayed_chapter_indices_.isEmpty())
    {
        return;
    }

    size_t last_loaded_index = displayed_chapter_indices_.last();
    if (last_loaded_index >= novel_manager_->get_total_chapters() - 1)
    {
        return;
    }

    is_loading_content_ = true;

    int next_index = static_cast<int>(last_loaded_index + 1);
    QString content = novel_manager_->get_chapter_content(next_index);
    QString html_content = QString("<a name='ch_%1'></a>").arg(next_index) + content.toHtmlEscaped().replace("\n", "<br>");

    text_display_->append(html_content);
    displayed_chapter_indices_.append(next_index);

    if (displayed_chapter_indices_.size() > kChaptersInView)
    {
        displayed_chapter_indices_.removeFirst();
        int first_chapter_after_trim = displayed_chapter_indices_.first();

        QString rebuilt_content;
        for (int index : displayed_chapter_indices_)
        {
            rebuilt_content.append(QString("<a name='ch_%1'></a>").arg(index));
            rebuilt_content.append(novel_manager_->get_chapter_content(index).toHtmlEscaped().replace("\n", "<br>"));
        }
        text_display_->setHtml(rebuilt_content);

        text_display_->scrollToAnchor(QString("ch_%1").arg(first_chapter_after_trim));
    }

    is_loading_content_ = false;
}

void main_window::rebuild_document_for_prepend()
{
    if (displayed_chapter_indices_.isEmpty())
    {
        return;
    }

    int first_loaded_index = displayed_chapter_indices_.first();
    if (first_loaded_index <= 0)
    {
        return;
    }

    is_loading_content_ = true;

    QTextCursor cursor = text_display_->cursorForPosition(QPoint(10, 10));
    QTextBlock top_block = cursor.block();
    QString anchor_to_restore = "";

    while (top_block.isValid())
    {
        QTextCursor block_cursor(top_block);
        block_cursor.select(QTextCursor::BlockUnderCursor);
        QString blockHtml = block_cursor.selection().toHtml();
        auto anchor_pos = blockHtml.indexOf("<a name=\"ch_");

        if (anchor_pos != -1)
        {
            auto start = anchor_pos + 13;
            auto end = blockHtml.indexOf('\"', start);
            if (end != -1)
            {
                anchor_to_restore = blockHtml.mid(start, end - start);
                break;
            }
        }
        top_block = top_block.previous();
    }

    if (anchor_to_restore.isEmpty())
    {
        // 作为备用方案，如果找不到精确的锚点，就恢复到旧窗口的第一个章节
        anchor_to_restore = QString("ch_%1").arg(first_loaded_index);
    }

    int new_first_index = first_loaded_index - 1;
    displayed_chapter_indices_.prepend(new_first_index);

    if (displayed_chapter_indices_.size() > kChaptersInView)
    {
        displayed_chapter_indices_.removeLast();
    }

    QString rebuilt_content;
    for (int index : displayed_chapter_indices_)
    {
        rebuilt_content.append(QString("<a name='ch_%1'></a>").arg(index));
        rebuilt_content.append(novel_manager_->get_chapter_content(index).toHtmlEscaped().replace("\n", "<br>"));
    }

    scroll_bar_->blockSignals(true);
    text_display_->setHtml(rebuilt_content);

    text_display_->scrollToAnchor(anchor_to_restore);
    scroll_bar_->blockSignals(false);

    is_loading_content_ = false;
}

void main_window::update_progress_status()
{
    if (displayed_chapter_indices_.isEmpty())
    {
        statusBar()->showMessage("进度: 0%");
        return;
    }
    auto total = novel_manager_->get_total_chapters();
    if (total == 0)
    {
        return;
    }

    int current_visible_chapter = displayed_chapter_indices_.first();
    double progress_ratio = static_cast<double>(current_visible_chapter) / static_cast<double>(total);
    int percentage = qBound(0, static_cast<int>(progress_ratio * 100), 100);

    statusBar()->showMessage(QString("进度: %1% (第 %2/%3 章)").arg(percentage).arg(current_visible_chapter + 1).arg(total));
}

void main_window::reset_auto_scroll_speed()
{
    auto_scroll_timer_->stop();
    speed_ = std::max(speed_, 10);
    qDebug() << "speed " << speed_;
    auto_scroll_timer_->setInterval(100 - speed_);
    auto_scroll_timer_->start();
}
void main_window::increase_auto_speed()
{
    speed_ = std::min(95, speed_ + 5);
    reset_auto_scroll_speed();
}
void main_window::decrease_auto_speed()
{
    speed_ = std::max(5, speed_ - 5);
    reset_auto_scroll_speed();
}
void main_window::update_font_size(int new_size)
{
    new_size = std::max(new_size, 8);
    new_size = std::min(new_size, 72);
    QFont current_font = text_display_->font();
    current_font.setPointSize(new_size);
    text_display_->setFont(current_font);
}
void main_window::increase_font_size()
{
    QFont current_font = text_display_->font();
    update_font_size(current_font.pointSize() + 2);
}
void main_window::decrease_font_size()
{
    QFont current_font = text_display_->font();
    update_font_size(current_font.pointSize() - 2);
}
void main_window::perform_auto_scroll()
{
    if (scroll_bar_->value() >= scroll_bar_->maximum())
    {
        auto_scroll_timer_->stop();
        statusBar()->showMessage("已滚动到底部，自动停止", 3000);
    }
    else
    {
        scroll_bar_->setValue(scroll_bar_->value() + 1);
    }
}
void main_window::auto_scroll_click()
{
    auto_scroll_ = !auto_scroll_;
    if (auto_scroll_)
    {
        main_tool_bar_->addAction(add_speed_);
        main_tool_bar_->addAction(del_speed_);
        scroll_action_->setText("关闭自动阅读");
        reset_auto_scroll_speed();
    }
    else
    {
        main_tool_bar_->removeAction(add_speed_);
        main_tool_bar_->removeAction(del_speed_);
        scroll_action_->setText("开启自动阅读");
        auto_scroll_timer_->stop();
    }
}
void main_window::toggle_chapter_list_visibility() { chapter_list_->setVisible(!chapter_list_->isVisible()); }
void main_window::on_color_action()
{
    is_dynamic_background_ = !is_dynamic_background_;
    if (is_dynamic_background_)
    {
        background_animation_timer_->start(50);
    }
    else
    {
        background_animation_timer_->stop();
    }
    update();
}
void main_window::update_background_gradient()
{
    hue_ = (hue_ + 1) % 360;
    update();
}
void main_window::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    if (is_dynamic_background_)
    {
        QColor color1 = QColor::fromHsv(hue_, 150, 235);
        QColor color2 = QColor::fromHsv((hue_ + 90) % 360, 150, 235);

        QLinearGradient gradient(0, 0, 0, height());
        gradient.setColorAt(0.0, color1);
        gradient.setColorAt(1.0, color2);
        painter.fillRect(rect(), gradient);
    }
    else
    {
        painter.fillRect(rect(), QColor("#FDF6E3"));
    }
}
