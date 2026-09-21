#!/bin/bash
cd `dirname $0`

mapfile -t sources < <(find src -type f \( -name '*.cpp' -o -name '*.h' \))

AST=translations/gxde-editor_ast.ts
if [ -f "$AST" ]; then
    trap 'sed -i "/^<TS /s/language=\"[^\"]*\"/language=\"ast\"/" "$AST"' EXIT
    sed -i '/^<TS /s/language="[^"]*"/language="es_ES"/' "$AST"
fi

/usr/lib/qt6/bin/lupdate -no-obsolete "${sources[@]}" -ts translations/gxde-editor*.ts
