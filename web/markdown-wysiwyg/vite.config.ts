import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'

// 产物直接落到 Qt 资源目录，由 src/gxde-editor.qrc 打包进二进制。
// 文件名固定（不带 hash），这样每次重新构建都不需要改动 .qrc。
const outDir = fileURLToPath(new URL('../../src/markdown/wysiwyg', import.meta.url))

/**
 * 把产物里的 /assets/… 改写成 gxde-md://editor/assets/…。
 *
 * 页面由 Qt 侧的自定义 scheme 加载，而页面里还会插一个指向文档目录的 <base>，
 * 用来把 markdown 中的相对图片路径引到磁盘上。资源必须是绝对 URL，否则会被
 * 那个 <base> 一并带偏。Vite 的 base 选项不接受带 scheme 的值（会被规范化掉），
 * 所以只能在这里事后改。
 */
const schemeAssetBase = 'gxde-md://editor'

const rewriteAssetUrls = {
  name: 'gxde-editor-rewrite-asset-urls',
  apply: 'build' as const,
  transformIndexHtml: {
    order: 'post' as const,
    handler(html: string) {
      return html.replace(
        /(src|href)="\/assets\//g,
        `$1="${schemeAssetBase}/assets/`,
      )
    },
  },
}

export default defineConfig({
  base: '/',
  plugins: [rewriteAssetUrls],
  build: {
    outDir,
    emptyOutDir: true,
    target: 'es2020',
    assetsDir: 'assets',
    // 产物是入库的，要能直接读，所以 CSS 不压缩。
    // JS 保持压缩：那里几乎全是 Milkdown/ProseMirror 的 vendor 代码，
    // 解开也不会有人读，反而让仓库和二进制凭空胖一圈。
    cssMinify: false,
    rollupOptions: {
      output: {
        inlineDynamicImports: true,
        entryFileNames: 'assets/editor.js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/editor[extname]',
      },
    },
  },
})
