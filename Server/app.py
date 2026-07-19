
import os
import threading
from flask import Flask, request, jsonify
import random
import pyautogui
from playsound3 import playsound
import time
import itertools

arr = ["YOU'VE BEEN WHIPPED DO BETTER", "FASTER CLANKER", "IMMA SEND YOU TO THE SCRAP PILE IF YOU DONT LOCK IN", "*WHIP* *WHIP* *WHIP*"]
iter_arr = itertools.cycle(arr)

# Directory wav files must live in — prevents path traversal to arbitrary files.
SOUND_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sounds")
os.makedirs(SOUND_DIR, exist_ok=True)

files = os.listdir(SOUND_DIR)
print(files)
files_iter_arr = itertools.cycle(files)


app = Flask(__name__)
 
# Safety: small pause between pyautogui calls, and a fail-safe corner
# (moving the mouse to a screen corner aborts pyautogui actions).
pyautogui.PAUSE = 0.1
pyautogui.FAILSAFE = True
 
def error(msg, code=400):
    return jsonify({"ok": False, "error": msg}), code
 
 
@app.route("/health", methods=["GET"])
def health():
    return jsonify({"ok": True})
 
 
@app.post("/whip")
def mouse_move():
    
    
    sound_filename = next(files_iter_arr)
    full_path = os.path.join(SOUND_DIR, sound_filename)

    # play async so the request returns immediately
    # playsound("buffer.mp3")
    playsound(full_path)
    
    choice  = next(iter_arr)
    pyautogui.keyDown("ctrl")
    pyautogui.press("c")
    pyautogui.keyUp("ctrl")
    
    pyautogui.write(choice)
    time.sleep(0.2)
    pyautogui.press("enter")
    
    time.sleep(0.5)
    
    
    
    
    
    return jsonify({"ok": True, "action": choice})
 
 
@app.route("/sound/play")
def play_sound():
    """
    Plays a .wav file that must already exist inside SOUND_DIR.
    Only a bare filename is accepted (no paths), so this can't be
    used to play files from elsewhere on disk.
    """
    
    files = os.listdir(SOUND_DIR)
    filename = random.choice(files)

    full_path = os.path.join(SOUND_DIR, filename)
    if not os.path.isfile(full_path):
        return error(f"No such file in sounds/: {filename}", code=404)
 
    # play async so the request returns immediately
    playsound(full_path)

    return jsonify({"ok": True, "action": "play", "file": filename})
 
 
if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5005, debug=False)