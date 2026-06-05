import React from 'react';
import { StyleSheet, Text, View } from 'react-native';
import { colors, radius, spacing, typography } from '@/theme';
import { AnyReading } from '@/lib/types';

interface Props {
  label: string;
  reading?: AnyReading;
  icon?: string;
  accent?: string;
  size?: 'sm' | 'md';
}

function formatReading(r: AnyReading | undefined): string {
  if (!r) return '—';
  if (r.kind === 'pir') return r.detected ? 'Hareket var' : 'Sakin';
  if (r.kind === 'window') return r.open ? 'Açık' : 'Kapalı';
  if (r.kind === 'camera') return `${Math.round(r.value)} kişi`;
  const decimals =
    r.kind === 'energy'
      ? 2
      : r.kind === 'temperature' || r.kind === 'humidity' || r.kind === 'power'
        ? 1
        : 0;
  const val = r.value.toFixed(decimals);
  const unit = r.unit ? ` ${r.unit}` : '';
  return `${val}${unit}`;
}

export function SensorCard({ label, reading, icon, accent = colors.accent, size = 'md' }: Props) {
  const stale = reading ? Date.now() - reading.receivedAt > 30_000 : false;
  return (
    <View style={[styles.card, size === 'sm' && styles.cardSm]}>
      <View style={styles.head}>
        <Text style={styles.label}>{label}</Text>
        {icon ? <Text style={[styles.icon, { color: accent }]}>{icon}</Text> : null}
      </View>
      <Text style={[styles.value, stale && styles.stale]} numberOfLines={1}>
        {formatReading(reading)}
      </Text>
      {reading?.kind === 'energy' && typeof reading.today === 'number' ? (
        <Text style={styles.subtitle}>Bugün: {reading.today.toFixed(2)} kWh</Text>
      ) : null}
      {reading?.sim ? <Text style={styles.simBadge}>SİMÜLE</Text> : null}
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: colors.surface,
    borderRadius: radius.lg,
    padding: spacing.lg,
    borderWidth: 1,
    borderColor: colors.border,
    minHeight: 96,
    flex: 1,
  },
  cardSm: {
    padding: spacing.md,
    minHeight: 80,
  },
  head: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: spacing.sm,
  },
  label: {
    ...typography.micro,
  },
  icon: {
    fontSize: 16,
  },
  value: {
    ...typography.metric,
  },
  stale: {
    color: colors.muted,
  },
  subtitle: {
    ...typography.caption,
    color: colors.muted,
    marginTop: spacing.xs,
  },
  simBadge: {
    ...typography.micro,
    color: colors.warning,
    marginTop: spacing.xs,
  },
});
