#include "BlogClient.h"
#include <QDebug>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include "src/SettingsDialog.h"
#include <QCoreApplication>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QCompleter>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QProgressDialog>

BlogClient::BlogClient(QWidget *parent)
    : QMainWindow(parent), m_isEditing(false)
{
    ui.setupUi(this);
    
    // 输出QSettings的文件路径和组织/应用信息
    qDebug() << "QSettings组织名:" << QCoreApplication::organizationName();
    qDebug() << "QSettings应用名:" << QCoreApplication::applicationName();
    
    // 连接WordPressAPI的信号
    connect(&WordPressAPI::instance(), &WordPressAPI::postsReceived, this, &BlogClient::onPostsReceived);
    connect(&WordPressAPI::instance(), &WordPressAPI::postCreated, this, &BlogClient::onPostCreated);
    connect(&WordPressAPI::instance(), &WordPressAPI::postUpdated, this, &BlogClient::onPostUpdated);
    connect(&WordPressAPI::instance(), &WordPressAPI::postDeleted, this, &BlogClient::onPostDeleted);
    connect(&WordPressAPI::instance(), &WordPressAPI::categoriesReceived, this, &BlogClient::onCategoriesReceived);
    connect(&WordPressAPI::instance(), &WordPressAPI::tagsReceived, this, &BlogClient::onTagsReceived);
    connect(&WordPressAPI::instance(), &WordPressAPI::mediaUploaded, this, &BlogClient::onMediaUploaded);
    connect(&WordPressAPI::instance(), &WordPressAPI::error, this, &BlogClient::onApiError);
    
    // 允许窗口自由调整大小
    this->setMinimumSize(640, 480);
    
    // 移除原来的编辑器容器，准备使用滚动区域替代
    QWidget* originalEditorContainer = ui.editorContainer;
    QLayout* originalLayout = originalEditorContainer->layout();
    
    // 创建新的滚动区域
    QScrollArea* scrollArea = new QScrollArea(ui.mainSplitter);
    m_scrollArea = scrollArea; // 保存引用
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 创建新的容器widget作为滚动区域的内容
    QWidget* newEditorContainer = new QWidget();
    QVBoxLayout* newLayout = new QVBoxLayout(newEditorContainer);
    
    // 将原有布局中的所有部件转移到新布局中
    if (QVBoxLayout* oldVLayout = qobject_cast<QVBoxLayout*>(originalLayout)) {
        while (oldVLayout->count() > 0) {
            QLayoutItem* item = oldVLayout->takeAt(0);
            if (QWidget* widget = item->widget()) {
                newLayout->addWidget(widget);
            } else if (QLayout* layout = item->layout()) {
                newLayout->addLayout(layout);
            } else {
                newLayout->addItem(item);
            }
        }
    }
    
    // 设置新布局的伸展因子
    for (int i = 0; i < newLayout->count(); ++i) {
        QWidget* widget = newLayout->itemAt(i)->widget();
        if (widget && widget->objectName() == "contentEdit") {
            newLayout->setStretchFactor(widget, 10); // 给内容编辑框更多的伸展空间
        }
    }
    
    // 设置新的编辑器容器
    newEditorContainer->setLayout(newLayout);
    scrollArea->setWidget(newEditorContainer);
    
    // 用滚动区域替换原来的编辑器容器
    int editorIndex = ui.mainSplitter->indexOf(originalEditorContainer);
    if (editorIndex >= 0) {
        ui.mainSplitter->insertWidget(editorIndex, scrollArea);
        originalEditorContainer->hide(); // 隐藏原来的，稍后可以删除
        ui.editorContainer = newEditorContainer; // 更新ui引用
    }
    
    // 检查标题栏是否存在且正确显示，如果找不到则手动创建
    QLineEdit* titleEdit = ui.editorContainer->findChild<QLineEdit*>("titleEdit");
    if (!titleEdit) {
        qDebug() << "标题输入框未找到，正在创建新的标题栏...";
        
        // 创建新的标题栏布局
        QHBoxLayout* titleLayout = new QHBoxLayout();
        
        // 创建标题标签
        QLabel* titleLabel = new QLabel(tr("标题:"), newEditorContainer);
        titleLabel->setObjectName("titleLabel");
        
        // 创建标题输入框
        titleEdit = new QLineEdit(newEditorContainer);
        titleEdit->setObjectName("titleEdit");
        titleEdit->setPlaceholderText(tr("请在此输入文章标题"));
        
        // 将组件添加到布局
        titleLayout->addWidget(titleLabel);
        titleLayout->addWidget(titleEdit);
        
        // 将标题栏布局添加到主布局的最前面
        newLayout->insertLayout(0, titleLayout);
        
        // 将标题编辑框赋值给UI成员变量
        ui.titleEdit = titleEdit;
    } else {
        qDebug() << "找到标题输入框";
        titleEdit->setEnabled(true);
        titleEdit->setPlaceholderText(tr("请在此输入文章标题"));
    }
    
    // 确保UI中分类和标签的按钮正确设置
    setupAddButtons();
    
    // 清空编辑器
    clearEditor();
}

BlogClient::~BlogClient()
{
    // 断开所有WordPressAPI的信号连接
    disconnect(&WordPressAPI::instance(), nullptr, this, nullptr);
    
    // 确保当前文章对象正确释放
    m_currentPost.reset();
    
    // 关闭数据库连接
    DatabaseManager::instance().close();
}

void BlogClient::on_actionNew_triggered()
{
    // 如果当前有未保存的更改，提示保存
    if (m_currentPost && !m_isEditing) {
        QMessageBox::StandardButton result = QMessageBox::question(this, tr("保存更改"),
            tr("您有未保存的更改。是否保存？"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            
        if (result == QMessageBox::Yes) {
            if (!saveCurrentPost()) {
                return; // 保存失败，不创建新文章
            }
        } else if (result == QMessageBox::Cancel) {
            return; // 取消操作
        }
    }
    
    // 清空编辑器
    clearEditor();
    
    // 创建新文章对象
    m_currentPost = std::make_unique<Post>();
    m_currentPost->setAuthor(QSettings().value("user/name").toString());
    m_currentPost->setPublishDate(QDateTime::currentDateTime());
    
    // 更新UI
    ui.isDraftCheckBox->setChecked(true);
    ui.publishDateEdit->setDateTime(QDateTime::currentDateTime());
    
    // 确保标题输入框是启用的并可以编辑
    QLineEdit* titleEdit = findChild<QLineEdit*>("titleEdit");
    if (titleEdit) {
        titleEdit->setEnabled(true);
        titleEdit->setFocus();
        titleEdit->setPlaceholderText(tr("请在此输入文章标题"));
    } else {
        qDebug() << "警告: 找不到标题输入控件";
    }
    
    // 确保添加按钮设置正确
    setupAddButtons();
    
    // 加载分类和标签列表，确保它们可编辑
    updateCategoriesList();
    updateTagsList();
    
    // 确保分类和标签相关控件可用
    ui.removeCategoryButton->setEnabled(true);
    ui.removeTagButton->setEnabled(true);
    ui.categoryCombo->setEnabled(true);
    ui.tagEdit->setEnabled(true);
    
    m_isEditing = false;
}

void BlogClient::on_actionOpen_triggered()
{
    // 从当前选择的列表项打开文章
    QListWidgetItem* currentItem = nullptr;
    
    if (ui.sidebarTabs->currentIndex() == 0) {
        currentItem = ui.postsListWidget->currentItem();
    } else {
        currentItem = ui.draftsListWidget->currentItem();
    }
    
    if (currentItem) {
        int postId = currentItem->data(Qt::UserRole).toInt();
        Post post = DatabaseManager::instance().getPostById(postId);
        populateEditor(post);
    }
}

void BlogClient::on_actionSave_triggered()
{
    saveCurrentPost();
}

void BlogClient::on_actionDelete_triggered()
{
    deleteCurrentPost();
}

void BlogClient::on_actionExit_triggered()
{
    close();
}

void BlogClient::on_actionPublish_triggered()
{
    publishCurrentPost();
}

void BlogClient::on_actionFetch_triggered()
{
    QSettings settings;
    QString apiUrl = settings.value("api/url").toString();
    QString username = settings.value("api/username").toString();
    QString password = settings.value("api/password").toString();
    
    qDebug() << "API设置：" << apiUrl << username << (password.isEmpty() ? "密码为空" : "密码已设置");
    
    if (apiUrl.isEmpty() || username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("API设置缺失"), 
            tr("请先在设置中配置WordPress API信息。\n\n"
               "URL: %1\n"
               "用户名: %2\n"
               "密码: %3\n\n"
               "请点击\"设置\"菜单，完成API配置。")
            .arg(apiUrl.isEmpty() ? "未设置" : apiUrl)
            .arg(username.isEmpty() ? "未设置" : username)
            .arg(password.isEmpty() ? "未设置" : "已设置"),
            QMessageBox::Ok);
        
        // 自动打开设置对话框
        on_actionSettings_triggered();
        return;
    }
    
    // 设置API信息
    WordPressAPI::instance().setApiUrl(apiUrl);
    WordPressAPI::instance().setCredentials(username, password);
    
    // 获取远程文章
    WordPressAPI::instance().fetchPosts();
    
    // 获取分类和标签
    WordPressAPI::instance().fetchCategories();
    WordPressAPI::instance().fetchTags();
    
    // 更新分类和标签列表
    updateCategoriesList();
    updateTagsList();
}

void BlogClient::on_actionSync_triggered()
{
    if (!m_currentPost) {
        QMessageBox::warning(this, tr("同步错误"), 
            tr("请先选择一篇文章进行同步。"));
        return;
    }
    
    QSettings settings;
    QString apiUrl = settings.value("api/url").toString();
    QString username = settings.value("api/username").toString();
    QString password = settings.value("api/password").toString();
    
    if (apiUrl.isEmpty() || username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("API设置缺失"), 
            tr("请先在设置中配置WordPress API信息。"));
        return;
    }
    
    // 设置API信息
    WordPressAPI::instance().setApiUrl(apiUrl);
    WordPressAPI::instance().setCredentials(username, password);
    
    // 同步当前文章到WordPress
    if (m_currentPost->hasRemoteId() && m_currentPost->status() == Post::Published) {
        // 有远程ID且已发布，执行更新
        qDebug() << "更新远程文章: 本地ID=" << m_currentPost->id() << "远程ID=" << m_currentPost->remoteId();
        WordPressAPI::instance().updatePost(*m_currentPost);
    } else {
        // 无远程ID或者是草稿，执行创建
        qDebug() << "创建远程文章: 本地ID=" << m_currentPost->id();
        // 如果是草稿，先将其状态改为已发布
        Post postCopy = *m_currentPost;
        postCopy.setStatus(Post::Published);
        WordPressAPI::instance().createPost(postCopy);
    }
}

void BlogClient::on_actionSettings_triggered()
{
    // 创建并显示设置对话框
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // 更新相关设置
        QSettings settings;
        QString userName = settings.value("user/name").toString();
        ui.authorEdit->setText(userName);
    }
}

void BlogClient::on_actionAbout_triggered()
{
    QMessageBox::about(this, tr("关于个人博客客户端"),
        tr("个人博客客户端\n\n"
           "版本: 1.0\n"
            "作者：吴宇涵\n"
           "基于Qt和WordPress REST API\n"
           "用于管理您的WordPress博客文章"));
}

void BlogClient::on_postsListWidget_itemClicked(QListWidgetItem *item)
{
    if (item) {
        int postId = item->data(Qt::UserRole).toInt();
        Post post = DatabaseManager::instance().getPostById(postId);
        populateEditor(post);
    }
}

void BlogClient::on_draftsListWidget_itemClicked(QListWidgetItem *item)
{
    if (item) {
        int postId = item->data(Qt::UserRole).toInt();
        Post post = DatabaseManager::instance().getPostById(postId);
        populateEditor(post);
    }
}

void BlogClient::on_saveButton_clicked()
{
    saveCurrentPost();
}

void BlogClient::on_deleteButton_clicked()
{
    deleteCurrentPost();
}

void BlogClient::on_publishButton_clicked()
{
    publishCurrentPost();
}

void BlogClient::on_cancelButton_clicked()
{
    // 如果有未保存的更改，提示保存
    if (m_currentPost && !m_isEditing) {
        QMessageBox::StandardButton result = QMessageBox::question(this, tr("保存更改"),
            tr("您有未保存的更改。是否保存？"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            
        if (result == QMessageBox::Yes) {
            if (!saveCurrentPost()) {
                return; // 保存失败，不清空编辑器
            }
        } else if (result == QMessageBox::Cancel) {
            return; // 取消操作
        }
    }
    
    // 清空编辑器
    clearEditor();
}

void BlogClient::on_addCategoryButton_clicked()
{
    QString category = ui.categoryCombo->currentText().trimmed();
    if (!category.isEmpty()) {
        // 检查是否已存在
        for (int i = 0; i < ui.categoriesList->count(); ++i) {
            if (ui.categoriesList->item(i)->text() == category) {
                return; // 已存在，不添加
            }
        }
        
        // 添加到列表
        QListWidgetItem* item = new QListWidgetItem(category);
        ui.categoriesList->addItem(item);
        ui.categoriesList->scrollToItem(item); // 滚动到新添加的项
        
        // 清空输入框
        ui.categoryCombo->setCurrentText("");
        
        // 更新当前文章
        if (m_currentPost) {
            m_currentPost->addCategory(category);
            qDebug() << "已添加分类：" << category;
        }
        
        // 立即保存以更新数据库
        saveCurrentPost();
    } else {
        QMessageBox::information(this, tr("提示"), tr("请先在分类框中输入分类名称"));
    }
}

void BlogClient::on_removeCategoryButton_clicked()
{
    QListWidgetItem* currentItem = ui.categoriesList->currentItem();
    if (currentItem) {
        QString category = currentItem->text();
        
        // 从列表中移除
        delete currentItem;
        
        // 更新当前文章
        if (m_currentPost) {
            m_currentPost->removeCategory(category);
        }
    }
}

void BlogClient::on_addTagButton_clicked()
{
    QString tag = ui.tagEdit->text().trimmed();
    if (!tag.isEmpty()) {
        // 检查是否已存在
        for (int i = 0; i < ui.tagsList->count(); ++i) {
            if (ui.tagsList->item(i)->text() == tag) {
                return; // 已存在，不添加
            }
        }
        
        // 添加到列表
        QListWidgetItem* item = new QListWidgetItem(tag);
        ui.tagsList->addItem(item);
        ui.tagsList->scrollToItem(item); // 滚动到新添加的项
        
        // 清空输入框
        ui.tagEdit->clear();
        
        // 更新当前文章
        if (m_currentPost) {
            m_currentPost->addTag(tag);
            qDebug() << "已添加标签：" << tag;
        }
        
        // 立即保存以更新数据库
        saveCurrentPost();
    } else {
        QMessageBox::information(this, tr("提示"), tr("请先在标签框中输入标签名称"));
    }
}

void BlogClient::on_removeTagButton_clicked()
{
    QListWidgetItem* currentItem = ui.tagsList->currentItem();
    if (currentItem) {
        QString tag = currentItem->text();
        
        // 从列表中移除
        delete currentItem;
        
        // 更新当前文章
        if (m_currentPost) {
            m_currentPost->removeTag(tag);
        }
    }
}

void BlogClient::on_uploadImageButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择图片"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        tr("图片文件 (*.png *.jpg *.jpeg *.gif)"));
        
    if (!filePath.isEmpty()) {
        QSettings settings;
        QString apiUrl = settings.value("api/url").toString();
        QString username = settings.value("api/username").toString();
        QString password = settings.value("api/password").toString();
        
        if (apiUrl.isEmpty() || username.isEmpty() || password.isEmpty()) {
            QMessageBox::warning(this, tr("API设置缺失"), 
                tr("请先在设置中配置WordPress API信息。"));
            return;
        }
        
        // 检查文件是否可读
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists() || !fileInfo.isReadable()) {
            QMessageBox::warning(this, tr("文件错误"), 
                tr("无法读取所选图片文件，请确认文件存在且有权限访问。"));
            return;
        }
        
        // 确保文件大小合理
        if (fileInfo.size() > 10 * 1024 * 1024) { // 10MB限制
            QMessageBox::warning(this, tr("文件过大"), 
                tr("所选图片文件大小超过10MB，请选择更小的文件。"));
            return;
        }
        
        // 使用QProgressDialog显示上传进度
        QProgressDialog *progressDialog = new QProgressDialog(tr("正在上传图片..."), tr("取消"), 0, 100, this);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->setAutoClose(false);
        progressDialog->setAutoReset(false);
        progressDialog->setValue(0);
        progressDialog->show();
        
        // 连接上传进度信号
        connect(&WordPressAPI::instance(), &WordPressAPI::uploadProgress, this, 
            [progressDialog](qint64 bytesSent, qint64 bytesTotal) {
                if (bytesTotal > 0 && progressDialog) {
                    int percent = (int)((100.0 * bytesSent) / bytesTotal);
                    progressDialog->setValue(percent);
                }
            });
        
        // 连接取消按钮
        connect(progressDialog, &QProgressDialog::canceled, this, 
            [this, progressDialog]() {
                // 通知用户上传已取消
                QMessageBox::information(this, tr("上传取消"), 
                    tr("图片上传已取消"));
                progressDialog->deleteLater();
            });
        
        // 设置API信息
        WordPressAPI::instance().setApiUrl(apiUrl);
        WordPressAPI::instance().setCredentials(username, password);
        
        // 上传图片
        QString title = ui.titleEdit->text().trimmed();
        if (title.isEmpty()) {
            title = fileInfo.baseName(); // 使用文件名作为标题
        }
        
        // 在上传完成后自动关闭进度对话框
        auto connectionSuccess = connect(&WordPressAPI::instance(), &WordPressAPI::mediaUploaded, this, 
            [progressDialog]() {
                if (progressDialog) {
                    progressDialog->setValue(100);
                    progressDialog->deleteLater();
                }
            }, Qt::SingleShotConnection);
        
        auto connectionError = connect(&WordPressAPI::instance(), &WordPressAPI::error, this, 
            [progressDialog, this](const QString &errorMsg) {
                if (progressDialog) {
                    progressDialog->deleteLater();
                }
                // 错误消息已经由onApiError处理
            }, Qt::SingleShotConnection);
        
        // 上传图片
        WordPressAPI::instance().uploadMedia(filePath, title);
    }
}

void BlogClient::onPostsReceived(const QList<Post>& posts)
{
    qDebug() << "从API接收到 " << posts.size() << " 篇文章，开始保存到数据库...";
    
    // 保存文章到本地数据库
    int savedCount = 0;
    for (const Post& post : posts) {
        Post localPost = post;
        qDebug() << "处理文章: ID=" << post.id() << "标题=" << post.title() << "状态=" << post.status();
        
        if (DatabaseManager::instance().savePost(localPost)) {
            savedCount++;
        } else {
            qDebug() << "保存文章失败: ID=" << post.id() << "标题=" << post.title();
        }
    }
    
    qDebug() << "成功保存 " << savedCount << " 篇文章到数据库";
    
    // 更新列表
    loadPostsList();
    loadDraftsList();
    
    QMessageBox::information(this, tr("获取成功"), 
        tr("成功获取并保存了 %1 篇文章。").arg(savedCount));
}

void BlogClient::onPostCreated(const Post& post)
{
    // 保存到本地数据库并设置远程ID
    Post localPost = post;
    if (m_currentPost) {
        // 如果有当前文章，使用其本地ID，并设置远程ID
        localPost.setId(m_currentPost->id());
        qDebug() << "为当前文章设置远程ID: " << post.remoteId();
    }
    
    // 保存到数据库
    DatabaseManager::instance().savePost(localPost);
    
    // 更新当前文章
    if (m_currentPost) {
        m_currentPost->setRemoteId(post.remoteId());
    }
    
    // 更新列表
    loadPostsList();
    loadDraftsList();
    
    QMessageBox::information(this, tr("发布成功"), 
        tr("文章已成功发布到WordPress。\n远程ID: %1").arg(post.remoteId()));
}

void BlogClient::onPostUpdated(const Post& post)
{
    // 保存到本地数据库
    Post localPost = post;
    
    if (m_currentPost) {
        // 保持本地ID，但更新远程ID和其他信息
        localPost.setId(m_currentPost->id());
        qDebug() << "更新文章: 本地ID=" << m_currentPost->id() << "远程ID=" << post.remoteId();
    }
    
    // 保存到数据库
    DatabaseManager::instance().savePost(localPost);
    
    // 更新当前文章
    if (m_currentPost) {
        m_currentPost->setRemoteId(post.remoteId());
        *m_currentPost = localPost;
    }
    
    // 更新列表
    loadPostsList();
    loadDraftsList();
    
    QMessageBox::information(this, tr("更新成功"), 
        tr("文章已成功更新到WordPress。\n远程ID: %1").arg(post.remoteId()));
}

void BlogClient::onPostDeleted(int postId)
{
    // 从本地数据库删除
    DatabaseManager::instance().deletePost(postId);
    
    // 更新列表
    loadPostsList();
    loadDraftsList();
    
    // 清空编辑器
    clearEditor();
    
    QMessageBox::information(this, tr("删除成功"), 
        tr("文章已成功删除。"));
}

void BlogClient::onCategoriesReceived(const QList<Category>& categories)
{
    // 保存分类到本地数据库
    qDebug() << "接收到" << categories.size() << "个分类";
    for (const Category& category : categories) {
        qDebug() << "保存分类: ID=" << category.id() << "名称=" << category.name();
        Category localCategory = category;
        DatabaseManager::instance().saveCategory(localCategory);
    }
    
    // 更新分类下拉列表
    updateCategoriesList();
    
    // 如果当前在编辑文章，更新UI中已选分类
    if (m_currentPost) {
        ui.categoriesList->clear();
        for (const QString& category : m_currentPost->categories()) {
            ui.categoriesList->addItem(category);
        }
    }
}

void BlogClient::onTagsReceived(const QList<Tag>& tags)
{
    // 保存标签到本地数据库
    qDebug() << "接收到" << tags.size() << "个标签";
    for (const Tag& tag : tags) {
        qDebug() << "保存标签: ID=" << tag.id() << "名称=" << tag.name();
        Tag localTag = tag;
        DatabaseManager::instance().saveTag(localTag);
    }
    
    // 更新标签建议
    updateTagsList();
    
    // 如果当前在编辑文章，更新UI中已选标签
    if (m_currentPost) {
        ui.tagsList->clear();
        for (const QString& tag : m_currentPost->tags()) {
            ui.tagsList->addItem(tag);
        }
    }
}

void BlogClient::onMediaUploaded(const QString& url,int mediaId)
{
    if (url.isEmpty()) {
        QMessageBox::warning(this, tr("上传错误"), 
            tr("服务器返回的媒体URL为空。"));
        return;
    }
    
    // 设置特色图片URL
    ui.featuredImageUrlEdit->setText(url);
    
    // 更新当前文章
    if (m_currentPost) {
        m_currentPost->setFeaturedImageUrl(url);
        m_currentPost->setFeatureMediaId(mediaId);
        
        // 立即保存更新
        if (DatabaseManager::instance().savePost(*m_currentPost)) {
            qDebug() << "已更新文章的特色图片URL: " << url;
        } else {
            qDebug() << "保存特色图片URL失败";
        }
    } else {
        qDebug() << "警告: 当前没有正在编辑的文章，特色图片URL将不会被保存";
    }
    
    QMessageBox::information(this, tr("上传成功"), 
        tr("图片已成功上传，URL: %1").arg(url));
}

void BlogClient::onApiError(const QString& errorMessage)
{
    QMessageBox::warning(this, tr("API错误"), 
        tr("发生错误: %1").arg(errorMessage));
}

void BlogClient::clearEditor()
{
    // 清空所有输入控件
    QLineEdit* titleEdit = findChild<QLineEdit*>("titleEdit");
    if (titleEdit) {
        titleEdit->clear();
        titleEdit->setEnabled(true);
        titleEdit->setPlaceholderText(tr("请在此输入文章标题"));
    } else {
        qDebug() << "警告: 找不到标题输入控件";
    }
    
    ui.contentEdit->clear();
    ui.excerptEdit->clear();
    ui.publishDateEdit->setDateTime(QDateTime::currentDateTime());
    ui.authorEdit->setText(QSettings().value("user/name").toString());
    ui.categoriesList->clear();
    ui.tagsList->clear();
    ui.featuredImageUrlEdit->clear();
    ui.isDraftCheckBox->setChecked(true);
    
    // 确保所有输入控件都是启用的
    ui.contentEdit->setEnabled(true);
    ui.excerptEdit->setEnabled(true);
    ui.publishDateEdit->setEnabled(true);
    ui.authorEdit->setEnabled(true);
    ui.featuredImageUrlEdit->setEnabled(true);
    
    // 确保分类和标签控件也是启用的
    ui.removeCategoryButton->setEnabled(true);
    ui.removeTagButton->setEnabled(true);
    ui.categoryCombo->setEnabled(true);
    ui.tagEdit->setEnabled(true);
    
    // 确保添加按钮设置正确
    setupAddButtons();
    
    m_currentPost.reset();
    m_isEditing = false;
}

void BlogClient::populateEditor(const Post& post)
{
    // 更新编辑器内容
    QLineEdit* titleEdit = findChild<QLineEdit*>("titleEdit");
    if (titleEdit) {
        titleEdit->setText(post.title());
        titleEdit->setEnabled(true);
    } else {
        qDebug() << "警告: 找不到标题输入控件";
    }
    
    ui.contentEdit->setPlainText(post.content());
    ui.excerptEdit->setText(post.excerpt());
    ui.publishDateEdit->setDateTime(post.publishDate());
    ui.authorEdit->setText(post.author());
    ui.featuredImageUrlEdit->setText(post.featuredImageUrl());
    ui.isDraftCheckBox->setChecked(post.status() == Post::Draft);
    
    // 确保添加按钮设置正确
    setupAddButtons();
    
    // 确保相关控件可用
    ui.removeCategoryButton->setEnabled(true);
    ui.removeTagButton->setEnabled(true);
    ui.categoryCombo->setEnabled(true);
    ui.tagEdit->setEnabled(true);
    
    // 更新分类列表
    ui.categoriesList->clear();
    for (const QString& category : post.categories()) {
        ui.categoriesList->addItem(category);
    }
    
    // 更新标签列表
    ui.tagsList->clear();
    for (const QString& tag : post.tags()) {
        ui.tagsList->addItem(tag);
    }
    
    // 保存当前编辑的文章
    m_currentPost = std::make_unique<Post>(post);
    m_isEditing = true;
}

void BlogClient::loadPostsList()
{
    ui.postsListWidget->clear();
    
    QList<Post> posts = DatabaseManager::instance().getAllPosts(true); // 只加载已发布的
    
    qDebug() << "加载已发布文章：" << posts.size() << "篇";
    
    if (posts.isEmpty()) {
        // 添加提示项
        QListWidgetItem* item = new QListWidgetItem("暂无已发布文章");
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled); // 禁用点击
        ui.postsListWidget->addItem(item);
        return;
    }
    
    for (const Post& post : posts) {
        // 创建更友好的显示格式：标题 (日期)
        QString displayText = QString("%1 (%2)").arg(
            post.title().length() > 30 ? post.title().left(30) + "..." : post.title(),
            post.publishDate().toString("yyyy-MM-dd")
        );
        
        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, post.id());
        item->setToolTip(post.title()); // 当鼠标悬停时显示完整标题
        ui.postsListWidget->addItem(item);
        
        qDebug() << "添加文章到列表：" << post.id() << post.title();
    }
    
    // 确保文章列表有合适的大小
    ui.postsListWidget->setMinimumWidth(200);
    ui.postsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void BlogClient::loadDraftsList()
{
    ui.draftsListWidget->clear();
    
    QList<Post> posts = DatabaseManager::instance().getAllPosts();
    int draftCount = 0;
    
    for (const Post& post : posts) {
        if (post.status() == Post::Draft) {
            draftCount++;
            
            // 创建更友好的显示格式：标题 (日期)
            QString displayText = QString("%1 (%2)").arg(
                post.title().length() > 30 ? post.title().left(30) + "..." : post.title(),
                post.publishDate().toString("yyyy-MM-dd")
            );
            
            QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, post.id());
            item->setToolTip(post.title()); // 当鼠标悬停时显示完整标题
            ui.draftsListWidget->addItem(item);
            
            qDebug() << "添加草稿到列表：" << post.id() << post.title();
        }
    }
    
    qDebug() << "加载草稿：" << draftCount << "篇";
    
    if (draftCount == 0) {
        // 添加提示项
        QListWidgetItem* item = new QListWidgetItem("暂无草稿");
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled); // 禁用点击
        ui.draftsListWidget->addItem(item);
    }
    
    // 确保草稿列表有合适的大小
    ui.draftsListWidget->setMinimumWidth(200);
    ui.draftsListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void BlogClient::updateCategoriesList()
{
    // 更新分类下拉列表
    ui.categoryCombo->clear();
    
    QList<Category> categories = DatabaseManager::instance().getAllCategories();
    
    qDebug() << "加载所有分类，共" << categories.size() << "个";
    for (const Category& category : categories) {
        qDebug() << "添加分类到下拉列表: ID=" << category.id() << "名称=" << category.name();
        ui.categoryCombo->addItem(category.name(), category.id());
    }
    
    // 确保分类下拉列表使用名称显示而不是ID
    ui.categoryCombo->setEditable(true);
    ui.categoryCombo->setInsertPolicy(QComboBox::NoInsert);
}

void BlogClient::updateTagsList()
{
    // 获取所有标签
    QList<Tag> tags = DatabaseManager::instance().getAllTags();
    
    // 创建自动完成器
    QStringList tagNames;
    qDebug() << "加载所有标签，共" << tags.size() << "个";
    for (const Tag& tag : tags) {
        qDebug() << "添加标签到自动完成列表: ID=" << tag.id() << "名称=" << tag.name();
        tagNames << tag.name();
    }
    
    // 设置自动完成器
    QCompleter* completer = new QCompleter(tagNames, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    ui.tagEdit->setCompleter(completer);
}

bool BlogClient::saveCurrentPost()
{
    // 创建或更新文章
    if (!m_currentPost) {
        m_currentPost = std::make_unique<Post>();
    }
    
    // 验证标题不能为空
    QLineEdit* titleEdit = findChild<QLineEdit*>("titleEdit");
    if (!titleEdit) {
        QMessageBox::warning(this, tr("UI错误"),
            tr("找不到标题输入控件，无法保存文章。"));
        return false;
    }
    
    QString title = titleEdit->text().trimmed();
    if (title.isEmpty()) {
        QMessageBox::warning(this, tr("标题为空"),
            tr("请输入文章标题后再保存。"));
        titleEdit->setFocus();
        return false;
    }
    
    // 从编辑器获取数据
    m_currentPost->setTitle(title);
    m_currentPost->setContent(ui.contentEdit->toPlainText());
    m_currentPost->setExcerpt(ui.excerptEdit->text());
    m_currentPost->setPublishDate(ui.publishDateEdit->dateTime());
    m_currentPost->setAuthor(ui.authorEdit->text());
    m_currentPost->setFeaturedImageUrl(ui.featuredImageUrlEdit->text());
    m_currentPost->setStatus(ui.isDraftCheckBox->isChecked() ? Post::Draft : Post::Published);
    
    // 保存到数据库
    if (DatabaseManager::instance().savePost(*m_currentPost)) {
        m_isEditing = true;
        
        // 更新列表
        loadPostsList();
        loadDraftsList();
        
        QMessageBox::information(this, tr("保存成功"), 
            tr("文章已成功保存。"));
        return true;
    } else {
        QMessageBox::warning(this, tr("保存失败"), 
            tr("无法保存文章。请稍后再试。"));
        return false;
    }
}

bool BlogClient::publishCurrentPost()
{
    // 首先保存文章
    if (!saveCurrentPost()) {
        return false;
    }
    
    // 将状态改为已发布
    m_currentPost->setStatus(Post::Published);
    
    // 保存到数据库
    if (DatabaseManager::instance().savePost(*m_currentPost)) {
        // 更新列表
        loadPostsList();
        loadDraftsList();
        
        // 同步到WordPress
        on_actionSync_triggered();
        return true;
    } else {
        QMessageBox::warning(this, tr("发布失败"), 
            tr("无法发布文章。请稍后再试。"));
        return false;
    }
}

void BlogClient::deleteCurrentPost()
{
    if (!m_currentPost || m_currentPost->id() == -1) {
        QMessageBox::warning(this, tr("删除错误"), 
            tr("请先选择一篇文章进行删除。"));
        return;
    }
    
    QMessageBox::StandardButton result = QMessageBox::question(this, tr("确认删除"),
        tr("您确定要删除这篇文章吗？此操作无法撤销。"),
        QMessageBox::Yes | QMessageBox::No);
        
    if (result == QMessageBox::Yes) {
        // 从数据库删除
        if (DatabaseManager::instance().deletePost(m_currentPost->id())) {
            // 尝试从WordPress删除
            QSettings settings;
            QString apiUrl = settings.value("api/url").toString();
            QString username = settings.value("api/username").toString();
            QString password = settings.value("api/password").toString();
            
            if (!apiUrl.isEmpty() && !username.isEmpty() && !password.isEmpty()) {
                WordPressAPI::instance().setApiUrl(apiUrl);
                WordPressAPI::instance().setCredentials(username, password);
                WordPressAPI::instance().deletePost(m_currentPost->id());
            }
            
            // 更新列表
            loadPostsList();
            loadDraftsList();
            
            // 清空编辑器
            clearEditor();
            
            QMessageBox::information(this, tr("删除成功"), 
                tr("文章已成功删除。"));
        } else {
            QMessageBox::warning(this, tr("删除失败"), 
                tr("无法删除文章。请稍后再试。"));
        }
    }
}

void BlogClient::closeEvent(QCloseEvent* event)
{
    // 如果有未保存的更改，提示保存
    if (m_currentPost && !m_isEditing) {
        QMessageBox::StandardButton result = QMessageBox::question(this, tr("保存更改"),
            tr("您有未保存的更改。是否保存？"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            
        if (result == QMessageBox::Yes) {
            if (!saveCurrentPost()) {
                event->ignore();
                return;
            }
        } else if (result == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }
    
    event->accept();
}

// 修改 resizeEvent 方法以适应新的滚动区域布局，并添加一个成员变量用于保存滚动区域的引用
void BlogClient::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    
    // 重新计算分割器大小
    int totalWidth = event->size().width();
    QList<int> sizes;
    sizes << totalWidth * 0.25 << totalWidth * 0.75; // 1/4 用于边栏，3/4 用于编辑区
    ui.mainSplitter->setSizes(sizes);
    
    // 确保滚动区域正确调整大小
    if (m_scrollArea) {
        m_scrollArea->updateGeometry();
        QWidget* content = m_scrollArea->widget();
        if (content) {
            content->updateGeometry();
        }
    }
}

// 添加setupAddButtons方法实现
void BlogClient::setupAddButtons()
{
    // 查找分类添加按钮和分类组合框
    QPushButton* addCategoryButton = ui.editorContainer->findChild<QPushButton*>("addCategoryButton");
    QComboBox* categoryCombo = ui.editorContainer->findChild<QComboBox*>("categoryCombo");
    
    if (addCategoryButton && categoryCombo) {
        // 确保按钮可见和可用
        addCategoryButton->setVisible(true);
        addCategoryButton->setEnabled(true);
        // 设置样式以使其更加醒目
        addCategoryButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
        addCategoryButton->setText(tr("添加分类"));
        addCategoryButton->setMinimumWidth(80);
    } else {
        qDebug() << "警告：找不到分类添加按钮或分类组合框";
    }
    
    // 查找标签添加按钮和标签输入框
    QPushButton* addTagButton = ui.editorContainer->findChild<QPushButton*>("addTagButton");
    QLineEdit* tagEdit = ui.editorContainer->findChild<QLineEdit*>("tagEdit");
    
    if (addTagButton && tagEdit) {
        // 确保按钮可见和可用
        addTagButton->setVisible(true);
        addTagButton->setEnabled(true);
        // 设置样式以使其更加醒目
        addTagButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; }");
        addTagButton->setText(tr("添加标签"));
        addTagButton->setMinimumWidth(80);
    } else {
        qDebug() << "警告：找不到标签添加按钮或标签输入框";
    }
}


