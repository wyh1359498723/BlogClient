#include "DatabaseManager.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

std::unique_ptr<DatabaseManager> DatabaseManager::s_instance = nullptr;

DatabaseManager& DatabaseManager::instance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<DatabaseManager>(new DatabaseManager());
    }
    return *s_instance;
}

DatabaseManager::DatabaseManager()
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::initialize()
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dataPath + "/blogclient.db");

    if (!m_db.open()) {
        qDebug() << "数据库连接失败";
        return false;
    } else {
        qDebug() << "数据库连接成功";
        return createTables();
    }
}

void DatabaseManager::close()
{
    m_db.close();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query;
    
    // 创建posts表
    if (!query.exec("CREATE TABLE IF NOT EXISTS posts ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "remote_id INTEGER DEFAULT -1, "
                    "title TEXT NOT NULL, "
                    "content TEXT, "
                    "excerpt TEXT, "
                    "publish_date DATETIME, "
                    "author TEXT, "
                    "status INTEGER, "
                    "featured_image_url TEXT)")) {
        qDebug() << "创建posts表失败: " << query.lastError().text();
        return false;
    }
    
    // 检查是否需要添加remote_id列
    QSqlQuery checkColumn;
    if (checkColumn.exec("PRAGMA table_info(posts)")) {
        bool hasRemoteIdColumn = false;
        while (checkColumn.next()) {
            if (checkColumn.value(1).toString() == "remote_id") {
                hasRemoteIdColumn = true;
                break;
            }
        }
        
        if (!hasRemoteIdColumn) {
            qDebug() << "添加remote_id列到posts表";
            if (!query.exec("ALTER TABLE posts ADD COLUMN remote_id INTEGER DEFAULT -1")) {
                qDebug() << "添加remote_id列失败: " << query.lastError().text();
            }
        }
    }
    
    // 创建categories表
    if (!query.exec("CREATE TABLE IF NOT EXISTS categories ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT NOT NULL UNIQUE)")) {
        qDebug() << "创建categories表失败: " << query.lastError().text();
        return false;
    }
    
    // 创建tags表
    if (!query.exec("CREATE TABLE IF NOT EXISTS tags ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT NOT NULL UNIQUE)")) {
        qDebug() << "创建tags表失败: " << query.lastError().text();
        return false;
    }
    
    // 创建post_categories关联表
    if (!query.exec("CREATE TABLE IF NOT EXISTS post_categories ("
                    "post_id INTEGER, "
                    "category_id INTEGER, "
                    "PRIMARY KEY (post_id, category_id), "
                    "FOREIGN KEY (post_id) REFERENCES posts (id) ON DELETE CASCADE, "
                    "FOREIGN KEY (category_id) REFERENCES categories (id) ON DELETE CASCADE)")) {
        qDebug() << "创建post_categories表失败: " << query.lastError().text();
        return false;
    }
    
    // 创建post_tags关联表
    if (!query.exec("CREATE TABLE IF NOT EXISTS post_tags ("
                    "post_id INTEGER, "
                    "tag_id INTEGER, "
                    "PRIMARY KEY (post_id, tag_id), "
                    "FOREIGN KEY (post_id) REFERENCES posts (id) ON DELETE CASCADE, "
                    "FOREIGN KEY (tag_id) REFERENCES tags (id) ON DELETE CASCADE)")) {
        qDebug() << "创建post_tags表失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

bool DatabaseManager::savePost(Post& post)
{
    QSqlQuery query;
    
    qDebug() << "保存文章到数据库: ID=" << post.id() << "远程ID=" << post.remoteId() << "标题=" << post.title() << "状态=" << post.status();
    
    // 首先检查文章是否已存在（通过本地ID匹配）
    if (post.id() > 0) {
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT id FROM posts WHERE id = :id");
        checkQuery.bindValue(":id", post.id());
        
        if (checkQuery.exec() && checkQuery.next()) {
            // 文章已存在，执行更新
            qDebug() << "更新已存在的文章: ID=" << post.id() << "远程ID=" << post.remoteId();
            
            query.prepare("UPDATE posts SET title = :title, content = :content, excerpt = :excerpt, "
                         "publish_date = :publish_date, author = :author, status = :status, "
                         "featured_image_url = :featured_image_url, remote_id = :remote_id WHERE id = :id");
            query.bindValue(":id", post.id());
            query.bindValue(":remote_id", post.remoteId());
        } else {
            // 文章不存在，执行插入
            qDebug() << "插入新文章: ID=" << post.id() << "远程ID=" << post.remoteId();
            
            query.prepare("INSERT INTO posts (title, content, excerpt, publish_date, author, status, featured_image_url, remote_id) "
                          "VALUES (:title, :content, :excerpt, :publish_date, :author, :status, :featured_image_url, :remote_id)");
            query.bindValue(":remote_id", post.remoteId());
        }
    } else {
        // 本地创建的新文章
        qDebug() << "插入本地创建的新文章，远程ID=" << post.remoteId();
        
        query.prepare("INSERT INTO posts (title, content, excerpt, publish_date, author, status, featured_image_url, remote_id) "
                      "VALUES (:title, :content, :excerpt, :publish_date, :author, :status, :featured_image_url, :remote_id)");
        query.bindValue(":remote_id", post.remoteId());
    }
    
    query.bindValue(":title", post.title());
    query.bindValue(":content", post.content());
    query.bindValue(":excerpt", post.excerpt());
    query.bindValue(":publish_date", post.publishDate());
    query.bindValue(":author", post.author());
    query.bindValue(":status", post.status());
    query.bindValue(":featured_image_url", post.featuredImageUrl());
    
    if (!query.exec()) {
        qDebug() << "保存文章失败: " << query.lastError().text() << "SQL=" << query.lastQuery();
        return false;
    }
    
    // 如果是新插入的本地文章（没有ID），获取自动生成的ID
    if (post.id() == -1) {
        post.setId(query.lastInsertId().toInt());
        qDebug() << "为新文章分配ID: " << post.id();
    }
    
    // 处理分类关联
    if (!post.categories().isEmpty()) {
        // 先删除旧的分类关联
        QSqlQuery deleteCategories;
        deleteCategories.prepare("DELETE FROM post_categories WHERE post_id = :post_id");
        deleteCategories.bindValue(":post_id", post.id());
        if (!deleteCategories.exec()) {
            qDebug() << "删除文章分类关联失败: " << deleteCategories.lastError().text();
        }
        
        qDebug() << "开始处理文章分类关联，文章ID=" << post.id() << "，分类数量=" << post.categories().size();
        
        // 添加新的分类关联
        for (const QString& categoryName : post.categories()) {
            qDebug() << "正在处理分类: " << categoryName;
            
            // 先查找分类是否已存在
            QSqlQuery findCategory;
            findCategory.prepare("SELECT id FROM categories WHERE name = :name");
            findCategory.bindValue(":name", categoryName);
            
            if (findCategory.exec() && findCategory.next()) {
                // 分类已存在，直接使用ID
                int categoryId = findCategory.value(0).toInt();
                qDebug() << "找到已存在的分类: ID=" << categoryId << "名称=" << categoryName;
                addCategoryToPost(post.id(), categoryId);
            } else {
                // 分类不存在，创建新分类
                Category category(-1, categoryName);
                if (saveCategory(category)) {
                    qDebug() << "创建新分类: ID=" << category.id() << "名称=" << category.name();
                    addCategoryToPost(post.id(), category.id());
                }
            }
        }
    }
    
    // 处理标签关联
    if (!post.tags().isEmpty()) {
        // 先删除旧的标签关联
        QSqlQuery deleteTags;
        deleteTags.prepare("DELETE FROM post_tags WHERE post_id = :post_id");
        deleteTags.bindValue(":post_id", post.id());
        if (!deleteTags.exec()) {
            qDebug() << "删除文章标签关联失败: " << deleteTags.lastError().text();
        }
        
        qDebug() << "开始处理文章标签关联，文章ID=" << post.id() << "，标签数量=" << post.tags().size();
        
        // 添加新的标签关联
        for (const QString& tagName : post.tags()) {
            qDebug() << "正在处理标签: " << tagName;
            
            // 先查找标签是否已存在
            QSqlQuery findTag;
            findTag.prepare("SELECT id FROM tags WHERE name = :name");
            findTag.bindValue(":name", tagName);
            
            if (findTag.exec() && findTag.next()) {
                // 标签已存在，直接使用ID
                int tagId = findTag.value(0).toInt();
                qDebug() << "找到已存在的标签: ID=" << tagId << "名称=" << tagName;
                addTagToPost(post.id(), tagId);
            } else {
                // 标签不存在，创建新标签
                Tag tag(-1, tagName);
                if (saveTag(tag)) {
                    qDebug() << "创建新标签: ID=" << tag.id() << "名称=" << tag.name();
                    addTagToPost(post.id(), tag.id());
                }
            }
        }
    }
    
    qDebug() << "文章保存成功: ID=" << post.id() << "标题=" << post.title();
    return true;
}

bool DatabaseManager::deletePost(int postId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM posts WHERE id = :id");
    query.bindValue(":id", postId);
    
    if (!query.exec()) {
        qDebug() << "删除文章失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

QList<Post> DatabaseManager::getAllPosts(bool publishedOnly)
{
    QList<Post> posts;
    QSqlQuery query;
    
    QString queryStr = "SELECT id, title, content, excerpt, publish_date, author, status, featured_image_url FROM posts";
    if (publishedOnly) {
        queryStr += " WHERE status = 1"; // 只获取已发布的帖子 (Post::Published = 1)
    }
    queryStr += " ORDER BY publish_date DESC";
    
    qDebug() << "执行查询获取文章: " << queryStr;
    
    if (!query.exec(queryStr)) {
        qDebug() << "获取文章失败: " << query.lastError().text();
        return posts;
    }
    
    int count = 0;
    while (query.next()) {
        int id = query.value(0).toInt();
        QString title = query.value(1).toString();
        QString content = query.value(2).toString();
        QString excerpt = query.value(3).toString();
        QDateTime publishDate = query.value(4).toDateTime();
        QString author = query.value(5).toString();
        Post::Status status = static_cast<Post::Status>(query.value(6).toInt());
        QString featuredImageUrl = query.value(7).toString();
        
        qDebug() << "加载文章: ID=" << id << "标题=" << title << "状态=" << status;
        
        Post post(id, title, content, excerpt, publishDate, author, status);
        post.setFeaturedImageUrl(featuredImageUrl);
        
        // 获取帖子的分类
        QList<Category> categories = getCategoriesForPost(id);
        QStringList categoryNames;
        for (const auto& category : categories) {
            categoryNames << category.name();
        }
        post.setCategories(categoryNames);
        
        // 获取帖子的标签
        QList<Tag> tags = getTagsForPost(id);
        QStringList tagNames;
        for (const auto& tag : tags) {
            tagNames << tag.name();
        }
        post.setTags(tagNames);
        
        posts.append(post);
        count++;
    }
    
    qDebug() << "从数据库加载了 " << count << " 篇文章" << (publishedOnly ? " (仅已发布)" : " (全部)");
    
    return posts;
}

Post DatabaseManager::getPostById(int postId)
{
    QSqlQuery query;
    query.prepare("SELECT id, title, content, excerpt, publish_date, author, status, featured_image_url, remote_id FROM posts WHERE id = :id");
    query.bindValue(":id", postId);
    
    qDebug() << "获取文章详情: 本地ID=" << postId;
    
    if (!query.exec() || !query.next()) {
        qDebug() << "根据ID获取文章失败: " << query.lastError().text();
        return Post();
    }
    
    int id = query.value(0).toInt();
    QString title = query.value(1).toString();
    QString content = query.value(2).toString();
    QString excerpt = query.value(3).toString();
    QDateTime publishDate = query.value(4).toDateTime();
    QString author = query.value(5).toString();
    Post::Status status = static_cast<Post::Status>(query.value(6).toInt());
    QString featuredImageUrl = query.value(7).toString();
    int remoteId = query.value(8).toInt();
    
    qDebug() << "找到文章: 本地ID=" << id << "远程ID=" << remoteId << "标题=" << title << "状态=" << status;
    
    Post post(id, title, content, excerpt, publishDate, author, status);
    post.setFeaturedImageUrl(featuredImageUrl);
    post.setRemoteId(remoteId);
    
    // 获取帖子的分类
    QList<Category> categories = getCategoriesForPost(id);
    for (const auto& category : categories) {
        post.addCategory(category.name());
    }
    qDebug() << "加载了文章分类: " << post.categories().join(", ");
    
    // 获取帖子的标签
    QList<Tag> tags = getTagsForPost(id);
    for (const auto& tag : tags) {
        post.addTag(tag.name());
    }
    qDebug() << "加载了文章标签: " << post.tags().join(", ");
    
    return post;
}

bool DatabaseManager::saveCategory(Category& category)
{
    QSqlQuery query;
    
    // 先检查是否已存在同名分类
    if (category.id() == -1 && !category.name().isEmpty()) {
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT id FROM categories WHERE name = :name");
        checkQuery.bindValue(":name", category.name());
        
        if (checkQuery.exec() && checkQuery.next()) {
            // 已存在同名分类，使用现有ID
            int existingId = checkQuery.value(0).toInt();
            category.setId(existingId);
            qDebug() << "使用已存在的分类: ID=" << existingId << "名称=" << category.name();
            return true;
        }
    }
    
    if (category.id() == -1) {
        // 插入新分类
        query.prepare("INSERT INTO categories (name) VALUES (:name)");
    } else {
        // 更新现有分类
        query.prepare("UPDATE categories SET name = :name WHERE id = :id");
        query.bindValue(":id", category.id());
    }
    
    query.bindValue(":name", category.name());
    
    if (!query.exec()) {
        qDebug() << "保存分类失败: " << query.lastError().text();
        return false;
    }
    
    // 如果是插入新分类，获取自动生成的ID
    if (category.id() == -1) {
        category.setId(query.lastInsertId().toInt());
        qDebug() << "创建新分类: ID=" << category.id() << "名称=" << category.name();
    }
    
    return true;
}

bool DatabaseManager::deleteCategory(int categoryId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM categories WHERE id = :id");
    query.bindValue(":id", categoryId);
    
    if (!query.exec()) {
        qDebug() << "删除分类失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

QList<Category> DatabaseManager::getAllCategories()
{
    QList<Category> categories;
    QSqlQuery query;
    
    if (!query.exec("SELECT id, name FROM categories ORDER BY name")) {
        qDebug() << "获取分类失败: " << query.lastError().text();
        return categories;
    }
    
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        
        categories.append(Category(id, name));
    }
    
    return categories;
}

QList<Category> DatabaseManager::getCategoriesForPost(int postId)
{
    QList<Category> categories;
    QSqlQuery query;
    
    query.prepare("SELECT c.id, c.name FROM categories c "
                 "JOIN post_categories pc ON c.id = pc.category_id "
                 "WHERE pc.post_id = :post_id");
    query.bindValue(":post_id", postId);
    
    if (!query.exec()) {
        qDebug() << "获取文章分类失败: " << query.lastError().text();
        return categories;
    }
    
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        
        categories.append(Category(id, name));
    }
    
    return categories;
}

bool DatabaseManager::saveTag(Tag& tag)
{
    QSqlQuery query;
    
    // 先检查是否已存在同名标签
    if (tag.id() == -1 && !tag.name().isEmpty()) {
        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT id FROM tags WHERE name = :name");
        checkQuery.bindValue(":name", tag.name());
        
        if (checkQuery.exec() && checkQuery.next()) {
            // 已存在同名标签，使用现有ID
            int existingId = checkQuery.value(0).toInt();
            tag.setId(existingId);
            qDebug() << "使用已存在的标签: ID=" << existingId << "名称=" << tag.name();
            return true;
        }
    }
    
    if (tag.id() == -1) {
        // 插入新标签
        query.prepare("INSERT INTO tags (name) VALUES (:name)");
    } else {
        // 更新现有标签
        query.prepare("UPDATE tags SET name = :name WHERE id = :id");
        query.bindValue(":id", tag.id());
    }
    
    query.bindValue(":name", tag.name());
    
    if (!query.exec()) {
        qDebug() << "保存标签失败: " << query.lastError().text();
        return false;
    }
    
    // 如果是插入新标签，获取自动生成的ID
    if (tag.id() == -1) {
        tag.setId(query.lastInsertId().toInt());
        qDebug() << "创建新标签: ID=" << tag.id() << "名称=" << tag.name();
    }
    
    return true;
}

bool DatabaseManager::deleteTag(int tagId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM tags WHERE id = :id");
    query.bindValue(":id", tagId);
    
    if (!query.exec()) {
        qDebug() << "删除标签失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

QList<Tag> DatabaseManager::getAllTags()
{
    QList<Tag> tags;
    QSqlQuery query;
    
    if (!query.exec("SELECT id, name FROM tags ORDER BY name")) {
        qDebug() << "获取标签失败: " << query.lastError().text();
        return tags;
    }
    
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        
        tags.append(Tag(id, name));
    }
    
    return tags;
}

QList<Tag> DatabaseManager::getTagsForPost(int postId)
{
    QList<Tag> tags;
    QSqlQuery query;
    
    query.prepare("SELECT t.id, t.name FROM tags t "
                 "JOIN post_tags pt ON t.id = pt.tag_id "
                 "WHERE pt.post_id = :post_id");
    query.bindValue(":post_id", postId);
    
    if (!query.exec()) {
        qDebug() << "获取文章标签失败: " << query.lastError().text();
        return tags;
    }
    
    while (query.next()) {
        int id = query.value(0).toInt();
        QString name = query.value(1).toString();
        
        tags.append(Tag(id, name));
    }
    
    return tags;
}

bool DatabaseManager::addCategoryToPost(int postId, int categoryId)
{
    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO post_categories (post_id, category_id) VALUES (:post_id, :category_id)");
    query.bindValue(":post_id", postId);
    query.bindValue(":category_id", categoryId);
    
    if (!query.exec()) {
        qDebug() << "添加文章分类失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

bool DatabaseManager::removeCategoryFromPost(int postId, int categoryId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM post_categories WHERE post_id = :post_id AND category_id = :category_id");
    query.bindValue(":post_id", postId);
    query.bindValue(":category_id", categoryId);
    
    if (!query.exec()) {
        qDebug() << "删除文章分类失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

bool DatabaseManager::addTagToPost(int postId, int tagId)
{
    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO post_tags (post_id, tag_id) VALUES (:post_id, :tag_id)");
    query.bindValue(":post_id", postId);
    query.bindValue(":tag_id", tagId);
    
    if (!query.exec()) {
        qDebug() << "添加文章标签失败: " << query.lastError().text();
        return false;
    }
    
    return true;
}

bool DatabaseManager::removeTagFromPost(int postId, int tagId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM post_tags WHERE post_id = :post_id AND tag_id = :tag_id");
    query.bindValue(":post_id", postId);
    query.bindValue(":tag_id", tagId);
    
    if (!query.exec()) {
        qDebug() << "删除文章标签失败: " << query.lastError().text();
        return false;
    }
    
    return true;
} 