// ============================================================
//  server.js — Backend Express + WebSocket (ws, noServer) + Dockerode
//
//  Tiap koneksi WebSocket terautentikasi -> membuka satu container
//  sandbox ephemeral (AutoRemove) yang menjalankan binary `gacoan`,
//  diakses secara interaktif melalui terminal web (Xterm.js).
//
//  Autentikasi WebSocket memakai TOKEN eksplisit (`wsToken`), bukan
//  cookie saja, sesuai §3.2 template.
// ============================================================

const path = require('path');
const http = require('http');
const crypto = require('crypto');

const express = require('express');
const session = require('express-session');
const { WebSocketServer } = require('ws');
const Docker = require('dockerode');

// ============================================================
//  Konfigurasi dari environment
// ============================================================
const PORT = process.env.PORT || 7878;
const WEB_USER = process.env.WEB_USER || 'admin';
const WEB_PASS = process.env.WEB_PASS || 'admin';
const SESSION_SECRET = process.env.SESSION_SECRET || 'please-change-this-secret';
const SANDBOX_IMAGE = process.env.SANDBOX_IMAGE || 'recipe-sandbox:latest';
const SANDBOX_NETWORK = process.env.SANDBOX_NETWORK || 'none';
const MAX_CONCURRENT_SESSIONS = parseInt(process.env.MAX_CONCURRENT_SESSIONS || '5', 10);

// ============================================================
//  Express + HTTP server + session (dibagi dengan WebSocket)
// ============================================================
const app = express();
const server = http.createServer(app);

const sessionMiddleware = session({
  name: 'recipe.sid',
  secret: SESSION_SECRET,
  resave: false,
  saveUninitialized: false,
  cookie: {
    httpOnly: true,
    sameSite: 'lax',
    maxAge: 1000 * 60 * 60 * 12, // 12 jam
  },
});

app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(sessionMiddleware);

// Melayani file statis (frontend)
app.use(express.static(path.join(__dirname, 'public')));

// ============================================================
//  Helper autentikasi
// ============================================================
function requireAuth(req, res, next) {
  if (req.session && req.session.authenticated) {
    return next();
  }
  return res.status(401).json({ error: 'Unauthorized' });
}

// ============================================================
//  Dockerode — akses Docker daemon via socket
// ============================================================
const docker = new Docker({ socketPath: '/var/run/docker.sock' });

// Penghitung sesi aktif untuk batas konkurensi
let activeSessions = 0;

// ============================================================
//  Route Auth
// ============================================================

// Health check (untuk healthcheck container)
app.get('/healthz', (req, res) => {
  res.json({ ok: true, uptime: process.uptime() });
});

// Halaman utama — memerlukan autentikasi
app.get('/', requireAuth, (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Login
app.post('/login', (req, res) => {
  const { user, pass } = req.body;

  // Pembandingan sederhana (demo). Dalam produksi, gunakan hashing aman.
  if (user === WEB_USER && pass === WEB_PASS) {
    req.session.authenticated = true;
    // Token eksplisit untuk WebSocket (bukan cookie saja) — §3.2
    req.session.wsToken = crypto.randomBytes(32).toString('hex');
    return res.json({ ok: true });
  }

  return res.status(401).json({ error: 'Kredensial salah' });
});

// Logout
app.post('/logout', requireAuth, (req, res) => {
  req.session.destroy(() => {
    res.clearCookie('recipe.sid');
    res.json({ ok: true });
  });
});

// Endpoint untuk mengambil token WebSocket — memerlukan autentikasi sesi
app.get('/api/ws-token', requireAuth, (req, res) => {
  // Token dibuat saat login; regen bila belum ada
  if (!req.session.wsToken) {
    req.session.wsToken = crypto.randomBytes(32).toString('hex');
  }
  return res.json({ token: req.session.wsToken });
});

// ============================================================
//  WebSocket — mode noServer, validasi token saat 'upgrade'
// ============================================================
const wss = new WebSocketServer({ noServer: true });

server.on('upgrade', (request, socket, head) => {
  // Parse token dari query string: /ws?token=...&cols=..&rows=..
  const url = new URL(request.url, `http://${request.headers.host || 'localhost'}`);

  // Pastikan path WebSocket benar
  if (url.pathname !== '/ws') {
    socket.destroy();
    return;
  }

  const token = url.searchParams.get('token');

  // Dummy response object untuk express-session saat memvalidasi
  // WebSocket upgrade (kita hanya membaca sesi, tidak menulis cookie).
  const dummyRes = {
    setHeader() {},
    getHeader() { return null; },
    end() {},
    write() {},
    on() { return this; },
    once() { return this; },
    removeListener() { return this; },
    emit() { return this; },
  };

  // Bootstrap sesi dari cookie untuk membaca req.session
  sessionMiddleware(request, dummyRes, () => {
    const authenticated =
      request.session &&
      request.session.authenticated &&
      request.session.wsToken &&
      request.session.wsToken === token;

    if (!authenticated) {
      socket.write('HTTP/1.1 401 Unauthorized\r\n\r\n');
      socket.destroy();
      return;
    }

    // Cek batas konkurensi
    if (activeSessions >= MAX_CONCURRENT_SESSIONS) {
      socket.write('HTTP/1.1 503 Service Unavailable\r\n\r\n');
      socket.destroy();
      return;
    }

    wss.handleUpgrade(request, socket, head, (ws) => {
      wss.emit('connection', ws, request);
    });
  });
});

wss.on('connection', (ws, request) => {
  // Ambil cols/rows awal dari query string
  const url = new URL(request.url, `http://${request.headers.host || 'localhost'}`);
  const cols = Math.max(parseInt(url.searchParams.get('cols') || '80', 10), 2);
  const rows = Math.max(parseInt(url.searchParams.get('rows') || '24', 10), 2);

  activeSessions += 1;

  let container = null;
  let attachStream = null;
  let closed = false;

  const closeHandler = () => {
    if (closed) return;
    closed = true;

    // Hentikan aliran attach agar tidak memory leak
    if (attachStream) {
      try { attachStream.destroy(); } catch (_) {}
    }

    // Hapus container sandbox (ephemeral)
    if (container) {
      try {
        container.remove({ force: true }).catch(() => {});
      } catch (_) {}
    }

    activeSessions = Math.max(0, activeSessions - 1);
  };

  ws.on('close', closeHandler);
  ws.on('error', closeHandler);

  // 1) Buat container sandbox (ephemeral, AutoRemove, TTY, stdin terbuka)
  docker.createContainer({
    Image: SANDBOX_IMAGE,
    Tty: true,
    OpenStdin: true,
    StdinOnce: false,
    AttachStdout: true,
    AttachStderr: true,
    AttachStdin: true,
    HostConfig: {
      AutoRemove: true,
      NetworkMode: SANDBOX_NETWORK,
      Memory: 128 * 1024 * 1024,       // 128 MB
      MemorySwap: 128 * 1024 * 1024,   // tanpa swap
      CpuShares: 512,                  // sebagian kecil dari CPU
      CapDrop: ['ALL'],
      SecurityOpt: ['no-new-privileges:true'],
      PidsLimit: 128,
    },
  })
    .then(async (ctn) => {
      container = ctn;

      // 2) Atur ukuran terminal awal
      await container.resize({ h: rows, w: cols }).catch(() => {});

      // 3) Attach ke container (interaktif, duplex untuk stdin/stdout)
      const stream = await container.attach({
        stream: true,
        stdin: true,
        stdout: true,
        stderr: true,
        hijack: true,
      });
      attachStream = stream;

      // 4) Pipe output container -> WebSocket client
      stream.on('data', (chunk) => {
        if (ws.readyState !== ws.OPEN) return;
        // chunk bisa Buffer / string. Kirim sebagai string teks.
        const data = chunk.toString('utf8');
        ws.send(data);
      });
      stream.on('end', () => {
        // Container berhenti -> tutup koneksi client
        if (ws.readyState === ws.OPEN) ws.close();
      });
      stream.on('error', () => {
        if (ws.readyState === ws.OPEN) ws.close();
      });

      // 5) Start container
      await container.start();

      // 6) Pipe input WebSocket client -> container stdin
      ws.on('message', (data) => {
        if (closed || !container) return;

        let text;
        try {
          text = data.toString('utf8');
        } catch (_) {
          return;
        }

        // Cek protokol resize JSON: {"resize":[cols,rows]} — §3.3
        if (text.startsWith('{') && text.includes('resize')) {
          try {
            const parsed = JSON.parse(text);
            if (Array.isArray(parsed.resize) && parsed.resize.length === 2) {
              const h = Math.max(parseInt(parsed.resize[1], 10), 2);
              const w = Math.max(parseInt(parsed.resize[0], 10), 2);
              container.resize({ h, w }).catch(() => {});
            }
          } catch (_) {
            // Bukan JSON resize -> abaikan
          }
          return;
        }

        // Bukan resize -> tulis ke stdin container
        try {
          stream.write(text);
        } catch (_) {}
      });
    })
    .catch((err) => {
      // Gagal membuat container
      closeHandler();
      if (ws.readyState === ws.OPEN) {
        ws.send(`\r\n[!] Gagal membuat sesi sandbox: ${err.message}\r\n`);
        ws.close();
      }
    });
});

// ============================================================
//  Start server
// ============================================================
server.listen(PORT, () => {
  console.log(`[gacoan-web] Server berjalan di http://localhost:${PORT}`);
});
