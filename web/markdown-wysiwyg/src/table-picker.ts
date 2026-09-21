/**
 * 插入表格前问行列数的小面板。
 *
 * 挂在 .milkdown 里（挂载点由调用方给），跟 Crepe 自己的斜杠菜单、块手柄一样 ——
 * 那组 --crepe-* 变量正是声明在 .milkdown 上的，挂到 document.body 就取不到了。
 * 它是 .ProseMirror 的兄弟节点而非子节点，所以点面板不会被当成文档里的鼠标事件。
 *
 * 定位用 position: fixed 加 caret 的视口坐标：一个静态锚点的弹层，不值得为它引入定位库。
 */

import { t, t1 } from './i18n'

export interface TableSizeAnchor {
  left: number
  top: number
  bottom: number
}

export type TableSizeConfirm = (rows: number, cols: number) => void

const MIN_SIZE = 1
const MAX_SIZE = 8
const DEFAULT_SIZE = 3
/** 面板与 caret 之间、以及与视口边缘之间留的空隙。 */
const GAP = 6
const MARGIN = 8

/**
 * 同一时刻只留一个面板。
 *
 * 存的是关闭函数而不只是元素：重开时得走一遍完整的关闭流程，否则上一个面板挂在
 * document 上的 keydown / pointerdown 监听会留在那儿。
 */
let openPanel: { element: HTMLElement; close: () => void } | null = null

function clampSize(value: number): number {
  if (!Number.isFinite(value)) {
    return DEFAULT_SIZE
  }
  return Math.min(MAX_SIZE, Math.max(MIN_SIZE, Math.round(value)))
}

/** 行数 / 列数各是一个 [−][数字][+] 步进器。 */
function buildStepper(label: string): { root: HTMLElement; input: HTMLInputElement } {
  const root = document.createElement('div')
  root.className = 'gxde-table-picker-stepper'

  const minus = document.createElement('button')
  minus.type = 'button'
  minus.className = 'gxde-table-picker-step'
  minus.textContent = '−'
  minus.setAttribute('aria-label', t1('Decrease %1', label))

  const input = document.createElement('input')
  input.type = 'number'
  input.className = 'gxde-table-picker-input'
  input.min = String(MIN_SIZE)
  input.max = String(MAX_SIZE)
  input.value = String(DEFAULT_SIZE)
  input.setAttribute('aria-label', label)

  const plus = document.createElement('button')
  plus.type = 'button'
  plus.className = 'gxde-table-picker-step'
  plus.textContent = '+'
  plus.setAttribute('aria-label', t1('Increase %1', label))

  const step = (delta: number) => {
    input.value = String(clampSize(Number(input.value) + delta))
  }
  // preventDefault：不让焦点从输入框跑掉，否则连点几下就得重新聚焦。
  minus.addEventListener('pointerdown', (e) => e.preventDefault())
  plus.addEventListener('pointerdown', (e) => e.preventDefault())
  minus.addEventListener('click', () => step(-1))
  plus.addEventListener('click', () => step(1))
  // 手输的数字可能越界或是空的，离开时归一。
  input.addEventListener('blur', () => {
    input.value = String(clampSize(Number(input.value)))
  })

  root.append(minus, input, plus)
  return { root, input }
}

function buildRow(label: string, stepper: HTMLElement): HTMLElement {
  const row = document.createElement('div')
  row.className = 'gxde-table-picker-row'
  const text = document.createElement('span')
  text.className = 'gxde-table-picker-label'
  text.textContent = label
  row.append(text, stepper)
  return row
}

function place(panel: HTMLElement, anchor: TableSizeAnchor): void {
  const { width, height } = panel.getBoundingClientRect()
  // 优先贴在 caret 下方；下方放不下就翻到上方。
  let top = anchor.bottom + GAP
  if (top + height > window.innerHeight - MARGIN) {
    top = anchor.top - height - GAP
  }
  const left = Math.min(
    Math.max(anchor.left, MARGIN),
    Math.max(MARGIN, window.innerWidth - width - MARGIN),
  )
  panel.style.left = `${left}px`
  panel.style.top = `${Math.max(MARGIN, top)}px`
}

export function showTableSizePicker(
  anchor: TableSizeAnchor,
  mount: HTMLElement,
  onConfirm: TableSizeConfirm,
): void {
  openPanel?.close()

  const panel = document.createElement('div')
  panel.className = 'gxde-table-picker'
  panel.setAttribute('role', 'dialog')
  panel.setAttribute('aria-label', t('Insert Table'))

  const rows = buildStepper(t('Rows'))
  const cols = buildStepper(t('Columns'))
  panel.append(buildRow(t('Rows'), rows.root), buildRow(t('Columns'), cols.root))

  const confirm = document.createElement('button')
  confirm.type = 'button'
  confirm.className = 'gxde-table-picker-confirm'
  confirm.textContent = t('Confirm')
  panel.append(confirm)

  const close = () => {
    document.removeEventListener('keydown', onKeyDown, true)
    document.removeEventListener('pointerdown', onOutside, true)
    panel.remove()
    if (openPanel?.element === panel) {
      openPanel = null
    }
  }

  openPanel = { element: panel, close }
  mount.append(panel)
  place(panel, anchor)

  const submit = () => {
    const rowCount = clampSize(Number(rows.input.value))
    const colCount = clampSize(Number(cols.input.value))
    close()
    onConfirm(rowCount, colCount)
  }

  function onKeyDown(e: KeyboardEvent): void {
    if (e.key === 'Escape') {
      e.preventDefault()
      close()
    } else if (e.key === 'Enter') {
      e.preventDefault()
      submit()
    }
  }

  function onOutside(e: PointerEvent): void {
    if (!panel.contains(e.target as Node)) {
      close()
    }
  }

  confirm.addEventListener('click', submit)

  // 这层监听得等当前这次点击走完再挂：面板是在菜单项的 click 处理里建的，
  // 此刻事件还在往 document 上冒泡，同步挂上去会被这一下自己关掉。
  setTimeout(() => {
    if (openPanel?.element !== panel) {
      return
    }
    document.addEventListener('keydown', onKeyDown, true)
    document.addEventListener('pointerdown', onOutside, true)
  }, 0)

  rows.input.focus()
  rows.input.select()
}
