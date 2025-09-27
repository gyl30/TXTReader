#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenuBar>
#include <QFontDialog>
#include <QFontDatabase>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QElapsedTimer>
#include <QToolBar>
#include <QThread>
#include "log.h"
#include "splitter.h"
#include "novel_view.h"
#include "main_window.h"
#include "novel_manager.h"

static QColor interpolate_color(const QColor& c1, const QColor& c2, qreal progress)
{
    qreal r = c1.redF() + ((c2.redF() - c1.redF()) * progress);
    qreal g = c1.greenF() + ((c2.greenF() - c1.greenF()) * progress);
    qreal b = c1.blueF() + ((c2.blueF() - c1.blueF()) * progress);
    return QColor::fromRgbF(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b));
}

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
main_window::main_window(QWidget* parent) : QMainWindow(parent)
{
    worker_thread_ = new QThread(this);
    novel_manager_ = new novel_manager();
    novel_manager_->moveToThread(worker_thread_);
    view_font_ = default_font(38.0);
    LOG_INFO("default font {}", view_font_.toString().toStdString());
    setup_ui();
    setup_color_schemes();
    setup_connections();
    worker_thread_->start();
    novel_view_->set_font_style(view_font_, line_spacing_, letter_spacing_);
    apply_font_and_spacing();
    setStyleSheet("QSplitter, QListWidget, QToolBar, QStatusBar, QAbstractScrollArea { background-color: transparent; border: none; }");
    setWindowTitle("TXT 小说阅读器");
    resize(1024, 768);
}

main_window::~main_window()
{
    worker_thread_->quit();
    worker_thread_->wait();
    delete transition_start_time_;
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
    novel_view_ = new novel_view(splitter_);
    novel_view_->setFrameShape(QFrame::NoFrame);
    splitter_->addWidget(chapter_list_);
    splitter_->addWidget(novel_view_);

    main_tool_bar_ = addToolBar("Main");
    main_tool_bar_->setMovable(false);
    open_file_action_ = main_tool_bar_->addAction("打开");
    toggle_list_action_ = main_tool_bar_->addAction("目录");
    main_tool_bar_->addSeparator();
    select_font_action_ = main_tool_bar_->addAction("设置字体");
    add_font_action_ = main_tool_bar_->addAction("字体+");
    del_font_action_ = main_tool_bar_->addAction("字体-");
    add_line_spacing_action_ = main_tool_bar_->addAction("行距+");
    del_line_spacing_action_ = main_tool_bar_->addAction("行距-");
    add_letter_spacing_action_ = main_tool_bar_->addAction("字距+");
    del_letter_spacing_action_ = main_tool_bar_->addAction("字距-");
    main_tool_bar_->addSeparator();
    scroll_action_ = main_tool_bar_->addAction("自动滚动");
    add_speed_ = main_tool_bar_->addAction("加速");
    del_speed_ = main_tool_bar_->addAction("减速");
    main_tool_bar_->addSeparator();
    color_action_ = main_tool_bar_->addAction("开启动态背景");

    auto_scroll_timer_ = new QTimer(this);
    background_animation_timer_ = new QTimer(this);
    color_change_timer_ = new QTimer(this);
    transition_start_time_ = new QElapsedTimer();

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

    connect(novel_view_, &novel_view::need_previous_chapter, this, &main_window::load_previous_chapter);
    connect(novel_view_, &novel_view::need_next_chapter, this, &main_window::load_next_chapter);

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

    connect(select_font_action_, &QAction::triggered, this, &main_window::select_font_dialog);
    connect(background_animation_timer_, &QTimer::timeout, this, &main_window::update_background_gradient);
    connect(color_action_, &QAction::triggered, this, &main_window::on_color_action);
    connect(color_change_timer_, &QTimer::timeout, this, &main_window::change_to_next_color_scheme);
}

void main_window::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    if (is_dynamic_background_)
    {
        constexpr int transition_duration_ms = 5000;
        qreal progress = static_cast<qreal>(transition_start_time_->elapsed()) / transition_duration_ms;
        progress = qMin(progress, 1.0);

        QColor interpolated_color = interpolate_color(current_color_, target_color_, progress);

        painter.fillRect(rect(), interpolated_color);
    }
    else
    {
        painter.fillRect(rect(), QColor("#FDF6E3"));
    }
}

void main_window::open_file_dialog()
{
    QString file_path = QFileDialog::getOpenFileName(this, "打开小说", "", "Text Files (*.txt)");
    if (!file_path.isEmpty())
    {
        chapter_list_->clear();
        novel_view_->clear_content();
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
        novel_view_->append_chapter_content(chapter_index, content);
        if (chapter_index + 1 < total_chapters_)
        {
            emit request_chapter_content(chapter_index + 1);
        }
        initial_chapter_to_load_ = -1;
    }
    else if (chapter_index < novel_view_->first_displayed_chapter_index())
    {
        novel_view_->prepend_chapter_content(chapter_index, content);
    }
    else
    {
        novel_view_->append_chapter_content(chapter_index, content);
    }
    is_loading_content_ = false;
}
void main_window::load_previous_chapter()
{
    if (is_loading_content_)
    {
        return;
    }
    int first_index = novel_view_->first_displayed_chapter_index();
    if (first_index <= 0)
    {
        return;
    }
    int prev_index = first_index - 1;
    if (novel_view_->is_chapter_displayed(prev_index))
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
    int last_index = novel_view_->last_displayed_chapter_index();
    if (static_cast<size_t>(last_index) >= total_chapters_ - 1)
    {
        return;
    }
    int next_index = last_index + 1;
    if (novel_view_->is_chapter_displayed(next_index))
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
    novel_view_->clear_content();
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
void main_window::select_font_dialog()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, view_font_, this, "选择字体");
    if (ok)
    {
        LOG_INFO("update font from {} to {}", view_font_.toString().toStdString(), font.toString().toStdString());
        view_font_ = font;
        apply_font_and_spacing();
    }
}

void main_window::apply_font_and_spacing()
{
    QFont final_font = view_font_;
    final_font.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing_);
    novel_view_->set_font_style(final_font, line_spacing_, letter_spacing_);
}

void main_window::increase_font_size()
{
    view_font_.setPointSizeF(view_font_.pointSizeF() + 2.0);
    apply_font_and_spacing();
}

void main_window::decrease_font_size()
{
    qreal new_size = qMax(8.0, view_font_.pointSizeF() - 2.0);
    view_font_.setPointSizeF(new_size);
    apply_font_and_spacing();
}

void main_window::increase_line_spacing()
{
    line_spacing_ += 0.1;
    apply_font_and_spacing();
}

void main_window::decrease_line_spacing()
{
    line_spacing_ = qMax(0.5, line_spacing_ - 0.1);
    apply_font_and_spacing();
}

void main_window::increase_letter_spacing()
{
    letter_spacing_ += 0.5;
    apply_font_and_spacing();
}

void main_window::decrease_letter_spacing()
{
    letter_spacing_ = qMax(0.0, letter_spacing_ - 0.5);
    apply_font_and_spacing();
}
void main_window::perform_auto_scroll()
{
    QScrollBar* scrollBar = novel_view_->verticalScrollBar();
    int currentValue = scrollBar->value();

    if (currentValue >= scrollBar->maximum())
    {
        auto_scroll_click();
        return;
    }
    novel_view_->viewport()->scroll(0, 1);

    bool oldState = scrollBar->blockSignals(true);
    scrollBar->setValue(currentValue + 1);
    scrollBar->blockSignals(oldState);
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

void main_window::setup_color_schemes()
{
    color_schemes_.append(QColor("#E0E6F8"));
    color_schemes_.append(QColor("#D7EEF9"));
    color_schemes_.append(QColor("#D7F9E9"));
    color_schemes_.append(QColor("#E3F8F1"));
    color_schemes_.append(QColor("#F8F0D5"));
    color_schemes_.append(QColor("#F7F3E9"));
    color_schemes_.append(QColor("#F9DED7"));
    color_schemes_.append(QColor("#F8E5E0"));
    color_schemes_.append(QColor("#E2E8F0"));
    color_schemes_.append(QColor("#EBE8F9"));
    color_schemes_.append(QColor("#F5F5DC"));
    if (!color_schemes_.isEmpty())
    {
        current_color_ = color_schemes_.first();
        target_color_ = color_schemes_.first();
    }
}

void main_window::change_to_next_color_scheme()
{
    if (color_schemes_.size() < 2)
    {
        return;
    }

    current_color_ = target_color_;

    scheme_index_ = (scheme_index_ + 1) % static_cast<int>(color_schemes_.size());

    target_color_ = color_schemes_[scheme_index_];

    transition_start_time_->restart();
}

void main_window::update_background_gradient() { update(); }

void main_window::on_color_action()
{
    is_dynamic_background_ = !is_dynamic_background_;
    if (is_dynamic_background_ && !color_schemes_.isEmpty())
    {
        color_action_->setText("关闭动态背景");
        change_to_next_color_scheme();
        background_animation_timer_->start(1200);
        color_change_timer_->start(5000);
    }
    else
    {
        color_action_->setText("开启动态背景");
        background_animation_timer_->stop();
        color_change_timer_->stop();
        update();
    }
}
