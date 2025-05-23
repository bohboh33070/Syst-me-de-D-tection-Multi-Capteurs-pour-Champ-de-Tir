import cv2
from yolov5 import YOLOv5
import numpy as np
import time

class BulletDetector:
    def __init__(self):
        # Load YOLOv5 model
        print("Chargement du modèle YOLOv5...")
        self.model = YOLOv5('best.pt')  # Votre modèle entraîné
        self.classes = ['bullet', 'bullet_out']
        self.colors = [(0, 255, 0), (0, 0, 255)]  # Vert pour bullet, Rouge pour bullet_out
        self.camera = None
        self.stream_url = "http://192.168.21.57/stream"  # URL corrigée pour l'ESP32-CAM
        
    def initialize_camera(self):
        """Initialize camera if not already done"""
        if self.camera is None:
            print(f"Tentative de connexion au flux : {self.stream_url}")
            self.camera = cv2.VideoCapture(self.stream_url)
            time.sleep(2)
            if not self.camera.isOpened():
                print("Erreur : Échec de l'ouverture du flux MJPEG")
                return False
            print("Flux MJPEG ouvert avec succès")
        return self.camera.isOpened()
        
    def close_camera(self):
        """Close the camera if open"""
        if self.camera is not None:
            print("Fermeture du flux caméra")
            self.camera.release()
            self.camera = None
            
    def capture_single_frame(self):
        """Capture a single frame from the camera"""
        if not self.initialize_camera():
            print("Erreur : Caméra non initialisée")
            return None
            
        success, frame = self.camera.read()
        if not success:
            print("Erreur : Impossible de capturer un frame")
            return None
        return frame
            
    def process_frame(self, frame, detection_state):
        """Process a single frame for bullet detection"""
        if frame is None:
            print("Erreur : Frame invalide")
            return None
            
        # Perform detection
        results = self.model.predict(frame)
        detections = results.pred[0]
        
        bullet_count = 0
        for det in detections:
            x1, y1, x2, y2, conf, cls = det
            if conf > 0.3:  # Seuil de confiance
                label = f"{self.classes[int(cls)]} {conf:.2f}"
                color = self.colors[int(cls)]
                
                # Dessiner la boîte englobante
                cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), color, 2)
                cv2.putText(frame, label, (int(x1), int(y1) - 10), 
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)
                
                # Compter les détections de balles
                if self.classes[int(cls)] == 'bullet':
                    bullet_count += 1
                    
        # Mettre à jour l'état de détection
        detection_state['bullets_detected'] = bullet_count
        
        # Encoder le frame au format JPG
        ret, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        
        return frame_bytes
            
    def generate_frames(self, detection_state, interval=0.5):
        """Generate frames using successive captures rather than continuous stream"""
        try:
            while detection_state['active']:
                frame = self.capture_single_frame()
                frame_bytes = self.process_frame(frame, detection_state)
                if frame_bytes is not None:
                    yield (b'--frame\r\n'
                           b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
                time.sleep(1)
        finally:
            self.close_camera()
            
    def take_snapshot(self, detection_state):
        """Take a single snapshot and process it"""
        frame = self.capture_single_frame()
        result = self.process_frame(frame, detection_state)
        self.close_camera()
        return result
    
detector = BulletDetector()
detection_state = {'active': True, 'bullets_detected': 0}

if detector.initialize_camera():
    print("Démarrage de la boucle de détection...")
    while True:
        frame = detector.capture_single_frame()
        if frame is None:
            print("Erreur : Impossible de capturer l'image")
            break
        detector.process_frame(frame, detection_state)
        cv2.imshow("Détection des balles", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    detector.close_camera()
    cv2.destroyAllWindows()
else:
    print("Erreur : Impossible d'initialiser la caméra.")
