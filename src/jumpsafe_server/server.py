from flask import Flask, request, jsonify, send_file
from PIL import Image
import numpy as np
import io
import os
import sys
from datetime import datetime

app = Flask(__name__)

FEATURES_PATH = "latest_features.bin"
FEEDBACK_LOG_PATH = "feedback_log.txt"

video_done = False       # global flag for end-of-video
frame_counter = 0        # counts how many frames have been uploaded total

# For sending latest feedback to ESP32
latest_feedback_id = 0
latest_feedback = {
    "timestamp": "",
    "rating": "",
    "comment": ""
}


@app.route("/features/upload", methods=["POST"])
def upload_features():
    data = request.get_json()
    if not data or "features" not in data:
        return jsonify({"error": "Missing 'features'"}), 400

    feats = np.array(data["features"], dtype=np.float32)
    if feats.shape[0] != 9216:
        return jsonify({"error": "Expected 9216 floats"}), 400

    feats.tofile(FEATURES_PATH)
    print("[features/upload] Got JSON features, wrote to", FEATURES_PATH)
    sys.stdout.flush()
    return jsonify({"status": "ok"}), 200


@app.route("/image/upload", methods=["POST"])
def upload_image():
    # We do NOT touch video_done here; /video/end controls that.
    global frame_counter

    img_bytes = None

    if request.data:
        img_bytes = request.data
    elif "file" in request.files:
        img_bytes = request.files["file"].read()

    if not img_bytes:
        return jsonify({"error": "no image data"}), 400

    try:
        img = Image.open(io.BytesIO(img_bytes))
        img = img.convert("L")
        img = img.resize((96, 96))

        arr = np.array(img).astype(np.float32) / 255.0
        features = arr.flatten()  # length 9216

        frame_counter += 1  # new frame uploaded
        print("[image/upload] Image shape:", arr.shape,
              "features length:", features.shape[0],
              "frame_counter:", frame_counter)
        sys.stdout.flush()

        features.tofile(FEATURES_PATH)
        return jsonify({"status": "ok", "len": int(features.shape[0])}), 200
    except Exception as e:
        print("[image/upload] Error handling image:", e)
        sys.stdout.flush()
        return jsonify({"error": str(e)}), 500


@app.route("/features/latest", methods=["GET"])
def latest_features():
    if not os.path.exists(FEATURES_PATH):
        print("[features/latest] No features yet")
        sys.stdout.flush()
        return "No features yet", 404

    print("[features/latest] Sending", FEATURES_PATH)
    sys.stdout.flush()
    return send_file(
        FEATURES_PATH,
        mimetype="application/octet-stream",
        as_attachment=False
    )


@app.route("/frame/status", methods=["GET"])
def frame_status():
    """Return the current frame_counter as plain text."""
    global frame_counter
    return (str(frame_counter), 200, {"Content-Type": "text/plain"})


@app.route("/video/end", methods=["POST"])
def video_end():
    global video_done
    video_done = True
    print("[video/end] Received: setting video_done = True")
    sys.stdout.flush()
    return jsonify({"status": "ok"}), 200


@app.route("/video/status", methods=["GET"])
def video_status():
    """
    Return plain "1" *once* when a video has finished,
    then reset to "0" so the next video can be detected cleanly.
    """
    global video_done
    print(f"[video/status] Called. Current video_done = {video_done}")
    if video_done:
        # One-shot: report "1" once, then reset for next video
        video_done = False
        print("[video/status] Returning '1' and resetting video_done to False")
        sys.stdout.flush()
        return ("1", 200, {"Content-Type": "text/plain"})
    else:
        print("[video/status] Returning '0'")
        sys.stdout.flush()
        return ("0", 200, {"Content-Type": "text/plain"})


@app.route("/feedback", methods=["POST"])
def feedback():
    """
    Receive thumbs up/down or comment feedback from the iPhone app.
    Also store it so the ESP32 can pull the latest feedback.
    """
    global latest_feedback_id, latest_feedback

    data = request.get_json(silent=True) or {}
    rating = data.get("rating", "")
    comment = data.get("comment", "")

    timestamp = datetime.utcnow().isoformat()

    print(f"[feedback] {timestamp} rating={rating!r}, comment={comment!r}")
    sys.stdout.flush()

    # Store latest feedback in memory
    latest_feedback_id += 1
    latest_feedback = {
        "timestamp": timestamp,
        "rating": rating,
        "comment": comment
    }

    # Also append to log file
    try:
        with open(FEEDBACK_LOG_PATH, "a", encoding="utf-8") as f:
            f.write(f"{timestamp}\tRATING: {rating}\tCOMMENT: {comment}\n")
    except Exception as e:
        print("[feedback] Error writing feedback log:", e)
        sys.stdout.flush()

    return jsonify({"status": "ok"}), 200


@app.route("/feedback/latest", methods=["GET"])
def feedback_latest():
    """
    Simple text endpoint for the ESP32:
    Returns "none" if no feedback yet, otherwise:
    "<id>\\t<timestamp>\\t<rating>\\t<comment>"
    """
    global latest_feedback_id, latest_feedback

    if latest_feedback_id == 0:
        return ("none", 200, {"Content-Type": "text/plain"})

    line = f"{latest_feedback_id}\t{latest_feedback['timestamp']}\t{latest_feedback['rating']}\t{latest_feedback['comment']}\n"
    return (line, 200, {"Content-Type": "text/plain"})


if __name__ == "__main__":
    print("Starting JumpSafe Flask server on 0.0.0.0:5055 (NO DEBUG, NO RELOADER)")
    sys.stdout.flush()
    app.run(host="0.0.0.0", port=5055, debug=False, use_reloader=False)
