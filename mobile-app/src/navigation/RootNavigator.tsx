import React from 'react';
import { ActivityIndicator, View } from 'react-native';
import { NavigationContainer, DarkTheme, Theme } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Text } from 'react-native';
import { colors } from '@/theme';
import { loadBrokerCredentials } from '@/lib/storage';
import { mqttService } from '@/lib/mqtt';
import { wireMqttToStore } from '@/store/wireMqtt';
import { SetupScreen } from '@/screens/SetupScreen';
import { DashboardScreen } from '@/screens/DashboardScreen';
import { SettingsScreen } from '@/screens/SettingsScreen';
import { ClassroomScreen } from '@/screens/ClassroomScreen';
import { ControlScreen } from '@/screens/ControlScreen';
import { AlertScreen } from '@/screens/AlertScreen';
import { OtaScreen } from '@/screens/OtaScreen';
import { ResetScreen } from '@/screens/ResetScreen';
import { RootStackParamList, TabParamList } from './types';

const Stack = createNativeStackNavigator<RootStackParamList>();
const Tab = createBottomTabNavigator<TabParamList>();

const navTheme: Theme = {
  ...DarkTheme,
  colors: {
    ...DarkTheme.colors,
    background: colors.bg,
    card: colors.bg,
    primary: colors.accent,
    text: colors.text,
    border: colors.border,
    notification: colors.accent,
  },
};

function TabIcon({ label, focused }: { label: string; focused: boolean }) {
  return (
    <Text
      style={{
        fontSize: 11,
        fontWeight: '600',
        color: focused ? colors.accent : colors.muted,
        letterSpacing: 0.4,
      }}
    >
      {label.toUpperCase()}
    </Text>
  );
}

function MainTabs() {
  return (
    <Tab.Navigator
      screenOptions={{
        headerShown: false,
        tabBarShowLabel: false,
        tabBarStyle: {
          backgroundColor: colors.surface,
          borderTopColor: colors.border,
          height: 60,
          paddingTop: 8,
        },
      }}
    >
      <Tab.Screen
        name="Dashboard"
        component={DashboardScreen}
        options={{
          tabBarIcon: ({ focused }) => <TabIcon label="Sınıflar" focused={focused} />,
        }}
      />
      <Tab.Screen
        name="Settings"
        component={SettingsScreen}
        options={{
          tabBarIcon: ({ focused }) => <TabIcon label="Ayarlar" focused={focused} />,
        }}
      />
    </Tab.Navigator>
  );
}

export function RootNavigator() {
  const [bootState, setBootState] = React.useState<'loading' | 'setup' | 'tabs'>(
    'loading'
  );

  React.useEffect(() => {
    wireMqttToStore();
    (async () => {
      let creds = null;
      try {
        creds = await loadBrokerCredentials();
      } catch {
        /* corrupted storage — fall through to setup */
      }
      if (!creds) {
        setBootState('setup');
        return;
      }
      setBootState('tabs');
      mqttService.connect(creds).catch(() => {
        /* connection state shown in UI; reconnect runs in background */
      });
    })();
  }, []);

  if (bootState === 'loading') {
    return (
      <View
        style={{
          flex: 1,
          backgroundColor: colors.bg,
          alignItems: 'center',
          justifyContent: 'center',
        }}
      >
        <ActivityIndicator color={colors.accent} />
      </View>
    );
  }

  return (
    <NavigationContainer theme={navTheme}>
      <Stack.Navigator
        initialRouteName={bootState === 'setup' ? 'Setup' : 'Tabs'}
        screenOptions={{
          headerStyle: { backgroundColor: colors.bg },
          headerTitleStyle: { color: colors.text, fontWeight: '600' },
          headerTintColor: colors.accent,
          contentStyle: { backgroundColor: colors.bg },
        }}
      >
        <Stack.Screen
          name="Setup"
          component={SetupScreen}
          options={{ headerShown: false }}
        />
        <Stack.Screen
          name="Tabs"
          component={MainTabs}
          options={{ headerShown: false }}
        />
        <Stack.Screen
          name="Classroom"
          component={ClassroomScreen}
          options={({ route }) => ({ title: route.params.id })}
        />
        <Stack.Screen
          name="Control"
          component={ControlScreen}
          options={{ title: 'Kontrol', presentation: 'modal' }}
        />
        <Stack.Screen
          name="Alert"
          component={AlertScreen}
          options={{ title: 'Uyarı Gönder', presentation: 'modal' }}
        />
        <Stack.Screen
          name="Ota"
          component={OtaScreen}
          options={{ title: 'OTA Güncelleme' }}
        />
        <Stack.Screen
          name="Reset"
          component={ResetScreen}
          options={{ title: 'Config Sıfırlama' }}
        />
      </Stack.Navigator>
    </NavigationContainer>
  );
}
