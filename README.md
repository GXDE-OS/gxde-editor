# GXDE Editor

GXDE Editor is a desktop text editor that supports common text editing features.

## Dependencies

In debian, use below command to install compile dependencies:

`sudo apt install cmake libqt5widgets5 libdtkcore-dev libdtkwidget-dev qt5-default libpolkit-qt5-1-dev libkf5syntaxhighlighting-dev libkf5codecs-dev qttools5-dev-tools qtbase5-private-dev`

## Installation

~~Build auto save daemon~~

```
sudo cp com.deepin.editor.policy /usr/share/polkit-1/actions
sudo cp com.deepin.editor.conf /usr/share/dbus-1/system.d/
sudo cp com.deepin.editor.daemon.service /usr/share/dbus-1/system-services

cd ./daemon
mkdir build
cd build
qmake ..
make
sudo ./gxde-editor-daemon
```

* Build editor

```
cd ./gxde-editor
mkdir build
cd build
cmake ..
make
./gxde-editor
```

## Usage

Below is keymap list for gxde-editor:

## Config file

configure save at: ~/.config/deepin/gxde-editor/config.conf

## Getting help

Any usage issues can ask for help via

* [Gitter](https://gitter.im/orgs/linuxdeepin/rooms)
* [IRC channel](https://webchat.freenode.net/?channels=deepin)
* [Forum](https://bbs.deepin.org)
* [WiKi](http://wiki.deepin.org/)

## Getting involved

We encourage you to report issues and contribute changes

* [Contribution guide for developers](https://github.com/linuxdeepin/developer-center/wiki/Contribution-Guidelines-for-Developers-en). (English)
* [开发者代码贡献指南](https://github.com/linuxdeepin/developer-center/wiki/Contribution-Guidelines-for-Developers) (中文)

## License

GXDE Editor is licensed under [GPLv3](LICENSE).
