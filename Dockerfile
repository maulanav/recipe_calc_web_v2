# ============================================================
#  Dockerfile — Image aplikasi web (Node.js)
#  Backend Express + WebSocket + Dockerode untuk mengontrol
#  container sandbox (gacoan).
# ============================================================
FROM node:20-slim

# GID grup docker host. Auth WebSocket/Dockerode perlu akses docker.sock,
# jadi `nodeapp` harus bergabung ke grup yang gid-nya sama dengan grup
# docker di HOST (yang socket-nya di-mount). Default 115 = gid docker di
# host pengguna ini. Override lewat build-arg DOCKER_GID bila diperlukan.
# Lihat docker-compose.yml bagian build.args.
ARG DOCKER_GID=115

# Install curl untuk healthcheck (node:20-slim tidak menyertakan curl bawaan)
RUN apt-get update \
    && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*

# User non-root untuk keamanan + akses grup docker (gid host dari build-arg).
# nodeapp TIDAK menentukan UID/GID tetap agar tidak bentrok dengan base image
# (node:20-slim memakai UID/GID 1000 untuk user `node`). Biarkan system
# memilih UID/GID otomatis. Grup `docker` memakai DOCKER_GID (gid host).
RUN groupadd nodeapp \
    && useradd --no-log-init --create-home --gid nodeapp --shell /usr/sbin/nologin nodeapp \
    && groupadd --gid ${DOCKER_GID} docker \
    && usermod -aG docker nodeapp

WORKDIR /app

# Salin package.json dulu untuk cache layer dependency
COPY package.json ./

# Instal dependency production (tanpa devDependency)
RUN npm install --omit=dev \
    && npm cache clean --force

# Salin source aplikasi
COPY server.js ./
COPY public/ ./public/

# User non-root
USER nodeapp

EXPOSE 7878

HEALTHCHECK --interval=30s --timeout=5s --retries=3 --start-period=10s \
    CMD ["node", "-e", "fetch('http://localhost:7878/healthz').then(r=>process.exit(r.ok?0:1)).catch(()=>process.exit(1))"]

CMD ["node", "server.js"]
