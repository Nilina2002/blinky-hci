# Drowsiness Detection System

This project implements a real-time drowsiness detection system using computer vision and an optional Arduino integration for physical alerts. The system monitors a user's eyes via a webcam and triggers an alarm if signs of drowsiness (prolonged eye closure) are detected.

## Features

*   Real-time eye aspect ratio (EAR) calculation.
*   Drowsiness detection based on configurable eye closure duration.
*   Visual feedback on screen (EAR value, status: Awake, Closing, Sleeping).
*   Optional serial communication with an Arduino for external alerts (e.g., buzzer, LED).

## Prerequisites

Before running the application, ensure you have the following installed:

*   Python 3.x
*   A webcam

### Python Dependencies

Install the required Python libraries using pip:

```bash
pip install opencv-python mediapipe pyserial
```

### Hardware (Optional)

For physical alerts, you will need:

*   An Arduino board
*   A serial connection to your computer (e.g., USB cable)
*   An alarm mechanism connected to the Arduino (e.g., an LED or buzzer)

The Arduino should be programmed to listen for serial input:
*   Receiving `'1'` should activate the alarm.
*   Receiving `'0'` should deactivate the alarm.

The default serial port is `COM13` and the baud rate is `115200`. You might need to adjust `SERIAL_PORT` in `drowsiness-Detection.py` to match your Arduino's port.

## How to Run

1.  **Clone the repository (if applicable) or navigate to the project directory.**
2.  **Ensure your webcam is connected.**
3.  **If using Arduino, connect it and upload the appropriate sketch.**
4.  **Run the main Python script:**

    ```bash
    python drowsiness-Detection.py
    ```

5.  **To exit the application, press the `ESC` key.**

## Configuration

You can adjust the following parameters in `drowsiness-Detection.py` to fine-tune the detection:

*   `SERIAL_PORT`: The serial port connected to your Arduino (e.g., `'COM13'`).
*   `BAUD_RATE`: Baud rate for serial communication (default: `115200`).
*   `EYE_THRESHOLD`: A float value representing the Eye Aspect Ratio below which eyes are considered closed (default: `0.21`).
*   `SLEEP_TIME`: The duration in seconds that eyes must remain closed to trigger the drowsiness alarm (default: `3`).

## File Structure

*   `drowsiness-Detection.py`: The main script for drowsiness detection.
*   `README.md`: This file.

## Troubleshooting

*   **"Error connecting to Serial"**:
    *   Ensure your Arduino is connected and the correct `SERIAL_PORT` is specified in `drowsiness-Detection.py`.
    *   Check if the Arduino IDE's Serial Monitor is closed, as it might be using the port.
    *   Verify that the necessary drivers for your Arduino are installed.
*   **Webcam not detected**:
    *   Ensure your webcam is properly connected and not in use by another application.
    *   If you have multiple webcams, you might need to change the `cv2.VideoCapture(0)` parameter to `1`, `2`, etc., to select the correct camera.
