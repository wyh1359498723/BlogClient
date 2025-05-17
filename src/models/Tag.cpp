#include "Tag.h"

Tag::Tag()
    : m_id(-1)
{
}

Tag::Tag(int id, const QString& name)
    : m_id(id), m_name(name)
{
}

Tag::~Tag()
{
}

int Tag::id() const
{
    return m_id;
}

void Tag::setId(int id)
{
    m_id = id;
}

QString Tag::name() const
{
    return m_name;
}

void Tag::setName(const QString& name)
{
    m_name = name;
} 