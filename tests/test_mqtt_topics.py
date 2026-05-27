"""MQTT topic kontrat testleri.

docs/mqtt-topics.md'deki topic semasini ve payload kalibini broker uzerinde
canli round-trip ile dogrular. Test broker'a baglanamiyorsa SKIP eder.
"""
import json
import queue
import threading
import time

import paho.mqtt.client as mqtt
import pytest


def _connect(cfg, client_id):
    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
    c.username_pw_set(cfg["user"], cfg["password"])
    try:
        c.connect(cfg["broker"], cfg["port"], keepalive=10)
    except (ConnectionRefusedError, OSError) as e:
        pytest.skip(f"MQTT broker {cfg['broker']}:{cfg['port']} erisilemiyor: {e}")
    c.loop_start()
    return c


@pytest.fixture
def publisher(mqtt_config):
    c = _connect(mqtt_config, "test-pub")
    yield c
    c.loop_stop()
    c.disconnect()


@pytest.fixture
def subscriber(mqtt_config):
    c = _connect(mqtt_config, "test-sub")
    yield c
    c.loop_stop()
    c.disconnect()


def _collect(client, topic, expected=1, timeout=3.0):
    received = queue.Queue()

    def on_message(_c, _u, msg):
        received.put((msg.topic, msg.payload.decode()))

    client.on_message = on_message
    client.subscribe(topic)
    time.sleep(0.2)  # subscribe propagation

    items = []
    deadline = time.time() + timeout
    while len(items) < expected and time.time() < deadline:
        try:
            items.append(received.get(timeout=deadline - time.time()))
        except queue.Empty:
            break
    return items


@pytest.mark.parametrize(
    "subtopic,payload",
    [
        ("sensors/temperature", {"value": 23.5, "unit": "C", "timestamp": 123}),
        ("sensors/humidity", {"value": 65, "unit": "%", "timestamp": 123}),
        ("sensors/air_quality", {"value": 120, "unit": "ppm", "timestamp": 123}),
        ("sensors/pir", {"detected": True, "timestamp": 123}),
        ("sensors/window", {"open": False, "timestamp": 123}),
        ("sensors/camera", {"person_count": 15, "timestamp": 123}),
    ],
)
def test_sensor_topic_roundtrip(publisher, subscriber, subtopic, payload):
    topic = f"akilli-sinif/test-classroom/{subtopic}"

    def collect():
        return _collect(subscriber, topic, expected=1, timeout=2.0)

    result_holder = {}
    t = threading.Thread(target=lambda: result_holder.setdefault("r", collect()))
    t.start()
    time.sleep(0.3)
    publisher.publish(topic, json.dumps(payload), qos=1)
    t.join(timeout=3.0)

    msgs = result_holder.get("r", [])
    assert len(msgs) == 1, f"Mesaj alinamadi: {topic}"
    assert msgs[0][0] == topic
    assert json.loads(msgs[0][1]) == payload


def test_broadcast_address(publisher, subscriber):
    """`akilli-sinif/all/control/...` broadcast subscribe ile alinabilir."""
    topic = "akilli-sinif/all/control/led"
    payload = {"state": "off"}

    msgs_holder = {}
    t = threading.Thread(
        target=lambda: msgs_holder.setdefault(
            "r", _collect(subscriber, topic, expected=1, timeout=2.0)
        )
    )
    t.start()
    time.sleep(0.3)
    publisher.publish(topic, json.dumps(payload), qos=1)
    t.join(timeout=3.0)

    assert len(msgs_holder.get("r", [])) == 1


def test_wildcard_subscribe_captures_all_sensors(publisher, subscriber):
    """`+/sensors/+` deseni ile coklu sinif coklu sensor yakalanir."""
    wildcard = "akilli-sinif/+/sensors/+"
    topics_payloads = [
        ("akilli-sinif/sinif-1/sensors/temperature", {"value": 21.0}),
        ("akilli-sinif/sinif-2/sensors/humidity", {"value": 70}),
    ]

    msgs_holder = {}
    t = threading.Thread(
        target=lambda: msgs_holder.setdefault(
            "r", _collect(subscriber, wildcard, expected=2, timeout=3.0)
        )
    )
    t.start()
    time.sleep(0.3)
    for topic, payload in topics_payloads:
        publisher.publish(topic, json.dumps(payload), qos=1)
    t.join(timeout=4.0)

    msgs = msgs_holder.get("r", [])
    assert len(msgs) == 2
    received_topics = {m[0] for m in msgs}
    assert received_topics == {t for t, _ in topics_payloads}
