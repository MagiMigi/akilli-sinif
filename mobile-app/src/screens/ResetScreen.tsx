import React from 'react';
import { Alert, ScrollView, StyleSheet, Text, View } from 'react-native';
import { colors, radius, spacing, typography } from '@/theme';
import { Button } from '@/components/Button';
import { mqttService } from '@/lib/mqtt';
import { useClassroomStore } from '@/store/classroomStore';

export function ResetScreen() {
  const classrooms = useClassroomStore((s) => s.classrooms);
  const list = React.useMemo(
    () => Object.values(classrooms).sort((a, b) => a.id.localeCompare(b.id, 'tr')),
    [classrooms],
  );

  const confirm = (target: string, label: string) => {
    Alert.alert(
      'Onay',
      `${label} cihazı portal moduna alınacak ve yeniden başlayacak. Devam edilsin mi?`,
      [
        { text: 'İptal', style: 'cancel' },
        {
          text: 'Sıfırla',
          style: 'destructive',
          onPress: () => {
            try {
              mqttService.publishReset(target as any);
            } catch (e: any) {
              Alert.alert('Hata', e?.message ?? 'Komut gönderilemedi');
            }
          },
        },
      ]
    );
  };

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      <View style={styles.warn}>
        <Text style={styles.warnTitle}>Dikkat</Text>
        <Text style={styles.warnText}>
          Config sıfırlama, cihazın WiFi/MQTT ayarlarını siler. Cihaz yeniden başlar
          ve WiFiManager portal'i açar.
        </Text>
      </View>

      <Text style={styles.sectionTitle}>Tek cihaz</Text>
      <View style={styles.list}>
        {list.length === 0 ? (
          <Text style={styles.empty}>Sınıf bulunamadı</Text>
        ) : (
          list.map((c) => (
            <Button
              key={c.id}
              label={`${c.id} sıfırla`}
              variant="secondary"
              onPress={() => confirm(c.id, c.id)}
            />
          ))
        )}
      </View>

      <Text style={styles.sectionTitle}>Hepsi</Text>
      <Button
        label="TÜM CİHAZLARI SIFIRLA"
        variant="danger"
        onPress={() => confirm('all', 'TÜM cihazlar')}
      />
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.xl, gap: spacing.lg },
  warn: {
    backgroundColor: colors.surface,
    borderLeftWidth: 4,
    borderLeftColor: colors.warning,
    padding: spacing.lg,
    borderRadius: radius.md,
    gap: spacing.xs,
  },
  warnTitle: { ...typography.bodyStrong, color: colors.warning },
  warnText: { ...typography.caption, color: colors.textDim },
  sectionTitle: { ...typography.micro, marginTop: spacing.sm },
  list: { gap: spacing.sm },
  empty: { ...typography.body, color: colors.muted },
});
