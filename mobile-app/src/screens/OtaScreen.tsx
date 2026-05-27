import React, { useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { NativeStackScreenProps } from '@react-navigation/native-stack';
import { colors, radius, spacing, typography } from '@/theme';
import { Button } from '@/components/Button';
import { mqttService } from '@/lib/mqtt';
import { useClassroomStore } from '@/store/classroomStore';
import { RootStackParamList } from '@/navigation/types';

type Props = NativeStackScreenProps<RootStackParamList, 'Ota'>;

interface Release {
  tag_name: string;
  name: string;
  published_at: string;
  assets: { name: string; browser_download_url: string }[];
}

const REPO = 'MagiMigi/akilli-sinif';

// CI artifact prefix'leri ile birebir eslesir (firmware-{plc,cam,sim}-vX.Y.Z.bin)
type OtaTarget = 'plc' | 'cam' | 'sim';
const TARGET_LABELS: Record<OtaTarget, string> = {
  plc: 'PLC',
  cam: 'CAM',
  sim: 'SIM',
};

export function OtaScreen({ route }: Props) {
  const { id } = route.params;
  const [releases, setReleases] = useState<Release[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [target, setTarget] = useState<OtaTarget>('plc');
  const otaStatus = useClassroomStore((s) =>
    id ? s.classrooms[id]?.ota : undefined
  );

  useEffect(() => {
    (async () => {
      try {
        const res = await fetch(
          `https://api.github.com/repos/${REPO}/releases?per_page=10`
        );
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = (await res.json()) as Release[];
        setReleases(data);
      } catch (e: any) {
        setError(e?.message ?? 'Sürüm listesi alınamadı');
      } finally {
        setLoading(false);
      }
    })();
  }, []);

  const send = (release: Release) => {
    const asset = release.assets.find(
      (a) => a.name.includes(`firmware-${target}`) && a.name.endsWith('.bin')
    );
    if (!asset) {
      Alert.alert('Hata', `Bu sürümde firmware-${target}-*.bin bulunamadı`);
      return;
    }
    Alert.alert(
      'OTA Onayı',
      `${id ?? 'TÜM CİHAZLAR'} → ${release.tag_name} (${target}) güncellenecek. Onaylıyor musun?`,
      [
        { text: 'İptal', style: 'cancel' },
        {
          text: 'Gönder',
          style: 'destructive',
          onPress: () => {
            try {
              mqttService.publishOta(id ?? 'all', release.tag_name, asset.browser_download_url);
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
      <Text style={styles.title}>OTA Güncelleme</Text>
      <Text style={styles.subtitle}>
        Hedef: {id ?? 'TÜM SINIFLAR (broadcast)'}
      </Text>

      <View style={styles.targetRow}>
        {(['plc', 'cam', 'sim'] as const).map((t) => (
          <Pressable
            key={t}
            onPress={() => setTarget(t)}
            style={[styles.targetChip, target === t && styles.targetChipOn]}
          >
            <Text
              style={[
                styles.targetText,
                target === t && { color: colors.accent },
              ]}
            >
              {TARGET_LABELS[t]}
            </Text>
          </Pressable>
        ))}
      </View>

      {otaStatus ? (
        <View style={styles.statusCard}>
          <Text style={styles.statusLabel}>Son OTA durumu</Text>
          <Text style={styles.statusValue}>
            {otaStatus.status}
            {typeof otaStatus.progress === 'number' ? ` · %${otaStatus.progress}` : ''}
          </Text>
          {otaStatus.target_version ? (
            <Text style={styles.statusMeta}>Hedef: {otaStatus.target_version}</Text>
          ) : null}
          {otaStatus.error ? (
            <Text style={[styles.statusMeta, { color: colors.danger }]}>
              Hata: {otaStatus.error}
            </Text>
          ) : null}
        </View>
      ) : null}

      {loading ? (
        <ActivityIndicator color={colors.accent} style={{ marginTop: spacing.xl }} />
      ) : error ? (
        <Text style={styles.error}>{error}</Text>
      ) : (
        <View style={styles.releaseList}>
          {(releases ?? []).map((r) => (
            <View key={r.tag_name} style={styles.releaseCard}>
              <View style={{ flex: 1 }}>
                <Text style={styles.releaseTag}>{r.tag_name}</Text>
                <Text style={styles.releaseDate}>
                  {new Date(r.published_at).toLocaleDateString('tr-TR')}
                </Text>
              </View>
              <Button
                label="Gönder"
                variant="secondary"
                onPress={() => send(r)}
                style={{ paddingHorizontal: spacing.lg }}
              />
            </View>
          ))}
          {releases && releases.length === 0 ? (
            <Text style={styles.empty}>Henüz yayınlanmış sürüm yok</Text>
          ) : null}
        </View>
      )}
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.xl, gap: spacing.lg },
  title: { ...typography.h1 },
  subtitle: { ...typography.body },
  targetRow: { flexDirection: 'row', gap: spacing.sm },
  targetChip: {
    paddingHorizontal: spacing.lg,
    paddingVertical: spacing.sm,
    borderRadius: radius.pill,
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
  },
  targetChipOn: {
    borderColor: colors.accent,
    backgroundColor: colors.accentSoft,
  },
  targetText: {
    ...typography.bodyStrong,
    color: colors.textDim,
    fontSize: 13,
  },
  statusCard: {
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radius.md,
    padding: spacing.lg,
    gap: 4,
  },
  statusLabel: { ...typography.micro },
  statusValue: { ...typography.h3, color: colors.accent },
  statusMeta: { ...typography.caption },
  releaseList: { gap: spacing.sm, marginTop: spacing.sm },
  releaseCard: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radius.md,
    padding: spacing.lg,
    gap: spacing.md,
  },
  releaseTag: { ...typography.bodyStrong },
  releaseDate: { ...typography.caption },
  empty: { ...typography.body, textAlign: 'center', marginTop: spacing.xl },
  error: { ...typography.body, color: colors.danger },
});
