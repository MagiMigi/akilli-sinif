import { TextStyle } from 'react-native';
import { colors } from './colors';

export const typography = {
  display: {
    fontSize: 32,
    fontWeight: '700',
    color: colors.text,
    letterSpacing: -0.5,
  },
  h1: {
    fontSize: 24,
    fontWeight: '700',
    color: colors.text,
    letterSpacing: -0.3,
  },
  h2: {
    fontSize: 20,
    fontWeight: '600',
    color: colors.text,
  },
  h3: {
    fontSize: 17,
    fontWeight: '600',
    color: colors.text,
  },
  body: {
    fontSize: 15,
    fontWeight: '400',
    color: colors.textDim,
  },
  bodyStrong: {
    fontSize: 15,
    fontWeight: '600',
    color: colors.text,
  },
  caption: {
    fontSize: 13,
    fontWeight: '400',
    color: colors.muted,
  },
  micro: {
    fontSize: 11,
    fontWeight: '500',
    color: colors.muted,
    letterSpacing: 0.4,
    textTransform: 'uppercase',
  },
  metric: {
    fontSize: 28,
    fontWeight: '700',
    color: colors.text,
    letterSpacing: -0.5,
  },
} as const satisfies Record<string, TextStyle>;
