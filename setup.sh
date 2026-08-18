#!/usr/bin/env bash
# ============================================================
#  setup.sh — Skrip bantuan untuk menjalankan proyek GACOAN Web
#
#  Perintah-perintah di sini TIDAK bisa dijalankan otomatis oleh
#  tooling/agent, sehingga Anda (manusia) menjalankannya manual
#  di terminal.
#
#  Cara pakai:
#      chmod +x setup.sh
#      ./setup.sh          # menampilkan menu bantuan
#      ./setup.sh prep     # siapkan .env + npm install
#      ./setup.sh build    # build image sandbox + stack
#      ./setup.sh logs     # lihat log cloudflared & web
#      ./setup.sh clean    # bersihkan sandbox yang menumpuk
#      ./setup.sh down     # stop stack
#      ./setup.sh restart  # rebuild + restart stack
#      ./setup.sh build push  # build + stack, lalu PUSH otomatis ke git
#      ./setup.sh push     # push manual ke git (lihat gitremote)
#      ./setup.sh gitremote <url>  # set URL remote git (sekali saja)
#      ./setup.sh gitcheck # cek status repo git + remote
# ============================================================

set -euo pipefail

SANDOX_IMAGE="recipe-sandbox:latest"
# Lokasi penyimpanan URL remote git (agar tidak hilang walau repo belum di-init)
GITREMOTE_FILE=".gitremote"

print_help() {
    cat <<'EOF'
📋 Perintah yang tersedia:

  ./setup.sh prep       Buat file .env (placeholder) + npm install lokal
  ./setup.sh build      Build image sandbox + build & up stack docker compose
  ./setup.sh logs       Tampilkan log cloudflared & web secara berurutan
  ./setup.sh cloud      Tampilkan log cloudflared saja (ikuti terus)
  ./setup.sh web        Tampilkan log web saja (ikuti terus)
  ./setup.sh clean      Hapus container sandbox yang menumpuk (opsional)
  ./setup.sh prune      Hapus image lama: dangling + image tak terpakai (hemat storage)
  ./setup.sh down       Stop seluruh stack
  ./setup.sh restart    Rebuild + restart seluruh stack
  ./setup.sh gid        Cek GID grup docker host (untuk DOCKER_GID build-arg)
  ./setup.sh help       Tampilkan menu bantuan ini

  — Git / push (opsional) —
  ./setup.sh gitremote <url>   Simpan URL remote git (mis. https://github.com/maulanav/recipe_calc_web_v2)
  ./setup.sh push              Commit & push perubahan ke git (branch otomatis)
  ./setup.sh gitcheck          Lihat status repo git + remote saat ini
  ./setup.sh build push        Build + stack, lalu push otomatis

Cek GID docker penting jika default DOCKER_GID=115 tidak cocok dgn host.
EOF
}

# ------------------------------------------------------------
# prep — buat .env placeholder + install dependency (opsional)
# ------------------------------------------------------------
cmd_prep() {
    echo "==> Membuat file .env placeholder..."
    if [ -f ".env" ]; then
        echo "    File .env sudah ada — melewati pembuatan."
    else
        cat > .env <<'ENVEOF'
# ============================================================
#  Token tunnel Cloudflare Zero Trust / Quick Tunnel
#  ⚠️ Isi token Anda sendiri DI SINI sebelum menjalankan compose!
# ============================================================
TUNNEL_TOKEN=<isikan token cloudflare Anda di sini>
ENVEOF
        echo "    File .env DIBUAT. SILAKAN EDIT & ISI TUNNEL_TOKEN di dalamnya."
        echo "    Lokasi: $(pwd)/.env"
    fi

    echo ""
    echo "==> Membuat direktori public (jika belum ada)..."
    mkdir -p public

    echo ""
    echo "==> Instal dependency Node.js (untuk run lokal / verifikasi)..."
    if command -v npm >/dev/null 2>&1; then
        npm install
    else
        echo "    ⚠️ npm tidak ditemukan di host. Dependency akan di-install di dalam image web."
    fi

    echo ""
    echo "✅ prep selesai."
}

# ------------------------------------------------------------
# build — build image sandbox, lalu build & up stack.
# Argumen opsional: 'push' → commit & push ke git setelah selesai
#   ./setup.sh build        # build saja
#   ./setup.sh build push   # build + push
# ------------------------------------------------------------
cmd_build() {
    echo "==> Membangun image sandbox ($SANDOX_IMAGE)..."
    sudo docker build -f Dockerfile.sandbox -t "$SANDOX_IMAGE" .

    echo ""
    echo "==> Memastikan container lama dihentikan & dihapus (hindari name conflict)..."
    sudo docker rm -f recipe-calc-web recipe-cloudflared 2>/dev/null || true

    echo ""
    echo "==> Build & start stack (web + cloudflared)..."
    sudo docker compose up -d --build

    echo ""
    echo "✅ Build selesai. Stack sudah berjalan."
    echo "   - Web lokal: http://localhost:7878"
    echo "   - Login:  username & password dari env WEB_USER / WEB_PASS (docker-compose.yml)"

    # Push opsional: ./setup.sh build push
    if [ "${2:-}" = "push" ]; then
        echo ""
        echo "==> Argumen 'push' terdeteksi — menjalankan commit & push ke git..."
        cmd_push
    fi
}

# ------------------------------------------------------------
# logs — log cloudflared lalu web
# ------------------------------------------------------------
cmd_logs() {
    echo "==> Log cloudflared (Ctrl+C untuk berhenti melihat)..."
    sudo docker logs -f recipe-cloudflared
}

cmd_web_logs() {
    echo "==> Log web (Ctrl+C untuk berhenti melihat)..."
    sudo docker logs -f recipe-calc-web
}

# ------------------------------------------------------------
# clean — bersihkan container sandbox menumpuk
# ------------------------------------------------------------
cmd_clean() {
    echo "==> Menghapus container sandbox menumpuk (jika ada)..."
    sudo docker rm -f $(sudo docker ps -q -f name=sandbox) 2>/dev/null || true
    echo "✅ Bersih."
}

# ------------------------------------------------------------
# prune — hapus image lama (dangling + tak terpakai) untuk hemat storage
# ------------------------------------------------------------
cmd_prune() {
    echo "==> 1/2 Hapus image dangling (tanpa tag / sisa rebuild)..."
    sudo docker image prune -f

    echo ""
    echo "==> 2/2 Hapus image yang TIDAK dipakai oleh container mana pun..."
    echo "    (termasuk image lama yang sudah diganti versinya)"
    echo ""
    read -r -p "    Lanjut? Ketik 'yes' untuk melanjutkan: " ans
    if [ "$ans" = "yes" ]; then
        sudo docker image prune -a -f
        echo ""
        echo "✅ Image lama dibersihkan."
    else
        echo "    Dibatalkan — langkah 2/2 dilewati."
    fi

    echo ""
    echo "==> Ringkasan penggunaan storage Docker saat ini:"
    sudo docker system df
}

# ------------------------------------------------------------
# down / restart
# ------------------------------------------------------------
cmd_down() {
    echo "==> Menghentikan stack..."
    sudo docker compose down
    echo "✅ Stack berhenti."
}

cmd_restart() {
    echo "==> Rebuild image sandbox + restart stack..."
    sudo docker build -f Dockerfile.sandbox -t "$SANDOX_IMAGE" .
    sudo docker rm -f recipe-calc-web recipe-cloudflared 2>/dev/null || true
    sudo docker compose up -d --build
    echo "✅ Restart selesai."
}

# ------------------------------------------------------------
# gid — cek GID grup docker host
# ------------------------------------------------------------
cmd_gid() {
    echo "==> GID grup docker host:"
    getent group docker | cut -d: -f3 | tee /dev/stderr
    echo ""
    echo "Jika nilai di atas TIDAK sama dengan DOCKER_GID default (115),"
    echo "setel env DOCKER_GID saat build, contoh:"
    echo ""
    echo "    DOCKER_GID=<gid> sudo docker compose up -d --build"
}

# ------------------------------------------------------------
# gitremote — simpan URL remote git (sekali saja)
#   ./setup.sh gitremote https://github.com/maulanav/recipe_calc_web_v2
# ------------------------------------------------------------
cmd_gitremote() {
    local url="${2:-}"
    if [ -z "$url" ]; then
        echo "⚠️  Berikan URL remote git, contoh:"
        echo "    ./setup.sh gitremote https://github.com/maulanav/recipe_calc_web_v2"
        return 1
    fi
    # Jangan commit file .gitremote (tambahkan ke .gitignore bila perlu)
    if command -v grep >/dev/null 2>&1 && [ -f ".gitignore" ] && ! grep -q "^\.gitremote$" ".gitignore"; then
        echo ".gitremote" >> ".gitignore"
    fi
    echo "$url" > "$GITREMOTE_FILE"
    echo "✅ URL remote disimpan: $url"
    echo "   (tersimpan di $GITREMOTE_FILE)"
    # Jika repo sudah di-init, langsung pasang remote-nya
    if [ -d ".git" ]; then
        if git remote get-url origin >/dev/null 2>&1; then
            git remote set-url origin "$url"
        else
            git remote add origin "$url"
        fi
        echo "   Origin git disetel => $(git remote get-url origin)"
    else
        echo "   (Belum ada repo git — jalankan './setup.sh push' untuk git init otomatis.)"
    fi
}

# ------------------------------------------------------------
# gitcheck — lihat status repo git + remote
# ------------------------------------------------------------
cmd_gitcheck() {
    echo "==> Status repo git:"
    if [ -d ".git" ]; then
        echo "   Branch saat ini : $(git branch --show-current 2>/dev/null || echo '(detached / belum ada commit)')"
        echo "   Remote origin   : $(git remote get-url origin 2>/dev/null || echo '(belum disetel)')"
        echo ""
        git status --short
    else
        echo "   ⚠️  Belum ada repo git (folder belum 'git init')."
        echo "   Untuk inisialisasi langsung: './setup.sh push'."
    fi
    echo ""
    echo "==> URL remote tersimpan (.gitremote):"
    if [ -f "$GITREMOTE_FILE" ]; then
        cat "$GITREMOTE_FILE"
    else
        echo "   Belum disetel. Gunakan: ./setup.sh gitremote <url>"
    fi
}

# ------------------------------------------------------------
# push — init git (jika perlu), commit & push semua perubahan.
#   Branch di-detect otomatis; origin dari .gitremote bila ada.
# ------------------------------------------------------------
cmd_push() {
    echo "==> Persiapan git…"

    # 1. Pastikan repo git ada
    if [ ! -d ".git" ]; then
        echo "   Repo git belum ada — inisialisasi (git init)..."
        git init -q
        git branch -M main
        echo "   Repo di-init dengan branch 'main'."
    fi

    # 2. Ambil branch aktif (otomatis)
    local branch
    branch="$(git branch --show-current 2>/dev/null || echo main)"
    echo "   Branch aktif: '$branch'"

    # 3. Pastikan remote origin
    local url="$(git remote get-url origin 2>/dev/null || true)"
    if [ -z "$url" ] && [ -f "$GITREMOTE_FILE" ]; then
        url="$(cat "$GITREMOTE_FILE")"
        git remote add origin "$url" 2>/dev/null || git remote set-url origin "$url"
    fi
    if [ -z "$url" ]; then
        echo ""
        echo "⚠️  Belum ada URL remote git."
        echo "   Commit hanya dibuat LOKAL (tidak di-push ke GitHub)."
        echo "   Untuk mengaktifkan push: ./setup.sh gitremote <url>"
        echo ""
    else
        echo "   Remote origin : $url"
    fi

    # 4. Commit semua perubahan (set identitas lokal bila belum ada)
    git config user.email >/dev/null 2>&1 || git config user.email "maulanapiki@users.noreply.github.com"
    git config user.name  >/dev/null 2>&1 || git config user.name  "maulanav"

    git add -A
    if git diff --cached --quiet; then
        echo "   Tidak ada perubahan untuk di-commit."
    else
        git commit -q -m "Auto-commit via setup.sh — $(date '+%Y-%m-%d %H:%M:%S')"
        echo "   Perubahan berhasil di-commit."
    fi

    # 5. Push (hanya jika remote tersedia)
    if [ -n "${url:-}" ]; then
        echo "==> Push ke '$branch' → origin…"
        git push -u origin "$branch"
        echo "✅ Push selesai."
    else
        echo "   (Lewati push — remote 'origin' belum disetel.)"
        echo "   Setelah mengatur remote, jalankan lagi: ./setup.sh push"
    fi
}

# ------------------------------------------------------------
# main dispatcher
# ------------------------------------------------------------
case "${1:-}" in
    prep)      cmd_prep ;;
    build)     cmd_build "$@" ;;
    logs)      cmd_logs ;;
    cloud)     cmd_logs ;;
    web)       cmd_web_logs ;;
    clean)     cmd_clean ;;
    prune)     cmd_prune ;;
    down)      cmd_down ;;
    restart)   cmd_restart ;;
    gid)       cmd_gid ;;
    push)      cmd_push ;;
    gitremote) cmd_gitremote "$@" ;;
    gitcheck)  cmd_gitcheck ;;
    help|-h|--help|"") print_help ;;
    *)         echo "Perintah tidak dikenal: '$1'"; print_help; exit 1 ;;
esac
