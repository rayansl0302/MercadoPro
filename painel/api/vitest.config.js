import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    // todos os arquivos usam o mesmo banco de teste: rodam um de cada vez
    fileParallelism: false,
    globalSetup: ['./test/globalSetup.js'],
    testTimeout: 30000,
    hookTimeout: 60000,
    environment: 'node',
  },
});
