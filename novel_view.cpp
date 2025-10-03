#include <QPainter>
#include <QScrollBar>
#include <QTextOption>
#include <utility>
#include "log.h"
#include "novel_view.h"
#include "context_menu.h"

static inline bool selection_is_valid(const text_position& pos) { return pos.chapter_layout_index != -1; }
static inline bool operator==(const text_position& lhs, const text_position& rhs)
{
    return lhs.chapter_layout_index == rhs.chapter_layout_index && lhs.paragraph_index == rhs.paragraph_index && lhs.char_index == rhs.char_index;
}
static inline bool operator!=(const text_position& lhs, const text_position& rhs) { return !(lhs == rhs); }
static inline bool operator<(const text_position& lhs, const text_position& rhs)
{
    if (lhs.chapter_layout_index != rhs.chapter_layout_index)
    {
        return lhs.chapter_layout_index < rhs.chapter_layout_index;
    }
    if (lhs.paragraph_index != rhs.paragraph_index)
    {
        return lhs.paragraph_index < rhs.paragraph_index;
    }
    return lhs.char_index < rhs.char_index;
}

static inline bool operator>(const text_position& lhs, const text_position& rhs) { return rhs < lhs; }
static inline bool operator<=(const text_position& lhs, const text_position& rhs) { return !(rhs < lhs); }
static inline bool operator>=(const text_position& lhs, const text_position& rhs) { return !(lhs < rhs); }

novel_view::novel_view(QWidget* parent) : QAbstractScrollArea(parent)
{
    verticalScrollBar()->setSingleStep(20);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &novel_view::on_scroll_value_changed);
    verticalScrollBar()->setVisible(false);
    horizontalScrollBar()->setVisible(false);
}

void novel_view::set_font_style(QFont font, qreal line_spacing, qreal letter_spacing)
{
    font_ = std::move(font);
    font_.setLetterSpacing(QFont::AbsoluteSpacing, letter_spacing);
    line_spacing_ = line_spacing;
    letter_spacing_ = letter_spacing;
    paragraph_spacing_ = font_.pointSizeF();
    relayout_and_redraw();
}

void novel_view::clear_content()
{
    chapter_layouts_.clear();
    total_height_ = 0;
    clear_selection();
    update_scrollbar();
    viewport()->update();
}

void novel_view::prepend_chapter_content(int chapter_index, const QString& content)
{
    chapter_layout new_layout;
    new_layout.chapter_index = chapter_index;
    layout_chapter(new_layout, content);

    qreal new_height = new_layout.height;
    if (new_height <= 0)
    {
        return;
    }

    viewport()->setUpdatesEnabled(false);
    int old_scroll_value = verticalScrollBar()->value();

    for (auto& chapter_layout : chapter_layouts_)
    {
        chapter_layout.y += new_height;
    }
    total_height_ += new_height;

    new_layout.y = 0;
    chapter_layouts_.prepend(std::move(new_layout));

    update_scrollbar();
    verticalScrollBar()->setValue(old_scroll_value + static_cast<int>(new_height));

    viewport()->setUpdatesEnabled(true);
    viewport()->update();
}

void novel_view::append_chapter_content(int chapter_index, const QString& content)
{
    chapter_layout new_layout;
    new_layout.chapter_index = chapter_index;
    layout_chapter(new_layout, content);

    if (new_layout.height <= 0)
    {
        return;
    }

    new_layout.y = total_height_;
    total_height_ += new_layout.height;
    chapter_layouts_.append(std::move(new_layout));

    update_scrollbar();
    viewport()->update();
}

void novel_view::relayout_and_redraw()
{
    relayout_all_chapters();
    update_scrollbar();
    viewport()->update();
}

bool novel_view::is_chapter_displayed(int chapter_index) const
{
    return std::find_if(chapter_layouts_.begin(),
                        chapter_layouts_.end(),
                        [chapter_index](const chapter_layout& layout) { return layout.chapter_index == chapter_index; }) != chapter_layouts_.end();
}

int novel_view::first_displayed_chapter_index() const { return chapter_layouts_.isEmpty() ? -1 : chapter_layouts_.first().chapter_index; }

int novel_view::last_displayed_chapter_index() const { return chapter_layouts_.isEmpty() ? -1 : chapter_layouts_.last().chapter_index; }

QPair<int, double> novel_view::current_progress() const
{
    if (chapter_layouts_.isEmpty())
    {
        return qMakePair(-1, 0.0);
    }

    QPoint top_of_viewport(viewport()->width() / 2, 0);
    text_position pos = map_point_to_text_position(top_of_viewport);

    if (!selection_is_valid(pos))
    {
        return qMakePair(chapter_layouts_.first().chapter_index, 0.0);
    }

    const auto& chapter = chapter_layouts_[pos.chapter_layout_index];
    int chapter_index = chapter.chapter_index;
    qreal chapter_height = chapter.height;

    qreal global_scroll_y = verticalScrollBar()->value();
    qreal chapter_top_y = chapter.y;
    qreal offset_in_chapter = global_scroll_y - chapter_top_y;

    double ratio = 0.0;
    if (chapter_height > 1)
    {
        ratio = qBound(0.0, offset_in_chapter / chapter_height, 1.0);
    }

    return qMakePair(chapter_index, ratio);
}

void novel_view::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRect& visible_rect = event->rect();
    const int scroll_y = verticalScrollBar()->value();

    painter.setClipRect(visible_rect.adjusted(0, -20, 0, 20));

    text_position selection_start_pos = std::min(selection_start_, selection_end_);
    text_position selection_end_pos = std::max(selection_start_, selection_end_);
    bool has_selection = selection_is_valid(selection_start_pos) && selection_is_valid(selection_end_pos) && selection_start_pos < selection_end_pos;

    for (int chap_idx = 0; chap_idx < chapter_layouts_.size(); ++chap_idx)
    {
        const auto& chapter = chapter_layouts_[chap_idx];
        qreal chapter_top_y = chapter.y - scroll_y;
        QRectF chapter_rect(0, chapter_top_y, viewport()->width(), chapter.height);

        if (!chapter_rect.intersects(visible_rect))
        {
            continue;
        }

        for (int para_idx = 0; para_idx < chapter.paragraphs.size(); ++para_idx)
        {
            const auto& para = chapter.paragraphs[para_idx];
            qreal para_top_y = chapter_top_y + para.y;
            QRectF para_rect(0, para_top_y, viewport()->width(), para.height);

            if (!para_rect.intersects(visible_rect))
            {
                continue;
            }

            QPointF draw_position(0, para_top_y);

            bool para_has_selection = false;
            if (has_selection)
            {
                text_position para_start_pos = {chap_idx, para_idx, 0};
                text_position para_end_pos = {chap_idx, para_idx, static_cast<int>(para.text_layout->text().length())};
                if (!(selection_end_pos < para_start_pos || selection_start_pos > para_end_pos))
                {
                    para_has_selection = true;
                }
            }

            if (para_has_selection)
            {
                QVector<QTextLayout::FormatRange> selections;
                int start = (selection_start_pos > text_position{chap_idx, para_idx, 0}) ? selection_start_pos.char_index : 0;
                int end = (selection_end_pos < text_position{chap_idx, para_idx, static_cast<int>(para.text_layout->text().length())})
                              ? selection_end_pos.char_index
                              : static_cast<int>(para.text_layout->text().length());

                if (start < end)
                {
                    QTextLayout::FormatRange selection_range;
                    selection_range.start = start;
                    selection_range.length = end - start;
                    selection_range.format.setBackground(palette().highlight());
                    selection_range.format.setForeground(palette().highlightedText());
                    selections.append(selection_range);
                }
                para.text_layout->draw(&painter, draw_position, selections);
            }
            else
            {
                int line_count = para.text_layout->lineCount();
                if (line_count == 0)
                {
                    continue;
                }

                qreal para_start_y = draw_position.y();
                int first_visible_line = 0;
                int last_visible_line = line_count - 1;

                int low = 0;
                int high = line_count - 1;
                while (low <= high)
                {
                    int mid = (low + high) / 2;
                    const QTextLine& line = para.text_layout->lineAt(mid);
                    qreal line_bottom = para_start_y + line.y() + line.height();
                    if (line_bottom < visible_rect.top())
                    {
                        low = mid + 1;
                    }
                    else
                    {
                        high = mid - 1;
                    }
                }
                first_visible_line = low;

                low = 0;
                high = line_count - 1;
                while (low <= high)
                {
                    int mid = (low + high) / 2;
                    const QTextLine& line = para.text_layout->lineAt(mid);
                    qreal line_top = para_start_y + line.y();
                    if (line_top > visible_rect.bottom())
                    {
                        high = mid - 1;
                    }
                    else
                    {
                        low = mid + 1;
                    }
                }
                last_visible_line = high;

                for (int i = first_visible_line; i <= last_visible_line; ++i)
                {
                    if (i < 0 || i >= line_count)
                    {
                        continue;
                    }
                    const QTextLine& line = para.text_layout->lineAt(i);
                    line.draw(&painter, draw_position);
                }
            }
        }
    }
}

void novel_view::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    relayout_and_redraw();
}

void novel_view::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        is_selecting_ = true;
        selection_start_ = map_point_to_text_position(event->pos());
        selection_end_ = selection_start_;
        viewport()->update();
    }
    QAbstractScrollArea::mousePressEvent(event);
}

void novel_view::mouseMoveEvent(QMouseEvent* event)
{
    if (is_selecting_)
    {
        selection_end_ = map_point_to_text_position(event->pos());
        viewport()->update();
    }
    QAbstractScrollArea::mouseMoveEvent(event);
}

void novel_view::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        is_selecting_ = false;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void novel_view::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new context_menu(this);

    menu->addAction("复制");

    connect(menu,
            &context_menu::triggered,
            this,
            [this](const QString& actionText)
            {
                if (actionText == "复制")
                {
                    copy_selection();
                }
            });

    QString selected = selected_text();
    menu->setActionEnabled("复制", !selected.isEmpty());
    menu->exec(event->globalPos());
}

void novel_view::copy_selection()
{
    QString selected = selected_text();
    if (!selected.isEmpty())
    {
        QApplication::clipboard()->setText(selected);
    }
}

void novel_view::layout_chapter(chapter_layout& chapter, const QString& content)
{
    chapter.paragraphs.clear();
    chapter.height = 0;

    QString clean_content = content.trimmed();
    QStringList paragraphs_text = clean_content.split('\n', Qt::SkipEmptyParts);

    const QString indent = "　　";
    qreal chapter_current_height = 0;

    for (const QString& para_text : paragraphs_text)
    {
        QString trimmed_para = para_text.trimmed();
        if (trimmed_para.isEmpty())
        {
            continue;
        }

        paragraph_layout para_layout;
        para_layout.text_layout = std::make_shared<QTextLayout>(indent + trimmed_para);
        para_layout.text_layout->setFont(font_);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        para_layout.text_layout->setTextOption(option);

        para_layout.text_layout->beginLayout();
        qreal para_height = 0;
        while (true)
        {
            QTextLine line = para_layout.text_layout->createLine();
            if (!line.isValid())
            {
                break;
            }
            line.setLineWidth(viewport()->width());
            line.setPosition(QPointF(0, para_height));
            para_height += line.height() * line_spacing_;
        }
        para_layout.text_layout->endLayout();
        para_layout.height = para_height;
        para_layout.y = chapter_current_height;
        chapter.paragraphs.append(std::move(para_layout));

        chapter_current_height += para_height + paragraph_spacing_;
    }

    if (!paragraphs_text.isEmpty())
    {
        chapter.height = chapter_current_height - paragraph_spacing_ + paragraph_spacing_;
    }
    else
    {
        chapter.height = 0;
    }
}

void novel_view::relayout_all_chapters()
{
    double scroll_ratio = 0.0;
    if (verticalScrollBar()->maximum() > 0)
    {
        scroll_ratio = static_cast<double>(verticalScrollBar()->value()) / verticalScrollBar()->maximum();
    }

    total_height_ = 0;
    for (auto& chapter_layout : chapter_layouts_)
    {
        qreal chapter_current_height = 0;
        for (auto& para : chapter_layout.paragraphs)
        {
            para.text_layout->setFont(font_);
            para.text_layout->beginLayout();
            qreal para_height = 0;
            while (true)
            {
                QTextLine line = para.text_layout->createLine();
                if (!line.isValid())
                {
                    break;
                }
                line.setLineWidth(viewport()->width());
                line.setPosition(QPointF(0, para_height));
                para_height += line.height() * line_spacing_;
            }
            para.text_layout->endLayout();
            para.height = para_height;
            para.y = chapter_current_height;
            chapter_current_height += para_height + paragraph_spacing_;
        }
        if (!chapter_layout.paragraphs.isEmpty())
        {
            chapter_layout.height = chapter_current_height;
        }
        else
        {
            chapter_layout.height = 0;
        }

        chapter_layout.y = total_height_;
        total_height_ += chapter_layout.height;
    }

    update_scrollbar();
    verticalScrollBar()->setValue(static_cast<int>(scroll_ratio * verticalScrollBar()->maximum()));
}

void novel_view::update_scrollbar()
{
    int max_scroll = qMax(0, qRound(total_height_ - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, max_scroll);
}

void novel_view::on_scroll_value_changed(int value)
{
    viewport()->update();

    int threshold = viewport()->height() / 2;
    if (value < threshold)
    {
        if (!chapter_layouts_.isEmpty())
        {
            emit need_previous_chapter(chapter_layouts_.first().chapter_index);
        }
    }
    auto scroll_max = verticalScrollBar()->maximum();
    auto diff = scroll_max - threshold;

    if (value > diff)
    {
        if (!chapter_layouts_.isEmpty())
        {
            LOG_INFO("scroll value {} max {} threshold {} diff {} current index {}",
                     value,
                     scroll_max,
                     threshold,
                     diff,
                     chapter_layouts_.last().chapter_index);

            emit need_next_chapter(chapter_layouts_.last().chapter_index);
        }
    }
}

text_position novel_view::map_point_to_text_position(const QPoint& point) const    // NOLINT
{
    if (chapter_layouts_.isEmpty())
    {
        return {};
    }

    int scroll_y = verticalScrollBar()->value();
    QPointF doc_point(point.x(), point.y() + scroll_y);

    if (doc_point.y() < chapter_layouts_.first().y)
    {
        return {0, 0, 0};
    }
    if (doc_point.y() >= total_height_)
    {
        int last_chap_idx = static_cast<int>(chapter_layouts_.size()) - 1;
        const auto& last_chap = chapter_layouts_.last();
        if (last_chap.paragraphs.isEmpty())
        {
            return {last_chap_idx, 0, 0};
        }
        int last_para_idx = static_cast<int>(last_chap.paragraphs.size()) - 1;
        const auto& last_para = last_chap.paragraphs.last();
        return {last_chap_idx, last_para_idx, static_cast<int>(last_para.text_layout->text().length())};
    }

    int target_chap_idx = -1;
    for (int i = 0; i < chapter_layouts_.size(); ++i)
    {
        const auto& chapter = chapter_layouts_[i];
        if (doc_point.y() >= chapter.y && doc_point.y() < chapter.y + chapter.height)
        {
            target_chap_idx = i;
            break;
        }
    }

    if (target_chap_idx == -1)
    {
        return {};
    }

    const auto& chapter = chapter_layouts_[target_chap_idx];
    if (chapter.paragraphs.isEmpty())
    {
        return {target_chap_idx, 0, 0};
    }

    int target_para_idx = -1;
    for (int i = 0; i < chapter.paragraphs.size(); ++i)
    {
        if (chapter.y + chapter.paragraphs[i].y <= doc_point.y())
        {
            target_para_idx = i;
        }
        else
        {
            break;
        }
    }

    if (target_para_idx == -1)
    {
        target_para_idx = 0;
    }

    const auto& para = chapter.paragraphs[target_para_idx];
    qreal para_top_y = chapter.y + para.y;

    if (doc_point.y() >= para_top_y + para.height)
    {
        return {target_chap_idx, target_para_idx, static_cast<int>(para.text_layout->text().length())};
    }

    QPointF para_point = doc_point - QPointF(0, para_top_y);

    int line_index = -1;
    for (int i = 0; i < para.text_layout->lineCount(); ++i)
    {
        if (para.text_layout->lineAt(i).rect().contains(para_point))
        {
            line_index = i;
            break;
        }
    }
    if (line_index == -1)
    {
        line_index = para.text_layout->lineForTextPosition(0).lineNumber();
        for (int i = 0; i < para.text_layout->lineCount(); ++i)
        {
            if (para.text_layout->lineAt(i).rect().top() <= para_point.y())
            {
                line_index = i;
            }
            else
            {
                break;
            }
        }
    }

    QTextLine line = para.text_layout->lineAt(line_index);
    int char_index = line.xToCursor(para_point.x());
    return {target_chap_idx, target_para_idx, char_index};
}

QString novel_view::selected_text() const
{
    if (!selection_is_valid(selection_start_) || !selection_is_valid(selection_end_) || selection_start_ == selection_end_)
    {
        return {};
    }

    text_position start_pos = std::min(selection_start_, selection_end_);
    text_position end_pos = std::max(selection_start_, selection_end_);

    QStringList result_parts;

    for (int chap_idx = start_pos.chapter_layout_index; chap_idx <= end_pos.chapter_layout_index; ++chap_idx)
    {
        const auto& chapter = chapter_layouts_[chap_idx];
        if (chapter.paragraphs.isEmpty())
        {
            continue;
        }

        int para_start = (chap_idx == start_pos.chapter_layout_index) ? start_pos.paragraph_index : 0;
        int para_end = (chap_idx == end_pos.chapter_layout_index) ? end_pos.paragraph_index : static_cast<int>(chapter.paragraphs.size()) - 1;

        for (int para_idx = para_start; para_idx <= para_end; ++para_idx)
        {
            const auto& para = chapter.paragraphs[para_idx];
            const QString& para_text = para.text_layout->text();

            int selection_start_index = 0;
            if (chap_idx == start_pos.chapter_layout_index && para_idx == start_pos.paragraph_index)
            {
                selection_start_index = start_pos.char_index;
            }

            int selection_end_index = static_cast<int>(para_text.length());
            if (chap_idx == end_pos.chapter_layout_index && para_idx == end_pos.paragraph_index)
            {
                selection_end_index = end_pos.char_index;
            }

            int length = selection_end_index - selection_start_index;
            if (length > 0)
            {
                result_parts.append(para_text.mid(selection_start_index, length));
            }
        }
    }

    return result_parts.join('\n');
}

void novel_view::clear_selection()
{
    selection_start_ = {};
    selection_end_ = {};
    is_selecting_ = false;
}
