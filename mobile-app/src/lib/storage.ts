import * as SecureStore from 'expo-secure-store';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { BrokerCredentials } from './types';

const BROKER_KEY = 'akilli.broker.credentials.v1';
const HISTORY_PREFIX = 'akilli.history.';

export async function saveBrokerCredentials(creds: BrokerCredentials): Promise<void> {
  await SecureStore.setItemAsync(BROKER_KEY, JSON.stringify(creds));
}

export async function loadBrokerCredentials(): Promise<BrokerCredentials | null> {
  const raw = await SecureStore.getItemAsync(BROKER_KEY);
  if (!raw) return null;
  try {
    return JSON.parse(raw) as BrokerCredentials;
  } catch {
    return null;
  }
}

export async function clearBrokerCredentials(): Promise<void> {
  await SecureStore.deleteItemAsync(BROKER_KEY);
}

export async function saveHistory<T>(classroomId: string, data: T): Promise<void> {
  await AsyncStorage.setItem(HISTORY_PREFIX + classroomId, JSON.stringify(data));
}

export async function loadHistory<T>(classroomId: string): Promise<T | null> {
  const raw = await AsyncStorage.getItem(HISTORY_PREFIX + classroomId);
  if (!raw) return null;
  try {
    return JSON.parse(raw) as T;
  } catch {
    return null;
  }
}
