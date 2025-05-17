#pragma once

#include <QString>

class Category {
public:
    Category();
    Category(int id, const QString& name);
    ~Category();

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

private:
    int m_id;
    QString m_name;
}; 