// Server statis kecil tanpa dependensi, untuk Azure Web App (Node) dan uji lokal.
const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = __dirname;
const PORT = process.env.PORT || 8080;
const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.png': 'image/png'
};
// Hanya berkas situs yang boleh diambil; sisanya (server, workflow, .git) tidak dilayani.
const allowed = (rel) => rel === 'index.html' || rel === 'preview.png' || /^vendor\/[\w.-]+\.js$/.test(rel);

http.createServer((req, res) => {
  let rel;
  try { rel = decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/^\/+/, ''); }
  catch { rel = null; }
  if (rel === '') rel = 'index.html';
  if (rel === null || !allowed(rel) || (req.method !== 'GET' && req.method !== 'HEAD')) {
    res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
    return res.end('Tidak ditemukan');
  }
  const file = path.join(ROOT, rel);
  fs.stat(file, (err, st) => {
    if (err || !st.isFile()) {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      return res.end('Tidak ditemukan');
    }
    res.writeHead(200, {
      'Content-Type': TYPES[path.extname(file)] || 'application/octet-stream',
      'Content-Length': st.size,
      'Cache-Control': rel.startsWith('vendor/') ? 'public, max-age=604800' : 'no-cache',
      'X-Content-Type-Options': 'nosniff'
    });
    if (req.method === 'HEAD') return res.end();
    fs.createReadStream(file).pipe(res);
  });
}).listen(PORT, () => console.log(`Smart Plant berjalan di port ${PORT}`));
