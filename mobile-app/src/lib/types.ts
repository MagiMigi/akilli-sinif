export type SensorKind =
  | 'temperature'
  | 'humidity'
  | 'light'
  | 'air_quality'
  | 'pir'
  | 'window'
  | 'camera'
  | 'current'
  | 'power'
  | 'energy';

export type AlertLevel = 'none' | 'info' | 'warning' | 'danger';

export interface SensorReading {
  kind: SensorKind;
  value: number;
  unit?: string;
  timestamp: number;
  receivedAt: number;
  sim?: boolean;
  today?: number; // energy: bugünkü kWh
}

export interface MotionReading {
  kind: 'pir';
  detected: boolean;
  timestamp: number;
  receivedAt: number;
  sim?: boolean;
}

export interface WindowReading {
  kind: 'window';
  open: boolean;
  timestamp: number;
  receivedAt: number;
  sim?: boolean;
}

export type AnyReading = SensorReading | MotionReading | WindowReading;

export interface ConnectionStatus {
  status: 'online' | 'offline';
  device: string;
  classroom: string;
  ip?: string;
  rssi?: number;
  uptime?: number;
  mock_mode?: boolean;
  firmware_version?: string;
  sim_hour?: number;
  receivedAt: number;
}

export interface ActuatorState {
  value: number;
  unit?: string;
  timestamp: number;
  receivedAt: number;
}

export interface RelayState {
  on: boolean;
  timestamp: number;
  receivedAt: number;
}

export interface OtaStatus {
  status: 'updating' | 'success' | 'up_to_date' | 'failed';
  progress?: number;
  current_version?: string;
  target_version?: string;
  error?: string;
  receivedAt: number;
}

export interface BrokerCredentials {
  host: string;
  port: number;
  username: string;
  password: string;
  useSsl: boolean;
}

export interface ClassroomState {
  id: string;
  sensors: Partial<Record<SensorKind, AnyReading>>;
  actuators: { led?: ActuatorState; cooling?: RelayState; heating?: RelayState };
  connection?: ConnectionStatus;
  ota?: OtaStatus;
  history: Partial<Record<SensorKind, AnyReading[]>>;
}

export type MqttConnectionState =
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'reconnecting'
  | 'error'
  | 'offline';
