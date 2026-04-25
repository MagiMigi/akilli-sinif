#!/usr/bin/env python3
"""
Akıllı Sınıf Sistemi - YOLO Kişi Sayma Sunucusu
===============================================

Bu sunucu:
1. ESP32-CAM'den HTTP POST ile JPEG foto alır
2. YOLOv8 ile kişi sayısını tespit eder
3. Sonucu MQTT'ye yayınlar
4. İsteğe bağlı olarak çöp tespiti de yapar

Kullanım:
    python yolo_server.py

Gereksinimler:
    pip install flask ultralytics paho-mqtt opencv-python pillow
"""

import os
import sys
import json
import time
import logging
from datetime import datetime
from io import BytesIO
from functools import wraps

from flask import Flask, request, jsonify
from PIL import Image
import numpy as np

# .env dosyasından ayarları yükle
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass  # dotenv yoksa ortam değişkenlerini doğrudan kullan

# YOLO import
try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    print("UYARI: ultralytics kurulu değil. Mock mod kullanılacak.")

# MQTT import
try:
    import paho.mqtt.client as mqtt
    MQTT_AVAILABLE = True
except ImportError:
    MQTT_AVAILABLE = False
    print("UYARI: paho-mqtt kurulu değil. MQTT devre dışı.")

# ============================================
# YAPILANDIRMA
# ============================================

# Flask ayarları
HOST = "0.0.0.0"
PORT = 5000
DEBUG = False  # Production mode - prevents reloader issues with MQTT

# MQTT ayarları (.env'den oku, yoksa varsayılanı kullan)
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USER = os.getenv("MQTT_USER", "nodered")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
MQTT_BASE_TOPIC = "akilli-sinif"

# API auth anahtarı (.env'den oku) — ZORUNLU. Bos olamaz.
API_KEY = os.getenv("API_KEY", "")
if not API_KEY:
    raise RuntimeError(
        "API_KEY .env'de tanimli olmali. Bos birakmak yetkisiz erisime "
        "izin verir. Rastgele guclu bir string uret: "
        "`python3 -c 'import secrets; print(secrets.token_urlsafe(32))'`"
    )
if len(API_KEY) < 16:
    raise RuntimeError("API_KEY en az 16 karakter olmali.")

# YOLO ayarları
MODEL_PATH = "yolov8n.pt"  # Nano model (en hızlı)
CONFIDENCE_THRESHOLD = 0.45  # Yanlış pozitif azaltmak için artırıldı
PERSON_CLASS_ID = 0  # COCO dataset'te 'person' class ID'si

# Tespit edilecek sınıflar (opsiyonel çöp tespiti için)
DETECT_CLASSES = {
    0: "person",      # Kişi
    # 39: "bottle",   # Şişe (çöp olarak sayılabilir)
    # 41: "cup",      # Bardak
    # 73: "book",     # Kitap
}

# Görüntü kaydetme (debug için)
SAVE_IMAGES = True
SAVE_PATH = "captured_images"
MAX_IMAGES = 200        # Klasörde en fazla bu kadar fotoğraf tutulur
MAX_AGE_DAYS = 7        # Bu günden eski fotoğraflar silinir

# ============================================
# LOGGING AYARLARI
# ============================================

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# ============================================
# FLASK UYGULAMASI
# ============================================

app = Flask(__name__)

# Global değişkenler
model = None
mqtt_client = None
last_person_count = {}  # Her sınıf için son kişi sayısı

# ============================================
# AUTH
# ============================================

def require_api_key(f):
    """
    API anahtarı kontrolü. İstek header'ında X-API-Key olmalı.
    API_KEY startup'ta zorunlu kılındı (boş olamaz).
    """
    @wraps(f)
    def decorated(*args, **kwargs):
        # API_KEY startup'ta zorunlu kilindi — bos olamaz.
        key = request.headers.get("X-API-Key", "")
        if key != API_KEY:
            logger.warning(f"Yetkisiz erişim denemesi: {request.remote_addr}")
            return jsonify({"error": "Yetkisiz erişim"}), 401
        return f(*args, **kwargs)
    return decorated

# ============================================
# DİSK TEMİZLİĞİ
# ============================================

def cleanup_images():
    """
    captured_images klasöründe:
    - MAX_AGE_DAYS günden eski fotoğrafları siler
    - MAX_IMAGES limitini aşıyorsa en eskileri siler
    """
    if not os.path.exists(SAVE_PATH):
        return

    now = time.time()
    max_age_seconds = MAX_AGE_DAYS * 86400

    # Tüm jpg dosyalarını değiştirilme zamanına göre sırala (eskiden yeniye)
    files = sorted(
        [os.path.join(SAVE_PATH, f) for f in os.listdir(SAVE_PATH) if f.endswith(".jpg")],
        key=os.path.getmtime
    )

    deleted = 0

    # Eskiden yeniye tara: yaşlıları sil
    remaining = []
    for fpath in files:
        age = now - os.path.getmtime(fpath)
        if age > max_age_seconds:
            os.remove(fpath)
            deleted += 1
        else:
            remaining.append(fpath)

    # Hâlâ MAX_IMAGES'ı aşıyorsa en eskileri sil
    while len(remaining) > MAX_IMAGES:
        os.remove(remaining.pop(0))
        deleted += 1

    if deleted > 0:
        logger.info(f"Disk temizliği: {deleted} eski fotoğraf silindi, kalan: {len(remaining)}")

# ============================================
# YOLO MODEL YUKLEME
# ============================================

def load_model():
    """YOLO modelini yükle"""
    global model
    
    if not YOLO_AVAILABLE:
        logger.warning("YOLO kullanılamıyor, mock mod aktif")
        return False
    
    try:
        # Model dosyası var mı kontrol et
        if not os.path.exists(MODEL_PATH):
            logger.info(f"Model dosyası bulunamadı, indiriliyor: {MODEL_PATH}")
        
        model = YOLO(MODEL_PATH)
        logger.info(f"YOLO modeli yüklendi: {MODEL_PATH}")
        
        # Warmup - ilk inference yavaş olur
        logger.info("Model warmup yapılıyor...")
        dummy_img = np.zeros((480, 640, 3), dtype=np.uint8)
        model.predict(dummy_img, verbose=False)
        logger.info("Model hazır!")
        
        return True
    except Exception as e:
        logger.error(f"Model yükleme hatası: {e}")
        return False

# ============================================
# MQTT BAGLANTISI
# ============================================

def setup_mqtt():
    """MQTT bağlantısını kur"""
    global mqtt_client
    
    if not MQTT_AVAILABLE:
        logger.warning("MQTT kullanılamıyor")
        return False
    
    try:
        # paho-mqtt v2.x için CallbackAPIVersion kullan
        mqtt_client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id="yolo-server"
        )
        mqtt_client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
        
        def on_connect(client, userdata, flags, reason_code, properties):
            if reason_code == 0:
                logger.info("MQTT broker'a bağlandı")
            else:
                logger.error(f"MQTT bağlantı hatası, kod: {reason_code}")
        
        def on_disconnect(client, userdata, flags, reason_code, properties):
            if reason_code != 0:
                logger.warning(f"MQTT bağlantısı koptu: {reason_code}")
        
        mqtt_client.on_connect = on_connect
        mqtt_client.on_disconnect = on_disconnect
        
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
        
        return True
    except Exception as e:
        logger.error(f"MQTT bağlantı hatası: {e}")
        return False

def publish_mqtt(classroom_id, person_count, detections):
    """Sonuçları MQTT'ye yayınla"""
    if mqtt_client is None or not mqtt_client.is_connected():
        logger.warning("MQTT bağlı değil, mesaj gönderilemedi")
        return
    
    # Kişi sayısı topic'i
    topic = f"{MQTT_BASE_TOPIC}/{classroom_id}/sensors/camera"
    payload = {
        "person_count": person_count,
        "timestamp": int(time.time() * 1000),
        "detections": detections
    }
    
    mqtt_client.publish(topic, json.dumps(payload), qos=1)
    logger.info(f"MQTT yayınlandı: {topic} -> {person_count} kişi")

# ============================================
# GORUNTU ISLEME
# ============================================

def process_image(image_bytes, classroom_id):
    """
    Görüntüyü işle ve kişi sayısını tespit et
    
    Args:
        image_bytes: JPEG formatında görüntü verisi
        classroom_id: Sınıf kimliği
    
    Returns:
        dict: Tespit sonuçları
    """
    try:
        # Bytes'ı PIL Image'a çevir
        image = Image.open(BytesIO(image_bytes))
        
        # RGB'ye çevir (JPEG bazen farklı modda olabilir)
        if image.mode != 'RGB':
            image = image.convert('RGB')
        
        # NumPy array'e çevir
        img_array = np.array(image)
        
        # Görüntüyü kaydet (debug için)
        if SAVE_IMAGES:
            save_image(image, classroom_id)
        
        # YOLO ile tespit
        if model is not None:
            results = model.predict(
                img_array,
                conf=CONFIDENCE_THRESHOLD,
                classes=[PERSON_CLASS_ID],  # Sadece kişi tespit et
                verbose=False
            )
            
            # Sonuçları parse et
            detections = []
            person_count = 0
            
            for result in results:
                boxes = result.boxes
                for box in boxes:
                    cls_id = int(box.cls[0])
                    conf = float(box.conf[0])
                    
                    if cls_id == PERSON_CLASS_ID:
                        person_count += 1
                        detections.append({
                            "class": "person",
                            "confidence": round(conf, 2),
                            "bbox": box.xyxy[0].tolist()
                        })
            
            logger.info(f"[{classroom_id}] Tespit: {person_count} kişi")
            
        else:
            # Mock mod - rastgele kişi sayısı
            import random
            person_count = random.randint(0, 5)
            detections = []
            logger.info(f"[{classroom_id}] Mock tespit: {person_count} kişi")
        
        # MQTT'ye yayınla
        publish_mqtt(classroom_id, person_count, detections)
        
        # Son sayıyı kaydet
        last_person_count[classroom_id] = person_count
        
        return {
            "success": True,
            "person_count": person_count,
            "detections": detections,
            "classroom_id": classroom_id,
            "timestamp": datetime.now().isoformat()
        }
        
    except Exception as e:
        logger.error(f"Görüntü işleme hatası: {e}")
        return {
            "success": False,
            "error": str(e),
            "person_count": 0
        }

def save_image(image, classroom_id):
    """Debug için görüntüyü kaydet, sonra disk temizliği yap"""
    try:
        if not os.path.exists(SAVE_PATH):
            os.makedirs(SAVE_PATH)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"{SAVE_PATH}/{classroom_id}_{timestamp}.jpg"
        image.save(filename, "JPEG", quality=85)
        logger.debug(f"Görüntü kaydedildi: {filename}")

        # Her kayıttan sonra temizlik kontrolü yap
        cleanup_images()
    except Exception as e:
        logger.error(f"Görüntü kaydetme hatası: {e}")

# ============================================
# FLASK ROUTE'LARI
# ============================================

@app.route('/', methods=['GET'])
def index():
    """Ana sayfa - sunucu durumu"""
    return jsonify({
        "service": "Akıllı Sınıf - YOLO Kişi Sayma Sunucusu",
        "status": "running",
        "yolo_model": MODEL_PATH if model else "not loaded",
        "mqtt_connected": mqtt_client.is_connected() if mqtt_client else False,
        "last_counts": last_person_count
    })

@app.route('/analyze', methods=['POST'])
@require_api_key
def analyze():
    """
    ESP32-CAM'den gelen görüntüyü analiz et
    
    Headers:
        X-Classroom-ID: Sınıf kimliği (opsiyonel, varsayılan: sinif-1)
        Content-Type: image/jpeg
    
    Body:
        JPEG görüntü verisi
    """
    try:
        # Sınıf ID'sini al
        classroom_id = request.headers.get('X-Classroom-ID', 'sinif-1')
        
        # Görüntü verisini al
        if request.content_type != 'image/jpeg':
            logger.warning(f"Geçersiz content-type: {request.content_type}")
        
        image_data = request.get_data()
        
        if len(image_data) == 0:
            return jsonify({"success": False, "error": "Boş görüntü"}), 400
        
        logger.info(f"Görüntü alındı: {len(image_data)} bytes, sınıf: {classroom_id}")
        
        # Görüntüyü işle
        result = process_image(image_data, classroom_id)
        
        return jsonify(result)
        
    except Exception as e:
        logger.error(f"Analyze hatası: {e}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/test', methods=['GET'])
@require_api_key
def test():
    """Test endpoint - webcam'den görüntü al ve analiz et"""
    try:
        import cv2
        
        # Webcam'i aç
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            return jsonify({"success": False, "error": "Webcam açılamadı"}), 500
        
        # Bir frame al
        ret, frame = cap.read()
        cap.release()
        
        if not ret:
            return jsonify({"success": False, "error": "Frame alınamadı"}), 500
        
        # JPEG'e çevir
        _, buffer = cv2.imencode('.jpg', frame)
        image_bytes = buffer.tobytes()
        
        # Analiz et
        result = process_image(image_bytes, "test-webcam")
        
        return jsonify(result)
        
    except Exception as e:
        logger.error(f"Test hatası: {e}")
        return jsonify({"success": False, "error": str(e)}), 500

@app.route('/count/<classroom_id>', methods=['GET'])
def get_count(classroom_id):
    """Son kişi sayısını döndür"""
    count = last_person_count.get(classroom_id, 0)
    return jsonify({
        "classroom_id": classroom_id,
        "person_count": count,
        "timestamp": datetime.now().isoformat()
    })

# ============================================
# ANA PROGRAM
# ============================================

def main():
    """Ana program"""
    logger.info("=" * 50)
    logger.info("Akıllı Sınıf - YOLO Kişi Sayma Sunucusu")
    logger.info("=" * 50)
    
    # YOLO modelini yükle
    if not load_model():
        logger.warning("YOLO modeli yüklenemedi, mock mod kullanılacak")
    
    # MQTT bağlantısını kur
    if not setup_mqtt():
        logger.warning("MQTT bağlantısı kurulamadı")
    
    # Flask sunucusunu başlat
    logger.info(f"Sunucu başlatılıyor: http://{HOST}:{PORT}")
    logger.info("ESP32-CAM görüntülerini bekleniyor...")
    logger.info("-" * 50)
    
    app.run(host=HOST, port=PORT, debug=DEBUG, threaded=True)

if __name__ == '__main__':
    main()
