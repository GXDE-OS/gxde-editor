import { fileURLToPath } from 'node:url'
import { defineConfig } from 'vite'

// 产物直接落到 Qt 资源目录，由 src/gxde-editor.qrc 打包进二进制。
// 文件名固定（不带 hash），这样每次重新构建都不需要改动 .qrc。
const outDir = fileURLToPath(new URL('../../src/markdown/wysiwyg', import.meta.url))

export default defineConfig({
  // 页面通过 qrc:/markdown/wysiwyg/index.html 加载，资源必须用相对路径。
  base: './',
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
        entryFileNames: 'assets/editor.js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/editor[extname]',
      },
    },
  },
})
