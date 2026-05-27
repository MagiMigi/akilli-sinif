import { SensorKind } from './types';

export const TOPIC_ROOT = 'akilli-sinif';

export const topics = {
  sensorsAll: `${TOPIC_ROOT}/+/sensors/#`,
  statusAll: `${TOPIC_ROOT}/+/status/#`,
  actuatorsAll: `${TOPIC_ROOT}/+/actuators/#`,

  sensor: (id: string, kind: SensorKind) => `${TOPIC_ROOT}/${id}/sensors/${kind}`,
  connection: (id: string) => `${TOPIC_ROOT}/${id}/status/connection`,
  ota: (id: string) => `${TOPIC_ROOT}/${id}/status/ota`,

  controlLed: (id: string) => `${TOPIC_ROOT}/${id}/control/led`,
  controlCooling: (id: string) => `${TOPIC_ROOT}/${id}/control/cooling`,
  controlHeating: (id: string) => `${TOPIC_ROOT}/${id}/control/heating`,
  controlAlert: (id: string) => `${TOPIC_ROOT}/${id}/control/alert`,
  controlOta: (id: string) => `${TOPIC_ROOT}/${id}/control/ota`,
  controlReset: (id: string) => `${TOPIC_ROOT}/${id}/control/reset`,

  broadcastOta: () => `${TOPIC_ROOT}/all/control/ota`,
  broadcastReset: () => `${TOPIC_ROOT}/all/control/reset`,
};

export interface ParsedTopic {
  classroomId: string;
  category: 'sensors' | 'status' | 'actuators' | 'control';
  subTopic: string;
}

export function parseTopic(topic: string): ParsedTopic | null {
  const parts = topic.split('/');
  if (parts.length < 4) return null;
  if (parts[0] !== TOPIC_ROOT) return null;
  const [, classroomId, category, ...rest] = parts;
  if (
    category !== 'sensors' &&
    category !== 'status' &&
    category !== 'actuators' &&
    category !== 'control'
  ) {
    return null;
  }
  return { classroomId, category, subTopic: rest.join('/') };
}
