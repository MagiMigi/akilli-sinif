export const colors = {
  bg: '#0F172A',
  surface: '#1E293B',
  surfaceAlt: '#172033',
  border: '#334155',
  text: '#F1F5F9',
  textDim: '#CBD5E1',
  muted: '#94A3B8',
  accent: '#14B8A6',
  accentMuted: '#0D9488',
  accentSoft: 'rgba(20, 184, 166, 0.12)',
  success: '#22C55E',
  warning: '#F59E0B',
  danger: '#EF4444',
  info: '#3B82F6',
  overlay: 'rgba(15, 23, 42, 0.85)',
} as const;

export type ColorKey = keyof typeof colors;
