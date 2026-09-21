/**
 * 界面文案。
 *
 * 页面里所有给人看的字都从这里出，包括 Crepe 自己那套英文 —— 它们虽然由
 * Crepe 提供，但每一个都能通过特性配置覆盖，所以同样归这里管。不这么收拢的话，
 * 中文用户会在同一个编辑器里同时看到 "Heading 1" 和 "行数"。
 *
 * 英文是源语言，跟 Qt 侧 tr() 的源语言一致，也是拿不到译文时的回退。
 * 译文由 Qt 宿主在页面脚本执行前注入到 window.__gxdeI18n，形状是
 *
 *     { texts: { "Heading 1": "标题 1", ... }, locale: "zh-CN" }
 *
 * 键就是英文原文，跟 .ts 里的 <source> 一字不差。于是这里不需要另建一张
 * key -> 英文 的对照表：查不到就用键本身。两边对不上的后果也只是回退成英文，
 * 不会显示成空白 —— 对照表那种写法，漏一条就是界面上少一块字。
 *
 * `npm run dev` 在浏览器里单独调试时没有宿主，全部走英文回退。
 */

interface InjectedI18n {
  texts?: Record<string, string>
  locale?: string
}

/** 惰性读：注入脚本在页面脚本之前跑，但早读一次也没什么好处。 */
let catalog: Record<string, string> | null = null

function injected(): InjectedI18n | undefined {
  return (window as unknown as { __gxdeI18n?: InjectedI18n }).__gxdeI18n
}

function texts(): Record<string, string> {
  if (!catalog) {
    catalog = injected()?.texts ?? {}
  }
  return catalog
}

/** 取译文；没有译文时返回英文原文。 */
export function t(source: string): string {
  const text = texts()[source]
  return text ? text : source
}

/**
 * 带占位符的文案，%1 会被替换掉。
 *
 * 占位符沿用 Qt 的写法而不是模板字符串，是为了让译文里的语序可以跟英文不同 ——
 * "Decrease %1" 换成中文是"减少%1"，一个 %1 两边都够用，而拼字符串的做法
 * 在中文里会多出一个空格。
 */
export function t1(source: string, value: string): string {
  return t(source).replace('%1', value)
}

/**
 * 把语言告诉浏览器。
 *
 * 不是可有可无的装饰：Chromium 拿 <html lang> 决定同一个码位的汉字该取哪套字形，
 * 页面里写死 zh-CN 的话，日文用户看到的是中文字形的"直"和"骨"。
 */
export function applyDocumentLocale(): void {
  const locale = injected()?.locale
  if (locale) {
    document.documentElement.lang = locale
  }
}
