import {
  Editor,
  defaultValueCtx,
  editorViewCtx,
  editorViewOptionsCtx,
  parserCtx,
  rootCtx,
  serializerCtx,
} from '@milkdown/kit/core'
import { clipboard } from '@milkdown/kit/plugin/clipboard'
import { cursor } from '@milkdown/kit/plugin/cursor'
import { history } from '@milkdown/kit/plugin/history'
import { listener, listenerCtx } from '@milkdown/kit/plugin/listener'
import { commonmark } from '@milkdown/kit/preset/commonmark'
import { gfm } from '@milkdown/kit/preset/gfm'
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

// 只读状态放在模块级变量里，ProseMirror 每次刷新 DOM 时重新求值。
let readOnly = false

let editor: Editor | null = null

function requireEditor(): Editor {
  if (!editor) {
    throw new Error('编辑器尚未创建')
  }
  return editor
}

function readMarkdown(): string {
  return requireEditor().action((ctx) => ctx.get(serializerCtx)(ctx.get(editorViewCtx).state.doc))
}

function writeMarkdown(markdown: string): void {
  requireEditor().action((ctx) => {
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

function applyReadOnly(value: boolean): void {
  readOnly = value
  requireEditor().action((ctx) => {
    ctx.get(editorViewCtx).setProps({ editable: () => !readOnly })
  })
}

function exposeApi(): void {
  window.gxdeEditor = {
    load(markdown) {
      writeMarkdown(markdown)
      reportBaseline()
    },
    getMarkdown: readMarkdown,
    setReadOnly: applyReadOnly,
    setBaseUrl: applyBaseUrl,
    focus() {
      requireEditor().action((ctx) => ctx.get(editorViewCtx).focus())
    },
  }
}

async function main(): Promise<void> {
  // 先连通道再建编辑器：宿主可能在收到 ready 之后立刻调用 gxdeEditor。
  await connectHost()

  editor = await Editor.make()
    .config((ctx) => {
      ctx.set(rootCtx, root)
      // 初始内容由宿主通过 load() 注入，这里先给空文档。
      ctx.set(defaultValueCtx, '')
      ctx.update(editorViewOptionsCtx, (prev) => ({
        ...prev,
        editable: () => !readOnly,
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

      // 注意：listener 插件对 markdownUpdated 做了 200ms 防抖
      //（见 @milkdown/plugin-listener 里的 debounce(..., 200)），所以这里收到的事件
      // 不一定对应用户操作 —— 载入文档后 ProseMirror 的收尾事务同样会走到这里。
      // 是不是"用户改过"由宿主按内容判断（对比 markdownLoaded 给出的基线），
      // 这里只负责如实上报。
      ctx.get(listenerCtx).markdownUpdated((_ctx, markdown, prevMarkdown) => {
        if (markdown === prevMarkdown) {
          return
        }
        notifyMarkdownChanged(markdown)
      })
    })
    .use(commonmark)
    .use(gfm)
    .use(listener)
    .use(history)
    .use(clipboard)
    .use(cursor)
    .create()

  exposeApi()
  notifyReady()
}

void main()
