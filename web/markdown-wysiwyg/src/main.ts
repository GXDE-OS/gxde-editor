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
