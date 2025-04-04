#!/bin/bash
cd `dirname $0`
lupdate -recursive */ -ts translations/gxde-editor_*.ts
