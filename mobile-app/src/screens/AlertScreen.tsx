import React, { useState } from 'react';
import { Alert, Pressable, ScrollView, StyleSheet, Text, View } from 'react-native';
import { NativeStackScreenProps } from '@react-navigation/native-stack';
import { colors, radius, spacing, typography } from '@/theme';
import { TextField } from '@/components/TextField';
import { Button } from '@/components/Button';
import { mqttService } from '@/lib/mqtt';
import { AlertLevel } from '@/lib/types';
import { RootStackParamList } from '@/navigation/types';

type Props = NativeStackScreenProps<RootStackParamList, 'Alert'>;

const LEVELS: { key: AlertLevel; label: string; color: string; description: string }[] = [
  { key: 'info', label: 'Bilgi', color: colors.info, description: 'Standart bilgilendirme' },
  { key: 'warning', label: 'Uyarı', color: colors.warning, description: 'Dikkat çekici sarı uyarı' },
  { key: 'danger', label: 'Tehlike', color: colors.danger, description: 'Kırmızı yanıp sönen uyarı' },
  { key: 'none', label: 'Temizle', color: colors.muted, description: 'Aktif uyarıyı kaldır' },
];

export function AlertScreen({ navigation, route }: Props) {
  const { id } = route.params;
  const [level, setLevel] = useState<AlertLevel>('warning');
  const [message, setMessage] = useState('');

  const onSend = () => {
    try {
      mqttService.publishAlert(id, level, level === 'none' ? undefined : message || undefined);
      navigation.goBack();
    } catch (e: any) {
      Alert.alert('Hata', e?.message ?? 'Komut gönderilemedi');
    }
  };

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      <Text style={styles.title}>{id}</Text>
      <Text style={styles.subtitle}>Sınıf TFT ekranında uyarı gösterilir</Text>

      <View style={styles.levelsCol}>
        {LEVELS.map((l) => (
          <Pressable
            key={l.key}
            onPress={() => setLevel(l.key)}
            style={[
              styles.levelRow,
              level === l.key && { borderColor: l.color, backgroundColor: colors.surfaceAlt },
            ]}
          >
            <View style={[styles.levelDot, { backgroundColor: l.color }]} />
            <View style={{ flex: 1 }}>
              <Text style={styles.levelLabel}>{l.label}</Text>
              <Text style={styles.levelDesc}>{l.description}</Text>
            </View>
          </Pressable>
        ))}
      </View>

      {level !== 'none' ? (
        <TextField
          label="Mesaj (opsiyonel)"
          value={message}
          onChangeText={setMessage}
          placeholder="Örn. Yangın tatbikatı"
          maxLength={48}
        />
      ) : null}

      <Button label="Gönder" onPress={onSend} />
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
    gap: spacing.lg,
  },
  title: {
    ...typography.h1,
    textTransform: 'capitalize',
  },
  subtitle: {
    ...typography.body,
  },
  levelsCol: {
    gap: spacing.sm,
  },
  levelRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.md,
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radius.md,
    padding: spacing.lg,
  },
  levelDot: {
    width: 14,
    height: 14,
    borderRadius: 7,
  },
  levelLabel: {
    ...typography.bodyStrong,
  },
  levelDesc: {
    ...typography.caption,
  },
});
