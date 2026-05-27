import {
  AnyReading,
  ConnectionStatus,
  OtaStatus,
  SensorKind,
  ActuatorState,
  RelayState,
} from './types';

const SENSOR_KINDS: SensorKind[] = [
  'temperature',
  'humidity',
  'light',
  'air_quality',
  'pir',
  'window',
  'camera',
];

export function isSensorKind(name: string): name is SensorKind {
  return (SENSOR_KINDS as string[]).includes(name);
}

export function parseSensorPayload(
  kind: SensorKind,
  payload: unknown
): AnyReading | null {
  if (typeof payload !== 'object' || payload === null) return null;
  const p = payload as Record<string, unknown>;
  const ts = typeof p.timestamp === 'number' ? p.timestamp : Date.now();
  const sim = p.sim === true;
  const receivedAt = Date.now();

  if (kind === 'pir') {
    return {
      kind: 'pir',
      detected: !!p.detected,
      timestamp: ts,
      sim,
      receivedAt,
    };
  }
  if (kind === 'window') {
    return {
      kind: 'window',
      open: !!p.open,
      timestamp: ts,
      sim,
      receivedAt,
    };
  }
  if (kind === 'camera') {
    const v = typeof p.person_count === 'number' ? p.person_count : 0;
    return {
      kind: 'camera',
      value: v,
      unit: 'kişi',
      timestamp: ts,
      sim,
      receivedAt,
    };
  }
  const value = typeof p.value === 'number' ? p.value : null;
  if (value === null) return null;
  return {
    kind,
    value,
    unit: typeof p.unit === 'string' ? p.unit : undefined,
    timestamp: ts,
    sim,
    receivedAt,
  };
}

export function parseConnectionPayload(
  classroomId: string,
  payload: unknown
): ConnectionStatus | null {
  if (typeof payload !== 'object' || payload === null) return null;
  const p = payload as Record<string, unknown>;
  if (p.status !== 'online' && p.status !== 'offline') return null;
  return {
    status: p.status,
    device: typeof p.device === 'string' ? p.device : `device-${classroomId}`,
    classroom: typeof p.classroom === 'string' ? p.classroom : classroomId,
    ip: typeof p.ip === 'string' ? p.ip : undefined,
    rssi: typeof p.rssi === 'number' ? p.rssi : undefined,
    uptime: typeof p.uptime === 'number' ? p.uptime : undefined,
    mock_mode: typeof p.mock_mode === 'boolean' ? p.mock_mode : undefined,
    firmware_version:
      typeof p.firmware_version === 'string' ? p.firmware_version : undefined,
    sim_hour: typeof p.sim_hour === 'number' ? p.sim_hour : undefined,
    receivedAt: Date.now(),
  };
}

export function parseOtaPayload(payload: unknown): OtaStatus | null {
  if (typeof payload !== 'object' || payload === null) return null;
  const p = payload as Record<string, unknown>;
  const status = p.status;
  if (
    status !== 'updating' &&
    status !== 'success' &&
    status !== 'up_to_date' &&
    status !== 'failed'
  ) {
    return null;
  }
  return {
    status,
    progress: typeof p.progress === 'number' ? p.progress : undefined,
    current_version:
      typeof p.current_version === 'string' ? p.current_version : undefined,
    target_version:
      typeof p.target_version === 'string' ? p.target_version : undefined,
    error: typeof p.error === 'string' ? p.error : undefined,
    receivedAt: Date.now(),
  };
}

export function parseActuatorPayload(payload: unknown): ActuatorState | null {
  if (typeof payload !== 'object' || payload === null) return null;
  const p = payload as Record<string, unknown>;
  if (typeof p.value !== 'number') return null;
  return {
    value: p.value,
    unit: typeof p.unit === 'string' ? p.unit : undefined,
    timestamp: typeof p.timestamp === 'number' ? p.timestamp : Date.now(),
    receivedAt: Date.now(),
  };
}

export function parseRelayPayload(payload: unknown): RelayState | null {
  if (typeof payload !== 'object' || payload === null) return null;
  const p = payload as Record<string, unknown>;
  let on: boolean;
  if (p.state === 'on' || p.state === 'off') {
    on = p.state === 'on';
  } else if (typeof p.on === 'boolean') {
    on = p.on;
  } else if (typeof p.value === 'number') {
    on = p.value > 0;
  } else {
    return null;
  }
  return {
    on,
    timestamp: typeof p.timestamp === 'number' ? p.timestamp : Date.now(),
    receivedAt: Date.now(),
  };
}
