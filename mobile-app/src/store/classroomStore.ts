import { create } from 'zustand';
import {
  ActuatorState,
  AnyReading,
  ClassroomState,
  ConnectionStatus,
  MqttConnectionState,
  OtaStatus,
  RelayState,
  SensorKind,
} from '@/lib/types';

const HISTORY_LIMIT = 120;

interface Store {
  classrooms: Record<string, ClassroomState>;
  brokerState: MqttConnectionState;
  brokerError?: string;

  setBrokerState: (state: MqttConnectionState, error?: string) => void;
  upsertReading: (id: string, reading: AnyReading) => void;
  setConnection: (id: string, status: ConnectionStatus) => void;
  setOta: (id: string, status: OtaStatus) => void;
  setActuator: (id: string, kind: 'led', state: ActuatorState) => void;
  setRelay: (id: string, kind: 'cooling' | 'heating', state: RelayState) => void;
  reset: () => void;

  classroomList: () => ClassroomState[];
}

const blankClassroom = (id: string): ClassroomState => ({
  id,
  sensors: {},
  actuators: {},
  history: {},
});

export const useClassroomStore = create<Store>((set, get) => ({
  classrooms: {},
  brokerState: 'idle',

  setBrokerState: (state, error) => set({ brokerState: state, brokerError: error }),

  upsertReading: (id, reading) =>
    set((s) => {
      const cur = s.classrooms[id] ?? blankClassroom(id);
      const kind = reading.kind as SensorKind;
      const prev = cur.history[kind] ?? [];
      const next = [...prev, reading].slice(-HISTORY_LIMIT);
      return {
        classrooms: {
          ...s.classrooms,
          [id]: {
            ...cur,
            sensors: { ...cur.sensors, [kind]: reading },
            history: { ...cur.history, [kind]: next },
          },
        },
      };
    }),

  setConnection: (id, status) =>
    set((s) => {
      const cur = s.classrooms[id] ?? blankClassroom(id);
      return {
        classrooms: {
          ...s.classrooms,
          [id]: { ...cur, connection: status },
        },
      };
    }),

  setOta: (id, status) =>
    set((s) => {
      const cur = s.classrooms[id] ?? blankClassroom(id);
      return {
        classrooms: { ...s.classrooms, [id]: { ...cur, ota: status } },
      };
    }),

  setActuator: (id, kind, state) =>
    set((s) => {
      const cur = s.classrooms[id] ?? blankClassroom(id);
      return {
        classrooms: {
          ...s.classrooms,
          [id]: {
            ...cur,
            actuators: { ...cur.actuators, [kind]: state },
          },
        },
      };
    }),

  setRelay: (id, kind, state) =>
    set((s) => {
      const cur = s.classrooms[id] ?? blankClassroom(id);
      return {
        classrooms: {
          ...s.classrooms,
          [id]: {
            ...cur,
            actuators: { ...cur.actuators, [kind]: state },
          },
        },
      };
    }),

  reset: () => set({ classrooms: {}, brokerState: 'idle', brokerError: undefined }),

  classroomList: () => {
    const list = Object.values(get().classrooms);
    return list.sort((a, b) => a.id.localeCompare(b.id, 'tr'));
  },
}));
