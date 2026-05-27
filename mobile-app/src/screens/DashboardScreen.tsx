import React from 'react';
import { FlatList, RefreshControl, StyleSheet, Text, View } from 'react-native';
import { SafeAreaView } from 'react-native-safe-area-context';
import { NativeStackScreenProps } from '@react-navigation/native-stack';
import { CompositeScreenProps, useNavigation } from '@react-navigation/native';
import { BottomTabScreenProps } from '@react-navigation/bottom-tabs';
import { colors, spacing, typography } from '@/theme';
import { ConnectionPill } from '@/components/ConnectionPill';
import { ClassroomCard } from '@/components/ClassroomCard';
import { EmptyState } from '@/components/EmptyState';
import { useClassroomStore } from '@/store/classroomStore';
import { mqttService } from '@/lib/mqtt';
import { loadBrokerCredentials } from '@/lib/storage';
import { RootStackParamList, TabParamList } from '@/navigation/types';

type Props = CompositeScreenProps<
  BottomTabScreenProps<TabParamList, 'Dashboard'>,
  NativeStackScreenProps<RootStackParamList>
>;

export function DashboardScreen({}: Props) {
  const nav = useNavigation<NativeStackScreenProps<RootStackParamList>['navigation']>();
  const brokerState = useClassroomStore((s) => s.brokerState);
  const classrooms = useClassroomStore((s) => s.classrooms);
  const list = React.useMemo(
    () => Object.values(classrooms).sort((a, b) => a.id.localeCompare(b.id, 'tr')),
    [classrooms],
  );
  const [refreshing, setRefreshing] = React.useState(false);

  const onRefresh = async () => {
    setRefreshing(true);
    const creds = await loadBrokerCredentials();
    if (creds) {
      try {
        await mqttService.connect(creds);
      } catch {
        /* state will reflect error */
      }
    }
    setRefreshing(false);
  };

  return (
    <SafeAreaView style={styles.safe} edges={['top']}>
      <View style={styles.header}>
        <View>
          <Text style={styles.title}>Sınıflar</Text>
          <Text style={styles.subtitle}>
            {list.length > 0 ? `${list.length} sınıf izleniyor` : 'Veri bekleniyor'}
          </Text>
        </View>
        <ConnectionPill state={brokerState} />
      </View>

      <FlatList
        data={list}
        keyExtractor={(c) => c.id}
        contentContainerStyle={styles.listContent}
        ItemSeparatorComponent={() => <View style={{ height: spacing.md }} />}
        renderItem={({ item }) => (
          <ClassroomCard
            classroom={item}
            onPress={() => nav.navigate('Classroom', { id: item.id })}
          />
        )}
        ListEmptyComponent={
          <EmptyState
            title="Henüz sınıf yok"
            description={
              brokerState === 'connected'
                ? 'Cihazlar online olduğunda sınıflar otomatik görünecek.'
                : 'MQTT bağlantısı kurulduğunda sınıflar otomatik tespit edilir.'
            }
          />
        }
        refreshControl={
          <RefreshControl
            refreshing={refreshing}
            onRefresh={onRefresh}
            tintColor={colors.accent}
          />
        }
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safe: {
    flex: 1,
    backgroundColor: colors.bg,
  },
  header: {
    paddingHorizontal: spacing.xl,
    paddingTop: spacing.lg,
    paddingBottom: spacing.lg,
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-end',
  },
  title: {
    ...typography.display,
  },
  subtitle: {
    ...typography.caption,
    marginTop: 2,
  },
  listContent: {
    paddingHorizontal: spacing.xl,
    paddingBottom: spacing.xxl,
    flexGrow: 1,
  },
});
