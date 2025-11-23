Deployment Overview

This document describes the deployment strategy used for the JumpSafe system, including the environment selection, tools, and the overall operational architecture.

1. Deployment Environment Selection

For this project, the deployment environment combines:

Local Deployment (MacBook + Flask server)

The Flask server receives video frames or extracted features from the iPhone app.

The server preprocesses frames into 96×96 grayscale arrays and provides them to the ESP32.

Ideal for development, debugging, and rapid iteration.

Edge Deployment (ESP32 Feather V2)

The trained Edge Impulse model runs directly on the ESP32 microcontroller.

Provides real-time, low-latency classification of jump quality.

Reduces reliance on cloud compute and supports offline functionality.

Why this environment was chosen

Low latency required for real-time athletic feedback.

Lightweight model compatible with microcontroller inference.

Cost-effective (no cloud compute needed).

Avoids network latency and cloud dependency.

2. Deployment Strategy

JumpSafe uses a hybrid coordination architecture:

Workflow: iPhone → Flask Server → ESP32
iPhone App

User selects a jump video.

App extracts frames (96×96 grayscale) and uploads them using:

POST /image/upload

POST /video/end

Flask Server

Accepts frame uploads.

Converts them into float32 features.

Stores features in latest_features.bin.

Tracks:

/frame/status

/video/status

Provides features to ESP32 via /features/latest.

ESP32 Feather V2

Polls the Flask endpoints.

Runs the Edge Impulse model locally for inference.

Performs majority voting to classify the entire video as:

GOOD_JUMP

BAD_JUMP

Prints inference details + decisions via Arduino Serial Monitor.

Tools Used

Flask (Python) for backend + REST API.

SwiftUI / PhotosPicker for iPhone video selection and frame extraction.

Edge Impulse for dataset creation, model training, and firmware generation.

Arduino IDE for microcontroller firmware upload.

Git LFS for storing large data files (.npy, .zip, .mp4).

How this strategy supports goals

Reliability: ESP32 never blocks; server uses clear one-shot video status signals.

Scalability: Multiple devices can upload videos to the same backend.

Maintainability: Updating server or firmware independently is simple.

3. Security and Compliance in Deployment
Security Measures Implemented

Local-only communication within a closed LAN (iPhone ↔ MacBook ↔ ESP32).

No raw video stored; only numerical feature arrays.

Feedback logs include only:

timestamp

thumbs up/down indicator

optional comment

Flask server runs as a non-root user.

Git LFS prevents model/data corruption.

Trustworthiness Strategies

Majority vote across frames stabilizes predictions.

Server logs allow tracking unexpected behavior.

Data protection: no personal or biometric data stored.

On-device inference ensures user performance remains private.

4. Running the System
Start the Flask Server

From the project root:

cd src/jumpsafe_server
python3 server.py

Upload a Video Using the iPhone App

Select a jump video in the app.

The app extracts frames automatically.

Each frame is uploaded to the Flask server.

When the video ends, /video/end is sent.

Run the ESP32

Plug in ESP32 via USB.

Open Arduino IDE → load jumpsafe_esp32.ino.

Flash the firmware.

Open Serial Monitor to observe:

inference per frame

GOOD/BAD decisions

running vote counts

final majority-vote video classification

timestamps of any user feedback received

5. Deployment Files

This directory contains:

Dockerfile — optional (for future containerized deployment)

docker-compose.yml — optional

README.md — this documentation

Because this project runs on:

a local Flask server, and

an edge microcontroller (ESP32),

deployment is currently performed manually, not via Docker/Kubernetes.

6. Not Applicable Items

The following were not used, as the system architecture does not require cloud-scale orchestration:

Kubernetes

AWS ECS / AWS Fargate

Cloud auto-scaling

Container orchestration systems

These tools were unnecessary because the project relies entirely on local networking + microcontroller inference.