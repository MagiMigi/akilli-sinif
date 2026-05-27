import Constants from 'expo-constants';
import { useClassroomStore } from '@/store/classroomStore';
import {
  AnyReading,
  ClassroomState,
  ConnectionStatus,
  MqttConnectionState,
  OtaStatus,
  SensorReading,
} from './types';

// Expo Go SDK 53+ logs noisy push-notification errors on import.
// Skip expo-notifications entirely there; full build (APK) loads it normally.
export const isExpoGo = Constants.executionEnvironment === 'storeClient';
const Notifications: typeof import('expo-notifications') | null = isExpoGo
  ? null
  : require('expo-notifications');

// Eşikler — config.json'daki değerlerle hizalı (sensors.temperature.max=35,
// sensors.air_quality.hazardous=500). Mobil tarafta kopyalanmıştır; eşik
// politikasını değiştirirsen iki yeri de güncelle.
const TEMP_CRITICAL = 35;
const AQI_HAZARD = 500;
const DEBOUNCE_MS = 60_000;

const lastFired = new Map<string, number>();

function shouldFire(key: string, gapMs = DEBOUNCE_MS): boolean {
  const now = Date.now();
  const prev = lastFired.get(key);
  if (prev !== undefined && now - prev < gapMs) return false;
  lastFired.set(key, now);
  return true;
}

function notify(title: string, body: string, key: string) {
  if (!shouldFire(key)) return;
  if (!Notifications) return;
  Notifications.scheduleNotificationAsync({
    content: { title, body, data: { key } },
    trigger: null,
  }).catch(() => {
    // Bildirim izni yoksa veya başka bir hata varsa sessizce yut.
  });
}

export async function setupNotifications(androidColor: string): Promise<void> {
  if (!Notifications) return;
  Notifications.setNotificationHandler({
    handleNotification: async () => ({
      shouldPlaySound: false,
      shouldSetBadge: false,
      shouldShowBanner: true,
      shouldShowList: true,
    }),
  });
  try {
    await Notifications.setNotificationChannelAsync('alerts', {
      name: 'Sınıf Uyarıları',
      importance: Notifications.AndroidImportance.HIGH,
      lightColor: androidColor,
    });
    const settings = await Notifications.getPermissionsAsync();
    if (!settings.granted) await Notifications.requestPermissionsAsync();
  } catch {
    /* silent */
  }
}

function tempValue(reading?: AnyReading): number | undefined {
  if (!reading || reading.kind !== 'temperature') return undefined;
  return (reading as SensorReading).value;
}

function aqValue(reading?: AnyReading): number | undefined {
  if (!reading || reading.kind !== 'air_quality') return undefined;
  return (reading as SensorReading).value;
}

function diffClassroom(
  id: string,
  prev: ClassroomState | undefined,
  cur: ClassroomState
) {
  // Sıcaklık kritik (kenar tetiklemeli)
  const prevT = tempValue(prev?.sensors.temperature);
  const curT = tempValue(cur.sensors.temperature);
  if (
    curT !== undefined &&
    curT >= TEMP_CRITICAL &&
    (prevT === undefined || prevT < TEMP_CRITICAL)
  ) {
    notify(
      `${id} — Kritik sıcaklık`,
      `${curT.toFixed(1)}°C (eşik: ${TEMP_CRITICAL}°C)`,
      `${id}:temp_critical`
    );
  }

  // Hava kalitesi tehlikeli (kenar)
  const prevA = aqValue(prev?.sensors.air_quality);
  const curA = aqValue(cur.sensors.air_quality);
  if (
    curA !== undefined &&
    curA >= AQI_HAZARD &&
    (prevA === undefined || prevA < AQI_HAZARD)
  ) {
    notify(
      `${id} — Hava kalitesi tehlikeli`,
      `${curA} ppm (eşik: ${AQI_HAZARD})`,
      `${id}:aqi_hazard`
    );
  }

  // OTA durum geçişleri
  diffOta(id, prev?.ota, cur.ota);

  // Cihaz online → offline
  diffConnection(id, prev?.connection, cur.connection);
}

function diffOta(id: string, prev: OtaStatus | undefined, cur: OtaStatus | undefined) {
  if (!cur) return;
  const prevS = prev?.status;
  if (cur.status === 'failed' && prevS !== 'failed') {
    notify(
      `${id} — OTA başarısız`,
      cur.error ? `Hata: ${cur.error}` : 'Güncelleme başarısız.',
      `${id}:ota`
    );
  } else if (cur.status === 'success' && prevS === 'updating') {
    notify(
      `${id} — OTA tamamlandı`,
      cur.target_version
        ? `Yeni sürüm: ${cur.target_version}`
        : 'Güncelleme başarılı.',
      `${id}:ota`
    );
  }
}

function diffConnection(
  id: string,
  prev: ConnectionStatus | undefined,
  cur: ConnectionStatus | undefined
) {
  if (!cur) return;
  if (cur.status === 'offline' && prev?.status === 'online') {
    notify(`${id} — Cihaz çevrimdışı`, 'Bağlantı koptu.', `${id}:conn`);
  }
}

function diffBroker(prev: MqttConnectionState, cur: MqttConnectionState) {
  if (cur === prev) return;
  if (cur === 'error' || cur === 'offline') {
    notify(
      'Broker bağlantısı kesildi',
      'MQTT broker erişilemez durumda.',
      'broker'
    );
  }
}

let unsubscribe: (() => void) | null = null;

export function startNotificationWatcher(): () => void {
  if (unsubscribe) return unsubscribe;

  // Zustand v5 plain subscribe: listener (state, prevState) alır.
  unsubscribe = useClassroomStore.subscribe((state, prev) => {
    if (state.brokerState !== prev.brokerState) {
      diffBroker(prev.brokerState, state.brokerState);
    }
    if (state.classrooms !== prev.classrooms) {
      for (const [id, cur] of Object.entries(state.classrooms)) {
        diffClassroom(id, prev.classrooms[id], cur);
      }
    }
  });

  return unsubscribe;
}

export function stopNotificationWatcher(): void {
  if (unsubscribe) {
    unsubscribe();
    unsubscribe = null;
  }
  lastFired.clear();
}
