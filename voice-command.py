import speech_recognition as sr
import requests

ESP32_IP = "192.168.8.106"

r = sr.Recognizer()
mic = sr.Microphone()

print("Listening for 'hello'...")

while True:
    with mic as source:
        r.adjust_for_ambient_noise(source)
        audio = r.listen(source)

    try:
        text = r.recognize_google(audio).lower()
        print("Heard:", text)

        if "hello" in text:
            requests.get(f"http://{ESP32_IP}/on")
            print("LED ON (wireless)")

    except:
        pass
