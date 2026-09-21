/**
 * 与 Qt 宿主通信的桥接层。
 *
 * 方向一（Qt -> 页面）：C++ 通过 QWebEnginePage::runJavaScript() 调用
 * `window.gxdeEditor` 上暴露的方法。
 * 方向二（页面 -> Qt）：页面通过 QWebChannel 注入的对象把事件回传给 C++。
 *
 * 页面在没有 Qt 宿主时（例如 `npm run dev` 在浏览器里调试）同样可用，
 * 此时所有回传事件退化为 console 输出。
 */

/** QWebChannel 注入的宿主对象，方法名即为 C++ 侧的 slot 名。 */
export interface HostBridge {
  /** 编辑器就绪，可以接收内容了。 */
  ready(): void
  /** 文档内容发生变化（参数为序列化后的 Markdown）。 */
  markdownChanged(markdown: string): void
}

interface QWebChannelInstance {
  objects: Record<string, unknown>
}

type QWebChannelConstructor = new (
  transport: unknown,
  callback: (channel: QWebChannelInstance) => void,
) => void

declare global {
  interface Window {
    qt?: { webChannelTransport?: unknown }
    QWebChannel?: QWebChannelConstructor
  }
}

let host: HostBridge | null = null

// qwebchannel.js 由 Qt 提供，位于 qrc 资源里。产物本身不打包它，
// 这样前端工程不依赖本机 Qt 的版本。
const QWEBCHANNEL_URL = 'qrc:///qtwebchannel/qwebchannel.js'

function injectQWebChannelScript(): Promise<void> {
  return new Promise((resolve, reject) => {
    const script = document.createElement('script')
    script.src = QWEBCHANNEL_URL
    script.onload = () => resolve()
    script.onerror = () => reject(new Error(`无法加载 ${QWEBCHANNEL_URL}`))
    document.head.appendChild(script)
  })
}

/** 建立到 Qt 宿主的通道；没有宿主时静默降级。 */
export async function connectHost(): Promise<void> {
  const transport = window.qt?.webChannelTransport
  if (!transport) {
    console.info('[gxde-editor] 未检测到 Qt 宿主，以独立模式运行')
    return
  }

  try {
    await injectQWebChannelScript()
    const QWebChannel = window.QWebChannel
    if (!QWebChannel) {
      throw new Error('qwebchannel.js 已加载但未导出 QWebChannel')
    }

    await new Promise<void>((resolve) => {
      new QWebChannel(transport, (channel) => {
        host = channel.objects.gxdeEditor as HostBridge | undefined ?? null
        resolve()
      })
    })

    if (!host) {
      console.warn('[gxde-editor] 宿主未注册 gxdeEditor 对象')
    }
  } catch (error) {
    console.warn('[gxde-editor] 连接 Qt 宿主失败：', error)
  }
}

function emit<K extends keyof HostBridge>(method: K, ...args: Parameters<HostBridge[K]>): void {
  if (!host) {
    console.debug(`[gxde-editor] ${String(method)}`, ...args)
    return
  }
  try {
    ;(host[method] as (...a: unknown[]) => void)(...args)
  } catch (error) {
    console.error(`[gxde-editor] 调用宿主 ${String(method)} 失败：`, error)
  }
}

export const notifyReady = (): void => emit('ready')
export const notifyMarkdownChanged = (markdown: string): void => emit('markdownChanged', markdown)
