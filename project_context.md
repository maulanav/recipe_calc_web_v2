# 📋 Konteks Proyek — GACOAN Web Recipe Calculator

> Dokumen ini dibuat agar LLM/agen lain bisa langsung memahami project
> dan melanjutkan pekerjaan tanpa membaca seluruh riwayat percakapan.

## 1. Ringkasan

Aplikasi web yang menyediakan **terminal interaktif** untuk program C++ 
bernama `gacoan` (kalkulator resep minuman). Web memakai:
- **Node.js + Express** (backend)
- **WebSocket** (terminal real-time via xterm.js)
- **Dockerode** (membuat container sandbox yang menjalankan `gacoan`)
- **cloudflared** (tunnel publik ke `https://resep.maulanapiki.site`)

## 2. Lokasi Project

```
/home/piki/project/recipe_calc_web_v2/
```

## 3. Struktur File Penting

| File | Fungsi |
|------|--------|
| `main.cpp` | Program C++ `gacoan` (kalkulator resep) — **interactive CLI** |
| `setup.sh` | Skrip bantuan: build, prep, logs, clean, down, restart |
| `server.js` | Backend Express + WebSocket + Dockerode |
| `Dockerfile.sandbox` | **Multi-stage**: compile `main.cpp` di image, runtime kecil |
| `Dockerfile` | Image web (Node.js slim, user non-root) |
| `docker-compose.yml` | Orchestrasi web + cloudflared, env config |
| `public/index.html` | Frontend (login password + terminal) |
| `.env` | Token cloudflare (`TUNNEL_TOKEN`) — diisi manual |

## 4. Alur Aplikasi

1. User login (`POST /login` — username/password dari env `WEB_USER`/`WEB_PASS`).
2. Login sukses → session + `wsToken` dibuat.
3. Frontend buka WebSocket: `/ws?token=...&cols=..&rows=..`.
4. Server (via Dockerode):
   - Buat **1 container sandbox** per koneksi WS.
   - `docker.createContainer({ Image: 'recipe-sandbox:latest', Tty: true, OpenStdin: true, ... HostConfig: { AutoRemove, NetworkMode: 'none', Memory: 128MB, CapDrop: ALL, PidsLimit: 128, SecurityOpt: 'no-new-privileges' } })`
   - Container menjalankan binary `gacoan`.
   - Pipe stdout/stderr container → WebSocket; input WebSocket → stdin container.
5. Container dihapus otomatis saat WS close (`AutoRemove`).

## 5. Program C++ `main.cpp`

- **4 resep dasar** (MILO, THAI GREEN TEA, LEMON TEA, ORANGE), masing-masing = 20 porsi.
- Menu utama: pilih 1-4, input jumlah resep (desimal), tampilkan rincian gram per bahan.
- **Mode Racing** (rahasia): input `"00"` di menu utama → hanya resep `bisaRacing` true, gabung air panas+suhu ruang.
- **Menu 5 — Biang Teh** (ditambahkan):
  - Komposisi persentase: Base Teh 45,5% | Air 24,2% | Simple Syrup 30,3%.
  - Berat/porsi = 200 gr.
  - Sub-menu 2 opsi: **(1)** Hitung berdasarkan acuan satu bahan, **(2)** Hitung berdasarkan porsi.
  - **Acuan porsi 20/30/33** → angka persis tabel (tabel acuan tetap).
  - **Porsi lain** → proporsional dari acuan **33** (skala = porsi/33; base=3000×skala, air=1600×skala, simple=2000×skala).
  - **Opsi acuan satu bahan**: jika input berat == nilai salah satu bahan pada baris acuan (20/30/33), pakai **seluruh baris secara exact**; jika tidak cocok → hitung persentase.
  - **Tidak masuk mode racing**.
- Resep Thai Green Tea: simple syrup **560 gr** (sudah diubah dari 660).

## 6. Image Sandbox — Multi-stage

`Dockerfile.sandbox` sekarang **multi-stage**:
- **Builder** (`debian:trixie-slim` + g++): compile `main.cpp` → binary `gacoan`.
- **Runtime** (`debian:trixie-slim`): hanya binary hasil compile + user non-root `sandboxuser`.

Alur kerja: **ubah `main.cpp` → `./setup.sh build`** → compile otomatis di image.
Tidak perlu compile manual di host (tapi tetap bisa: `g++ -std=c++17 -O2 -o gacoan main.cpp`).

## 7. Login Web

- **Hanya username/password** (dari env `WEB_USER`/`WEB_PASS`), default `lmgsud`/`pesandulu`.
- **TIDAK ada** Google OAuth / passport. (Sempat dicoba ditambahkan tapi di-REVERT — jangan ditambahkan lagi tanpa izin.)

## 8. Autentikasi WebSocket

- Token eksplisit `wsToken` dibuat saat login, disimpan di session.
- `/api/ws-token` mengembalikan token.
- WS upgrade: validasi `session.authenticated && session.wsToken === token`.
- Batas konkurensi `MAX_CONCURRENT_SESSIONS` default **5**.

## 9. Environment (`.env`)

Isi saat ini:
```
TUNNEL_TOKEN=<token cloudflare>
```

## 10. Perintah `setup.sh`

| Perintah | Fungsi |
|----------|--------|
| `./setup.sh prep` | Buat `.env` placeholder + npm install |
| `./setup.sh build` | Build image sandbox + build & up stack |
| `./setup.sh logs` | Log cloudflared + web |
| `./setup.sh cloud` | Log cloudflared saja |
| `./setup.sh web` | Log web saja |
| `./setup.sh clean` | Hapus container sandbox menumpuk |
| `./setup.sh prune` | Bersihkan image lama |
| `./setup.sh down` | Stop stack |
| `./setup.sh restart` | Rebuild + restart stack |
| `./setup.sh gid` | Cek GID docker host |

## 11. Fitur yang Sedang Dalam Proses (Belum Selesai)

**Auto-push ke git** — ditambahkan ke `setup.sh`, tapi **belum diimplementasikan**:
- Desain yang disepakati:
  - Push **opsional**: `./setup.sh build push`.
  - Branch: **deteksi otomatis** (`git branch --show-current`).
  - Origin: set manual via perintah baru `./setup.sh gitremote <url>`.
- **Belum ada repo git** (folder belum `git init`).
- URL GitHub user: `https://github.com/maulanav/` — nama repo **belum ditentukan**.
- Perlu: buat repo di GitHub, dapatkan URL, set origin, jalankan push.

## 12. Catatan Penting / Kendala

- Tool baca/edit file pada LLM sesi sebelumnya **sering tidak sinkron ke disk** — jika ada perubahan file, verifikasi dengan `cat`/`grep` di terminal, bukan hanya percaya pada tool edit.
- Saat mengedit `server.js`, jangan tambahkan Google OAuth (sudah di-revert).
- `main.cpp` dan `Dockerfile.sandbox` memuat fitur biang teh & multi-stage yang **harus dipertahankan**.

## 13. Tugas Selanjutnya (Jika Dilanjutkan)

1. Buat repo GitHub dengan nama (mis. `recipe_calc_web_v2`).
2. Set remote origin: `git remote add origin <url>`.
3. Implementasikan fitur git push di `setup.sh` (lihat §11).
