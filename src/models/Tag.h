#pragma once

#include <QString>

class Tag {
public:
    Tag();
    Tag(int id, const QString& name);
    ~Tag();

    int id() const;
    void setId(int id);

    QString name() const;
    void setName(const QString& name);

private:
    int m_id;
    QString m_name;
}; 