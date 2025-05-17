#include "Post.h"

Post::Post()
    : m_id(-1), m_remoteId(-1), m_status(Status::Draft)
{
    m_publishDate = QDateTime::currentDateTime();
}

Post::Post(int id, const QString& title, const QString& content, 
         const QString& excerpt, const QDateTime& publishDate,
         const QString& author, Status status)
    : m_id(id), m_remoteId(-1), m_title(title), m_content(content),
      m_excerpt(excerpt), m_publishDate(publishDate),
      m_author(author), m_status(status)
{
}

Post::~Post()
{
}

int Post::id() const
{
    return m_id;
}

void Post::setId(int id)
{
    m_id = id;
}

int Post::remoteId() const
{
    return m_remoteId;
}

void Post::setRemoteId(int remoteId)
{
    m_remoteId = remoteId;
}

bool Post::hasRemoteId() const
{
    return m_remoteId > 0;
}

QString Post::title() const
{
    return m_title;
}

void Post::setTitle(const QString& title)
{
    m_title = title;
}

QString Post::content() const
{
    return m_content;
}

void Post::setContent(const QString& content)
{
    m_content = content;
}

QString Post::excerpt() const
{
    return m_excerpt;
}

void Post::setExcerpt(const QString& excerpt)
{
    m_excerpt = excerpt;
}

QDateTime Post::publishDate() const
{
    return m_publishDate;
}

void Post::setPublishDate(const QDateTime& date)
{
    m_publishDate = date;
}

QString Post::author() const
{
    return m_author;
}

void Post::setAuthor(const QString& author)
{
    m_author = author;
}

Post::Status Post::status() const
{
    return m_status;
}

void Post::setStatus(Post::Status status)
{
    m_status = status;
}

QStringList Post::categories() const
{
    return m_categories;
}

void Post::setCategories(const QStringList& categories)
{
    m_categories = categories;
}

void Post::addCategory(const QString& category)
{
    if (!m_categories.contains(category)) {
        m_categories.append(category);
    }
}

void Post::removeCategory(const QString& category)
{
    m_categories.removeAll(category);
}

QStringList Post::tags() const
{
    return m_tags;
}

void Post::setTags(const QStringList& tags)
{
    m_tags = tags;
}

void Post::addTag(const QString& tag)
{
    if (!m_tags.contains(tag)) {
        m_tags.append(tag);
    }
}

void Post::removeTag(const QString& tag)
{
    m_tags.removeAll(tag);
}

QString Post::featuredImageUrl() const
{
    return m_featuredImageUrl;
}

void Post::setFeaturedImageUrl(const QString& url)
{
    m_featuredImageUrl = url;
} 

void Post::setFeatureMediaId(int id) { m_featureMediaId = id; }
int Post::featureMediaId() const { return m_featureMediaId; }