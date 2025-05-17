#pragma once

#include <QtWidgets/QMainWindow>
#include <QListWidgetItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <memory>
#include "ui_BlogClient.h"
#include "models/Post.h"
#include "models/Category.h"
#include "models/Tag.h"
#include "api/WordPressAPI.h"
#include "database/DatabaseManager.h"

class BlogClient : public QMainWindow
{
    Q_OBJECT

public:
    BlogClient(QWidget *parent = nullptr);
    ~BlogClient();

private slots:
    // 菜单动作
    void on_actionNew_triggered();
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionDelete_triggered();
    void on_actionExit_triggered();
    void on_actionPublish_triggered();
    void on_actionFetch_triggered();
    void on_actionSync_triggered();
    void on_actionSettings_triggered();
    void on_actionAbout_triggered();
    
    // UI事件
    void on_postsListWidget_itemClicked(QListWidgetItem *item);
    void on_draftsListWidget_itemClicked(QListWidgetItem *item);
    void on_saveButton_clicked();
    void on_deleteButton_clicked();
    void on_publishButton_clicked();
    void on_cancelButton_clicked();
    void on_addCategoryButton_clicked();
    void on_removeCategoryButton_clicked();
    void on_addTagButton_clicked();
    void on_removeTagButton_clicked();
    void on_uploadImageButton_clicked();
    
    // API回调
    void onPostsReceived(const QList<Post>& posts);
    void onPostCreated(const Post& post);
    void onPostUpdated(const Post& post);
    void onPostDeleted(int postId);
    void onCategoriesReceived(const QList<Category>& categories);
    void onTagsReceived(const QList<Tag>& tags);
    void onMediaUploaded(const QString& url,int mediaId);
    void onApiError(const QString& errorMessage);

private:
    // UI辅助方法
    void clearEditor();
    void populateEditor(const Post& post);
    void loadPostsList();
    void loadDraftsList();
    void updateCategoriesList();
    void updateTagsList();
    void setupAddButtons();  // 添加此方法用于设置添加按钮
    
    // 数据操作
    bool saveCurrentPost();
    bool publishCurrentPost();
    void deleteCurrentPost();
    
    // 窗口事件
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
    Ui::BlogClientClass ui;
    std::unique_ptr<Post> m_currentPost;
    bool m_isEditing;
    QScrollArea* m_scrollArea; // 滚动区域引用
};
