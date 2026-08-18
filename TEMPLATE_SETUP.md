# 🧩 TEMPLATE SETUP — Aplikasi Web Terminal (Node.js + Xterm.js + Docker Sandbox)

> Template untuk **LLM/agent berikutnya (Continue)** membuat ulang proyek dari nol TANPA mengulang kesalahan.
> Proyek menjalankan program C++ (`gacoan`) di dalam **container sandbox**, diakses via web terminal interaktif,
> dipublikasikan via **Cloudflare Tunnel (cloudflared)**.

---

## 1. Ringkasan Arsitektur
- **Backend**: Node.js (Express) + WebSocket (`ws`, mode `noServer`) + **Dockerode**.
- **Frontend**: Xterm.js (terminal interaktif).
- **Isolasi**: tiap koneksi WebSocket → satu **container sandbox** ephemeral (AutoRemove) menjalankan `gacoan`.
- **Akses**: lokal `http://localhost:7878` + **Cloudflare Tunnel** (`cloudflared`).
- **Program aplikasi**: binary `gacoan` (**sudah di-compile**) + source `main.cpp` (referensi, **JANGAN diubah LLM**).

---

## 2. Struktur Folder Wajib
```
recipe_calc_web/
├── package.json          # express, ws, express-session, dockerode
├── server.js             # Backend Express + WebSocket + Dockerode
├── Dockerfile            # Image aplikasi web (Node.js)
├── Dockerfile.sandbox    # Image sandbox (berisi binary gacoan)
├── docker-compose.yml    # service: web + cloudflared (TERINTEGRASI)
├── public/
│   └── index.html        # Frontend terminal (Xterm.js)
├── main.cpp              # (SUDAH ADA) source program C++ — tidak diubah
├── gacoan                # (SUDAH ADA) binary hasil compile main.cpp
├── .env                  # rahasia: TUNNEL_TOKEN (JANGAN dicommit)
└── .gitignore            # abaikan node_modules/, .env, gacoan, home/
```

---

## 3. Aturan WAJIB (agar tidak mengulang kesalahan)

> ⛔ **JANGAN** mengubah `main.cpp` atau membuat program C++ baru.
> `gacoan` (binary) & `main.cpp` (source) **sudah disediakan** dan terbukti jalan.
> LLM cukup menyalin/mengompilasi ulang (opsional) & membangun infrastruktur di sekitarnya.

### 3.1. Sandbox image — WAJIB `debian:trixie-slim`
- Binary `gacoan` di-compile **GCC 13+**, butuh **`GLIBCXX_3.4.32`**.
- Base **`debian:bookworm-slim`** → hanya libstdc++ GCC 12 (`GLIBCXX_3.4.30`) → **gagal**.
- Image **`gcc:13-slim`** → **tidak ada** (image `gcc` resmi tak punya tag `-slim`; valid: `gcc:13`, `gcc:13-bookworm`).
- Copy libstdc++ dari `gcc:13-bookworm` → **tidak cukup** (base tetap libstdc++ GCC 12).
- ✅ **Solusi**: base **`debian:trixie-slim`** (Debian 13 / GCC 14) → punya `GLIBCXX_3.4.32+`, backward compatible `GLIBC_2.34`.

```dockerfile
# Dockerfile.sandbox
FROM debian:trixie-slim
COPY gacoan /usr/local/bin/gacoan
RUN chmod +x /usr/local/bin/gacoan
RUN useradd --uid 1001 --create-home --shell /usr/sbin/nologin sandboxuser
WORKDIR /home/sandboxuser
USER sandboxuser
CMD ["/usr/local/bin/gacoan"]
```

### 3.2. Autentikasi WebSocket — pakai TOKEN, bukan cookies saja
- Token eksplisit dibuat saat login: `req.session.wsToken`.
- Endpoint `/api/ws-token` mengembalikan `{ token }`.
- Frontend buka WS: `/ws?token=...&cols=..&rows=..`.
- `server.on('upgrade')` validasi `session.authenticated && session.wsToken === token`.

### 3.3. Resize PTY (terminal tidak terpotong / muat mobile)
- Frontend: `ResizeObserver` + `window.resize` → hitung `cols/rows` via `proposeSize()`.
- Kirim `{"resize":[cols,rows]}` (JSON) via WebSocket.
- Server: tangkap `{"resize":...}` → `container.resize({h: rows, w: cols})`.

### 3.4. Hindari `term.onDispose` (bisa bikin UI stuck)
- `term.onDispose(...)` **tidak tersedia** di Xterm versi tertentu → error JS → `connect()` tak jalan (stuck "Menyiapkan sesi...").
- ✅ Pakai `ResizeObserver` langsung; bungkus `connect()` & `fitTerminal()` dengan `try/catch`.

### 3.5. ASCII art header (jika menyisipkan di C++)
- Jika ASCII art mengandung backslash `\`, wajib pakai **raw string literal** `R"( ... )"`.

### 3.6. Layout CSS yang aman
- Flex column jelas: `header { flex: 0 0 auto }`, `main { flex: 1 1 auto; min-height: 0 }`, `.banner { flex: 0 0 auto }`.
- Jangan over-ride Xterm dengan `!important` berlebihan.

---

## 4. Langkah Pembuatan dari NOL

### Langkah 1 — Inisialisasi Node
```bash
mkdir -p public
npm init -y
npm install express ws express-session dockerode
```

### Langkah 2 — `server.js`
- Express + HTTP server; session middleware dibagi HTTP & WebSocket.
- `new WebSocketServer({ noServer: true })` + `server.on('upgrade')` + validasi token (3.2).
- Dockerode: create (openStdin, Tty, AutoRemove), resize, attach; hapus container saat WS tutup.
- Endpoint: `POST /login`, `/logout`, `/api/ws-token`, `requireAuth` untuk `/`.

### Langkah 3 — `public/index.html`
- Xterm.js dari CDN (`xterm@5.3.0`).
- Ambil token `/api/ws-token` → buka WS `?token=...`.
- Auto-fit & resize PTY (3.3); hindari `term.onDispose` (3.4).

### Langkah 4 — Program C++ (sudah ada)
- **`gacoan`** (binary) & **`main.cpp`** (source) **sudah disediakan**.
- Opsional recompile (hanya jika `main.cpp` diubah — **jangan**):
  ```bash
  g++ -std=c++17 -O2 -o gacoan main.cpp
  ```
- Pastikan jalan: `echo "0" | ./gacoan`

### Langkah 5 — Image
- **`Dockerfile`** (web, Node.js slim).
- **`Dockerfile.sandbox`** → WAJIB `debian:trixie-slim` (3.1).

### Langkah 6 — `docker-compose.yml` (termasuk cloudflared)
```yaml
services:
  web:
    build: .
    container_name: recipe-calc-web
    restart: unless-stopped
    ports: ["7878:7878"]
    environment:
      - WEB_USER=lmgsud
      - WEB_PASS=pesandulu
      - SESSION_SECRET=please-change-this-secret
      - SANDBOX_IMAGE=recipe-sandbox:latest
      - SANDBOX_NETWORK=none
      - MAX_CONCURRENT_SESSIONS=5
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    security_opt: [no-new-privileges:true]
    cap_drop: [ALL]
    deploy:
      resources:
        limits: { cpus: "1.0", memory: 256M }
        reservations: { cpus: "0.25", memory: 128M }
    healthcheck:
      test: ["CMD", "curl", "-fsS", "http://localhost:7878/healthz"]
      interval: 30s
      timeout: 5s
      retries: 3
      start_period: 10s

  cloudflared:
    image: cloudflare/cloudflared:latest
    container_name: recipe-cloudflared
    restart: unless-stopped
    command: tunnel --no-autoupdate run --token ${TUNNEL_TOKEN}
    environment:
      - TUNNEL_TOKEN=${TUNNEL_TOKEN}
    depends_on:
      - web
    deploy:
      resources:
        limits: { cpus: "0.25", memory: 64M }

networks:
  default:
    driver: bridge
```

### Langkah 7 — Buat `.env` (JANGAN dicommit)
```bash
cat > .env <<'EOF'
# Token tunnel Cloudflare Zero Trust
TUNNEL_TOKEN=<isikan token cloudflare Anda di sini>
EOF
```

### Langkah 8 — Build & jalankan
```bash
sudo docker build -f Dockerfile.sandbox -t recipe-sandbox:latest .
sudo docker compose up -d --build
```

---

## 5. 🚨 Daftar Kesalahan Historis & Solusinya
| # | Masalah | Penyebab | Solusi |
|---|---------|----------|--------|
| 1 | `GLIBCXX_3.4.32 not found` | binary GCC 13+ vs base bookworm GCC 12 | base `debian:trixie-slim` |
| 2 | WS tidak terautentikasi | hanya cookie, tidak terbaca di `upgrade` | token eksplisit `wsToken` |
| 3 | Terminal terpotong di mobile | PTY statis, tidak ada resize baru | kirim `{"resize":[...]}` → `container.resize` |
| 4 | Stuck "Menyiapkan sesi..." | `term.onDispose` tak ada → error JS → `connect()` tak jalan | hindari `term.onDispose`, bungkus `try/catch` |
| 5 | ASCII art rusak | backslash perlu escape | raw string `R"(...)"` |
| 6 | Header berantakan | over-ride Xterm `!important` + flex salah | flex column yang jelas |
| 7 | image `gcc:13-slim` not found | tag tidak ada | jangan pakai; pakai `debian:trixie-slim` |

---

## 6. Checklist Akhir
- [ ] Base sandbox **`debian:trixie-slim`**.
- [ ] WS pakai **token** (`wsToken`), bukan cookies.
- [ ] Resize PTY kirim **JSON `{"resize":[...]}`**.
- [ ] **`main.cpp` / `gacoan` tidak diubah** oleh LLM.
- [ ] ASCII art (jika ada) pakai **raw string `R"(...)"`**.
- [ ] cloudflared **terintegrasi di `docker-compose.yml`**, token via **`.env`**.
- [ ] `.gitignore` abaikan `node_modules/`, `.env`, `gacoan`, `home/`.
- [ ] Setelah ubah `main.cpp`: **recompile + rebuild image + restart**.

---

## 7. Perintah Sering Dipakai
```bash
cd recipe_calc_web

# Rebuild image sandbox
sudo docker build -f Dockerfile.sandbox -t recipe-sandbox:latest .

# Build & start stack (web + cloudflared)
sudo docker compose up -d --build

# Log cloudflared
sudo docker logs -f recipe-cloudflared

# Log web
sudo docker logs -f recipe-calc-web

# Bersihkan sandbox menumpuk (jika ada)
docker rm -f $(docker ps -q -f name=sandbox) 2>/dev/null
```

---

## 8. Cara Pakai Template ini di Continue
1. Buka **Continue** (VS Code extension) di folder `recipe_calc_web`.
2. Beri instruksi/prompt yang merujuk ke template ini, misalnya:

> "Bangun proyek dari nol mengikuti file `TEMPLATE_SETUP.md`. `main.cpp` dan `gacoan` sudah ada — JANGAN diubah. Buat `server.js`, `public/index.html`, `Dockerfile`, `Dockerfile.sandbox` (base `debian:trixie-slim`), `docker-compose.yml` dengan cloudflared terintegrasi, dan `.env` (placeholder token). Patuhi seluruh §3, §5, dan §6."

3. Jangan lupa token cloudflare diisi manual di `.env`.
