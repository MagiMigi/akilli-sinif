import React from 'react';
import { StyleSheet, Text, View } from 'react-native';
import { colors, radius, spacing, typography } from '@/theme';
import { MqttConnectionState } from '@/lib/types';

interface Props {
  state: MqttConnectionState;
  compact?: boolean;
}

const LABELS: Record<MqttConnectionState, string> = {
  idle: 'Bağlanmadı',
  connecting: 'Bağlanıyor',
  connected: 'Bağlı',
  reconnecting: 'Yeniden bağlanıyor',
  offline: 'Çevrim dışı',
  error: 'Hata',
};

const COLOR: Record<MqttConnectionState, string> = {
  idle: colors.muted,
  connecting: colors.warning,
  connected: colors.success,
  reconnecting: colors.warning,
  offline: colors.danger,
  error: colors.danger,
};

export function ConnectionPill({ state, compact }: Props) {
  const dotColor = COLOR[state];
  return (
    <View style={[styles.pill, compact && styles.compact]}>
      <View style={[styles.dot, { backgroundColor: dotColor }]} />
      <Text style={styles.label}>{LABELS[state]}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  pill: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: colors.surface,
    paddingHorizontal: spacing.md,
    paddingVertical: spacing.xs + 2,
    borderRadius: radius.pill,
    borderWidth: 1,
    borderColor: colors.border,
    gap: spacing.sm,
    alignSelf: 'flex-start',
  },
  compact: {
    paddingHorizontal: spacing.sm,
    paddingVertical: 3,
  },
  dot: {
    width: 8,
    height: 8,
    borderRadius: 4,
  },
  label: {
    ...typography.caption,
    color: colors.textDim,
    fontWeight: '500',
  },
});
