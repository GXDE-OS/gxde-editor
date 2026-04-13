#ifndef EDITORFACTORY_H
#define EDITORFACTORY_H

#include <QString>
#include <memory>

class AbstractEditor;

class EditorFactory
{
public:
    static std::unique_ptr<AbstractEditor> create(const QString &engine);
};

#endif
