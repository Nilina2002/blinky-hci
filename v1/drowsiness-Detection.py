import cv2
import mediapipe as mp
import time
import serial

# ---- Configuration ----
SERIAL_PORT = 'COM13' 
BAUD_RATE = 115200
EYE_THRESHOLD = 0.21   # Adjusted slightly for better sensitivity
SLEEP_TIME = 3         # Seconds eyes must be closed to trigger alarm

# ---- Initialize Serial with Error Handling ----
try:
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    time.sleep(2)  # Wait for Arduino to reset
    print(f"Connected to Arduino on {SERIAL_PORT}")
except Exception as e:
    print(f"Error connecting to Serial: {e}")
    arduino = None

# ---- Mediapipe Setup ----
mp_face_mesh = mp.solutions.face_mesh
face_mesh = mp_face_mesh.FaceMesh(refine_landmarks=True, max_num_faces=1)

# Landmarks for EAR calculation
LEFT_EYE = [33, 160, 158, 133, 153, 144]
RIGHT_EYE = [362, 385, 387, 263, 373, 380]

def eye_aspect_ratio(eye_points, landmarks, w, h):
    # Convert normalized landmarks to pixel coordinates
    pts = [(int(landmarks[p].x * w), int(landmarks[p].y * h)) for p in eye_points]
    
    # EAR Formula: Vertical distance / Horizontal distance
    # We use a simplified version: (p2-p6) / (p1-p4)
    vertical = abs(pts[1][1] - pts[5][1])
    horizontal = abs(pts[0][0] - pts[3][0])
    
    # Avoid division by zero
    if horizontal == 0:
        return 0
    return vertical / horizontal

# ---- State Variables ----
eyes_closed_start = None
is_sleeping = False

cap = cv2.VideoCapture(0)

while cap.isOpened():
    success, frame = cap.read()
    if not success:
        break

    frame = cv2.flip(frame, 1) # Mirror view
    h, w, _ = frame.shape
    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = face_mesh.process(rgb)

    status_text = "Status: Awake"
    color = (0, 255, 0) # Green for awake

    if results.multi_face_landmarks:
        # We only care about the first face detected
        landmarks = results.multi_face_landmarks[0].landmark

        left_ear = eye_aspect_ratio(LEFT_EYE, landmarks, w, h)
        right_ear = eye_aspect_ratio(RIGHT_EYE, landmarks, w, h)
        ear = (left_ear + right_ear) / 2.0

        # Check for Eye Closure
        if ear < EYE_THRESHOLD:
            if eyes_closed_start is None:
                eyes_closed_start = time.time()
            
            elapsed = time.time() - eyes_closed_start
            
            # If closed longer than SLEEP_TIME
            if elapsed >= SLEEP_TIME:
                status_text = "!!! SLEEPING !!!"
                color = (0, 0, 255) # Red for alert
                if not is_sleeping:
                    print("Sending ALARM to Arduino")
                    if arduino: arduino.write(b'1')
                    is_sleeping = True
            else:
                status_text = f"Closing: {elapsed:.1f}s"
                color = (0, 255, 255) # Yellow for warning
        else:
            # Eyes are open
            eyes_closed_start = None
            if is_sleeping:
                print("Student Woke Up")
                if arduino: arduino.write(b'0')
                is_sleeping = False

        # Visual Feedback on Screen
        cv2.putText(frame, f"EAR: {ear:.2f}", (20, 40), cv2.FONT_HERSHEY_DUPLEX, 0.8, color, 2)
        cv2.putText(frame, status_text, (20, 80), cv2.FONT_HERSHEY_DUPLEX, 1, color, 2)

    cv2.imshow("Student Monitor AI", frame)
    
    if cv2.waitKey(1) & 0xFF == 27: # Press 'ESC' to quit
        break

# Clean up
cap.release()
cv2.destroyAllWindows()
if arduino:
    arduino.write(b'0') # Ensure alarm is off on exit
    arduino.close()