// Cores da logo MercadoPro: verde (destaque) e azul-marinho (texto e títulos)
export default {
  content: ['./index.html', './src/**/*.{js,jsx}'],
  theme: {
    extend: {
      colors: {
        marca: { DEFAULT: '#17963a', escuro: '#107a2e', claro: '#ddf1e1' },
        marinho: { DEFAULT: '#142d45', claro: '#1d384a' },
      },
    },
  },
  plugins: [],
};
