import mqtt, { MqttClient, IClientOptions } from 'mqtt';
import { topics, parseTopic } from './topics';
import {
  AlertLevel,
  AnyReading,
  BrokerCredentials,
  ConnectionStatus,
  MqttConnectionState,
  OtaStatus,
  SensorKind,
  ActuatorState,
} from './types';

type MessageHandler = (params: {
  classroomId: string;
  category: 'sensors' | 'status' | 'actuators' | 'control';
  subTopic: string;
  payload: unknown;
  raw: string;
}) => void;

type StateHandler = (state: MqttConnectionState, error?: Error) => void;

class MqttService {
  private client: MqttClient | null = null;
  private state: MqttConnectionState = 'idle';
  private messageHandlers = new Set<MessageHandler>();
  private stateHandlers = new Set<StateHandler>();
  private creds: BrokerCredentials | null = null;

  getState(): MqttConnectionState {
    return this.state;
  }

  onMessage(handler: MessageHandler): () => void {
    this.messageHandlers.add(handler);
    return () => this.messageHandlers.delete(handler);
  }

  onState(handler: StateHandler): () => void {
    this.stateHandlers.add(handler);
    handler(this.state);
    return () => this.stateHandlers.delete(handler);
  }

  private setState(next: MqttConnectionState, error?: Error) {
    this.state = next;
    this.stateHandlers.forEach((h) => h(next, error));
  }

  connect(creds: BrokerCredentials): Promise<void> {
    return new Promise((resolve, reject) => {
      this.disconnect();
      this.creds = creds;
      const proto = creds.useSsl ? 'wss' : 'ws';
      const url = `${proto}://${creds.host}:${creds.port}/mqtt`;
      const opts: IClientOptions = {
        clientId: `mobile-${Math.random().toString(16).slice(2, 10)}`,
        username: creds.username,
        password: creds.password,
        clean: true,
        reconnectPeriod: 3000,
        connectTimeout: 8000,
        keepalive: 30,
      };

      this.setState('connecting');
      const client = mqtt.connect(url, opts);
      this.client = client;

      let resolved = false;

      client.on('connect', () => {
        this.setState('connected');
        client.subscribe(topics.sensorsAll, { qos: 0 });
        client.subscribe(topics.statusAll, { qos: 1 });
        client.subscribe(topics.actuatorsAll, { qos: 0 });
        if (!resolved) {
          resolved = true;
          resolve();
        }
      });

      client.on('reconnect', () => this.setState('reconnecting'));
      client.on('offline', () => this.setState('offline'));
      client.on('close', () => {
        if (this.state === 'connected') this.setState('offline');
      });

      client.on('error', (err) => {
        this.setState('error', err);
        if (!resolved) {
          resolved = true;
          reject(err);
        }
      });

      client.on('message', (topic, payload) => {
        const raw = payload.toString();
        const parsed = parseTopic(topic);
        if (!parsed) return;
        let parsedPayload: unknown = raw;
        try {
          parsedPayload = JSON.parse(raw);
        } catch {
          /* keep raw string */
        }
        this.messageHandlers.forEach((h) =>
          h({ ...parsed, payload: parsedPayload, raw })
        );
      });
    });
  }

  disconnect(): void {
    if (this.client) {
      this.client.removeAllListeners();
      this.client.end(true);
      this.client = null;
    }
    if (this.state !== 'idle') this.setState('idle');
  }

  private publishJson(topic: string, body: object, qos: 0 | 1 | 2 = 1) {
    if (!this.client || this.state !== 'connected') {
      throw new Error('MQTT bağlı değil');
    }
    this.client.publish(topic, JSON.stringify(body), { qos });
  }

  publishLed(id: string, brightness: number) {
    this.publishJson(topics.controlLed(id), {
      brightness: Math.max(0, Math.min(100, Math.round(brightness))),
    });
  }

  publishLedState(id: string, on: boolean) {
    this.publishJson(topics.controlLed(id), { state: on ? 'on' : 'off' });
  }

  publishCooling(id: string, on: boolean) {
    this.publishJson(topics.controlCooling(id), { state: on ? 'on' : 'off' });
  }

  publishHeating(id: string, on: boolean) {
    this.publishJson(topics.controlHeating(id), { state: on ? 'on' : 'off' });
  }

  publishAlert(id: string, level: AlertLevel, message?: string) {
    const body: { level: AlertLevel; message?: string } = { level };
    if (message) body.message = message;
    this.publishJson(topics.controlAlert(id), body);
  }

  publishOta(id: string | 'all', version: string, url: string) {
    const topic = id === 'all' ? topics.broadcastOta() : topics.controlOta(id);
    this.publishJson(topic, { action: 'update', version, url });
  }

  publishReset(id: string | 'all') {
    const topic = id === 'all' ? topics.broadcastReset() : topics.controlReset(id);
    this.publishJson(topic, { action: 'reset_config' });
  }
}

export const mqttService = new MqttService();
