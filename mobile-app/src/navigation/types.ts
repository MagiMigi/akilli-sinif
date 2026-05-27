export type RootStackParamList = {
  Setup: { reconfigure?: boolean } | undefined;
  Tabs: undefined;
  Classroom: { id: string };
  Control: { id: string };
  Alert: { id: string };
  Ota: { id?: string };
  Reset: undefined;
};

export type TabParamList = {
  Dashboard: undefined;
  Settings: undefined;
};

declare global {
  namespace ReactNavigation {
    interface RootParamList extends RootStackParamList {}
  }
}
