# API do painel (e, quando existir, o site React compilado em /app/web)
FROM node:22-alpine
WORKDIR /app
ENV NODE_ENV=production
COPY package.json package-lock.json* ./
RUN npm install --omit=dev --no-audit --no-fund
COPY src ./src
USER node
EXPOSE 3000
CMD ["node", "src/server.js"]
