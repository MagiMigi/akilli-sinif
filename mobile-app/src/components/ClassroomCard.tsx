import React from 'react';
import { Pressable, StyleSheet, Text, View } from 'react-native';
import { colors, radius, spacing, typography } from '@/theme';
import { ClassroomState, SensorReading } from '@/lib/types';

interface Props {
  classroom: ClassroomState;
  onPress: () => void;
}

function reading(c: ClassroomState, kind: 'temperature' | 'humidity' | 'camera'): string {
  const r = c.sensors[kind] as SensorReading | undefined;
  if (!r) return '—';
  if (kind === 'camera') return `${Math.round(r.value)} kişi`;
  const dec = kind === 'temperature' ? 1 : 0;
  return `${r.value.toFixed(dec)}${r.unit ? r.unit : ''}`;
}

export function ClassroomCard({ classroom, onPress }: Props) {
  const conn = classroom.connection;
  const isOnline = conn?.status === 'online';
  const dotColor = isOnline ? colors.success : conn ? colors.danger : colors.muted;

  return (
    <Pressable
      onPress={onPress}
      style={({ pressed }) => [styles.card, pressed && styles.pressed]}
    >
      <View style={styles.head}>
        <View style={styles.headLeft}>
          <View style={[styles.dot, { backgroundColor: dotColor }]} />
          <Text style={styles.title}>{classroom.id}</Text>
        </View>
        {conn?.firmware_version ? (
          <Text style={styles.fw}>v{conn.firmware_version}</Text>
        ) : null}
      </View>

      <View style={styles.row}>
        <Metric label="Sıcaklık" value={reading(classroom, 'temperature')} />
        <Metric label="Nem" value={reading(classroom, 'humidity')} />
        <Metric label="Kişi" value={reading(classroom, 'camera')} />
      </View>

      <View style={styles.foot}>
        <Text style={styles.footText}>
          {isOnline ? `IP ${conn?.ip ?? '?'}` : conn ? 'Çevrim dışı' : 'Veri bekleniyor'}
        </Text>
        <Text style={styles.footChevron}>›</Text>
      </View>
    </Pressable>
  );
}

function Metric({ label, value }: { label: string; value: string }) {
  return (
    <View style={styles.metric}>
      <Text style={styles.metricLabel}>{label}</Text>
      <Text style={styles.metricValue} numberOfLines={1}>
        {value}
      </Text>
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
    gap: spacing.md,
  },
  pressed: {
    backgroundColor: colors.surfaceAlt,
  },
  head: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  headLeft: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
  },
  dot: {
    width: 10,
    height: 10,
    borderRadius: 5,
  },
  title: {
    ...typography.h2,
    textTransform: 'capitalize',
  },
  fw: {
    ...typography.caption,
    color: colors.muted,
  },
  row: {
    flexDirection: 'row',
    gap: spacing.md,
  },
  metric: {
    flex: 1,
  },
  metricLabel: {
    ...typography.micro,
  },
  metricValue: {
    ...typography.h3,
    marginTop: 2,
  },
  foot: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: spacing.xs,
  },
  footText: {
    ...typography.caption,
  },
  footChevron: {
    color: colors.muted,
    fontSize: 22,
    lineHeight: 22,
  },
});
