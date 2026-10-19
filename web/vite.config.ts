import { defineConfig } from 'vite';

// the two headers that make the page cross-origin isolated, which browsers require before
// they hand out shared memory for threads. web/vercel.json sends the same ones on the live site
const isolation = {
  'Cross-Origin-Opener-Policy': 'same-origin',
  'Cross-Origin-Embedder-Policy': 'require-corp',
};

// the rest of what web/vercel.json sends, so vite preview serves the production build under the same rules
// the dev server leaves these out, its live reload needs inline scripts and a websocket
const hardening = {
  'X-Content-Type-Options': 'nosniff',
  'Referrer-Policy': 'strict-origin-when-cross-origin',
  'Content-Security-Policy':
    "default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; worker-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'self'; form-action 'none'; frame-ancestors 'none'",
};

export default defineConfig({
  server: { headers: isolation },
  preview: { headers: { ...isolation, ...hardening } },
  // the threaded build starts its workers as es modules
  worker: { format: 'es' },
  build: {
    // the sim page and the benchmark page
    rollupOptions: { input: { main: 'index.html', bench: 'bench.html' } },
  },
});
