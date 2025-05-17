#include "Category.h"

Category::Category()
    : m_id(-1)
{
}

Category::Category(int id, const QString& name)
    : m_id(id), m_name(name)
{
}

Category::~Category()
{
}

int Category::id() const
{
    return m_id;
}

void Category::setId(int id)
{
    m_id = id;
}

QString Category::name() const
{
    return m_name;
}

void Category::setName(const QString& name)
{
    m_name = name;
} 