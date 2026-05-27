"""YOLO Flask API kontrat testleri.

Server `localhost:5000`'de calismiyorsa testler SKIP. Model agirligini, GPU'yu
veya kamera donanimini gerektirmez — yalnizca HTTP davranisini kontrol eder.
"""
import pytest
import requests


def _server_up(url):
    try:
        r = requests.get(f"{url}/", timeout=1.5)
        return r.status_code == 200
    except requests.RequestException:
        return False


@pytest.fixture(autouse=True)
def _skip_if_down(yolo_url):
    if not _server_up(yolo_url):
        pytest.skip(f"YOLO server {yolo_url} erisilemiyor")


def test_index_returns_service_info(yolo_url):
    r = requests.get(f"{yolo_url}/", timeout=2)
    assert r.status_code == 200
    body = r.json() if "application/json" in r.headers.get("content-type", "") else None
    if body is not None:
        assert "service" in body or "name" in body or len(body) > 0


def test_status_returns_200(yolo_url):
    r = requests.get(f"{yolo_url}/status", timeout=2)
    assert r.status_code == 200


def test_analyze_requires_api_key(yolo_url):
    """Header API key olmadan POST -> 401 veya 403 dondurmeli."""
    r = requests.post(
        f"{yolo_url}/analyze",
        data=b"\xff\xd8\xff\xe0fake",
        headers={"Content-Type": "image/jpeg"},
        timeout=2,
    )
    assert r.status_code in (401, 403), f"API key kontrolu yok: {r.status_code}"


def test_analyze_rejects_empty_body(yolo_url, api_key):
    r = requests.post(
        f"{yolo_url}/analyze",
        data=b"",
        headers={
            "Content-Type": "image/jpeg",
            "X-API-Key": api_key,
            "X-Classroom-ID": "sinif-1",
        },
        timeout=2,
    )
    assert r.status_code == 400
    body = r.json()
    assert body.get("success") is False
    assert "error" in body


def test_classroom_id_whitelist(yolo_url, api_key):
    """Gecersiz classroom_id (path traversal denemesi) sanitize edilmeli — 200 veya 400, asla 500."""
    r = requests.post(
        f"{yolo_url}/analyze",
        data=b"",
        headers={
            "Content-Type": "image/jpeg",
            "X-API-Key": api_key,
            "X-Classroom-ID": "../../../etc/passwd",
        },
        timeout=2,
    )
    assert r.status_code != 500, "Sanitize yok, sunucu 500 atti"


def test_get_count_endpoint(yolo_url):
    r = requests.get(f"{yolo_url}/count/sinif-1", timeout=2)
    assert r.status_code in (200, 404)  # sinif yoksa 404 kabul
