import React, { useEffect, useState } from 'react';
import { Alert, ScrollView, StyleSheet, Text, View } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { useNavigation } from '@react-navigation/native';
import { NativeStackNavigationProp } from '@react-navigation/native-stack';
import { colors, radius, spacing, typography } from '@/theme';
import { Button } from '@/components/Button';
import { ConnectionPill } from '@/components/ConnectionPill';
import { useClassroomStore } from '@/store/classroomStore';
import { mqttService } from '@/lib/mqtt';
import { clearBrokerCredentials, loadBrokerCredentials } from '@/lib/storage';
import { BrokerCredentials } from '@/lib/types';
import { RootStackParamList } from '@/navigation/types';
import Constants from 'expo-constants';

export function SettingsScreen() {
  const nav = useNavigation<NativeStackNavigationProp<RootStackParamList>>();
  const brokerState = useClassroomStore((s) => s.brokerState);
  const error = useClassroomStore((s) => s.brokerError);
  const reset = useClassroomStore((s) => s.reset);
  const [creds, setCreds] = useState<BrokerCredentials | null>(null);

  useEffect(() => {
    loadBrokerCredentials().then(setCreds);
  }, []);

  const onLogout = () => {
    Alert.alert('Bağlantıyı kes', 'Broker bilgileri silinecek. Onaylıyor musun?', [
      { text: 'İptal', style: 'cancel' },
      {
        text: 'Bağlantıyı kes',
        style: 'destructive',
        onPress: async () => {
          mqttService.disconnect();
          await clearBrokerCredentials();
          reset();
          nav.reset({ index: 0, routes: [{ name: 'Setup' }] });
        },
      },
    ]);
  };

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.title}>Ayarlar</Text>

        <View style={styles.card}>
          <View style={styles.cardHead}>
            <Text style={styles.cardTitle}>MQTT Bağlantısı</Text>
            <ConnectionPill state={brokerState} compact />
          </View>
          {creds ? (
            <View style={styles.kvList}>
              <KV k="Host" v={creds.host} />
              <KV k="Port" v={String(creds.port)} />
              <KV k="Kullanıcı" v={creds.username} />
              <KV k="TLS" v={creds.useSsl ? 'Açık' : 'Kapalı'} />
            </View>
          ) : (
            <Text style={styles.muted}>Yapılandırma yok</Text>
          )}
          {error ? <Text style={styles.error}>{error}</Text> : null}
          <Button
            label="Broker Bilgilerini Düzenle"
            variant="secondary"
            onPress={() => nav.navigate('Setup', { reconfigure: true })}
          />
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>Cihaz Yönetimi</Text>
          <Button
            label="OTA Broadcast (Tüm sınıflar)"
            variant="secondary"
            onPress={() => nav.navigate('Ota', {})}
          />
          <Button
            label="Config Sıfırlama"
            variant="secondary"
            onPress={() => nav.navigate('Reset')}
          />
        </View>

        <View style={styles.card}>
          <Text style={styles.cardTitle}>Hakkında</Text>
          <KV k="Uygulama" v="Akıllı Sınıf" />
          <KV
            k="Sürüm"
            v={Constants.expoConfig?.version ?? '1.0.0'}
          />
          <KV k="Lisans" v="MIT" />
        </View>

        <Button label="Bağlantıyı Kes" variant="danger" onPress={onLogout} />
      </ScrollView>
    </SafeAreaView>
  );
}

function KV({ k, v }: { k: string; v: string }) {
  return (
    <View style={styles.kv}>
      <Text style={styles.kvKey}>{k}</Text>
      <Text style={styles.kvValue}>{v}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  safe: { flex: 1, backgroundColor: colors.bg },
  content: { padding: spacing.xl, gap: spacing.lg },
  title: { ...typography.display, marginBottom: spacing.xs },
  card: {
    backgroundColor: colors.surface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radius.lg,
    padding: spacing.lg,
    gap: spacing.md,
  },
  cardHead: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  cardTitle: { ...typography.h3 },
  kvList: { gap: spacing.sm },
  kv: { flexDirection: 'row', justifyContent: 'space-between' },
  kvKey: { ...typography.caption },
  kvValue: { ...typography.bodyStrong },
  muted: { ...typography.body, color: colors.muted },
  error: { ...typography.caption, color: colors.danger },
});
