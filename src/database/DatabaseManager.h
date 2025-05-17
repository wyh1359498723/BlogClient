#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include <memory>

#include "models/Post.h"
#include "models/Category.h"
#include "models/Tag.h"

class DatabaseManager
{
public:
    static DatabaseManager& instance();
    ~DatabaseManager();
    
    // 初始化和关闭数据库
    bool initialize();
    void close();
    
    // Post操作
    bool savePost(Post& post);
    bool deletePost(int postId);
    QList<Post> getAllPosts(bool publishedOnly = false);
    Post getPostById(int postId);
    
    // Category操作
    bool saveCategory(Category& category);
    bool deleteCategory(int categoryId);
    QList<Category> getAllCategories();
    QList<Category> getCategoriesForPost(int postId);
    
    // Tag操作
    bool saveTag(Tag& tag);
    bool deleteTag(int tagId);
    QList<Tag> getAllTags();
    QList<Tag> getTagsForPost(int postId);
    
    // 分类与帖子的关联
    bool addCategoryToPost(int postId, int categoryId);
    bool removeCategoryFromPost(int postId, int categoryId);
    
    // 标签与帖子的关联
    bool addTagToPost(int postId, int tagId);
    bool removeTagFromPost(int postId, int tagId);

private:
    DatabaseManager();
    
    // 禁止复制构造和赋值操作
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    // 创建表
    bool createTables();
    
    QSqlDatabase m_db;
    
    static std::unique_ptr<DatabaseManager> s_instance;
}; 