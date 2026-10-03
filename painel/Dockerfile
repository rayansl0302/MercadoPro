# Imagem única do painel: compila o site React e entrega junto com a API Node.
# Contexto de build: a pasta painel/
FROM node:22-alpine AS web
WORKDIR /web
COPY web/package.json web/package-lock.json* ./
RUN npm install --no-audit --no-fund
COPY web/ ./
RUN npx vite build

FROM node:22-alpine
WORKDIR /app
ENV NODE_ENV=production PASTA_WEB=/app/web
COPY api/package.json api/package-lock.json* ./
RUN npm install --omit=dev --no-audit --no-fund
COPY api/src ./src
COPY --from=web /web/dist ./web
USER node
EXPOSE 3000
HEALTHCHECK --interval=30s --timeout=5s --retries=3 CMD wget -qO- http://127.0.0.1:3000/saude || exit 1
CMD ["node", "src/server.js"]
