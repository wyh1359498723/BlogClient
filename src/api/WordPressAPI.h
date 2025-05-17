#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QByteArray>
#include <QFile>
#include <QString>
#include <QUrl>
#include <QList>
#include <QMap>

#include "models/Post.h"
#include "models/Category.h"
#include "models/Tag.h"

class WordPressAPI : public QObject
{
    Q_OBJECT

public:
    static WordPressAPI& instance();
    ~WordPressAPI();

    // 设置API访问信息
    void setApiUrl(const QString& url);
    void setCredentials(const QString& username, const QString& password);
    QString apiUrl() const;
    
    // 博客文章操作
    void fetchPosts();
    void createPost(const Post& post);
    void updatePost(const Post& post);
    void deletePost(int postId);
    
    // 分类操作
    void fetchCategories();
    void createCategory(const Category& category);
    void updateCategory(const Category& category);
    void deleteCategory(int categoryId);
    
    // 标签操作
    void fetchTags();
    void createTag(const Tag& tag);
    void updateTag(const Tag& tag);
    void deleteTag(int tagId);
    
    // 媒体上传
    void uploadMedia(const QString& filePath, const QString& title = "");

signals:
    // 博客文章信号
    void postsReceived(const QList<Post>& posts);
    void postCreated(const Post& post);
    void postUpdated(const Post& post);
    void postDeleted(int postId);
    
    // 分类信号
    void categoriesReceived(const QList<Category>& categories);
    void categoryCreated(const Category& category);
    void categoryUpdated(const Category& category);
    void categoryDeleted(int categoryId);
    
    // 标签信号
    void tagsReceived(const QList<Tag>& tags);
    void tagCreated(const Tag& tag);
    void tagUpdated(const Tag& tag);
    void tagDeleted(int tagId);
    
    // 媒体信号
    void mediaUploaded(const QString& url,int mediaId);
    
    // 上传进度信号
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    
    // 错误信号
    void error(const QString& errorMessage);

private slots:
    void onPostsReceived();
    void onPostCreated();
    void onPostUpdated();
    void onPostDeleted();
    void onCategoriesReceived();
    void onTagsReceived();
    void onMediaUploaded();
    void handleNetworkError(QNetworkReply::NetworkError errorCode);

private:
    WordPressAPI(QObject* parent = nullptr);
    
    // 禁止复制构造和赋值操作
    WordPressAPI(const WordPressAPI&) = delete;
    WordPressAPI& operator=(const WordPressAPI&) = delete;
    
    // 创建认证头
    QByteArray createAuthHeader() const;
    
    // 解析返回的JSON数据
    QList<Post> parsePosts(const QJsonArray& jsonArray);
    QList<Category> parseCategories(const QJsonArray& jsonArray);
    QList<Tag> parseTags(const QJsonArray& jsonArray);
    
    QString m_apiUrl;
    QString m_username;
    QString m_password;
    QNetworkAccessManager* m_networkManager;
    
    static std::unique_ptr<WordPressAPI> s_instance;
}; 