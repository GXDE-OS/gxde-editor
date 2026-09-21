import { HighlightStyle, syntaxHighlighting } from '@codemirror/language'
import { languages } from '@codemirror/language-data'
import { tags } from '@lezer/highlight'
import { CrepeBuilder } from '@milkdown/crepe/builder'
import { blockEdit } from '@milkdown/crepe/feature/block-edit'
import { codeMirror } from '@milkdown/crepe/feature/code-mirror'
import { cursor } from '@milkdown/crepe/feature/cursor'
import { linkTooltip } from '@milkdown/crepe/feature/link-tooltip'
import { listItem } from '@milkdown/crepe/feature/list-item'
import { placeholder } from '@milkdown/crepe/feature/placeholder'
import { table } from '@milkdown/crepe/feature/table'
import { toolbar } from '@milkdown/crepe/feature/toolbar'
import { topBar } from '@milkdown/crepe/feature/top-bar'

import '@milkdown/crepe/theme/common/prosemirror.css'
import '@milkdown/crepe/theme/common/reset.css'
import '@milkdown/crepe/theme/common/block-edit.css'
import '@milkdown/crepe/theme/common/code-mirror.css'
import '@milkdown/crepe/theme/common/cursor.css'
import '@milkdown/crepe/theme/common/link-tooltip.css'
import '@milkdown/crepe/theme/common/list-item.css'
import '@milkdown/crepe/theme/common/placeholder.css'
import '@milkdown/crepe/theme/common/table.css'
import '@milkdown/crepe/theme/common/toolbar.css'
import '@milkdown/crepe/theme/common/top-bar.css'
import '@milkdown/crepe/theme/classic.css'
import {
  commandsCtx,
  editorViewCtx,
  editorViewOptionsCtx,
  nodeViewCtx,
  parserCtx,
} from '@milkdown/kit/core'
import type { Ctx } from '@milkdown/kit/ctx'
import {
  addBlockTypeCommand,
  clearTextInCurrentBlockCommand,
  selectTextNearPosCommand,
} from '@milkdown/kit/preset/commonmark'
import { createTable } from '@milkdown/kit/preset/gfm'
import type { Node as ProseNode } from '@milkdown/kit/prose/model'
import type { NodeView, NodeViewConstructor } from '@milkdown/kit/prose/view'
import '@milkdown/kit/prose/view/style/prosemirror.css'
import '@milkdown/kit/prose/gapcursor/style/gapcursor.css'
import {
  connectHost,
  notifyMarkdownChanged,
  notifyMarkdownLoaded,
  notifyReady,
} from './bridge'
import { applyDocumentLocale, t } from './i18n'
import { showTableSizePicker } from './table-picker'
import './style.css'

/** 暴露给 Qt（QWebEnginePage::runJavaScript）的接口。 */
interface GxdeEditorApi {
  load(markdown: string): void
  /** 取回当前文档序列化后的 Markdown。 */
  getMarkdown(): string
  /** 切换只读；只读时页面仍可滚动与选择。 */
  setReadOnly(value: boolean): void
  /**
   * 指定相对 URL 的基准目录（文档所在目录），必须在 load() 之前调用。
   *
   * 页面自身是从 gxde-md://editor/ 加载的，markdown 里的相对图片路径在它底下
   * 只会解析到编辑器资源那儿去，那儿并没有图。把基准指到文档目录，相对路径
   * 才落在磁盘上真正的位置。
   */
  setBaseUrl(url: string): void
  /** 让编辑区获得焦点。 */
  focus(): void
}

declare global {
  interface Window {
    gxdeEditor?: GxdeEditorApi
  }
}

const root = document.getElementById('app')
if (!root) {
  throw new Error('index.html 缺少 #app 容器')
}

let crepe: CrepeBuilder | null = null

function requireCrepe(): CrepeBuilder {
  if (!crepe) {
    throw new Error('编辑器尚未创建')
  }
  return crepe
}

function readMarkdown(): string {
  return requireCrepe().getMarkdown()
}

function writeMarkdown(markdown: string): void {
  requireCrepe().editor.action((ctx) => {
    const view = ctx.get(editorViewCtx)
    const doc = ctx.get(parserCtx)(markdown)
    if (!doc) {
      console.warn('[gxde-editor] Markdown 解析失败，已忽略本次内容写入')
      return
    }

    const { state } = view
    // 宿主写入不进撤销栈，否则用户一次 Ctrl+Z 就会把切换视图前的旧内容撤回来。
    const tr = state.tr
      .replaceWith(0, state.doc.content.size, doc.content)
      .setMeta('addToHistory', false)
    view.dispatch(tr)
    view.focus()
  })
}

/**
 * 回传规范化后的基线。
 *
 * 必须在 dispatch 之后同步取：这时 state 已经是新文档了，不必等 listener 的防抖。
 * 宿主拿这个基线（而不是它自己写进来的原文）来判断内容有没有被改过，就不会被
 * Markdown 往返的规范化（`-` → `*`、表格对齐等）误判成用户编辑。
 */
function reportBaseline(): void {
  notifyMarkdownLoaded(readMarkdown())
}

/**
 * 原生 HTML 在编辑器里按它本来的意思渲染。
 *
 * Milkdown 的 html 节点默认是把 HTML 源码当纯文本塞进一个 <span>
 * （见 preset-commonmark 里 htmlSchema 的 toDOM），于是 README 里
 * <img src="…"> 这类在线徽章全成了一行行源码文字，图片一张都看不见。
 * 这里自己接管渲染：把源码解析成真正的 DOM 插进来。
 *
 * 节点本身仍是不可编辑的原子节点，写回 Markdown 走的还是 node.attrs.value，
 * 所以文件里的一个字节都不会被这段渲染动到。
 */
class HtmlNodeView implements NodeView {
  readonly dom: HTMLElement

  constructor(node: ProseNode) {
    this.dom = document.createElement('span')
    this.dom.className = 'gxde-html-node'
    this.dom.setAttribute('data-type', 'html')
    this.renderValue(node)
  }

  private renderValue(node: ProseNode): void {
    const parsed = new DOMParser().parseFromString(
      String(node.attrs.value ?? ''),
      'text/html',
    )
    // parseFromString 出来的脚本不会跑，但 appendChild 插进去的会 —— 得先摘掉。
    for (const script of Array.from(parsed.body.querySelectorAll('script'))) {
      script.remove()
    }

    this.dom.replaceChildren(...Array.from(parsed.body.childNodes))
  }

  update(node: ProseNode): boolean {
    if (node.type.name !== 'html') {
      return false
    }
    this.renderValue(node)
    return true
  }

  /** 里面的 DOM 是我们自己画的，别让 ProseMirror 当成用户改动去重绘。 */
  ignoreMutation(): boolean {
    return true
  }
}

/**
 * 改写页面的 <base>，从而改变相对 URL 的解析基准。
 *
 * 只对之后解析的 URL 生效：已经挂到 DOM 上的 <img> 不会自己重算，
 * 所以宿主得在 load()（也就是重画文档）之前调用它。
 */
function applyBaseUrl(url: string): void {
  let base = document.head.querySelector('base')
  if (!base) {
    base = document.createElement('base')
    document.head.insertBefore(base, document.head.firstChild)
  }
  base.href = url
}

/**
 * 表格图标，取自 Crepe 顶部工具栏自带的那个，只去掉了外面那层 <clipPath>：
 * 它裁的是一整个 24×24 视口，纯属冗余，而两个入口各插一份会让它的 id 撞车。
 */
const TABLE_ICON =
  '<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">' +
  '<path d="M20 3H5C3.9 3 3 3.9 3 5V19C3 20.1 3.9 21 5 21H20C21.1 21 22 20.1 22 19V5' +
  'C22 3.9 21.1 3 20 3ZM20 5V8H5V5H20ZM15 19H10V10H15V19ZM5 10H8V19H5V10ZM17 19V10H20V19H17Z"/></svg>'

/**
 * 按给定行列数插入表格。
 *
 * 步骤照抄 Crepe 内置那套（见 crepe 的 block-edit / top-bar 里 table 项的 onRun），
 * 只有落光标那一步不一样，理由写在下面。
 */
function insertTable(ctx: Ctx, rows: number, cols: number, fromSlashMenu: boolean): void {
  const commands = ctx.get(commandsCtx)
  if (fromSlashMenu) {
    commands.call(clearTextInCurrentBlockCommand.key)
  }

  // 位置必须等清完 `/table` 再取。清文本那步会缩短当前段落，之后所有位置都往前挪，
  // 开面板时记下的那个已经作废了 —— 它是用来给面板定位的，不是用来插的。
  const { from } = ctx.get(editorViewCtx).state.selection
  commands.call(addBlockTypeCommand.key, {
    nodeType: createTable(ctx, rows, cols),
  })

  // 光标往表里走一格。
  //
  // 这里跟 Crepe 内置的不一样：它用原来的 `from`，而那个位置在光标位于非空段落时会
  // 解析回**表格前面那个段落**里，于是从顶部工具栏插完表，光标还停在上面的段落，
  // 得手动点进表格才能打字。斜杠菜单那条路因为光标所在是空段落、整段被表格顶掉，
  // 才碰巧没事 —— 也就是说两个入口的行为本来就不一致。
  //
  // `from + 1` 落在表格上、而不是任何文本块内部，TextSelection.near 便会自己往下钻到
  // 第一个单元格。表格起于哪里取决于光标当时在哪种块里，三种都实跑过：
  //   空段落（斜杠菜单）—— 整段被表格顶掉，表格起于 `from - 1`，`from + 1` 是它的内容
  //   段末（顶栏）      —— 表格接在段落之后、另补一个空段落，起于 `from + 1`
  //   段中（顶栏）      —— 段落被劈成两半、表格夹在中间，仍起于 `from + 1`
  // 三种落点 `from + 1` 都还没走出表格。
  commands.call(selectTextNearPosCommand.key, { pos: from + 1 })
  ctx.get(editorViewCtx).focus()
}

/**
 * 问尺寸，问到了再插。
 *
 * 面板的位置在这里就取好：`coordsAtPos` 返回的正是视口坐标，直接拿来定位。
 * 但插入位置不在这里定 —— 见 insertTable 里的说明。
 */
function requestTableSize(ctx: Ctx, fromSlashMenu: boolean): void {
  const view = ctx.get(editorViewCtx)
  const { from } = view.state.selection
  // 挂到 .milkdown 上：--crepe-* 那组变量声明在那儿，挂到 document.body 上取不到。
  const mount = view.dom.parentElement ?? document.body
  showTableSizePicker(view.coordsAtPos(from), mount, (rows, cols) => {
    insertTable(ctx, rows, cols, fromSlashMenu)
  })
}

function exposeApi(): void {
  window.gxdeEditor = {
    load(markdown) {
      writeMarkdown(markdown)
      reportBaseline()
    },
    getMarkdown: readMarkdown,
    setReadOnly(value) {
      requireCrepe().setReadonly(value)
    },
    setBaseUrl: applyBaseUrl,
    focus() {
      requireCrepe().editor.action((ctx) => ctx.get(editorViewCtx).focus())
    },
  }
}

const CODE_HIGHLIGHT = syntaxHighlighting(HighlightStyle.define([
  { tag: tags.keyword, class: 'tok-keyword' },
  { tag: [tags.controlKeyword, tags.moduleKeyword], class: 'tok-keyword' },
  { tag: [tags.string, tags.special(tags.string), tags.regexp], class: 'tok-string' },
  { tag: [tags.number, tags.bool, tags.null, tags.atom], class: 'tok-number' },
  { tag: [tags.comment, tags.lineComment, tags.blockComment], class: 'tok-comment' },
  {
    tag: [
      tags.function(tags.variableName),
      tags.function(tags.propertyName),
      tags.labelName,
    ],
    class: 'tok-function',
  },
  {
    tag: [tags.typeName, tags.className, tags.namespace, tags.tagName],
    class: 'tok-type',
  },
  { tag: [tags.propertyName, tags.attributeName], class: 'tok-property' },
  { tag: [tags.definition(tags.variableName), tags.variableName], class: 'tok-variable' },
  { tag: [tags.operator, tags.punctuation, tags.bracket], class: 'tok-operator' },
  { tag: [tags.meta, tags.processingInstruction], class: 'tok-meta' },
]))

async function main(): Promise<void> {
  applyDocumentLocale()

  // 先连通道再建编辑器：宿主可能在收到 ready 之后立刻调用 gxdeEditor。
  await connectHost()

  crepe = new CrepeBuilder({
    root,
    // 初始内容由宿主通过 load() 注入，这里先给空文档。
    defaultValue: '',
  })
    // 块左侧的手柄：拖拽排序，点开有删除/复制。它同时提供输入 `/` 的块类型菜单。
    .addFeature(blockEdit, {
      // 斜杠菜单里的分组名和每一项都是纯文字，Crepe 只给英文默认值，逐条换掉。
      // 只写 label 不写 icon 是有意的：icon 缺省时 Crepe 用自己的图标，
      // 类型上是 DeepPartial，允许这么给。
      textGroup: {
        label: t('Text'),
        text: { label: t('Text') },
        h1: { label: t('Heading 1') },
        h2: { label: t('Heading 2') },
        h3: { label: t('Heading 3') },
        h4: { label: t('Heading 4') },
        h5: { label: t('Heading 5') },
        h6: { label: t('Heading 6') },
        quote: { label: t('Quote') },
        divider: { label: t('Divider') },
      },
      listGroup: {
        label: t('List'),
        bulletList: { label: t('Bullet List') },
        orderedList: { label: t('Ordered List') },
        taskList: { label: t('Task List') },
      },
      // 内置的 Table 项写死插 3×3，而且只开放 label/icon、没有 onRun 可配，先关掉，
      // 再在 buildMenu 里往同一个分组补一个会问尺寸的。分组一定在：它由上面这个
      // advancedGroup 非 null 决定，正是我们给的值。
      advancedGroup: {
        label: t('Advanced'),
        codeBlock: { label: t('Code') },
        table: null,
      },
      buildMenu: (builder) => {
        builder.getGroup('advanced').addItem('table', {
          label: t('Table'),
          icon: TABLE_ICON,
          onRun: (ctx) => requestTableSize(ctx, true),
        })
      },
    })
    // 选中文字后浮出的格式化工具栏。按钮上只有图标，label 是悬停提示，
    // 同时也是无障碍名字，一样得跟着界面语言走。
    .addFeature(toolbar, {
      boldLabel: t('Bold'),
      italicLabel: t('Italic'),
      strikethroughLabel: t('Strikethrough'),
      codeLabel: t('Inline code'),
      linkLabel: t('Link'),
    })
    // 常驻顶部的工具栏。它不在 defaultFeatures 里，Crepe 默认是关的，得显式加。
    .addFeature(topBar, {
      // 标题下拉里那几项是纯文字，是顶部工具栏上唯一直接显示文字的地方。
      headingOptions: [
        { label: t('Paragraph'), level: null },
        { label: t('Heading 1'), level: 1 },
        { label: t('Heading 2'), level: 2 },
        { label: t('Heading 3'), level: 3 },
        { label: t('Heading 4'), level: 4 },
        { label: t('Heading 5'), level: 5 },
        { label: t('Heading 6'), level: 6 },
      ],
      // 顶部工具栏那个表格按钮也写死 3×3，而且没有开关可关（只取决于 table 特性
      // 挂没挂），只能从 insert 分组里把它摘掉再换一个。insert 分组是无条件建的。
      buildTopBar: (builder) => {
        const insert = builder.getGroup('insert')
        const at = insert.group.items.findIndex((item) => item.key === 'table')
        if (at >= 0) {
          insert.group.items.splice(at, 1)
        }
        insert.addItem('table', {
          icon: TABLE_ICON,
          active: () => false,
          onRun: (ctx) => requestTableSize(ctx, false),
        })
      },
    })
    .addFeature(linkTooltip, {
      inputPlaceholder: t('Paste link...'),
    })
    .addFeature(table)
    .addFeature(listItem)
    .addFeature(cursor)
    .addFeature(codeMirror, {
      // 全量语言表，按需加载：每个语言的解析器都是 import()，由 vite 的
      // inlineDynamicImports 内联进 editor.js，产物仍是单文件、离线可用。
      languages,
      theme: CODE_HIGHLIGHT,
      // 代码块工具栏上的三处文字：语言搜索框的占位、搜不到时的提示、复制按钮。
      searchPlaceholder: t('Search language'),
      noResultText: t('No result'),
      copyText: t('Copy'),
    })
    .addFeature(placeholder, { text: t('Type / to insert') })
    // 这里故意不挂 imageBlock。它的 schema 把图片的 alt 当成自己的缩放比例存储槽：
    // 解析时 `ratio = Number(node.alt || 1)`，序列化时把比例 toFixed(2) 写回 alt。
    // 于是 ![架构图](x.png "图注") 存一次就变成 ![1.00](x.png "图注")，alt 直接没了 ——
    // 这是 schema 里写死的，没有配置能关。图片该不该有缩放句柄是小事，把用户写的
    // alt 文字吃掉是大事。不挂它，图片仍按 commonmark 的 image 节点渲染成普通 <img>，
    // alt/title 原样保留。

  // Crepe 构造时用 ctx.set 整个替换了 editorViewOptionsCtx，原来挂在这儿的 class
  // 被冲掉了，得补回去。
  crepe.editor.config((ctx) => {
    ctx.update(editorViewOptionsCtx, (prev) => ({
      ...prev,
      attributes: {
        ...prev.attributes,
        class: 'gxde-milkdown-content',
        spellcheck: 'false',
      },
    }))

    // html 节点视图走 nodeViewCtx，不能塞进上面那个 editorViewOptionsCtx.nodeViews。
    //
    // core 建 EditorView 时写的是 `new EditorView(el, { nodeViews, markViews, ...options })`，
    // `...options` 展开在后面，于是 options 里只要有一个 nodeViews 键，就会把由
    // nodeViewCtx（也就是 $view 注册的地方）归拢出来的那一整份顶掉。Crepe 的表格
    // 节点视图正在那一份里：它负责给表格套上 .milkdown-table-block 这层外壳，而表格
    // 的边框和内边距规则全都限定在这层外壳下（见 crepe 的 common/table.css），
    // 外壳没了，表格就是一副没框的样子。所以往 nodeViewCtx 里追加，跟 $view 同路。
    ctx.update(nodeViewCtx, (prev): [string, NodeViewConstructor][] => [
      ...prev,
      ['html', (node: ProseNode) => new HtmlNodeView(node)],
    ])
  })

  // 注意：listener 插件对 markdownUpdated 做了 200ms 防抖
  //（见 @milkdown/plugin-listener 里的 debounce(..., 200)），所以这里收到的事件
  // 不一定对应用户操作 —— 载入文档后 ProseMirror 的收尾事务同样会走到这里。
  // 是不是"用户改过"由宿主按内容判断（对比 markdownLoaded 给出的基线），
  // 这里只负责如实上报。
  crepe.on((listener) => {
    listener.markdownUpdated((_ctx, markdown, prevMarkdown) => {
      if (markdown === prevMarkdown) {
        return
      }
      notifyMarkdownChanged(markdown)
    })
  })

  await crepe.create()

  exposeApi()
  notifyReady()
}

void main()
