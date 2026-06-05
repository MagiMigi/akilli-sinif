import { mqttService } from '@/lib/mqtt';
import {
  isSensorKind,
  parseActuatorPayload,
  parseConnectionPayload,
  parseOtaPayload,
  parseRelayPayload,
  parseSensorPayload,
} from '@/lib/parsers';
import { useClassroomStore } from './classroomStore';

let unsubMessage: (() => void) | null = null;
let unsubState: (() => void) | null = null;

export function wireMqttToStore(): void {
  if (unsubMessage) return;

  unsubState = mqttService.onState((state, error) => {
    useClassroomStore
      .getState()
      .setBrokerState(state, error ? error.message : undefined);
  });

  unsubMessage = mqttService.onMessage(({ classroomId, category, subTopic, payload }) => {
    const store = useClassroomStore.getState();

    if (category === 'sensors' && isSensorKind(subTopic)) {
      const reading = parseSensorPayload(subTopic, payload);
      if (reading) store.upsertReading(classroomId, reading);
      return;
    }

    if (category === 'status') {
      if (subTopic === 'connection') {
        const conn = parseConnectionPayload(classroomId, payload);
        if (conn) store.setConnection(classroomId, conn);
        return;
      }
      if (subTopic === 'ota') {
        const ota = parseOtaPayload(payload);
        if (ota) store.setOta(classroomId, ota);
        return;
      }
      // Firmware role/LED durumunu status/* altinda (retained) yayar
      if (subTopic === 'cooling' || subTopic === 'heating') {
        const relay = parseRelayPayload(payload);
        if (relay) store.setRelay(classroomId, subTopic, relay);
        return;
      }
      if (subTopic === 'led') {
        const act = parseActuatorPayload(payload);
        if (act) store.setActuator(classroomId, 'led', act);
        return;
      }
    }

    if (category === 'actuators') {
      if (subTopic === 'led') {
        const act = parseActuatorPayload(payload);
        if (act) store.setActuator(classroomId, 'led', act);
      } else if (subTopic === 'cooling' || subTopic === 'heating') {
        const relay = parseRelayPayload(payload);
        if (relay) store.setRelay(classroomId, subTopic, relay);
      }
    }
  });
}

export function unwireMqtt(): void {
  unsubMessage?.();
  unsubState?.();
  unsubMessage = null;
  unsubState = null;
}
