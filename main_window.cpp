#include "main_window.h"
#include <QApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenuBar>
#include <QFontDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QElapsedTimer>
#include <QToolBar>
#include <QVariantList>
#include <QInputDialog>
#include <QKeySequence>
#include <QShortcut>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include "log.h"
#include "splitter.h"
#include "novel_view.h"

static const char* kChapterRegexShortcut = "Ctrl+R";

static QColor interpolate_color(const QColor& c1, const QColor& c2, qreal progress)
{
    qreal r = c1.redF() + ((c2.redF() - c1.redF()) * progress);
    qreal g = c1.greenF() + ((c2.greenF() - c1.greenF()) * progress);
    qreal b = c1.blueF() + ((c2.blueF() - c1.blueF()) * progress);
    return QColor::fromRgbF(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b));
}

main_window::main_window(app_state* app_state, settings_manager* settings_manager, QWidget* parent)
    : QMainWindow(parent), app_state_(app_state), settings_manager_(settings_manager)
{
    setAcceptDrops(true);
    setup_static_backgrounds();
    setup_ui();
    setup_color_schemes();
    setup_connections();
    setup_shortcuts();
    apply_font_and_spacing();
    setStyleSheet("QSplitter, QListWidget, QToolBar, QStatusBar, QAbstractScrollArea { background-color: transparent; border: none; }");
    setWindowTitle("TXT 小说阅读器");
    auto_save_timer_->start(8000);
    resize(1024, 768);
}

main_window::~main_window() { delete transition_start_time_; }

void main_window::setup_ui()
{
    tray_icon_ = new tray_icon(this);
    connect(tray_icon_, &tray_icon::show_hide_triggered, this, [this]() { isVisible() ? hide() : show(); });
    connect(tray_icon_, &tray_icon::quit_triggered, this, [this]() { emit application_quit_triggered(); });
    tray_icon_->show();

    splitter_ = new animated_splitter(Qt::Horizontal, this);
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
    recent_files_action_ = main_tool_bar_->addAction("最近");
    recent_files_menu_ = new QMenu(this);
    recent_files_action_->setMenu(recent_files_menu_);

    auto* recent_button = qobject_cast<QToolButton*>(main_tool_bar_->widgetForAction(recent_files_action_));
    if (recent_button != nullptr)
    {
        recent_button->setPopupMode(QToolButton::InstantPopup);
        recent_button->setStyleSheet("QToolButton::menu-indicator { image: none; }");
    }

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
    switch_background_action_ = main_tool_bar_->addAction("切换背景");

    main_tool_bar_->addSeparator();
    search_input_ = new QLineEdit(this);
    search_input_->setPlaceholderText("搜索...");
    search_input_->setMaximumWidth(150);
    main_tool_bar_->addWidget(search_input_);
    find_prev_action_ = main_tool_bar_->addAction("上一个");
    find_next_action_ = main_tool_bar_->addAction("下一个");

    auto_scroll_timer_ = new QTimer(this);
    auto_save_timer_ = new QTimer(this);
    background_animation_timer_ = new QTimer(this);
    color_change_timer_ = new QTimer(this);
    transition_start_time_ = new QElapsedTimer();

    status_chapter_label_ = new QLabel(" 就绪");
    status_progress_label_ = new QLabel("进度: --% ");
    status_search_label_ = new QLabel();
    statusBar()->addWidget(status_chapter_label_, 1);
    statusBar()->addPermanentWidget(status_search_label_);
    statusBar()->addPermanentWidget(status_progress_label_);

    statusBar()->setSizeGripEnabled(false);
}

void main_window::setup_connections()
{
    connect(open_file_action_, &QAction::triggered, this, [this]() { emit open_file_triggered(); });
    connect(toggle_list_action_, &QAction::triggered, this, [this]() { chapter_list_->setVisible(!chapter_list_->isVisible()); });
    connect(chapter_list_, &QListWidget::itemClicked, this, &main_window::on_chapter_list_item_clicked);

    connect(novel_view_, &novel_view::need_previous_chapter, this, [this]() { emit request_load_previous_chapter(); });
    connect(novel_view_, &novel_view::need_next_chapter, this, [this]() { emit request_load_next_chapter(); });
    connect(novel_view_->verticalScrollBar(), &QScrollBar::valueChanged, this, &main_window::on_view_scrolled);

    connect(scroll_action_, &QAction::triggered, this, &main_window::on_auto_scroll_click);
    connect(auto_scroll_timer_, &QTimer::timeout, this, &main_window::perform_auto_scroll);
    connect(auto_save_timer_, &QTimer::timeout, this, [this]() { emit save_progress_requested(); });

    connect(add_speed_, &QAction::triggered, this, [this]() { emit auto_scroll_speed_increase_triggered(); });
    connect(del_speed_, &QAction::triggered, this, [this]() { emit auto_scroll_speed_decrease_triggered(); });
    connect(add_font_action_, &QAction::triggered, this, [this]() { emit font_size_increase_triggered(); });
    connect(del_font_action_, &QAction::triggered, this, [this]() { emit font_size_decrease_triggered(); });
    connect(add_line_spacing_action_, &QAction::triggered, this, [this]() { emit line_spacing_increase_triggered(); });
    connect(del_line_spacing_action_, &QAction::triggered, this, [this]() { emit line_spacing_decrease_triggered(); });
    connect(add_letter_spacing_action_, &QAction::triggered, this, [this]() { emit letter_spacing_increase_triggered(); });
    connect(del_letter_spacing_action_, &QAction::triggered, this, [this]() { emit letter_spacing_decrease_triggered(); });

    connect(recent_files_menu_, &QMenu::aboutToShow, this, &main_window::populate_recent_files_menu);
    connect(select_font_action_, &QAction::triggered, this, &main_window::on_select_font_dialog);

    connect(background_animation_timer_, &QTimer::timeout, this, &main_window::update_background_gradient);
    connect(color_action_, &QAction::triggered, this, &main_window::on_color_action);
    connect(color_change_timer_, &QTimer::timeout, this, &main_window::change_to_next_color_scheme);
    connect(switch_background_action_, &QAction::triggered, this, &main_window::switch_to_next_background);

    connect(search_input_, &QLineEdit::returnPressed, this, &main_window::on_search_return_pressed);
    connect(find_prev_action_, &QAction::triggered, this, [this](){ emit find_previous_result_triggered(); });
    connect(find_next_action_, &QAction::triggered, this, [this](){ emit find_next_result_triggered(); });

    connect(app_state_, &app_state::chapter_list_cleared, this, &main_window::on_app_state_chapter_list_cleared);
    connect(app_state_, &app_state::chapter_found, this, &main_window::on_app_state_chapter_found);
    connect(app_state_, &app_state::current_chapter_index_changed, this, &main_window::on_app_state_current_chapter_index_changed);
    connect(app_state_, &app_state::search_results_changed, this, &main_window::on_app_state_search_results_changed);

    connect(settings_manager_, &settings_manager::font_changed, this, &main_window::on_settings_font_changed);
    connect(settings_manager_, &settings_manager::spacing_changed, this, &main_window::on_settings_spacing_changed);
}

void main_window::closeEvent(QCloseEvent* event)
{
    emit save_progress_requested();
    if (tray_icon_->isVisible())
    {
        hide();
        event->ignore();
    }
    else
    {
        event->accept();
    }
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
        if (!static_backgrounds_.isEmpty())
        {
            painter.fillRect(rect(), static_backgrounds_[current_static_bg_index_]);
        }
        else
        {
            painter.fillRect(rect(), QColor("#FDF6E3"));
        }
    }
}

void main_window::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void main_window::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QList<QUrl> url_list = mimeData->urls();
        if (!url_list.isEmpty())
        {
            QString filePath = url_list.first().toLocalFile();
            if (!filePath.isEmpty())
            {
                if (QFileInfo(filePath).suffix().toLower() == "txt")
                {
                    QVariantMap file_info;
                    file_info["filePath"] = filePath;
                    file_info["regex"] = settings_manager_->get_chapter_regex();
                    emit open_recent_file_triggered(file_info);
                }
                else
                {
                    QMessageBox::warning(this, "文件类型不支持", "请拖拽 txt 格式的文本文件");
                }
            }
        }
    }
    event->acceptProposedAction();
}

void main_window::on_chapter_list_item_clicked(QListWidgetItem* item)
{
    int index = chapter_list_->row(item);
    emit chapter_selected(index);
}

void main_window::perform_auto_scroll()
{
    QScrollBar* scrollBar = novel_view_->verticalScrollBar();
    if (scrollBar->value() >= scrollBar->maximum())
    {
        on_auto_scroll_click();
        return;
    }
    scrollBar->setValue(scrollBar->value() + 1);
}

void main_window::on_auto_scroll_click()
{
    auto_scroll_ = !auto_scroll_;
    if (auto_scroll_)
    {
        auto_scroll_timer_->start(settings_manager_->get_auto_scroll_speed());
        scroll_action_->setText("停止滚动");
    }
    else
    {
        auto_scroll_timer_->stop();
        scroll_action_->setText("自动滚动");
    }
}

void main_window::setup_static_backgrounds()
{
    static_backgrounds_.append(QColor("#FDF6E3"));
    static_backgrounds_.append(QColor("#C7EDCC"));
    static_backgrounds_.append(QColor("#D6EAF8"));
    static_backgrounds_.append(QColor("#F5EEF8"));
    static_backgrounds_.append(QColor("#FAE5D3"));
    static_backgrounds_.append(QColor("#E5E7E9"));
    if (!static_backgrounds_.isEmpty())
    {
        last_used_static_color_ = static_backgrounds_.first();
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
        last_used_static_color_ = static_backgrounds_[current_static_bg_index_];
        color_action_->setText("关闭动态背景");
        switch_background_action_->setEnabled(false);
        change_to_next_color_scheme();
        background_animation_timer_->start(1200);
        color_change_timer_->start(5000);
    }
    else
    {
        color_action_->setText("开启动态背景");
        switch_background_action_->setEnabled(true);
        background_animation_timer_->stop();
        color_change_timer_->stop();
        current_static_bg_index_ = static_backgrounds_.indexOf(last_used_static_color_);
        if (current_static_bg_index_ == -1)
        {
            current_static_bg_index_ = 0;
        }
        update();
    }
}

void main_window::switch_to_next_background()
{
    if (static_backgrounds_.size() < 2)
    {
        return;
    }
    current_static_bg_index_ = (current_static_bg_index_ + 1) % static_cast<int>(static_backgrounds_.size());
    update();
}

void main_window::populate_recent_files_menu()
{
    recent_files_menu_->clear();
    QVariantList recent_files = settings_manager_->get_recent_files();
    if (recent_files.isEmpty())
    {
        recent_files_menu_->addAction("（无最近文件）")->setEnabled(false);
    }
    else
    {
        for (const QVariant& file_variant : recent_files)
        {
            QVariantMap file_info = file_variant.toMap();
            QString file_path = file_info.value("filePath").toString();
            QString file_name = QFileInfo(file_path).fileName();
            QAction* action = recent_files_menu_->addAction(file_name);
            action->setData(file_info);
            connect(action, &QAction::triggered, this, &main_window::on_open_recent_file_action);
        }
    }
    recent_files_menu_->addSeparator();
    QAction* clear_action = recent_files_menu_->addAction("清空列表");
    connect(clear_action, &QAction::triggered, this, [this]() { emit clear_recent_files_triggered(); });
}

void main_window::on_open_recent_file_action()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        emit open_recent_file_triggered(action->data().toMap());
    }
}

void main_window::setup_shortcuts()
{
    auto* regex_shortcut = new QShortcut(QKeySequence(tr(kChapterRegexShortcut)), this);
    connect(regex_shortcut, &QShortcut::activated, this, [this]() { emit regex_dialog_triggered(); });
    auto* next_chapter_shortcut = new QShortcut(QKeySequence(Qt::Key_N), this);
    connect(next_chapter_shortcut, &QShortcut::activated, this, &main_window::on_load_next_chapter_action);
    auto* prev_chapter_shortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(prev_chapter_shortcut, &QShortcut::activated, this, &main_window::on_load_previous_chapter_action);
}

void main_window::on_select_font_dialog()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, settings_manager_->get_font(), this, "选择字体");
    if (ok)
    {
        emit font_selected(font);
    }
}

void main_window::apply_font_and_spacing()
{
    QFont font = settings_manager_->get_font();
    qreal line_spacing = settings_manager_->get_line_spacing();
    qreal letter_spacing = settings_manager_->get_letter_spacing();
    novel_view_->set_font_style(font, line_spacing, letter_spacing);
}

void main_window::set_status_message(const QString& chapter_text, const QString& progress_text)
{
    status_chapter_label_->setText(chapter_text);
    status_progress_label_->setText(progress_text);
}

void main_window::show_transient_status_message(const QString& message, int timeout) { statusBar()->showMessage(message, timeout); }

void main_window::clear_novel_view() { novel_view_->clear_content(); }

void main_window::append_chapter_to_view(int chapter_index, const QString& content) { novel_view_->append_chapter_content(chapter_index, content); }

void main_window::prepend_chapter_to_view(int chapter_index, const QString& content) { novel_view_->prepend_chapter_content(chapter_index, content); }

void main_window::restore_scroll_position(double ratio)
{
    QTimer::singleShot(0,
                       this,
                       [this, ratio]()
                       {
                           QScrollBar* scroll_bar = novel_view_->verticalScrollBar();
                           int new_value = static_cast<int>(scroll_bar->maximum() * ratio);
                           scroll_bar->setValue(new_value);
                       });
}

void main_window::perform_local_search(const QString& keyword) { novel_view_->search(keyword); }
void main_window::jump_to_match(int match_index) { novel_view_->jump_to_match(match_index); }
void main_window::clear_local_search() { novel_view_->clear_search(); }

int main_window::first_displayed_chapter_index() const { return novel_view_->first_displayed_chapter_index(); }

int main_window::last_displayed_chapter_index() const { return novel_view_->last_displayed_chapter_index(); }

bool main_window::is_chapter_displayed(int chapter_index) const { return novel_view_->is_chapter_displayed(chapter_index); }

QPair<int, double> main_window::get_current_progress() const { return novel_view_->current_progress(); }

void main_window::on_app_state_chapter_list_cleared()
{
    chapter_list_->clear();
    novel_view_->clear_content();
    search_input_->clear();
    status_chapter_label_->setText(" 正在解析章节...");
    status_progress_label_->setText("");
    status_search_label_->setText("");
}

void main_window::on_app_state_chapter_found(const QString& title) { chapter_list_->addItem(title); }

void main_window::on_app_state_current_chapter_index_changed(int index)
{
    chapter_list_->blockSignals(true);
    chapter_list_->setCurrentRow(index);
    chapter_list_->blockSignals(false);
    ensure_chapter_is_visible(index);
}

void main_window::on_settings_font_changed(const QFont& font)
{
    LOG_INFO("UI received font changed to {}", font.toString().toStdString());
    apply_font_and_spacing();
}

void main_window::on_settings_spacing_changed(qreal line_spacing, qreal letter_spacing)
{
    LOG_INFO("UI received spacing changed to line: {}, letter: {}", line_spacing, letter_spacing);
    apply_font_and_spacing();
}

void main_window::on_view_scrolled() { update_progress_status(); }

void main_window::update_progress_status()
{
    QPair<int, double> progress_pair = novel_view_->current_progress();
    int current_chapter_idx = progress_pair.first;
    double ratio_in_chapter = progress_pair.second;

    const auto& chapters = app_state_->chapters();
    if (current_chapter_idx < 0 || current_chapter_idx >= chapters.size())
    {
        return;
    }

    QString chapter_title = " " + QString::fromStdString(chapters[current_chapter_idx].title);
    status_chapter_label_->setText(chapter_title);

    const QString current_file_path = app_state_->file_path();
    if (current_file_path.isEmpty() || app_state_->total_chapters() == 0)
    {
        status_progress_label_->setText("");
        return;
    }

    qint64 file_size = QFileInfo(current_file_path).size();
    if (file_size > 0)
    {
        qint64 current_offset = chapters[current_chapter_idx].offset;
        qint64 next_offset = (current_chapter_idx + 1 < chapters.size()) ? chapters[current_chapter_idx + 1].offset : file_size;
        qint64 chapter_size = next_offset - current_offset;

        qint64 read_bytes = current_offset + static_cast<qint64>(static_cast<double>(chapter_size) * ratio_in_chapter);
        double overall_progress = static_cast<double>(read_bytes) * 100.0 / static_cast<double>(file_size);

        status_progress_label_->setText(QString("进度: %1% ").arg(overall_progress, 0, 'f', 2));
    }
    else
    {
        status_progress_label_->setText("进度: --% ");
    }

    if (current_chapter_idx != app_state_->current_chapter_index())
    {
        app_state_->set_current_chapter_index(current_chapter_idx);
    }
}

void main_window::update_search_status()
{
    if (app_state_->total_search_results() > 0)
    {
        status_search_label_->setText(QString(" %1 / %2 ").arg(app_state_->current_search_result_index() + 1).arg(app_state_->total_search_results()));
    }
    else
    {
        status_search_label_->setText("");
    }
}

void main_window::on_app_state_search_results_changed()
{
    update_search_status();
}


void main_window::ensure_chapter_is_visible(int chapter_index)
{
    if (chapter_index < 0 || chapter_index >= chapter_list_->count())
    {
        return;
    }
    QListWidgetItem* item = chapter_list_->item(chapter_index);
    if (item != nullptr)
    {
        chapter_list_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
    }
}

void main_window::on_load_next_chapter_action()
{
    int current_index = app_state_->current_chapter_index();
    if (app_state_->total_chapters() > 0 && current_index + 1 < static_cast<int>(app_state_->total_chapters()))
    {
        emit chapter_selected(current_index + 1);
    }
}

void main_window::on_load_previous_chapter_action()
{
    int current_index = app_state_->current_chapter_index();
    if (current_index - 1 >= 0)
    {
        emit chapter_selected(current_index - 1);
    }
}

void main_window::on_search_return_pressed()
{
    emit search_triggered(search_input_->text());
}
