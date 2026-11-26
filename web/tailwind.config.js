/** @type {import('tailwindcss').Config} */
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        ink: '#070a0d',
        panel: '#0d1218',
        raised: '#111820',
        line: '#1d2a36',
        acid: '#c7ff5e',
        cyan: '#6ee7ff',
        muted: '#7d8b99',
      },
      fontFamily: {
        sans: ['Inter', 'ui-sans-serif', 'system-ui', 'sans-serif'],
        mono: ['"JetBrains Mono"', 'ui-monospace', 'SFMono-Regular', 'monospace'],
      },
      boxShadow: {
        glow: '0 0 40px rgba(199,255,94,0.08)',
        panel: '0 24px 80px rgba(0,0,0,0.35)',
      },
    },
  },
  plugins: [],
};
