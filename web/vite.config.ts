import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import { defineConfig } from 'vite'

// Dev-Proxy: /api -> FastAPI (uvicorn --reload auf Port 8000).
// `npm run dev` ist nur auf diesem Rechner erreichbar.
// `npm run dev:mobil` (= vite --host) oeffnet es fuers ganze WLAN, z. B. fuers Handy.
// Der Proxy laeuft auf dem Rechner selbst - das Backend kann also auf 127.0.0.1 bleiben.
export default defineConfig({
  plugins: [react(), tailwindcss()],
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:8000',
        changeOrigin: true,
      },
    },
  },
})
