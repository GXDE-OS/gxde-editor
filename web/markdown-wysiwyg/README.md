# Markdown 所见即所得编辑器

gxde-editor 的 Markdown WYSIWYG 编辑视图，基于 [Milkdown](https://milkdown.dev/)
（ProseMirror）实现。Qt 侧通过 `QWebEngineView` 加载本工程构建出的静态页面，
用 `QWebChannel` 与 `runJavaScript` 双向通信。

## 构建

```bash
cd web/markdown-wysiwyg
npm install
npm run build
```

产物写入 `src/markdown/wysiwyg/`（由 `vite.config.ts` 指定），并由
`src/gxde-editor.qrc` 打包进可执行文件。**产物随源码一起提交**，因此日常编译
gxde-editor 不需要 Node.js；只有在改动本目录的源码后才需要重新构建。

产物文件名固定（`assets/editor.js`、`assets/editor.css`，不带 hash），
所以重新构建不会导致 `.qrc` 需要同步修改。

## 开发

```bash
npm run dev
```

浏览器里打开提示的地址即可调试。页面在没有 Qt 宿主时会自动降级：
`window.qt` 不存在，所有回传给 Qt 的事件改为打印到控制台。

## 与 Qt 的接口

C++ 通过 `QWebEnginePage::runJavaScript()` 调用 `window.gxdeEditor`：

| 方法 | 说明 |
| --- | --- |
| `load(markdown)` | 用宿主的 Markdown 覆盖文档，不会触发 `markdownChanged` |
| `getMarkdown()` | 返回当前文档序列化后的 Markdown |
| `setReadOnly(bool)` | 切换只读 |
| `focus()` | 编辑区获取焦点 |

页面通过 QWebChannel 上名为 `gxdeEditor` 的对象回传（方法名即 C++ 侧的 slot）：

| 方法 | 说明 |
| --- | --- |
| `ready()` | 编辑器创建完成，宿主可以调用 `load()` 了 |
| `markdownLoaded(markdown)` | `load()` 写入后的规范化结果，宿主用它作为"未被编辑"的基线 |
| `markdownChanged(markdown)` | 文档内容变化 |

`qwebchannel.js` 不打包进产物，运行时从 `qrc:///qtwebchannel/qwebchannel.js`
加载，避免前端工程绑定某个具体的 Qt 版本。

## 已知行为

### 往返不逐字节一致

序列化由 `remark` 完成，Markdown 往返不保证逐字节一致：无序列表的 `-` 会变成
`*`，表格单元格会被补齐空格。所以 `load()` 之后会回传一次 `markdownLoaded`，
宿主必须拿这个规范化结果当基线，不能拿自己写进去的原文去比较——否则光是打开
文件就会被判定成"编辑过"。

### markdownChanged 是防抖过的

`@milkdown/plugin-listener` 对 `markdownUpdated` 做了 200ms 防抖，而且载入文档后
ProseMirror 的收尾事务同样会触发它。因此**收到 `markdownChanged` 不等于用户编辑过**，
宿主侧要按内容与基线比较来判断（`MarkdownWysiwygWidget::isEdited` 就是这么做的）。
