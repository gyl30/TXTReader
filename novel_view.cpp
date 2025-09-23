#include <QPainter>
#include <QScrollBar>
#include <QTextOption>
#include <QDebug>
#include <utility>
#include "novel_view.h"

NovelView::NovelView(QWidget* parent) : QAbstractScrollArea(parent)
{
    verticalScrollBar()->setSingleStep(20);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &NovelView::onScrollValueChanged);
}

void NovelView::setFontStyle(const QFont& font, qreal lineSpacing, qreal letterSpacing)
{
    font_ = font;
    font_.setLetterSpacing(QFont::AbsoluteSpacing, letterSpacing);
    lineSpacing_ = lineSpacing;
    letterSpacing_ = letterSpacing;
    paragraphSpacing_ = font.pointSizeF();
    relayoutAndRedraw();
}

void NovelView::clearContent()
{
    chapterLayouts_.clear();
    totalHeight_ = 0;
    updateScrollbar();
    viewport()->update();
}

void NovelView::prependChapterContent(int chapterIndex, const QString& content)
{
    ChapterLayout newLayout;
    newLayout.chapter_index = chapterIndex;
    layoutChapter(newLayout, content);

    qreal newHeight = newLayout.height;
    if (newHeight <= 0)
    {
        return;
    }

    viewport()->setUpdatesEnabled(false);
    int oldScrollValue = verticalScrollBar()->value();

    for (int i = 0; i < chapterLayouts_.size(); ++i)
    {
        chapterLayouts_[i].y += newHeight;
    }
    totalHeight_ += newHeight;

    newLayout.y = 0;
    chapterLayouts_.prepend(std::move(newLayout));

    updateScrollbar();
    verticalScrollBar()->setValue(oldScrollValue + static_cast<int>(newHeight));

    viewport()->setUpdatesEnabled(true);
    viewport()->update();
}

void NovelView::appendChapterContent(int chapterIndex, const QString& content)
{
    ChapterLayout newLayout;
    newLayout.chapter_index = chapterIndex;
    layoutChapter(newLayout, content);

    if (newLayout.height <= 0)
    {
        return;
    }

    newLayout.y = totalHeight_;
    totalHeight_ += newLayout.height;
    chapterLayouts_.append(std::move(newLayout));

    updateScrollbar();
    viewport()->update();
}

void NovelView::relayoutAndRedraw()
{
    relayoutAllChapters();
    updateScrollbar();
    viewport()->update();
}

bool NovelView::isChapterDisplayed(int chapterIndex) const
{
    for (const auto& layout : chapterLayouts_)
    {
        if (layout.chapter_index == chapterIndex)
        {
            return true;
        }
    }
    return false;
}

int NovelView::getFirstDisplayedChapterIndex() const { return chapterLayouts_.isEmpty() ? -1 : chapterLayouts_.first().chapter_index; }

int NovelView::getLastDisplayedChapterIndex() const { return chapterLayouts_.isEmpty() ? -1 : chapterLayouts_.last().chapter_index; }

void NovelView::paintEvent(QPaintEvent* event)
{
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRect& visibleRect = event->rect();
    const int scrollY = verticalScrollBar()->value();

    for (const auto& chapter : chapterLayouts_)
    {
        qreal chapterTopY = chapter.y - scrollY;
        QRectF chapterRect(0, chapterTopY, viewport()->width(), chapter.height);

        if (!chapterRect.intersects(visibleRect))
        {
            continue;
        }

        for (const auto& para : chapter.paragraphs)
        {
            qreal paraTopY = chapterTopY + para.y;
            QRectF paraRect(0, paraTopY, viewport()->width(), para.height);

            if (paraRect.intersects(visibleRect))
            {
                QPointF drawPosition(0, paraTopY);
                para.text_layout->draw(&painter, drawPosition);
            }
        }
    }
}

void NovelView::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    relayoutAndRedraw();
}

void NovelView::layoutChapter(ChapterLayout& chapter, const QString& content)
{
    chapter.paragraphs.clear();
    chapter.height = 0;

    QString cleanContent = content;
    cleanContent = cleanContent.trimmed();

    QStringList paragraphsText = cleanContent.split('\n', Qt::SkipEmptyParts);

    const QString indent = "　　";
    qreal chapterCurrentHeight = 0;

    for (const QString& paraText : paragraphsText)
    {
        QString trimmedPara = paraText.trimmed();
        if (trimmedPara.isEmpty())
        {
            continue;
        }

        ParagraphLayout paraLayout;

        paraLayout.text_layout = std::make_shared<QTextLayout>(indent + trimmedPara);
        paraLayout.text_layout->setFont(font_);
        QTextOption option;
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        paraLayout.text_layout->setTextOption(option);

        paraLayout.text_layout->beginLayout();
        qreal paraHeight = 0;
        while (true)
        {
            QTextLine line = paraLayout.text_layout->createLine();
            if (!line.isValid())
            {
                break;
            }
            line.setLineWidth(viewport()->width());
            line.setPosition(QPointF(0, paraHeight));
            paraHeight += line.height() * lineSpacing_;
        }
        paraLayout.text_layout->endLayout();
        paraLayout.height = paraHeight;

        paraLayout.y = chapterCurrentHeight;
        chapter.paragraphs.append(std::move(paraLayout));

        chapterCurrentHeight += paraHeight + paragraphSpacing_;
    }

    if (!paragraphsText.isEmpty())
    {
        chapter.height = chapterCurrentHeight - paragraphSpacing_;
    }
    else
    {
        chapter.height = 0;
    }
}

void NovelView::relayoutAllChapters()
{
    double scrollRatio = 0.0;
    if (verticalScrollBar()->maximum() > 0)
    {
        scrollRatio = static_cast<double>(verticalScrollBar()->value()) / verticalScrollBar()->maximum();
    }

    totalHeight_ = 0;
    for (int i = 0; i < chapterLayouts_.size(); ++i)
    {
        qreal chapterCurrentHeight = 0;
        for (auto& para : chapterLayouts_[i].paragraphs)
        {
            para.text_layout->setFont(font_);
            para.text_layout->beginLayout();
            qreal paraHeight = 0;
            while (true)
            {
                QTextLine line = para.text_layout->createLine();
                if (!line.isValid())
                    break;
                line.setLineWidth(viewport()->width());
                line.setPosition(QPointF(0, paraHeight));
                paraHeight += line.height() * lineSpacing_;
            }
            para.text_layout->endLayout();
            para.height = paraHeight;
            para.y = chapterCurrentHeight;
            chapterCurrentHeight += paraHeight + paragraphSpacing_;
        }
        if (!chapterLayouts_[i].paragraphs.isEmpty())
        {
            chapterLayouts_[i].height = chapterCurrentHeight - paragraphSpacing_;
        }
        else
        {
            chapterLayouts_[i].height = 0;
        }

        chapterLayouts_[i].y = totalHeight_;
        totalHeight_ += chapterLayouts_[i].height;
    }

    updateScrollbar();
    verticalScrollBar()->setValue(static_cast<int>(scrollRatio * verticalScrollBar()->maximum()));
}

void NovelView::updateScrollbar()
{
    int maxScroll = qMax(0, qRound(totalHeight_ - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0, maxScroll);
}

void NovelView::onScrollValueChanged(int value)
{
    viewport()->update();

    int threshold = viewport()->height() / 2;
    if (value < threshold)
    {
        if (!chapterLayouts_.isEmpty())
        {
            emit needPreviousChapter(chapterLayouts_.first().chapter_index);
        }
    }

    if (value > verticalScrollBar()->maximum() - threshold)
    {
        if (!chapterLayouts_.isEmpty())
        {
            emit needNextChapter(chapterLayouts_.last().chapter_index);
        }
    }
}
