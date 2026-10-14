import { defineConfig } from 'vite';

// the two headers that make the page cross-origin isolated, which browsers require before
// they hand out shared memory for threads. netlify.toml sends the same ones on the live site
const isolation = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
};

export default defineConfig({
  server: { headers: isolation },
  preview: { headers: isolation },
  // the threaded build starts its workers as es modules
  worker: { format: 'es' },
  build: {
    // the sim page and the benchmark page
    rollupOptions: { input: { main: 'index.html', bench: 'bench.html' } },
  },
});
