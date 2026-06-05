import React, { useMemo } from 'react';
import { ScrollView, StyleSheet, Text, View } from 'react-native';
import { NativeStackScreenProps } from '@react-navigation/native-stack';
import { colors, radius, spacing, typography } from '@/theme';
import { SensorCard } from '@/components/SensorCard';
import { LineChart } from '@/components/LineChart';
import { Button } from '@/components/Button';
import { ConnectionPill } from '@/components/ConnectionPill';
import { useClassroomStore } from '@/store/classroomStore';
import { AnyReading, SensorReading } from '@/lib/types';
import { RootStackParamList } from '@/navigation/types';

type Props = NativeStackScreenProps<RootStackParamList, 'Classroom'>;

export function ClassroomScreen({ navigation, route }: Props) {
  const { id } = route.params;
  const classroom = useClassroomStore((s) => s.classrooms[id]);
  const brokerState = useClassroomStore((s) => s.brokerState);

  const tempHistory = useMemo(
    () =>
      ((classroom?.history.temperature ?? []) as SensorReading[])
        .map((r) => r.value)
        .filter((v): v is number => typeof v === 'number'),
    [classroom?.history.temperature]
  );
  const humHistory = useMemo(
    () =>
      ((classroom?.history.humidity ?? []) as SensorReading[])
        .map((r) => r.value)
        .filter((v): v is number => typeof v === 'number'),
    [classroom?.history.humidity]
  );
  const personHistory = useMemo(
    () =>
      ((classroom?.history.camera ?? []) as SensorReading[])
        .map((r) => r.value)
        .filter((v): v is number => typeof v === 'number'),
    [classroom?.history.camera]
  );
  const powerHistory = useMemo(
    () =>
      ((classroom?.history.power ?? []) as SensorReading[])
        .map((r) => r.value)
        .filter((v): v is number => typeof v === 'number'),
    [classroom?.history.power]
  );

  if (!classroom) {
    return (
      <View style={styles.empty}>
        <Text style={styles.emptyText}>Bu sınıf için henüz veri yok.</Text>
      </View>
    );
  }

  const conn = classroom.connection;

  return (
    <ScrollView
      style={styles.container}
      contentContainerStyle={styles.content}
      showsVerticalScrollIndicator={false}
    >
      <View style={styles.header}>
        <View style={styles.headerStatus}>
          <ConnectionPill state={brokerState} compact />
          {conn ? (
            <Text style={styles.headerInfo}>
              {conn.status === 'online' ? `Online · ${conn.ip ?? '-'}` : 'Cihaz çevrim dışı'}
            </Text>
          ) : (
            <Text style={styles.headerInfo}>Veri bekleniyor</Text>
          )}
        </View>
        {conn?.firmware_version ? (
          <Text style={styles.fw}>FW v{conn.firmware_version}</Text>
        ) : null}
      </View>

      <View style={styles.grid}>
        <SensorCard
          label="Sıcaklık"
          icon="🌡"
          reading={classroom.sensors.temperature as AnyReading | undefined}
        />
        <SensorCard
          label="Nem"
          icon="💧"
          reading={classroom.sensors.humidity as AnyReading | undefined}
        />
      </View>
      <View style={styles.grid}>
        <SensorCard
          label="Işık"
          icon="☀"
          reading={classroom.sensors.light as AnyReading | undefined}
        />
        <SensorCard
          label="Hava"
          icon="💨"
          reading={classroom.sensors.air_quality as AnyReading | undefined}
        />
      </View>
      <View style={styles.grid}>
        <SensorCard
          label="Kişi"
          icon="👥"
          reading={classroom.sensors.camera as AnyReading | undefined}
        />
        <SensorCard
          label="Pencere"
          icon="🪟"
          reading={classroom.sensors.window as AnyReading | undefined}
        />
      </View>
      <View style={styles.grid}>
        <SensorCard
          label="Akım"
          icon="🔌"
          reading={classroom.sensors.current as AnyReading | undefined}
        />
        <SensorCard
          label="Güç"
          icon="⚡"
          reading={classroom.sensors.power as AnyReading | undefined}
        />
      </View>
      <View style={styles.grid}>
        <SensorCard
          label="Toplam Enerji"
          icon="🔋"
          reading={classroom.sensors.energy as AnyReading | undefined}
        />
      </View>

      <LineChart
        title="Sıcaklık (son 120 örnek)"
        data={tempHistory}
        unit="°C"
        color={colors.accent}
      />

      <LineChart
        title="Nem (son 120 örnek)"
        data={humHistory}
        unit="%"
        color={colors.info}
      />

      <LineChart
        title="Doluluk (son 120 örnek)"
        data={personHistory}
        unit="kişi"
        color={colors.warning}
      />

      <LineChart
        title="Güç (son 120 örnek)"
        data={powerHistory}
        unit="W"
        color={colors.danger}
      />

      <View style={styles.actuators}>
        <Text style={styles.sectionTitle}>Aktüatörler</Text>
        <View style={styles.actRow}>
          <ActuatorPill
            label="LED"
            value={`${Math.round(classroom.actuators.led?.value ?? 0)}%`}
          />
          <ActuatorPill
            label="❄️ Soğutma"
            value={classroom.actuators.cooling?.on ? 'AÇIK' : 'kapalı'}
          />
          <ActuatorPill
            label="🔥 Isıtma"
            value={classroom.actuators.heating?.on ? 'AÇIK' : 'kapalı'}
          />
        </View>
      </View>

      <View style={styles.actions}>
        <Button
          label="Kontrol"
          onPress={() => navigation.navigate('Control', { id })}
        />
        <Button
          label="Uyarı Gönder"
          variant="secondary"
          onPress={() => navigation.navigate('Alert', { id })}
        />
        <Button
          label="OTA Güncelleme"
          variant="secondary"
          onPress={() => navigation.navigate('Ota', { id })}
        />
      </View>
    </ScrollView>
  );
}

function ActuatorPill({
  label,
  value,
}: {
  label: string;
  value: string;
}) {
  return (
    <View style={styles.actCard}>
      <Text style={styles.actLabel}>{label}</Text>
      <Text style={styles.actValue}>{value}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.bg,
  },
  content: {
    padding: spacing.lg,
    gap: spacing.md,
    paddingBottom: spacing.xxxl,
  },
  empty: {
    flex: 1,
    backgroundColor: colors.bg,
    alignItems: 'center',
    justifyContent: 'center',
  },
  emptyText: {
    ...typography.body,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: spacing.xs,
  },
  headerStatus: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.md,
    flex: 1,
  },
  headerInfo: {
    ...typography.caption,
  },
  fw: {
    ...typography.caption,
    color: colors.muted,
  },
  grid: {
    flexDirection: 'row',
    gap: spacing.md,
  },
  sectionTitle: {
    ...typography.h3,
    marginTop: spacing.md,
  },
  actuators: {
    gap: spacing.md,
  },
  actRow: {
    flexDirection: 'row',
    gap: spacing.md,
  },
  actCard: {
    flex: 1,
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radius.md,
    padding: spacing.md,
  },
  actLabel: {
    ...typography.micro,
  },
  actValue: {
    ...typography.h2,
    color: colors.accent,
    marginTop: 4,
  },
  actions: {
    gap: spacing.sm,
    marginTop: spacing.lg,
  },
});
