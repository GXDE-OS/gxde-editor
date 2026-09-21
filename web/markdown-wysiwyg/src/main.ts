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
import {
  editorViewCtx,
  editorViewOptionsCtx,
  parserCtx,
} from '@milkdown/kit/core'
import type { Node as ProseNode } from '@milkdown/kit/prose/model'
import type { NodeView } from '@milkdown/kit/prose/view'
import '@milkdown/kit/prose/view/style/prosemirror.css'
import '@milkdown/kit/prose/gapcursor/style/gapcursor.css'
import {
  connectHost,
  notifyMarkdownChanged,
  notifyMarkdownLoaded,
  notifyReady,
} from './bridge'
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

async function main(): Promise<void> {
  // 先连通道再建编辑器：宿主可能在收到 ready 之后立刻调用 gxdeEditor。
  await connectHost()

  crepe = new CrepeBuilder({
    root,
    // 初始内容由宿主通过 load() 注入，这里先给空文档。
    defaultValue: '',
  })
    // 块左侧的手柄：拖拽排序，点开有删除/复制。它同时提供输入 `/` 的块类型菜单。
    .addFeature(blockEdit)
    // 选中文字后浮出的格式化工具栏。
    .addFeature(toolbar)
    // 常驻顶部的工具栏。它不在 defaultFeatures 里，Crepe 默认是关的，得显式加。
    .addFeature(topBar)
    .addFeature(linkTooltip)
    .addFeature(table)
    .addFeature(listItem)
    .addFeature(cursor)
    .addFeature(codeMirror)
    .addFeature(placeholder, { text: '输入 / 插入内容' })
    // 这里故意不挂 imageBlock。它的 schema 把图片的 alt 当成自己的缩放比例存储槽：
    // 解析时 `ratio = Number(node.alt || 1)`，序列化时把比例 toFixed(2) 写回 alt。
    // 于是 ![架构图](x.png "图注") 存一次就变成 ![1.00](x.png "图注")，alt 直接没了 ——
    // 这是 schema 里写死的，没有配置能关。图片该不该有缩放句柄是小事，把用户写的
    // alt 文字吃掉是大事。不挂它，图片仍按 commonmark 的 image 节点渲染成普通 <img>，
    // alt/title 原样保留。

  // 两处定制得补回去：Crepe 构造时用 ctx.set 整个替换了 editorViewOptionsCtx，
  // 我们原来挂在这儿的 class 和 html 节点渲染一并被冲掉了。
  crepe.editor.config((ctx) => {
    ctx.update(editorViewOptionsCtx, (prev) => ({
      ...prev,
      attributes: {
        ...prev.attributes,
        class: 'gxde-milkdown-content',
        spellcheck: 'false',
      },
      nodeViews: {
        ...prev.nodeViews,
        html: (node) => new HtmlNodeView(node),
      },
    }))
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
