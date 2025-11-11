#include <QFileDialog>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSettings>
#include <QToolButton>
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
#include <QVariantList>
#include <QInputDialog>
#include <QKeySequence>
#include <QShortcut>
#include <QLabel>
#include <QtMath>
#include "log.h"
#include "splitter.h"
#include "novel_view.h"
#include "main_window.h"
#include "novel_manager.h"

static const char* kSelfName = "TXTReader";
static const char* kRecentFiles = "recent_files";
static const char* kLastChapterIndex = "last_chapter_index";
static const char* kLastScrollRatio = "last_scroll_ratio";
static const char* kChapterRegex = "chapter_regex";
static const char* kDefaultChapterRegex = "第[一二三四五六七八九十百千万两0-9]+章[^\\r\\n]*";
static const char* kChapterRegexShortcut = "Ctrl+R";

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
    setAcceptDrops(true);
    worker_thread_ = new QThread(this);
    novel_manager_ = new novel_manager();
    novel_manager_->moveToThread(worker_thread_);
    view_font_ = default_font(38.0);
    LOG_INFO("default font {}", view_font_.toString().toStdString());
    setup_static_backgrounds();
    setup_ui();
    setup_color_schemes();
    setup_connections();
    setup_shortcuts();
    worker_thread_->start();
    novel_view_->set_font_style(view_font_, line_spacing_, letter_spacing_);
    apply_font_and_spacing();
    setStyleSheet("QSplitter, QListWidget, QToolBar, QStatusBar, QAbstractScrollArea { background-color: transparent; border: none; }");
    setWindowTitle("TXT 小说阅读器");
    auto_save_timer_->start(8000);
    resize(1024, 768);
}

main_window::~main_window()
{
    save_progress();
    worker_thread_->quit();
    worker_thread_->wait();
    delete transition_start_time_;
}

void main_window::setup_ui()
{
    tray_icon_ = new tray_icon(this);
    connect(tray_icon_, &tray_icon::show_hide_triggered, this, [this]() { isVisible() ? hide() : show(); });
    connect(tray_icon_, &tray_icon::quit_triggered, this, &main_window::quit_application);
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

    auto_scroll_timer_ = new QTimer(this);
    auto_save_timer_ = new QTimer(this);
    background_animation_timer_ = new QTimer(this);
    color_change_timer_ = new QTimer(this);
    transition_start_time_ = new QElapsedTimer();

    status_chapter_label_ = new QLabel(" 就绪");
    status_progress_label_ = new QLabel("进度: --% ");
    statusBar()->addWidget(status_chapter_label_, 1);
    statusBar()->addPermanentWidget(status_progress_label_);

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
    connect(auto_save_timer_, &QTimer::timeout, this, &main_window::save_progress);
    connect(add_speed_, &QAction::triggered, this, &main_window::increase_auto_speed);
    connect(del_speed_, &QAction::triggered, this, &main_window::decrease_auto_speed);
    connect(add_font_action_, &QAction::triggered, this, &main_window::increase_font_size);
    connect(del_font_action_, &QAction::triggered, this, &main_window::decrease_font_size);
    connect(add_line_spacing_action_, &QAction::triggered, this, &main_window::increase_line_spacing);
    connect(del_line_spacing_action_, &QAction::triggered, this, &main_window::decrease_line_spacing);
    connect(add_letter_spacing_action_, &QAction::triggered, this, &main_window::increase_letter_spacing);
    connect(del_letter_spacing_action_, &QAction::triggered, this, &main_window::decrease_letter_spacing);
    connect(recent_files_menu_, &QMenu::aboutToShow, this, &main_window::populate_recent_files_menu);
    connect(select_font_action_, &QAction::triggered, this, &main_window::select_font_dialog);
    connect(background_animation_timer_, &QTimer::timeout, this, &main_window::update_background_gradient);
    connect(color_action_, &QAction::triggered, this, &main_window::on_color_action);
    connect(color_change_timer_, &QTimer::timeout, this, &main_window::change_to_next_color_scheme);
    connect(switch_background_action_, &QAction::triggered, this, &main_window::switch_to_next_background);
}
void main_window::quit_application()
{
    save_progress();
    hide();
    QApplication::quit();
}
void main_window::closeEvent(QCloseEvent* event)
{
    save_progress();
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

    if (!mimeData->hasUrls())
    {
        return;
    }
    QList<QUrl> url_list = mimeData->urls();

    if (!url_list.isEmpty())
    {
        QString filePath = url_list.first().toLocalFile();

        if (!filePath.isEmpty())
        {
            LOG_INFO("file dropped {}", filePath.toStdString());

            if (QFileInfo(filePath).suffix().toLower() == "txt")
            {
                load_new_file(filePath);
            }
            else
            {
                QMessageBox::warning(this, "文件类型不支持", "请拖拽 txt 格式的文本文件");
            }
        }
    }
    event->acceptProposedAction();
}

void main_window::open_file_dialog()
{
    QString file_path = QFileDialog::getOpenFileName(this, "打开小说", "", "Text Files (*.txt)");
    load_new_file(file_path);
}
void main_window::toggle_chapter_list_visibility() { chapter_list_->setVisible(!chapter_list_->isVisible()); }
void main_window::on_chapter_list_item_clicked(QListWidgetItem* item)
{
    int index = chapter_list_->row(item);
    if (index >= 0 && index != current_chapter_index_)
    {
        load_chapter(index);
    }
}
void main_window::on_chapter_found(const QString& title, qint64 offset)
{
    chapter_list_->addItem(title);
    chapters_info_.append({title.toStdString(), offset});
}
void main_window::on_parsing_finished(size_t total_chapters)
{
    total_chapters_ = total_chapters;
    statusBar()->showMessage(QString("找到 %1 个章节。").arg(total_chapters_), 3000);
    if (total_chapters == 0)
    {
        status_chapter_label_->setText(" 未找到章节");
        status_progress_label_->setText("进度: 0.00%");
    }

    QString current_file_path = novel_manager_->property("current_file_path").toString();
    if (!current_file_path.isEmpty())
    {
        load_progress(current_file_path);
    }
    else if (total_chapters_ > 0)
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
    if (chapter_index == chapter_index_to_restore_)
    {
        QTimer::singleShot(0,
                           this,
                           [this]()
                           {
                               QScrollBar* scroll_bar = novel_view_->verticalScrollBar();
                               int new_value = static_cast<int>(scroll_bar->maximum() * scroll_ratio_to_restore_);
                               scroll_bar->setValue(new_value);
                               chapter_index_to_restore_ = -1;
                               scroll_ratio_to_restore_ = 0.0;
                           });
    }
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
    LOG_INFO("total_chapters {} load next index {}", total_chapters_, last_index + 1);
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
    save_progress();
    if (chapter_index < 0 || static_cast<size_t>(chapter_index) >= total_chapters_)
    {
        return;
    }

    current_chapter_index_ = chapter_index;
    chapter_list_->blockSignals(true);
    chapter_list_->setCurrentRow(current_chapter_index_);
    chapter_list_->blockSignals(false);

    ensure_chapter_is_visible(chapter_index);
    is_loading_content_ = true;
    novel_view_->clear_content();
    initial_chapter_to_load_ = chapter_index;
    emit request_chapter_content(chapter_index);
}

void main_window::update_progress_status()
{
    QPair<int, double> progress_pair = novel_view_->current_progress();
    int current_chapter_idx = progress_pair.first;
    double ratio_in_chapter = progress_pair.second;

    if (current_chapter_idx < 0 || current_chapter_idx >= chapters_info_.size())
    {
        return;
    }

    QString chapter_title = " " + QString::fromStdString(chapters_info_[current_chapter_idx].title);
    status_chapter_label_->setText(chapter_title);

    QString current_file_path = novel_manager_->property("current_file_path").toString();
    if (current_file_path.isEmpty() || total_chapters_ == 0)
    {
        status_progress_label_->setText("");
        return;
    }

    qint64 file_size = QFileInfo(current_file_path).size();
    if (file_size > 0)
    {
        qint64 current_offset = chapters_info_[current_chapter_idx].offset;
        qint64 next_offset = (current_chapter_idx + 1 < chapters_info_.size()) ? chapters_info_[current_chapter_idx + 1].offset : file_size;
        qint64 chapter_size = next_offset - current_offset;

        qint64 read_bytes = current_offset + static_cast<qint64>(static_cast<double>(chapter_size) * ratio_in_chapter);
        double overall_progress = static_cast<double>(read_bytes) * 100.0 / static_cast<double>(file_size);

        status_progress_label_->setText(QString("进度: %1% ").arg(overall_progress, 0, 'f', 2));
    }
    else
    {
        status_progress_label_->setText("进度: --% ");
    }

    if (current_chapter_idx != current_chapter_index_)
    {
        current_chapter_index_ = current_chapter_idx;

        chapter_list_->blockSignals(true);
        chapter_list_->setCurrentRow(current_chapter_index_);
        chapter_list_->blockSignals(false);

        ensure_chapter_is_visible(current_chapter_index_);
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

    scrollBar->setValue(currentValue + 1);
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

void main_window::save_progress()
{
    QString current_file_path = novel_manager_->property("current_file_path").toString();
    if (current_file_path.isEmpty() || total_chapters_ == 0)
    {
        return;
    }

    QPair<int, double> progress = novel_view_->current_progress();
    int chapter_index = progress.first;
    double scroll_ratio = progress.second;

    if (chapter_index < 0)
    {
        return;
    }

    if (current_file_path == last_saved_file_path_ && chapter_index == last_saved_chapter_index_ &&
        qAbs(scroll_ratio - last_saved_scroll_ratio_) < 0.001)
    {
        return;
    }

    QSettings settings(kSelfName, kSelfName);
    QString absolute_path = QFileInfo(current_file_path).absoluteFilePath();
    QByteArray key = absolute_path.toUtf8().toBase64(QByteArray::Base64UrlEncoding);
    settings.beginGroup(QString(key));
    settings.setValue(kLastChapterIndex, chapter_index);
    settings.setValue(kLastScrollRatio, scroll_ratio);
    settings.endGroup();

    last_saved_file_path_ = current_file_path;
    last_saved_chapter_index_ = chapter_index;
    last_saved_scroll_ratio_ = scroll_ratio;

    LOG_INFO("progress saved {} chapter {} ratio {}", key.toStdString(), chapter_index, scroll_ratio);
}

void main_window::load_progress(const QString& file_path)
{
    QSettings settings(kSelfName, kSelfName);
    QString absolute_path = QFileInfo(file_path).absoluteFilePath();
    QByteArray key = absolute_path.toUtf8().toBase64(QByteArray::Base64UrlEncoding);
    auto setting_list = settings.childGroups();
    for (const auto& it : setting_list)
    {
        LOG_INFO("loading progress {}", it.toStdString());
    }
    if (setting_list.contains(QString(key)))
    {
        settings.beginGroup(QString(key));
        int chapter_index = settings.value(kLastChapterIndex, 0).toInt();
        double scroll_ratio = settings.value(kLastScrollRatio, 0.0).toDouble();
        settings.endGroup();

        LOG_INFO("loading progress {} chapter {} ratio {}", absolute_path.toStdString(), chapter_index, scroll_ratio);
        if (chapter_index >= 0 && static_cast<size_t>(chapter_index) < total_chapters_)
        {
            chapter_index_to_restore_ = chapter_index;
            scroll_ratio_to_restore_ = scroll_ratio;
            load_chapter(chapter_index);
        }
    }
    else
    {
        if (total_chapters_ > 0)
        {
            load_chapter(0);
        }
    }
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

void main_window::update_recent_files(const QString& file_path)
{
    (void)this;
    QSettings settings(kSelfName, kSelfName);
    QVariantList recent_files = settings.value(kRecentFiles).toList();
    int existing_index = -1;
    for (int i = 0; i < recent_files.size(); ++i)
    {
        if (recent_files[i].toMap().value("filePath").toString() == file_path)
        {
            existing_index = i;
            break;
        }
    }

    if (existing_index != -1)
    {
        recent_files.removeAt(existing_index);
    }

    QVariantMap file_info;
    file_info["filePath"] = file_path;
    file_info["regex"] = get_current_regex();

    recent_files.prepend(file_info);

    while (recent_files.size() > 10)
    {
        recent_files.removeLast();
    }

    settings.setValue(kRecentFiles, recent_files);
}

void main_window::populate_recent_files_menu()
{
    recent_files_menu_->clear();
    QSettings settings(kSelfName, kSelfName);
    QVariantList recent_files = settings.value(kRecentFiles).toList();

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
            connect(action, &QAction::triggered, this, &main_window::open_recent_file);
        }
    }

    recent_files_menu_->addSeparator();
    QAction* clear_action = recent_files_menu_->addAction("清空列表");
    connect(clear_action, &QAction::triggered, this, &main_window::clear_recent_files);
}

void main_window::open_recent_file()
{
    auto* action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
    {
        return;
    }
    QVariantMap file_info = action->data().toMap();
    QString file_path = file_info.value("filePath").toString();
    QString regex = file_info.value("regex").toString();

    if (!QFile::exists(file_path))
    {
        QMessageBox::warning(this, "文件未找到", QString("无法找到文件：\n%1\n\n该记录将被移除。").arg(file_path));
        remove_recent_file(file_path);
        return;
    }

    LOG_INFO("opening recent file {} with its specific regex {}", file_path.toStdString(), regex.toStdString());

    QSettings settings(kSelfName, kSelfName);
    settings.setValue(kChapterRegex, regex);
    load_new_file(file_path);
}

void main_window::remove_recent_file(const QString& file_path)
{
    (void)this;
    QSettings settings(kSelfName, kSelfName);
    QVariantList recent_files = settings.value(kRecentFiles).toList();

    for (int i = 0; i < recent_files.size(); ++i)
    {
        if (recent_files[i].toMap().value("filePath").toString() == file_path)
        {
            recent_files.removeAt(i);
            break;
        }
    }

    settings.setValue(kRecentFiles, recent_files);
    LOG_INFO("removed non exist file from recent list {}", file_path.toStdString());
}

void main_window::clear_recent_files()
{
    (void)this;
    QSettings settings(kSelfName, kSelfName);
    settings.remove(kRecentFiles);
    LOG_INFO("recent files list cleared.");
}

void main_window::load_new_file(const QString& file_path)
{
    if (file_path.isEmpty())
    {
        return;
    }

    save_progress();

    last_saved_file_path_.clear();
    last_saved_chapter_index_ = -1;
    last_saved_scroll_ratio_ = -1.0;

    chapters_info_.clear();
    chapter_list_->clear();
    novel_view_->clear_content();
    status_chapter_label_->setText(" 正在解析章节...");
    status_progress_label_->setText("");

    update_recent_files(file_path);
    emit request_load_file(file_path, get_current_regex());
}
void main_window::setup_shortcuts()
{
    auto* regex_shortcut = new QShortcut(QKeySequence(tr(kChapterRegexShortcut)), this);
    connect(regex_shortcut, &QShortcut::activated, this, &main_window::open_regex_dialog);

    auto* next_chapter_shortcut = new QShortcut(QKeySequence(Qt::Key_N), this);
    connect(next_chapter_shortcut, &QShortcut::activated, this, &main_window::load_next_chapter_action);

    auto* prev_chapter_shortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(prev_chapter_shortcut, &QShortcut::activated, this, &main_window::load_previous_chapter_action);
}

QString main_window::get_current_regex()
{
    (void)this;
    QSettings settings(kSelfName, kSelfName);
    return settings.value(kChapterRegex, kDefaultChapterRegex).toString();
}

void main_window::open_regex_dialog()
{
    QString old_regex = get_current_regex();

    bool ok;
    QString new_regex = QInputDialog::getText(this, "设置章节正则表达式", "正则表达式:", QLineEdit::Normal, old_regex, &ok);
    if (!ok)
    {
        return;
    }
    if (new_regex.isEmpty())
    {
        return;
    }
    if (new_regex == old_regex)
    {
        return;
    }

    LOG_INFO("chapter regex changed from {} to {}", old_regex.toStdString(), new_regex.toStdString());

    QSettings settings(kSelfName, kSelfName);
    settings.setValue(kChapterRegex, new_regex);

    QString current_file_path = novel_manager_->property("current_file_path").toString();
    if (!current_file_path.isEmpty())
    {
        LOG_INFO("reloading file {} with new regex", current_file_path.toStdString());
        load_new_file(current_file_path);
    }
}

void main_window::load_next_chapter_action()
{
    if (total_chapters_ > 0 && current_chapter_index_ + 1 < static_cast<int>(total_chapters_))
    {
        load_chapter(current_chapter_index_ + 1);
    }
}

void main_window::load_previous_chapter_action()
{
    if (current_chapter_index_ - 1 >= 0)
    {
        load_chapter(current_chapter_index_ - 1);
    }
}
