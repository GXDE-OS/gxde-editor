#include "editorfactory.h"

#include "abstracteditor.h"
#include "legacytexteditor.h"
#include "scintillaeditor.h"

std::unique_ptr<AbstractEditor> EditorFactory::create(const QString &engine)
{
    if (engine.isEmpty() || engine == QStringLiteral("scintilla")) {
        return std::unique_ptr<AbstractEditor>(new ScintillaEditor());
    }

    return std::unique_ptr<AbstractEditor>(new LegacyTextEditor());
}
