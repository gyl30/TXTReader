#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QThread>
#include "log.h"
#include "splitter.h"
#include "novel_view.h"
#include "main_window.h"
#include "novel_manager.h"

main_window::main_window(QWidget* parent) : QMainWindow(parent)
{
    worker_thread_ = new QThread(this);
    novel_manager_ = new novel_manager();
    novel_manager_->moveToThread(worker_thread_);
    setup_ui();
    setup_connections();
    worker_thread_->start();

    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
    setStyleSheet("QSplitter, QListWidget, QToolBar, QStatusBar, QAbstractScrollArea { background-color: transparent; border: none; }");
    setWindowTitle("TXT 小说阅读器");
    resize(1024, 768);
}

main_window::~main_window()
{
    worker_thread_->quit();
    worker_thread_->wait();
}

void main_window::setup_ui()
{
    splitter_ = new AnimatedSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter_);

    chapter_list_ = new QListWidget(splitter_);
    chapter_list_->setFixedWidth(250);
    chapter_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chapter_list_->setStyleSheet(R"(
    QScrollBar:vertical { background: transparent; width: 10px; margin: 0px; }
    QScrollBar::handle:vertical { background: rgba(120,120,120,120); border-radius: 7px; min-height: 60px; }
    QScrollBar::handle:vertical:hover { background: rgba(80,80,80,180); }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )");
    novel_view_ = new NovelView(splitter_);
    novel_view_->setFrameShape(QFrame::NoFrame);
    splitter_->addWidget(chapter_list_);
    splitter_->addWidget(novel_view_);

    main_tool_bar_ = addToolBar("Main");
    main_tool_bar_->setMovable(false);
    open_file_action_ = main_tool_bar_->addAction("打开");
    toggle_list_action_ = main_tool_bar_->addAction("目录");
    add_font_action_ = main_tool_bar_->addAction("字体+");
    del_font_action_ = main_tool_bar_->addAction("字体-");
    scroll_action_ = main_tool_bar_->addAction("自动滚动");
    add_speed_ = main_tool_bar_->addAction("加速");
    del_speed_ = main_tool_bar_->addAction("减速");
    add_line_spacing_action_ = main_tool_bar_->addAction("行距+");
    del_line_spacing_action_ = main_tool_bar_->addAction("行距-");
    add_letter_spacing_action_ = main_tool_bar_->addAction("字距+");
    del_letter_spacing_action_ = main_tool_bar_->addAction("字距-");
    color_action_ = main_tool_bar_->addAction("动态背景");

    auto_scroll_timer_ = new QTimer(this);
    background_animation_timer_ = new QTimer(this);
    hue_ = 0;

    statusBar()->showMessage("就绪");
    statusBar()->setSizeGripEnabled(false);
}

void main_window::setup_connections()
{
    connect(this, &main_window::request_load_file, novel_manager_, &novel_manager::load_file);
    connect(this, &main_window::request_chapter_content, novel_manager_, &novel_manager::fetch_chapter_content);

    connect(novel_manager_, &novel_manager::chapter_found, this, &main_window::on_chapter_found);
    connect(novel_manager_, &novel_manager::parsing_finished, this, &main_window::on_parsing_finished);
    connect(novel_manager_, &novel_manager::chapter_content_ready, this, &main_window::on_chapter_content_ready);

    connect(worker_thread_, &QThread::finished, novel_manager_, &QObject::deleteLater);

    connect(open_file_action_, &QAction::triggered, this, &main_window::open_file_dialog);
    connect(toggle_list_action_, &QAction::triggered, this, &main_window::toggle_chapter_list_visibility);
    connect(chapter_list_, &QListWidget::itemClicked, this, &main_window::on_chapter_list_item_clicked);

    connect(novel_view_, &NovelView::needPreviousChapter, this, &main_window::load_previous_chapter);
    connect(novel_view_, &NovelView::needNextChapter, this, &main_window::load_next_chapter);

    connect(novel_view_->verticalScrollBar(), &QScrollBar::valueChanged, this, &main_window::update_progress_status);
    connect(scroll_action_, &QAction::triggered, this, &main_window::auto_scroll_click);
    connect(auto_scroll_timer_, &QTimer::timeout, this, &main_window::perform_auto_scroll);
    connect(add_speed_, &QAction::triggered, this, &main_window::increase_auto_speed);
    connect(del_speed_, &QAction::triggered, this, &main_window::decrease_auto_speed);
    connect(add_font_action_, &QAction::triggered, this, &main_window::increase_font_size);
    connect(del_font_action_, &QAction::triggered, this, &main_window::decrease_font_size);
    connect(add_line_spacing_action_, &QAction::triggered, this, &main_window::increase_line_spacing);
    connect(del_line_spacing_action_, &QAction::triggered, this, &main_window::decrease_line_spacing);
    connect(add_letter_spacing_action_, &QAction::triggered, this, &main_window::increase_letter_spacing);
    connect(del_letter_spacing_action_, &QAction::triggered, this, &main_window::decrease_letter_spacing);
    connect(background_animation_timer_, &QTimer::timeout, this, &main_window::update_background_gradient);
    connect(color_action_, &QAction::triggered, this, &main_window::on_color_action);
}

void main_window::paintEvent(QPaintEvent* event)
{
    if (is_dynamic_background_)
    {
        QPainter painter(this);
        QRect r = rect();

        QColor color1 = QColor::fromHsv(hue_ % 360, 80, 235);
        QColor color2 = QColor::fromHsv((hue_ + 40) % 360, 80, 235);
        QColor color3 = QColor::fromHsv((hue_ + 80) % 360, 80, 235);
        QLinearGradient gradient(0, -gradient_offset_, 0, r.height() - gradient_offset_);

        gradient.setColorAt(0.0, color1);
        gradient.setColorAt(0.5, color2);
        gradient.setColorAt(1.0, color3);
        painter.fillRect(r, gradient);
    }
    else
    {
        QMainWindow::paintEvent(event);
    }
}

void main_window::open_file_dialog()
{
    QString file_path = QFileDialog::getOpenFileName(this, "打开小说", "", "Text Files (*.txt)");
    if (!file_path.isEmpty())
    {
        chapter_list_->clear();
        novel_view_->clearContent();
        statusBar()->showMessage("正在解析章节...");
        emit request_load_file(file_path);
    }
}

void main_window::toggle_chapter_list_visibility() { chapter_list_->setVisible(!chapter_list_->isVisible()); }

void main_window::on_chapter_list_item_clicked(QListWidgetItem* item)
{
    int index = chapter_list_->row(item);
    if (index >= 0)
    {
        load_chapter(index);
    }
}

void main_window::on_chapter_found(const QString& title) { chapter_list_->addItem(title); }

void main_window::on_parsing_finished(size_t total_chapters)
{
    total_chapters_ = total_chapters;
    statusBar()->showMessage(QString("找到 %1 个章节。").arg(total_chapters_));
    if (total_chapters_ > 0)
    {
        load_chapter(0);
    }
}

void main_window::on_chapter_content_ready(int chapter_index, const QString& content)
{
    if (content.isEmpty())
    {
        is_loading_content_ = false;
        return;
    }

    if (chapter_index == initial_chapter_to_load_)
    {
        novel_view_->appendChapterContent(chapter_index, content);

        if (chapter_index + 1 < total_chapters_)
        {
            emit request_chapter_content(chapter_index + 1);
        }
        initial_chapter_to_load_ = -1;
    }
    else if (chapter_index < novel_view_->getFirstDisplayedChapterIndex())
    {
        novel_view_->prependChapterContent(chapter_index, content);
    }
    else
    {
        novel_view_->appendChapterContent(chapter_index, content);
    }

    is_loading_content_ = false;    // 内容加载并显示完成后，重置标志
}

void main_window::load_previous_chapter()
{
    if (is_loading_content_)
    {
        return;
    }
    int first_index = novel_view_->getFirstDisplayedChapterIndex();
    if (first_index <= 0)
    {
        return;
    }

    int prev_index = first_index - 1;
    if (novel_view_->isChapterDisplayed(prev_index))
    {
        return;
    }

    is_loading_content_ = true;
    emit request_chapter_content(prev_index);
}

void main_window::load_next_chapter()
{
    if (is_loading_content_)
    {
        return;
    }
    int last_index = novel_view_->getLastDisplayedChapterIndex();
    if (static_cast<size_t>(last_index) >= total_chapters_ - 1)
    {
        return;
    }

    int next_index = last_index + 1;
    if (novel_view_->isChapterDisplayed(next_index))
    {
        return;
    }

    is_loading_content_ = true;
    emit request_chapter_content(next_index);
}

void main_window::load_chapter(int chapter_index)
{
    if (chapter_index < 0 || static_cast<size_t>(chapter_index) >= total_chapters_)
    {
        return;
    }

    is_loading_content_ = true;
    novel_view_->clearContent();

    initial_chapter_to_load_ = chapter_index;
    emit request_chapter_content(chapter_index);
}

void main_window::update_progress_status()
{
    QScrollBar* scrollBar = novel_view_->verticalScrollBar();
    if (scrollBar->maximum() > 0)
    {
        double progress = static_cast<double>(scrollBar->value()) / scrollBar->maximum() * 100.0;
        statusBar()->showMessage(QString("进度: %1%").arg(progress, 0, 'f', 2));
    }
}
void main_window::increase_font_size()
{
    font_size_ += 2.0;
    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
}
void main_window::decrease_font_size()
{
    font_size_ -= 2.0;
    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
}
void main_window::increase_line_spacing()
{
    line_spacing_ += 0.1;
    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
}
void main_window::decrease_line_spacing()
{
    line_spacing_ = qMax(0.5, line_spacing_ - 0.1);
    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
}
void main_window::increase_letter_spacing()
{
    letter_spacing_ += 0.5;
    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
}
void main_window::decrease_letter_spacing()
{
    letter_spacing_ = qMax(0.0, letter_spacing_ - 0.5);
    novel_view_->setFontStyle(font_size_, line_spacing_, letter_spacing_);
}
void main_window::perform_auto_scroll()
{
    QScrollBar* scrollBar = novel_view_->verticalScrollBar();
    scrollBar->setValue(scrollBar->value() + 1);
}
void main_window::auto_scroll_click()
{
    auto_scroll_ = !auto_scroll_;
    if (auto_scroll_)
    {
        auto_scroll_timer_->start(speed_);
        scroll_action_->setText("停止滚动");
    }
    else
    {
        auto_scroll_timer_->stop();
        scroll_action_->setText("自动滚动");
    }
}
void main_window::increase_auto_speed()
{
    auto speed = speed_;
    speed_ = qMax(3, speed_ - 1);
    LOG_INFO("speed from {} to {}", speed, speed_);
    reset_auto_scroll_speed();
}
void main_window::decrease_auto_speed()
{
    auto speed = speed_;
    speed_ += 1;
    LOG_INFO("speed from {} to {}", speed, speed_);
    reset_auto_scroll_speed();
}
void main_window::reset_auto_scroll_speed()
{
    if (auto_scroll_timer_->isActive())
    {
        auto_scroll_timer_->start(speed_);
    }
}
void main_window::update_background_gradient()
{
    gradient_offset_ = (gradient_offset_ + 1) % height();
    static double hue_f = hue_;
    hue_f += 0.25;
    if (hue_f >= 360.0)
    {
        hue_f -= 360.0;
    }
    hue_ = static_cast<int>(hue_f);
    update();
}
void main_window::on_color_action()
{
    is_dynamic_background_ = !is_dynamic_background_;
    if (is_dynamic_background_)
    {
        background_animation_timer_->start(30);
    }
    else
    {
        background_animation_timer_->stop();
        update();
    }
}
