import os
import pytest


@pytest.fixture(scope="session")
def mqtt_config():
    return {
        "broker": os.getenv("MQTT_BROKER", "localhost"),
        "port": int(os.getenv("MQTT_PORT", "1883")),
        "user": os.getenv("MQTT_USER", "esp32"),
        "password": os.getenv("MQTT_PASS", "akilli321"),
    }


@pytest.fixture(scope="session")
def yolo_url():
    return os.getenv("YOLO_URL", "http://localhost:5000")


@pytest.fixture(scope="session")
def api_key():
    return os.getenv("API_KEY", "ultra-mega-giga-secret-key")
