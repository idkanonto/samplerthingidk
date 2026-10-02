import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  define: {
    __RECOMPILER_VERSION__: JSON.stringify('3.0.0'),
    __RECOMPILER_BUILD_ID__: JSON.stringify(process.env.GITHUB_SHA?.slice(0, 12) ?? 'LOCAL-DEVELOPMENT')
  },
  base: './',
  plugins: [react()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    rollupOptions: {
      output: {
        entryFileNames: 'assets/app.js',
        chunkFileNames: 'assets/chunk-[name].js',
        assetFileNames: (assetInfo) =>
          assetInfo.name?.endsWith('.css') ? 'assets/app.css' : 'assets/[name][extname]'
      }
    }
  }
})
