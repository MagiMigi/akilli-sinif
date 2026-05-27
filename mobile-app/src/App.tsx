import 'react-native-url-polyfill/auto';
import { Buffer } from 'buffer';

if (typeof global.Buffer === 'undefined') {
  // @ts-expect-error — globalThis Buffer shim for mqtt.js
  global.Buffer = Buffer;
}

import React, { useEffect } from 'react';
import { StatusBar } from 'expo-status-bar';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { colors } from '@/theme';
import { RootNavigator } from '@/navigation/RootNavigator';
import {
  setupNotifications,
  startNotificationWatcher,
  stopNotificationWatcher,
} from '@/lib/notifications';

export default function App() {
  useEffect(() => {
    setupNotifications(colors.accent).catch(() => {
      /* silent — notifications optional */
    });
    startNotificationWatcher();
    return () => stopNotificationWatcher();
  }, []);

  return (
    <GestureHandlerRootView style={{ flex: 1, backgroundColor: colors.bg }}>
      <SafeAreaProvider>
        <StatusBar style="light" backgroundColor={colors.bg} />
        <RootNavigator />
      </SafeAreaProvider>
    </GestureHandlerRootView>
  );
}
