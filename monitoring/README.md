# Monitoring, Evaluation, and Metrics

This document describes how the JumpSafe system is evaluated and monitored after deployment.  
The focus is on **system-level behavior in the deployed pipeline**, not only on training metrics.

---

## 1. System Evaluation and Monitoring Overview

The deployed system is monitored using:

- **ESP32 serial logs** (Arduino IDE Serial Monitor)
- **Flask server logs** (terminal output)
- **Feedback log file** (`feedback_log.txt`)

Together, these provide visibility into:

- Model performance on real jump videos
- Runtime behavior (timing, number of frames per video)
- User feedback on prediction quality

The monitoring approach is intentionally lightweight and suitable for an **edge + local server** prototype, rather than a cloud-scale Prometheus/Grafana setup.

---

## 2. Metrics Tracked

### 2.1. Video-Level Classification Accuracy (Field Evaluation)

**Goal:** Measure how often the system correctly classifies a full jump video as GOOD or BAD in real conditions.

**How it is computed:**

1. Record a set of test videos with known labels (GOOD_JUMP / BAD_JUMP) based on expert or user judgment.
2. Run each video through the pipeline (iPhone → Flask → ESP32).
3. On the ESP32 Serial Monitor, record the final **VIDEO SUMMARY** line, for example:

   ```text
   ====================================
   =========== VIDEO SUMMARY ==========
   Frames processed: 12, good frames: 9, bad frames: 3
   VIDEO CLASS: GOOD_JUMP
   ====================================
Compare the predicted VIDEO CLASS with the known true label.

Compute:

Video-level accuracy
=
# correctly classified videos
total # evaluated videos
Video-level accuracy=
total # evaluated videos
# correctly classified videos
	​


This metric is reported in the project report to show real-world performance of the system.

2.2. Runtime Inference Timing

Goal: Ensure the system remains responsive and suitable for near real-time feedback.

The ESP32 prints timing information for each frame:

Timing: DSP 22 ms, inference 1330 ms, anomaly 0 ms


DSP time (ms): Time spent preparing the features (Digital Signal Processing) before inference.

Inference time (ms): Time spent running the model on the ESP32.

Anomaly time (ms): Not used in this project (set to 0).

During evaluation, we monitor:

Typical inference time per frame (e.g., ~1300 ms)

Whether this remains stable across different videos and sessions

This helps verify that the system can keep up with the frame rate and that there are no performance regressions after changes to the model or firmware.

2.3. Frames Per Video and Majority Vote Behavior

For each processed video, the ESP32 reports:

Frames processed: N, good frames: G, bad frames: B
VIDEO CLASS: GOOD_JUMP / BAD_JUMP / TIE


We track:

N (Frames processed): Indicates how many frames contributed to the video decision.

G vs. B: Shows how strong the majority is (e.g., 11–1 vs 6–5).

This acts as a sanity check:

Very low Frames processed may indicate an issue (e.g., very short video, upload problem).

A narrow majority (e.g., 6 good / 5 bad) suggests borderline cases where human review is valuable.

2.4. User Feedback Logs

The iPhone app provides a feedback mechanism after each video:

👍 “Good” (thumbs up)

👎 “Bad” (thumbs down)

Optional text comment

The app sends this to the Flask endpoint /feedback, which logs entries in feedback_log.txt:

2025-11-17T18:23:45.123456  RATING: thumbs_up  COMMENT: great prediction
2025-11-17T18:25:10.654321  RATING: thumbs_down  COMMENT: model missed knee valgus


These logs support:

Qualitative monitoring of model performance

Identifying systematic failure cases to guide future retraining

Evidence for continuous improvement in the project report

3. Drift and Behavior Monitoring

This prototype does not use automated drift detection tools like NannyML or Alibi Detect.
Instead, it uses manual drift checks based on:

Prediction distribution shifts

Over time, we can compare how often the system outputs GOOD vs BAD.

A sudden change (e.g., almost everything becomes BAD) may indicate:

Changes in data collection (different camera, lighting, environment)

Firmware or preprocessing issues

Increased user complaints (thumbs down)

A rise in negative feedback in feedback_log.txt is treated as a signal that:

The model may no longer fit the new data context.

Retraining or threshold adjustment is required.

These qualitative checks are appropriate for a student prototype and align with the project’s scale.

4. How to Collect Monitoring Data
4.1. ESP32 Serial Logs

Open Arduino IDE.

Select the ESP32 Board and correct serial port.

Upload the firmware.

Open Tools → Serial Monitor (115200 baud).

Record:

Per-frame predictions and timing

Video-level summaries

Any error messages (e.g., network issues)

You can copy logs into a text file (e.g., monitoring/esp32_logs_example.txt) for reference and inclusion in the report.

4.2. Flask Server Logs

From the project root, run:

cd src/jumpsafe_server
python3 server.py


The terminal will display:

Frame uploads and frame_counter

Calls to /features/latest, /frame/status, and /video/status

Feedback received from the iPhone app:

[feedback] 2025-11-17T18:23:45.123456 rating='thumbs_up', comment='great prediction'


These logs help:

Debug deployment issues

Confirm that feedback is being captured

Provide evidence of end-to-end behavior in the report

5. Files in This Directory

This monitoring/ directory is intended to contain:

README.md — this documentation file.

(Optional) esp32_logs_example.txt — sample serial monitor log.

(Optional) server_logs_example.txt — sample Flask server output.

(Optional) metrics_example.csv — a small table summarizing:

Video ID

True label

Predicted VIDEO CLASS

Correct / incorrect flag

Inference time (avg per frame)

These files are not strictly required by the system, but they are helpful artifacts for documentation and grading.

6. Not Using Prometheus / Grafana

The professor’s template mentions Prometheus and Grafana for monitoring at production scale.
In this project, we did not use those tools because:

The system runs on a single local server (Flask on MacBook) and an edge microcontroller (ESP32).

The scale is small and interactive.

Text logs and simple metrics are sufficient to monitor performance and behavior.

Instead, monitoring is implemented via:

Serial logs (ESP32)

Terminal logs (Flask)

Lightweight log files (e.g., feedback_log.txt)