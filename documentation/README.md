# JumpSafe: Real-Time Jump Classification System

JumpSafe is an AI-powered real-time athletic movement evaluation system that classifies jump quality ("good" vs. "bad") using a hybrid deployment architecture involving an iPhone, a local Flask server, and an ESP32 microcontroller running an Edge Impulse model.

This repository contains the full system source, deployment logic, monitoring setup, documentation, and demo materials required for the EGN 6216 AI Systems Project.

---

# 📁 Repository Contents

### **src/**
Core system code.

- **main.py** — System entry point (if running Flask server as main script)
- **jumpsafe_server/server.py** — Flask backend that receives frames, preprocesses images, exposes APIs to ESP32
- **models/** — Trained Edge Impulse model files (uploaded using Git LFS)
- **utils/** — Additional Python utilities (optional)
- **data/** — Sample dataset (also stored using Git LFS)
- **requirements.txt** — Python dependencies

---

### **deployment/**
Configuration and documentation for deployment.

- **Dockerfile** (optional; included for demonstration even if not used)
- **docker-compose.yml** (optional)
- **README.md** — Deployment documentation detailing the environment selection, deployment strategy, and execution steps.

---

### **monitoring/**
Performance monitoring and evaluation materials.

- **metrics.md** — System monitoring plan (latency, inference time, feedback logging)
- **prometheus/** (optional placeholder)
- **grafana/** (optional placeholder)

---

### **documentation/**
All written project deliverables.

- **README.md** — This file
- **AI System Project Proposal Template**
- **Project Report** (to be added upon final submission)

---

### **videos/**
Demonstration videos (stored using Git LFS)

- **system_demo.mp4** — End-to-end demonstration of the JumpSafe system

---

# 🚀 System Overview

JumpSafe performs real-time jump evaluation using:

1. **iPhone App (SwiftUI)**  
   - User selects a recorded jump video.  
   - App extracts frames and uploads them to the Flask server.  
   - User submits optional feedback (thumbs up/down + text comment).

2. **Local Flask Server (MacBook)**  
   - Receives video frames (`/image/upload`).  
   - Converts frames to 96×96 grayscale arrays.  
   - Saves features as `latest_features.bin`.  
   - Tracks frame count and video-end status.  
   - Logs user feedback via `/feedback`.

3. **ESP32 Feather V2 (Edge Model Deployment)**  
   - Polls server for current frame index, video status, and feature buffers.  
   - Runs the Edge Impulse model locally.  
   - Performs majority vote over an entire video.  
   - Displays results on on-board LEDs.  
   - Logs inference results, timing, and feedback timestamps to Serial Monitor.

---

# 🧩 Deployment Documentation

## 1. Deployment Environment Selection

JumpSafe uses a hybrid topology:

### **Local Deployment**
- The Flask server runs on a MacBook.
- Handles preprocessing, feature storage, and API communication.
- Enables fast debugging and controlled experimentation.

### **Edge Deployment**
- ESP32 performs all inference using quantized Edge Impulse firmware.
- Enables low-latency evaluation without cloud dependence.
- Highly suitable for real-time athletic performance feedback.

### **Why this matters**
- Low latency  
- No cloud billing  
- Reliable offline inference  
- Robust performance for athletic use cases  

---

## 2. Deployment Strategy

JumpSafe uses a **coordinated multi-device pipeline**:


### **Workflow**
- 📱 iPhone extracts frames → uploads to server  
- 💻 Flask server → preprocesses → exposes `/features/latest`  
- 🔌 ESP32 → polls server → loads features → predicts  
- 💡 Majority vote → GOOD_JUMP or BAD_JUMP  
- 📝 Feedback (thumbs up/down + comment) logged with timestamp  

### **Tools Used**
- Flask (Python)
- Edge Impulse (ML training + firmware generation)
- Arduino IDE
- SwiftUI (iOS app)
- Git LFS (large file management)

---

## 3. Security & Compliance (Trustworthiness)

- All communication occurs **within a local private network (LAN)**.
- No raw video is stored on the server.
- Only extracted 96×96 grayscale features are saved.
- Feedback logging includes:
  - timestamp (UTC)
  - rating (“up”/“down”)
  - optional text comment
- Model binaries and datasets are stored securely using Git LFS.

---

# 📊 Monitoring & Evaluation

## 1. System Evaluation and Monitoring

### Metrics tracked
| Component | Metric | Purpose |
|----------|--------|---------|
| ESP32 | Inference time (ms) | Real-time model speed |
| Server | Frame upload count | Ensures sequential processing |
| Server | Feedback logs | For continuous improvement |
| Entire System | Video-level accuracy via majority vote | Reduces noise & instability |

### Drift Detection
Manual drift detection via:
- Changes in user feedback
- Increasing misclassifications over time
- Low confidence predictions

(Automated drift tools were unnecessary for system scale.)

---

## 2. Feedback Collection & Continuous Improvement

### Implemented Mechanism
- In-app “👍 / 👎” buttons
- Optional text comment box
- Server stores feedback in:
- ESP32 displays feedback timestamps in Serial Monitor

### Uses
- Detect mislabeled training samples  
- Identify patterns in misclassifications  
- Guide future dataset expansions  

---

## 3. Maintenance & Compliance Audits

### Maintenance Performed
- Periodic retraining on newer jump samples  
- Refreshing Edge Impulse model and firmware  
- Updating Python dependencies  
- Reviewing feedback logs for issues  

### Compliance considerations
- No personally identifiable information collected  
- All data remains local to user devices  
- No cloud services = minimized privacy risk  

---

# ▶️ Running the System

## **Start the Flask Server**

```bash
cd src/jumpsafe_server
python3 server.py
Run the iPhone App

Select a jump video.

App extracts frames automatically.

Frames are uploaded sequentially.

Submit optional feedback at the end.

Run the ESP32

Plug ESP32 Feather V2 into USB.

Flash firmware with Arduino IDE.

Open Serial Monitor → Watch:

inference timing

frame-by-frame prediction

final majority vote

feedback timestamp messages

🐳 Deployment Files

Even though Docker was not required, placeholders are included for completeness:

deployment/Dockerfile

deployment/docker-compose.yml

These document how the system would be containerized for future cloud/edge expansion.

📦 Not Applicable

The following tools were not used because:

System is fully local + edge-based

No cloud compute or multi-node orchestration needed

Not used:

Kubernetes

AWS ECS / Fargate

Auto-scaling

Prometheus/Grafana dashboards (placeholders only)

🎥 Video Demonstration

A full end-to-end demo is included in:
videos/system_demo.mp4

📚 Documentation Links

Project Proposal Template — documentation/AI System project proposal template

Project Report — (to be added at submission time)

🔧 Version Control Practices

GitHub Desktop used for version control

Git LFS enabled for large files (videos, models, datasets)

Meaningful commit messages for traceability

Branching optional due to single-developer workflow