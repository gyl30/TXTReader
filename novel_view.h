#ifndef TXTREADER_NOVEL_VIEW_H
#define TXTREADER_NOVEL_VIEW_H

#include <QAbstractScrollArea>
#include <QFont>
#include <QList>
#include <QTextLayout>
#include <memory>

struct ParagraphLayout
{
    std::shared_ptr<QTextLayout> text_layout;
    qreal height = 0.0;
    qreal y = 0.0;
};

struct ChapterLayout
{
    int chapter_index;
    QList<ParagraphLayout> paragraphs;
    qreal height = 0.0;
    qreal y = 0.0;
};

class NovelView : public QAbstractScrollArea
{
    Q_OBJECT

   public:
    explicit NovelView(QWidget* parent = nullptr);

    void setFontStyle(const QFont& font, qreal lineSpacing, qreal letterSpacing);
    void clearContent();
    void prependChapterContent(int chapterIndex, const QString& content);
    void appendChapterContent(int chapterIndex, const QString& content);
    void relayoutAndRedraw();

    bool isChapterDisplayed(int chapterIndex) const;
    int getFirstDisplayedChapterIndex() const;
    int getLastDisplayedChapterIndex() const;

   signals:
    void needPreviousChapter(int currentFirstIndex);
    void needNextChapter(int currentLastIndex);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private:
    void layoutChapter(ChapterLayout& chapter, const QString& content);
    void relayoutAllChapters();
    void updateScrollbar();

   private slots:
    void onScrollValueChanged(int value);

   private:
    QList<ChapterLayout> chapterLayouts_;
    qreal totalHeight_ = 0.0;

    QFont font_;
    qreal lineSpacing_ = 1.5;
    qreal letterSpacing_ = 1.5;
    qreal paragraphSpacing_ = 10.0;
};

#endif
