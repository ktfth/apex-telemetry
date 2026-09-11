/** @type {import('tailwindcss').Config} */
module.exports = {
  darkMode: 'class',
  content: [
    './src/**/*.{js,ts,jsx,tsx,mdx}',
    '../../packages/ui/src/**/*.{js,ts,jsx,tsx,mdx}'
  ],
  theme: {
    extend: {
      colors: {
        apex: {
          bg: '#0a0c10',
          surface: '#11141a',
          surfaceHover: '#161b24',
          surfaceRaised: '#1b212c',
          border: '#212836',
          borderActive: '#374151',
          accent: '#38bdf8',
          textMuted: '#6b7280',
          textSecondary: '#9ca3af',
          textPrimary: '#f3f4f6'
        }
      },
      fontFamily: {
        mono: [
          'JetBrains Mono',
          'Geist Mono',
          'ui-monospace',
          'SFMono-Regular',
          'Menlo',
          'Monaco',
          'Consolas',
          'monospace'
        ],
        sans: [
          'Inter',
          'Geist',
          '-apple-system',
          'BlinkMacSystemFont',
          'Segoe UI',
          'Roboto',
          'sans-serif'
        ]
      }
    }
  },
  plugins: []
};
