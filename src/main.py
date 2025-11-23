"""
main.py – Entry point for the JumpSafe system.

This script starts the Flask server that:
- Receives frames from the iPhone app,
- Converts them to 96x96 grayscale feature vectors,
- Serves features + frame/video status to the ESP32.
"""

from jumpsafe_server.server import app

if __name__ == "__main__":
    
    print("Starting JumpSafe Flask server from main.py on 0.0.0.0:5055")
    app.run(host="0.0.0.0", port=5055, debug=False, use_reloader=False)
