# DualityRF homepage — static site on nginx.
#
#   docker compose up --build         → http://localhost:1337
#   (or) docker build -t dualityrf-web .
#        docker run --rm -p 1337:80 dualityrf-web
#
FROM nginx:1.27-alpine

LABEL org.opencontainers.image.title="DualityRF Homepage" \
      org.opencontainers.image.description="Retro-2000s landing page with a live WebGL Raspberry Pi 3 viewer" \
      org.opencontainers.image.licenses="MIT"

# Replace the stock server block with ours.
RUN rm -f /etc/nginx/conf.d/default.conf
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Copy the site (explicit paths so the Dockerfile/nginx.conf never land in webroot).
COPY index.html /usr/share/nginx/html/index.html
COPY css        /usr/share/nginx/html/css
COPY js         /usr/share/nginx/html/js
COPY public     /usr/share/nginx/html/public

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
  CMD wget -qO /dev/null http://127.0.0.1/ || exit 1

# nginx:alpine already runs `nginx -g 'daemon off;'` as CMD.
