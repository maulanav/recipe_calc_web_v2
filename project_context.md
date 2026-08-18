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
| `resep.json` | **Data berat semua resep + biang teh** (dibaca program; sumber utama dari GitHub) |
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

## 6b. Data Bahan dari JSON (update tanpa rebuild C++)

- **Berat bahan semua resep + biang teh** kini disimpan di `resep.json`, **tidak hardcode** di `main.cpp`.
- Saat `gacoan` berjalan, data dimuat dengan **prioritas**:
  1. **Env `RESEP_JSON`** (dipakai di container sandbox — server.js mengisinya dari GitHub).
  2. File `resep.json` lokal (dari image sandbox / folder project).
  3. Fallback hardcoded (agar program tetap jalan jika JSON gagal dibaca).
- `main.cpp` memakai **parser JSON minimal (tanpa library eksternal)** — tetap kompatibel dengan builder yang hanya berisi `g++`.
- Untuk **update resep tanpa rebuild C++**:
  - Ubah `resep.json` di GitHub → restart web server → `server.js` mengunduh ulang & menyuntikkan via env ke tiap container.
- `server.js` membaca env `RESEP_JSON_URL` (default `https://raw.githubusercontent.com/maulanav/recipe_calc_web_v2/main/resep.json`) lalu memvalidasi & menyimpan ke `cachedResepJson`, diteruskan lewat `Env: ['RESEP_JSON=...']` di `createContainer`.
- `Dockerfile.sandbox` menyalin `resep.json` ke `/usr/local/share/gacoan/resep.json` sebagai fallback lokal di image.

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

Env tambahan (opsional, untuk sumber resep dari GitHub):
```
# URL raw resep.json di GitHub (jika bukan default)
RESEP_JSON_URL=https://raw.githubusercontent.com/maulanav/recipe_calc_web_v2/main/resep.json
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
| `./setup.sh build push` | Build + stack, lalu push otomatis ke git |
| `./setup.sh push` | Commit & push semua perubahan ke git |
| `./setup.sh gitremote <url>` | Simpan URL remote git |
| `./setup.sh gitcheck` | Lihat status repo git + remote |

## 11. Fitur Auto-push ke Git (SELESAI diimplementasikan)

**Auto-push ke git** sudah DIIMPLEMENTASIKAN di `setup.sh`:
- Push **opsional**: `./setup.sh build push` (arg `push` sebagai argumen kedua pada `build`).
- Push manual: `./setup.sh push` (commit semua & push; branch otomatis).
- Branch: **deteksi otomatis** (`git branch --show-current`).
- Origin: set via `./setup.sh gitremote <url>`; disimpan di file `.gitremote`.
- `.gitremote` ditambahkan otomatis ke `.gitignore` agar tidak ikut dicommit.
- Jika repo belum ada, `./setup.sh push` melakukan `git init` + `git branch -M main` otomatis.
- `.env` (TUNNEL_TOKEN) tetap aman tidak dicommit (sudah di `.gitignore`).
- Perintah tambahan: `./setup.sh gitcheck` (lihat status repo + remote).

**Kondisi repo saat ini:**
- Folder **sudah** `git init` (branch `main`), commit awal sudah dibuat.
- Remote origin diset ke **SSH**: `git@github.com:maulanav/recipe_calc_web_v2.git` (tersimpan lokal di file `.gitremote`).
- **GIT AUTH SECARA SSH SUDAH TERHUBUNG** — SSH key baru (`~/.ssh/id_ed25519_new`) sudah terdaftar di akun GitHub pengguna.
- **GITHUB REPO** status: lihat catatan di bagian 13 tentang kebutuhan buat repo / push.

## 12. Catatan Penting / Kendala

- Tool baca/edit file pada LLM sesi sebelumnya **sering tidak sinkron ke disk** — jika ada perubahan file, verifikasi dengan `cat`/`grep` di terminal, bukan hanya percaya pada tool edit.
- Saat mengedit `server.js`, jangan tambahkan Google OAuth (sudah di-revert).
- `main.cpp` dan `Dockerfile.sandbox` memuat fitur biang teh & multi-stage yang **harus dipertahankan**.

## 13. Tugas Selanjutnya (Jika Dilanjutkan)

1. **Buat repo GitHub** (mis. `recipe_calc_web_v2`) di akun `maulanav`.
2. **Daftarkan SSH key baru** ke akun GitHub:
   - Public key baru: `~/.ssh/id_ed25519_new.pub` (key lama `~/.ssh/id_ed25519` tidak konsisten/hilang private-nya).
   - Tambahkan di GitHub → Settings → SSH and GPG keys → New SSH key.
3. **Push**: jalankan `./setup.sh push` (remote origin SSH sudah diset).
4. **Build image sandbox** dengan `main.cpp` baru: `./setup.sh build`.
5. Pastikan `resep.json` ter-upload → `server.js` mengunduhnya → tiap container sandbox menerima `RESEP_JSON` via env.
6. (Opsional) Verifikasi status dengan `./setup.sh gitcheck`.
