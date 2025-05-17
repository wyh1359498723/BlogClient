#include "WordPressAPI.h"
#include <QDebug>
#include <QUrlQuery>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QBuffer>
#include <QDateTime>
#include <QSqlQuery>
#include "database/DatabaseManager.h"
#include <QSslConfiguration>
#include <QSslSocket>
#include <QApplication>

std::unique_ptr<WordPressAPI> WordPressAPI::s_instance = nullptr;

WordPressAPI& WordPressAPI::instance()
{
    if (!s_instance) {
        s_instance = std::unique_ptr<WordPressAPI>(new WordPressAPI());
    }
    return *s_instance;
}

WordPressAPI::WordPressAPI(QObject* parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
}

WordPressAPI::~WordPressAPI()
{
    // 取消所有未完成的网络请求以避免应用程序退出时引发的访问冲突
    QList<QNetworkReply*> activeReplies = m_networkManager->findChildren<QNetworkReply*>();
    for (QNetworkReply* reply : activeReplies) {
        if (reply && reply->isRunning()) {
            qDebug() << "取消未完成的网络请求: " << reply->url().toString();
            reply->abort();
            reply->deleteLater();
        }
    }
    
    // 等待所有网络请求完成
    qApp->processEvents();
    
    // 确保网络管理器被正确释放
    if (m_networkManager) {
        m_networkManager->deleteLater();
        m_networkManager = nullptr;
    }
}

void WordPressAPI::setApiUrl(const QString& url)
{
    m_apiUrl = url;
    
    // 记录原始URL
    qDebug() << "原始API URL: " << m_apiUrl;
    
    // 确保URL以斜杠结尾
    if (!m_apiUrl.endsWith("/")) {
        m_apiUrl += "/";
    }
    
    // 确保URL包含wp-json/wp/v2路径
    if (!m_apiUrl.contains("wp-json")) {
        if (m_apiUrl.endsWith("/")) {
            m_apiUrl += "wp-json/";
        } else {
            m_apiUrl += "/wp-json/";
        }
    }
    
    if (!m_apiUrl.contains("wp/v2")) {
        if (m_apiUrl.endsWith("/")) {
            m_apiUrl += "wp/v2/";
        } else {
            m_apiUrl += "/wp/v2/";
        }
    }
    
    qDebug() << "处理后的API基础URL: " << m_apiUrl;
}

void WordPressAPI::setCredentials(const QString& username, const QString& password)
{
    m_username = username;
    m_password = password;
}

QString WordPressAPI::apiUrl() const
{
    return m_apiUrl;
}

QByteArray WordPressAPI::createAuthHeader() const
{
    if (m_username.isEmpty() || m_password.isEmpty()) {
        return QByteArray();
    }
    
    QString concatenated = m_username + ":" + m_password;
    QByteArray data = concatenated.toUtf8().toBase64();
    return "Basic " + data;
}

void WordPressAPI::fetchPosts()
{
    if (m_apiUrl.isEmpty()) {
        emit error("API URL 没有设置");
        return;
    }
    
    // 构建API URL
    QString apiEndpoint = m_apiUrl;
    if (!apiEndpoint.endsWith("/")) {
        apiEndpoint += "/";
    }
    if (!apiEndpoint.contains("posts")) {
        apiEndpoint += "posts";
    }
    
    QUrl url(apiEndpoint);
    QUrlQuery query;
    query.addQueryItem("per_page", "100"); // 每页获取100篇文章
    query.addQueryItem("page", "1");       // 获取第1页
    url.setQuery(query);
    
    qDebug() << "获取文章API URL: " << url.toString();
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
        qDebug() << "添加认证头: " << authHeader;
    } else {
        qDebug() << "警告: 未设置认证信息";
    }
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onPostsReceived);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
}

void WordPressAPI::createPost(const Post& post)
{
    if (m_apiUrl.isEmpty()) {
        emit error("API URL 没有设置");
        return;
    }
    
    QUrl url(m_apiUrl + "posts");
    qDebug() << "发布文章 API URL: " << url.toString();
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 启用SSL错误忽略（仅在开发环境使用，生产环境应移除）
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    QSslConfiguration conf = request.sslConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(conf);
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
        qDebug() << "添加认证头: " << authHeader;
    } else {
        qDebug() << "警告: 未设置认证信息";
        emit error("认证信息未设置，无法发布文章");
        return;
    }
    
    QJsonObject postObject;
    
    // 设置标题，使用proper JSON对象格式
    QJsonObject titleObject;
    titleObject["raw"] = post.title();
    postObject["title"] = titleObject;
    
    // 设置内容，使用proper JSON对象格式
    QJsonObject contentObject;
    contentObject["raw"] = post.content();
    postObject["content"] = contentObject;
    
    // 设置摘要，使用proper JSON对象格式
    QJsonObject excerptObject;
    excerptObject["raw"] = post.excerpt();
    postObject["excerpt"] = excerptObject;
    
    postObject["status"] = post.status() == Post::Published ? "publish" : "draft";
    
    // 设置分类 - 必须是ID数组
    if (!post.categories().isEmpty()) {
        QJsonArray categoriesArray;
        for (const QString& categoryName : post.categories()) {
            // 从数据库查找分类ID
            QSqlQuery query;
            query.prepare("SELECT id FROM categories WHERE name = :name");
            query.bindValue(":name", categoryName);
            
            if (query.exec() && query.next()) {
                int categoryId = query.value(0).toInt();
                categoriesArray.append(categoryId);
                qDebug() << "添加分类ID: " << categoryId << " 名称: " << categoryName;
            } else {
                qDebug() << "警告: 找不到分类 '" << categoryName << "' 的ID";
            }
        }
        
        if (!categoriesArray.isEmpty()) {
            postObject["categories"] = categoriesArray;
        }
    }
    
    // 设置标签 - 必须是ID数组
    if (!post.tags().isEmpty()) {
        QJsonArray tagsArray;
        for (const QString& tagName : post.tags()) {
            // 从数据库查找标签ID
            QSqlQuery query;
            query.prepare("SELECT id FROM tags WHERE name = :name");
            query.bindValue(":name", tagName);
            
            if (query.exec() && query.next()) {
                int tagId = query.value(0).toInt();
                tagsArray.append(tagId);
                qDebug() << "添加标签ID: " << tagId << " 名称: " << tagName;
            } else {
                qDebug() << "警告: 找不到标签 '" << tagName << "' 的ID";
            }
        }
        
        if (!tagsArray.isEmpty()) {
            postObject["tags"] = tagsArray;
        }
    }

    if (post.featureMediaId() > 0) {
        postObject["featured_media"] = post.featureMediaId();
    }
    
    QJsonDocument doc(postObject);
    QByteArray data = doc.toJson();
    
    qDebug() << "发送POST请求数据: " << data;
    
    QNetworkReply* reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onPostCreated);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
    
    // 添加SSL错误处理
    connect(reply, &QNetworkReply::sslErrors, [this](const QList<QSslError> &errors) {
        QString errorStr = "SSL错误: ";
        for (const QSslError &error : errors) {
            errorStr += error.errorString() + "; ";
        }
        qDebug() << errorStr;
        emit error(errorStr);
    });
}

void WordPressAPI::updatePost(const Post& post)
{
    if (m_apiUrl.isEmpty() || !post.hasRemoteId()) {
        emit error("API URL 没有设置或无效的远程文章ID");
        return;
    }
    
    QUrl url(m_apiUrl + "posts/" + QString::number(post.remoteId()));
    qDebug() << "更新文章 API URL: " << url.toString();
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    // 启用SSL错误忽略（仅在开发环境使用，生产环境应移除）
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    QSslConfiguration conf = request.sslConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(conf);
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
        qDebug() << "添加认证头: " << authHeader;
    } else {
        qDebug() << "警告: 未设置认证信息";
        emit error("认证信息未设置，无法更新文章");
        return;
    }
    
    QJsonObject postObject;
    
    // 设置标题，使用proper JSON对象格式
    QJsonObject titleObject;
    titleObject["raw"] = post.title();
    postObject["title"] = titleObject;
    
    // 设置内容，使用proper JSON对象格式
    QJsonObject contentObject;
    contentObject["raw"] = post.content();
    postObject["content"] = contentObject;
    
    // 设置摘要，使用proper JSON对象格式
    QJsonObject excerptObject;
    excerptObject["raw"] = post.excerpt();
    postObject["excerpt"] = excerptObject;
    
    postObject["status"] = post.status() == Post::Published ? "publish" : "draft";
    
    // 设置分类 - 必须是ID数组
    if (!post.categories().isEmpty()) {
        QJsonArray categoriesArray;
        for (const QString& categoryName : post.categories()) {
            // 从数据库查找分类ID
            QSqlQuery query;
            query.prepare("SELECT id FROM categories WHERE name = :name");
            query.bindValue(":name", categoryName);
            
            if (query.exec() && query.next()) {
                int categoryId = query.value(0).toInt();
                categoriesArray.append(categoryId);
            }
        }
        
        if (!categoriesArray.isEmpty()) {
            postObject["categories"] = categoriesArray;
        }
    }
    
    // 设置标签 - 必须是ID数组
    if (!post.tags().isEmpty()) {
        QJsonArray tagsArray;
        for (const QString& tagName : post.tags()) {
            // 从数据库查找标签ID
            QSqlQuery query;
            query.prepare("SELECT id FROM tags WHERE name = :name");
            query.bindValue(":name", tagName);
            
            if (query.exec() && query.next()) {
                int tagId = query.value(0).toInt();
                tagsArray.append(tagId);
            }
        }
        
        if (!tagsArray.isEmpty()) {
            postObject["tags"] = tagsArray;
        }
    }
    
    QJsonDocument doc(postObject);
    QByteArray data = doc.toJson();
    
    qDebug() << "发送PUT请求数据: " << data;
    
    QNetworkReply* reply = m_networkManager->put(request, data);
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onPostUpdated);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
    
    // 添加SSL错误处理
    connect(reply, &QNetworkReply::sslErrors, [this](const QList<QSslError> &errors) {
        QString errorStr = "SSL错误: ";
        for (const QSslError &error : errors) {
            errorStr += error.errorString() + "; ";
        }
        qDebug() << errorStr;
        emit error(errorStr);
    });
}

void WordPressAPI::deletePost(int postId)
{
    if (m_apiUrl.isEmpty() || postId == -1) {
        emit error("API URL 没有设置或无效的文章ID");
        return;
    }
    
    QUrl url(m_apiUrl + "posts/" + QString::number(postId));
    QUrlQuery query;
    query.addQueryItem("force", "true"); // 强制删除
    url.setQuery(query);
    
    QNetworkRequest request(url);
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
    }
    
    QNetworkReply* reply = m_networkManager->deleteResource(request);
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onPostDeleted);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
}

void WordPressAPI::fetchCategories()
{
    if (m_apiUrl.isEmpty()) {
        emit error("API URL 没有设置");
        return;
    }
    
    QUrl url(m_apiUrl + "categories");
    QUrlQuery query;
    query.addQueryItem("per_page", "100"); // 每页获取100个分类
    url.setQuery(query);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
    }
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onCategoriesReceived);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
}

void WordPressAPI::fetchTags()
{
    if (m_apiUrl.isEmpty()) {
        emit error("API URL 没有设置");
        return;
    }
    
    QUrl url(m_apiUrl + "tags");
    QUrlQuery query;
    query.addQueryItem("per_page", "100"); // 每页获取100个标签
    url.setQuery(query);
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
    }
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onTagsReceived);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
}

void WordPressAPI::uploadMedia(const QString& filePath, const QString& title)
{
    if (m_apiUrl.isEmpty()) {
        emit error("API URL未设置");
        return;
    }
    
    QFile* file=new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit error("无法打开文件: " + filePath);
        file->deleteLater();
        return;
    }
    
    QUrl url(m_apiUrl + "media");
    qDebug() << "上传媒体 API URL: " << url.toString();
    
    QNetworkRequest request(url);
    
    // 忽略SSL错误，用于开发环境（在生产环境中应移除）
    QSslConfiguration conf = request.sslConfiguration();
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(conf);
    
    QByteArray authHeader = createAuthHeader();
    if (!authHeader.isEmpty()) {
        request.setRawHeader("Authorization", authHeader);
        qDebug() << "已添加认证头";
    } else {
        emit error("认证信息未设置，无法上传媒体");
        return;
    }
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        emit error("文件不存在或无法读取: " + filePath);
        return;
    }
    
    // 检查文件大小
    qint64 fileSize = fileInfo.size();
    if (fileSize > 50 * 1024 * 1024) { // 50MB限制
        emit error("文件过大，超过50MB的限制: " + QString::number(fileSize / (1024.0 * 1024.0), 'f', 2) + "MB");
        return;
    }
    
    qDebug() << "正在上传文件:" << filePath << "，大小:" << QString::number(fileSize / 1024.0, 'f', 2) + "KB";
    
    // 创建multipart请求
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    
    // 添加文件数据
    QHttpPart filePart;
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(filePath);
    QString mimeType = mime.name();
    
    // 确保MIME类型是图片
    if (!mimeType.startsWith("image/")) {
        mimeType = "image/" + fileInfo.suffix().toLower();
        qDebug() << "文件MIME类型不是图片，强制设置为:" << mimeType;
    }
    
    qDebug() << "文件MIME类型:" << mimeType;
    
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(mimeType));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader, 
                      QVariant("form-data; name=\"file\"; filename=\"" + fileInfo.fileName() + "\""));
    filePart.setBodyDevice(file);
    file->setParent(multiPart); // 文件的所有权转移到multiPart
    
    multiPart->append(filePart);
    
    // 添加标题（如果有）
    if (!title.isEmpty()) {
        QHttpPart titlePart;
        titlePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"title\""));
        titlePart.setBody(title.toUtf8());
        multiPart->append(titlePart);
        qDebug() << "添加标题:" << title;
    }
    
    QNetworkReply* reply = m_networkManager->post(request, multiPart);
    multiPart->setParent(reply); // 当回复完成时，自动删除multiPart
    
    connect(reply, &QNetworkReply::finished, this, &WordPressAPI::onMediaUploaded);
    connect(reply, &QNetworkReply::errorOccurred, this, &WordPressAPI::handleNetworkError);
    
    // 连接并转发上传进度信号
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            qDebug() << "上传进度: " << bytesSent << "/" << bytesTotal 
                     << "(" << int(100.0 * bytesSent / bytesTotal) << "%)";
            emit uploadProgress(bytesSent, bytesTotal);
        }
    });
    
    // 连接SSL错误信号
    connect(reply, &QNetworkReply::sslErrors, this, [this, reply](const QList<QSslError> &errors) {
        QString errorMsg = "SSL错误：";
        for (const QSslError &error : errors) {
            errorMsg += error.errorString() + "; ";
        }
        qDebug() << errorMsg;
        emit error(errorMsg);
        
        // 在开发环境中忽略SSL错误
        reply->ignoreSslErrors();
    });
}

void WordPressAPI::onPostsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "错误: 无效的网络回复对象";
        return;
    }
    
    // 检查HTTP状态码
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "获取文章 HTTP状态码: " << statusCode;
    
    // 获取响应头信息
    QList<QByteArray> headerList = reply->rawHeaderList();
    for (const QByteArray& header : headerList) {
        qDebug() << "响应头: " << header << "=" << reply->rawHeader(header);
    }
    
    QByteArray responseData = reply->readAll();
    qDebug() << "响应数据长度: " << responseData.size() << "字节";
    
    // 仅输出前200字符，避免日志过长
    if (!responseData.isEmpty()) {
        qDebug() << "响应数据预览: " << responseData.left(200) << "...";
    }
    
    reply->deleteLater();
    
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误: " << parseError.errorString();
        emit error("JSON解析错误: " + parseError.errorString());
        return;
    }
    
    if (jsonDoc.isArray()) {
        QJsonArray jsonArray = jsonDoc.array();
        qDebug() << "获取到的文章数量: " << jsonArray.size();
        
        QList<Post> posts = parsePosts(jsonArray);
        qDebug() << "解析后的文章数量: " << posts.size();
        
        emit postsReceived(posts);
    } else if (jsonDoc.isObject()) {
        // 某些WordPress API可能在错误时返回对象而不是数组
        QJsonObject errorObj = jsonDoc.object();
        if (errorObj.contains("code") || errorObj.contains("message")) {
            QString errorCode = errorObj["code"].toString();
            QString errorMessage = errorObj["message"].toString();
            qDebug() << "API错误: " << errorCode << " - " << errorMessage;
            emit error("API错误: " + errorMessage);
        } else {
            qDebug() << "响应不是文章数组: " << jsonDoc.toJson().left(200) << "...";
            emit error("响应格式无效，预期是文章数组");
        }
    } else {
        qDebug() << "无效的响应格式，既不是数组也不是对象";
        emit error("无效的响应格式");
    }
}

void WordPressAPI::onPostCreated()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        emit error("无效的网络回复对象");
        return;
    }
    
    // 检查HTTP状态码
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "创建文章 HTTP状态码: " << statusCode;
    
    QByteArray responseData = reply->readAll();
    qDebug() << "响应数据长度: " << responseData.size() << "字节";
    if (!responseData.isEmpty()) {
        qDebug() << "响应数据预览: " << responseData.left(200) << "...";
    }
    
    reply->deleteLater();
    
    // 检查状态码是否表示成功
    if (statusCode < 200 || statusCode >= 300) {
        QString errorMsg = QString("创建文章失败，HTTP错误: %1").arg(statusCode);
        
        // 尝试从响应中提取更多错误信息
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            if (obj.contains("message")) {
                errorMsg += "\n错误信息: " + obj["message"].toString();
            }
        }
        
        emit error(errorMsg);
        return;
    }
    
    // 解析成功响应
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit error("JSON解析错误: " + parseError.errorString());
        return;
    }
    
    if (!jsonDoc.isObject()) {
        emit error("无效的响应格式，预期是文章对象");
        return;
    }
    
    QJsonObject jsonObj = jsonDoc.object();
    
    // 提取文章信息
    int remoteId = jsonObj["id"].toInt();  // 这是WordPress返回的远程ID
    qDebug() << "WordPress返回的远程ID: " << remoteId;
    
    QString title;
    if (jsonObj.contains("title") && jsonObj["title"].isObject()) {
        title = jsonObj["title"].toObject()["rendered"].toString();
    } else {
        title = "未知标题";
    }
    
    QString content;
    if (jsonObj.contains("content") && jsonObj["content"].isObject()) {
        content = jsonObj["content"].toObject()["rendered"].toString();
    } else {
        content = "";
    }
    
    QString excerpt;
    if (jsonObj.contains("excerpt") && jsonObj["excerpt"].isObject()) {
        excerpt = jsonObj["excerpt"].toObject()["rendered"].toString();
    } else {
        excerpt = "";
    }
    
    QDateTime publishDate = QDateTime::currentDateTime();
    if (jsonObj.contains("date")) {
        publishDate = QDateTime::fromString(jsonObj["date"].toString(), Qt::ISODate);
    }
    
    QString author = jsonObj["author"].toString();
    Post::Status status = jsonObj["status"].toString() == "publish" ? Post::Published : Post::Draft;
    
    // 创建一个新的Post对象，使用-1作为本地ID（将由数据库自动分配）
    // 并设置远程ID为WordPress返回的ID
    Post post(-1, title, content, excerpt, publishDate, author, status);
    post.setRemoteId(remoteId);  // 设置WordPress远程ID
    
    // 处理分类和标签
    if (jsonObj.contains("categories") && jsonObj["categories"].isArray()) {
        QJsonArray categories = jsonObj["categories"].toArray();
        for (const QJsonValue& catId : categories) {
            // 从数据库获取分类名称
            QSqlQuery query;
            query.prepare("SELECT name FROM categories WHERE id = :id");
            query.bindValue(":id", catId.toInt());
            if (query.exec() && query.next()) {
                post.addCategory(query.value(0).toString());
            }
        }
    }
    
    if (jsonObj.contains("tags") && jsonObj["tags"].isArray()) {
        QJsonArray tags = jsonObj["tags"].toArray();
        for (const QJsonValue& tagId : tags) {
            // 从数据库获取标签名称
            QSqlQuery query;
            query.prepare("SELECT name FROM tags WHERE id = :id");
            query.bindValue(":id", tagId.toInt());
            if (query.exec() && query.next()) {
                post.addTag(query.value(0).toString());
            }
        }
    }
    
    emit postCreated(post);
}

void WordPressAPI::onPostUpdated()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        emit error("无效的网络回复对象");
        return;
    }
    
    // 检查HTTP状态码
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "更新文章 HTTP状态码: " << statusCode;
    
    QByteArray responseData = reply->readAll();
    qDebug() << "响应数据长度: " << responseData.size() << "字节";
    if (!responseData.isEmpty()) {
        qDebug() << "响应数据预览: " << responseData.left(200) << "...";
    }
    
    reply->deleteLater();
    
    // 检查状态码是否表示成功
    if (statusCode < 200 || statusCode >= 300) {
        QString errorMsg = QString("更新文章失败，HTTP错误: %1").arg(statusCode);
        
        // 尝试从响应中提取更多错误信息
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            if (obj.contains("message")) {
                errorMsg += "\n错误信息: " + obj["message"].toString();
            }
        }
        
        emit error(errorMsg);
        return;
    }
    
    // 解析成功响应
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit error("JSON解析错误: " + parseError.errorString());
        return;
    }
    
    if (!jsonDoc.isObject()) {
        emit error("无效的响应格式，预期是文章对象");
        return;
    }
    
    QJsonObject jsonObj = jsonDoc.object();
    
    // 提取文章信息
    int remoteId = jsonObj["id"].toInt();
    qDebug() << "WordPress返回的远程ID: " << remoteId;
    
    QString title;
    if (jsonObj.contains("title") && jsonObj["title"].isObject()) {
        title = jsonObj["title"].toObject()["rendered"].toString();
    } else {
        title = "未知标题";
    }
    
    QString content;
    if (jsonObj.contains("content") && jsonObj["content"].isObject()) {
        content = jsonObj["content"].toObject()["rendered"].toString();
    } else {
        content = "";
    }
    
    QString excerpt;
    if (jsonObj.contains("excerpt") && jsonObj["excerpt"].isObject()) {
        excerpt = jsonObj["excerpt"].toObject()["rendered"].toString();
    } else {
        excerpt = "";
    }
    
    QDateTime publishDate = QDateTime::currentDateTime();
    if (jsonObj.contains("date")) {
        publishDate = QDateTime::fromString(jsonObj["date"].toString(), Qt::ISODate);
    }
    
    QString author = jsonObj["author"].toString();
    Post::Status status = jsonObj["status"].toString() == "publish" ? Post::Published : Post::Draft;
    
    // 创建一个新的Post对象
    // 使用-1作为本地ID（稍后会由调用者更新）
    Post post(-1, title, content, excerpt, publishDate, author, status);
    post.setRemoteId(remoteId);  // 设置WordPress远程ID
    
    // 处理分类和标签
    if (jsonObj.contains("categories") && jsonObj["categories"].isArray()) {
        QJsonArray categories = jsonObj["categories"].toArray();
        for (const QJsonValue& catId : categories) {
            // 从数据库获取分类名称
            QSqlQuery query;
            query.prepare("SELECT name FROM categories WHERE id = :id");
            query.bindValue(":id", catId.toInt());
            if (query.exec() && query.next()) {
                post.addCategory(query.value(0).toString());
            }
        }
    }
    
    if (jsonObj.contains("tags") && jsonObj["tags"].isArray()) {
        QJsonArray tags = jsonObj["tags"].toArray();
        for (const QJsonValue& tagId : tags) {
            // 从数据库获取标签名称
            QSqlQuery query;
            query.prepare("SELECT name FROM tags WHERE id = :id");
            query.bindValue(":id", tagId.toInt());
            if (query.exec() && query.next()) {
                post.addTag(query.value(0).toString());
            }
        }
    }
    
    emit postUpdated(post);
}

void WordPressAPI::onPostDeleted()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QByteArray responseData = reply->readAll();
    reply->deleteLater();
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isObject()) {
        QJsonObject jsonObj = jsonDoc.object();
        
        if (jsonObj["deleted"].toBool()) {
            int id = jsonObj["previous"].toObject()["id"].toInt();
            emit postDeleted(id);
        } else {
            emit error("删除文章失败");
        }
    } else {
        emit error("无效的响应格式");
    }
}

void WordPressAPI::onCategoriesReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QByteArray responseData = reply->readAll();
    reply->deleteLater();
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isArray()) {
        QList<Category> categories = parseCategories(jsonDoc.array());
        emit categoriesReceived(categories);
    } else {
        emit error("无效的响应格式");
    }
}

void WordPressAPI::onTagsReceived()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QByteArray responseData = reply->readAll();
    reply->deleteLater();
    
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (jsonDoc.isArray()) {
        QList<Tag> tags = parseTags(jsonDoc.array());
        emit tagsReceived(tags);
    } else {
        emit error("无效的响应格式");
    }
}

void WordPressAPI::onMediaUploaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        emit error("无效的网络回复对象");
        return;
    }
    
    // 检查HTTP状态码
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "媒体上传 HTTP状态码: " << statusCode;
    
    QByteArray responseData = reply->readAll();
    qDebug() << "响应数据长度: " << responseData.size() << "字节";
    if (!responseData.isEmpty()) {
        qDebug() << "响应数据预览: " << responseData.left(200) << "...";
    }
    
    reply->deleteLater();
    
    // 检查状态码是否表示成功
    if (statusCode < 200 || statusCode >= 300) {
        QString errorMsg = QString("上传媒体失败，HTTP错误: %1").arg(statusCode);
        
        // 尝试从响应中提取更多错误信息
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
            QJsonObject obj = jsonDoc.object();
            if (obj.contains("message")) {
                errorMsg += "\n错误信息: " + obj["message"].toString();
            }
        }
        
        emit error(errorMsg);
        return;
    }
    
    // 解析成功响应
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit error("JSON解析错误: " + parseError.errorString());
        return;
    }
    
    if (!jsonDoc.isObject()) {
        emit error("无效的响应格式，预期是媒体对象");
        return;
    }
    
    QJsonObject jsonObj = jsonDoc.object();
    
    QString url;
    if (jsonObj.contains("source_url")) {
        url = jsonObj["source_url"].toString();
    } else if (jsonObj.contains("guid") && jsonObj["guid"].isObject()) {
        url = jsonObj["guid"].toObject()["rendered"].toString();
    }
    
    int mediaId = -1;
    if (jsonObj.contains("id")) {
        mediaId = jsonObj["id"].toInt();
    }

    if (!url.isEmpty()) {
        qDebug() << "媒体上传成功，URL: " << url;
        emit mediaUploaded(url,mediaId);
    } else {
        emit error("在响应中找不到媒体URL");
    }

    
    
}

void WordPressAPI::handleNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (reply) {
        QString errorDetails = reply->errorString();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QUrl requestUrl = reply->request().url();
        QByteArray responseData = reply->readAll();
        
        QString fullErrorMsg = QString("网络错误 [%1]: %2\nURL: %3\n")
            .arg(statusCode)
            .arg(errorDetails)
            .arg(requestUrl.toString());
            
        // 尝试解析响应内容，获取更多错误信息
        if (!responseData.isEmpty()) {
            QJsonParseError parseError;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
            
            if (parseError.error == QJsonParseError::NoError && jsonDoc.isObject()) {
                QJsonObject errorObj = jsonDoc.object();
                if (errorObj.contains("message")) {
                    fullErrorMsg += "API错误信息: " + errorObj["message"].toString() + "\n";
                }
                if (errorObj.contains("code")) {
                    fullErrorMsg += "错误代码: " + errorObj["code"].toString();
                }
            } else {
                fullErrorMsg += "响应内容: " + QString::fromUtf8(responseData);
            }
        }
        
        qDebug() << "API错误详情:" << fullErrorMsg;
        emit this->error(fullErrorMsg);
    } else {
        emit this->error("未知网络错误");
    }
}

QList<Post> WordPressAPI::parsePosts(const QJsonArray& jsonArray)
{
    QList<Post> posts;
    
    for (const QJsonValue& value : jsonArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject jsonObj = value.toObject();
        
        int id = jsonObj["id"].toInt();
        QString title = jsonObj["title"].toObject()["rendered"].toString();
        QString content = jsonObj["content"].toObject()["rendered"].toString();
        QString excerpt = jsonObj["excerpt"].toObject()["rendered"].toString();
        QDateTime publishDate = QDateTime::fromString(jsonObj["date"].toString(), Qt::ISODate);
        QString author = jsonObj["author"].toString(); // 理想情况下应该获取作者名称
        Post::Status status = jsonObj["status"].toString() == "publish" ? Post::Published : Post::Draft;
        
        Post post(id, title, content, excerpt, publishDate, author, status);
        
        // 处理特色图片
        if (jsonObj.contains("featured_media") && jsonObj["featured_media"].toInt() > 0) {
            // 理想情况下，我们应该单独获取媒体信息
            // 这里简化处理
        }
        
        // 处理分类
        if (jsonObj.contains("categories") && jsonObj["categories"].isArray()) {
            QJsonArray categoriesArray = jsonObj["categories"].toArray();
            qDebug() << "处理文章分类，文章ID:" << id << "，分类数量:" << categoriesArray.size();
            
            for (const QJsonValue& catValue : categoriesArray) {
                int categoryId = catValue.toInt();
                
                // 从数据库中查找分类名称
                QSqlQuery query;
                query.prepare("SELECT name FROM categories WHERE id = :id");
                query.bindValue(":id", categoryId);
                
                QString categoryName;
                if (query.exec() && query.next()) {
                    categoryName = query.value(0).toString();
                    qDebug() << "找到分类: ID=" << categoryId << "名称=" << categoryName;
                } else {
                    // 如果数据库中没有找到，从远程获取分类信息
                    categoryName = QString("分类%1").arg(categoryId); // 临时名称
                    qDebug() << "未找到分类，使用临时名称: ID=" << categoryId << "名称=" << categoryName;
                    
                    // 创建新的分类条目
                    Category category(categoryId, categoryName);
                    DatabaseManager::instance().saveCategory(category);
                }
                
                post.addCategory(categoryName);
            }
        }
        
        // 处理标签
        if (jsonObj.contains("tags") && jsonObj["tags"].isArray()) {
            QJsonArray tagsArray = jsonObj["tags"].toArray();
            qDebug() << "处理文章标签，文章ID:" << id << "，标签数量:" << tagsArray.size();
            
            for (const QJsonValue& tagValue : tagsArray) {
                int tagId = tagValue.toInt();
                
                // 从数据库中查找标签名称
                QSqlQuery query;
                query.prepare("SELECT name FROM tags WHERE id = :id");
                query.bindValue(":id", tagId);
                
                QString tagName;
                if (query.exec() && query.next()) {
                    tagName = query.value(0).toString();
                    qDebug() << "找到标签: ID=" << tagId << "名称=" << tagName;
                } else {
                    // 如果数据库中没有找到，从远程获取标签信息
                    tagName = QString("标签%1").arg(tagId); // 临时名称
                    qDebug() << "未找到标签，使用临时名称: ID=" << tagId << "名称=" << tagName;
                    
                    // 创建新的标签条目
                    Tag tag(tagId, tagName);
                    DatabaseManager::instance().saveTag(tag);
                }
                
                post.addTag(tagName);
            }
        }
        
        posts.append(post);
    }
    
    return posts;
}

QList<Category> WordPressAPI::parseCategories(const QJsonArray& jsonArray)
{
    QList<Category> categories;
    
    for (const QJsonValue& value : jsonArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject jsonObj = value.toObject();
        
        int id = jsonObj["id"].toInt();
        QString name = jsonObj["name"].toString();
        
        categories.append(Category(id, name));
    }
    
    return categories;
}

QList<Tag> WordPressAPI::parseTags(const QJsonArray& jsonArray)
{
    QList<Tag> tags;
    
    for (const QJsonValue& value : jsonArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject jsonObj = value.toObject();
        
        int id = jsonObj["id"].toInt();
        QString name = jsonObj["name"].toString();
        
        tags.append(Tag(id, name));
    }
    
    return tags;
} 