#pragma once

#include <QString>
#include <QDateTime>
#include <QStringList>

class Post {
public:
    enum Status {
        Draft,
        Published
    };

    Post();
    Post(int id, const QString& title, const QString& content, 
         const QString& excerpt, const QDateTime& publishDate,
         const QString& author, Status status);
    ~Post();

    // 获取器和设置器
    int id() const;
    void setId(int id);

    // 远程WordPress ID
    int remoteId() const;
    void setRemoteId(int remoteId);
    bool hasRemoteId() const;

    QString title() const;
    void setTitle(const QString& title);

    QString content() const;
    void setContent(const QString& content);

    QString excerpt() const;
    void setExcerpt(const QString& excerpt);

    QDateTime publishDate() const;
    void setPublishDate(const QDateTime& date);

    QString author() const;
    void setAuthor(const QString& author);

    Status status() const;
    void setStatus(Status status);

    QStringList categories() const;
    void setCategories(const QStringList& categories);
    void addCategory(const QString& category);
    void removeCategory(const QString& category);

    QStringList tags() const;
    void setTags(const QStringList& tags);
    void addTag(const QString& tag);
    void removeTag(const QString& tag);

    QString featuredImageUrl() const;
    void setFeaturedImageUrl(const QString& url);

    void setFeatureMediaId(int id);
    int featureMediaId() const;

private:
    int m_id;           // 本地数据库ID
    int m_remoteId;     // WordPress远程ID
    int m_featureMediaId = -1;//特色图片ID
    QString m_title;
    QString m_content;
    QString m_excerpt;
    QDateTime m_publishDate;
    QString m_author;
    Status m_status;
    QStringList m_categories;
    QStringList m_tags;
    QString m_featuredImageUrl;
}; 