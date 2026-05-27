import React, { useState } from 'react';
import { Alert, ScrollView, StyleSheet, Text, View } from 'react-native';
import { NativeStackScreenProps } from '@react-navigation/native-stack';
import { colors, radius, spacing, typography } from '@/theme';
import { Slider } from '@/components/Slider';
import { Button } from '@/components/Button';
import { mqttService } from '@/lib/mqtt';
import { useClassroomStore } from '@/store/classroomStore';
import { RootStackParamList } from '@/navigation/types';

type Props = NativeStackScreenProps<RootStackParamList, 'Control'>;

export function ControlScreen({ route }: Props) {
  const { id } = route.params;
  const ledState = useClassroomStore((s) => s.classrooms[id]?.actuators.led?.value ?? 0);
  const coolingOn = useClassroomStore((s) => s.classrooms[id]?.actuators.cooling?.on ?? false);
  const heatingOn = useClassroomStore((s) => s.classrooms[id]?.actuators.heating?.on ?? false);

  const [led, setLed] = useState(Math.round(ledState));

  const commitLed = (v: number) => {
    try {
      mqttService.publishLed(id, v);
    } catch (e: any) {
      Alert.alert('Hata', e?.message ?? 'Komut gönderilemedi');
    }
  };

  const sendCooling = (on: boolean) => {
    try {
      mqttService.publishCooling(id, on);
    } catch (e: any) {
      Alert.alert('Hata', e?.message ?? 'Komut gönderilemedi');
    }
  };

  const sendHeating = (on: boolean) => {
    try {
      mqttService.publishHeating(id, on);
    } catch (e: any) {
      Alert.alert('Hata', e?.message ?? 'Komut gönderilemedi');
    }
  };

  return (
    <ScrollView
      style={styles.container}
      contentContainerStyle={styles.content}
    >
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>{id} kontrolü</Text>
        <Text style={styles.sectionHint}>
          Slider'ı bırakınca komut anında gönderilir
        </Text>
      </View>

      <View style={styles.card}>
        <Slider
          label="LED Parlaklık"
          value={led}
          onChange={setLed}
          onCommit={commitLed}
          accent={colors.accent}
        />
        <View style={styles.row}>
          <Button
            label="Aç (%100)"
            variant="secondary"
            onPress={() => {
              setLed(100);
              mqttService.publishLedState(id, true);
            }}
            style={{ flex: 1 }}
          />
          <Button
            label="Kapat"
            variant="secondary"
            onPress={() => {
              setLed(0);
              mqttService.publishLedState(id, false);
            }}
            style={{ flex: 1 }}
          />
        </View>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>
          ❄️ Soğutma {coolingOn ? '· AÇIK' : '· kapalı'}
        </Text>
        <View style={styles.row}>
          <Button
            label="Soğutmayı Aç"
            variant={coolingOn ? 'primary' : 'secondary'}
            onPress={() => sendCooling(true)}
            style={{ flex: 1 }}
          />
          <Button
            label="Kapat"
            variant="secondary"
            onPress={() => sendCooling(false)}
            style={{ flex: 1 }}
          />
        </View>
      </View>

      <View style={styles.card}>
        <Text style={styles.cardTitle}>
          🔥 Isıtma {heatingOn ? '· AÇIK' : '· kapalı'}
        </Text>
        <View style={styles.row}>
          <Button
            label="Isıtmayı Aç"
            variant={heatingOn ? 'primary' : 'secondary'}
            onPress={() => sendHeating(true)}
            style={{ flex: 1 }}
          />
          <Button
            label="Kapat"
            variant="secondary"
            onPress={() => sendHeating(false)}
            style={{ flex: 1 }}
          />
        </View>
      </View>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.bg,
  },
  content: {
    padding: spacing.xl,
    gap: spacing.xl,
  },
  section: {
    gap: spacing.xs,
  },
  sectionTitle: {
    ...typography.h1,
    textTransform: 'capitalize',
  },
  sectionHint: {
    ...typography.caption,
  },
  card: {
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radius.lg,
    padding: spacing.lg,
    gap: spacing.lg,
  },
  cardTitle: {
    ...typography.h3,
  },
  row: {
    flexDirection: 'row',
    gap: spacing.md,
  },
});
