import React, { useEffect, useState } from 'react';
import {
  KeyboardAvoidingView,
  Platform,
  Pressable,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { NativeStackScreenProps } from '@react-navigation/native-stack';
import { colors, spacing, typography } from '@/theme';
import { Button } from '@/components/Button';
import { TextField } from '@/components/TextField';
import { mqttService } from '@/lib/mqtt';
import { loadBrokerCredentials, saveBrokerCredentials } from '@/lib/storage';
import { BrokerCredentials } from '@/lib/types';
import { RootStackParamList } from '@/navigation/types';

type Props = NativeStackScreenProps<RootStackParamList, 'Setup'>;

export function SetupScreen({ navigation, route }: Props) {
  const reconfigure = route.params?.reconfigure;
  const [host, setHost] = useState('');
  const [port, setPort] = useState('9001');
  const [username, setUsername] = useState('mobile');
  const [password, setPassword] = useState('');
  const [useSsl, setUseSsl] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    (async () => {
      const saved = await loadBrokerCredentials();
      if (saved) {
        setHost(saved.host);
        setPort(String(saved.port));
        setUsername(saved.username);
        setPassword(saved.password);
        setUseSsl(saved.useSsl);
      }
    })();
  }, []);

  const onConnect = async () => {
    setError(null);
    const portNum = parseInt(port, 10);
    if (!host || !portNum || !username) {
      setError('Host, port ve kullanıcı adı zorunlu');
      return;
    }
    const creds: BrokerCredentials = {
      host: host.trim(),
      port: portNum,
      username: username.trim(),
      password,
      useSsl,
    };
    setBusy(true);
    try {
      await mqttService.connect(creds);
      await saveBrokerCredentials(creds);
      navigation.replace('Tabs');
    } catch (e: any) {
      setError(e?.message ?? 'Bağlantı kurulamadı');
    } finally {
      setBusy(false);
    }
  };

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>
      <KeyboardAvoidingView
        behavior={Platform.OS === 'ios' ? 'padding' : undefined}
        style={{ flex: 1 }}
      >
        <ScrollView
          contentContainerStyle={styles.scroll}
          keyboardShouldPersistTaps="handled"
        >
          <View style={styles.header}>
            <Text style={styles.brand}>Akıllı Sınıf</Text>
            <Text style={styles.subtitle}>
              MQTT broker bilgilerini girerek sisteme bağlanın
            </Text>
          </View>

          <View style={styles.form}>
            <TextField
              label="Broker Host / IP"
              value={host}
              onChangeText={setHost}
              placeholder="192.168.1.100"
              autoCapitalize="none"
              autoCorrect={false}
              keyboardType="default"
            />
            <TextField
              label="WebSocket Port"
              value={port}
              onChangeText={setPort}
              placeholder="9001"
              keyboardType="number-pad"
            />
            <TextField
              label="Kullanıcı Adı"
              value={username}
              onChangeText={setUsername}
              placeholder="mobile"
              autoCapitalize="none"
              autoCorrect={false}
            />
            <TextField
              label="Şifre"
              value={password}
              onChangeText={setPassword}
              placeholder="••••••••"
              secureTextEntry
              autoCapitalize="none"
            />

            <Pressable onPress={() => setUseSsl(!useSsl)} style={styles.sslRow}>
              <View style={[styles.checkbox, useSsl && styles.checkboxOn]}>
                {useSsl ? <Text style={styles.checkmark}>✓</Text> : null}
              </View>
              <Text style={styles.sslLabel}>TLS / WSS kullan (port 8443)</Text>
            </Pressable>

            {error ? <Text style={styles.error}>{error}</Text> : null}

            <Button label="Bağlan" onPress={onConnect} loading={busy} />

            {reconfigure ? (
              <Button
                label="İptal"
                variant="ghost"
                onPress={() => navigation.goBack()}
              />
            ) : null}
          </View>
        </ScrollView>
      </KeyboardAvoidingView>
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe: {
    flex: 1,
    backgroundColor: colors.bg,
  },
  scroll: {
    padding: spacing.xl,
    gap: spacing.xxl,
  },
  header: {
    marginTop: spacing.xxl,
    gap: spacing.sm,
  },
  brand: {
    ...typography.display,
  },
  subtitle: {
    ...typography.body,
  },
  form: {
    gap: spacing.lg,
  },
  sslRow: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.md,
  },
  checkbox: {
    width: 22,
    height: 22,
    borderRadius: 6,
    borderWidth: 1.5,
    borderColor: colors.border,
    alignItems: 'center',
    justifyContent: 'center',
  },
  checkboxOn: {
    backgroundColor: colors.accent,
    borderColor: colors.accent,
  },
  checkmark: {
    color: '#04221F',
    fontWeight: '800',
    fontSize: 14,
  },
  sslLabel: {
    ...typography.body,
    color: colors.textDim,
  },
  error: {
    ...typography.body,
    color: colors.danger,
  },
});
