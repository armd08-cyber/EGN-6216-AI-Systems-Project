/* Edge Impulse ingestion SDK + WiFi receiver example
 *
 * Board: Adafruit Feather ESP32 V2
 */

#include <WiFi.h>
#include <JumpSafe_inferencing.h>   // your Edge Impulse project
#include <ctype.h>

/******************** LEDs *************************/
// Optional: LEDs to show GOOD / BAD
const int GOOD_LED_PIN = LED_BUILTIN;  // on Feather ESP32 V2 this is pin 13
const int BAD_LED_PIN  = 12;

/***************** Majority vote *******************/
int frameCounter    = 0;   // frames classified in current video
int videoGoodCount  = 0;
int videoBadCount   = 0;

// For response-time measurement
unsigned long videoStartTime = 0;
bool videoInProgress = false;

// Track server frame counter so we only process NEW frames
unsigned long lastFrameCounterServer = 0;

// Track latest feedback id so we only print new feedback once
unsigned long lastFeedbackId = 0;

/********* WIFI + SERVER CONFIG *********/
const char* WIFI_SSID     = "GunaWifi";         // <-- update for new location
const char* WIFI_PASSWORD = "braverock639";    // <-- update if needed

const char* SERVER_HOST       = "192.168.86.246";  // <-- update to Mac's IP
const uint16_t SERVER_PORT    = 5055;
const char* FEATURES_PATH     = "/features/latest";
const char* VIDEO_STATUS_PATH = "/video/status";
const char* FRAME_STATUS_PATH = "/frame/status";
const char* FEEDBACK_STATUS_PATH = "/feedback/latest";
/**************************************************/

// This is the feature buffer that will be filled from WiFi.
// Size MUST match what the EI library expects.
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

/**
 * @brief  Copy raw feature data into out_ptr
 *         Called by the Edge Impulse library.
 */
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void print_inference_result(ei_impulse_result_t result);

/********* WIFI HELPERS *********/

void connect_wifi() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 60) { // ~30s timeout
        delay(500);
        Serial.print(".");
        retries++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected, IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connect FAILED");
    }
}

/**
 * @brief Read the HTTP body into buffer, skipping headers robustly.
 * Returns true if we got at least 1 byte in the body.
 */
bool read_http_body(WiFiClient &client, char *buffer, size_t bufferSize) {
    client.setTimeout(2000);

    // Skip headers: look for "\r\n\r\n"
    if (!client.find("\r\n\r\n")) {
        Serial.println("Did not find end of headers.");
        return false;
    }

    // Read body
    size_t idx = 0;
    unsigned long start = millis();
    while (client.connected() && (millis() - start < 2000) && idx < bufferSize - 1) {
        while (client.available() && idx < bufferSize - 1) {
            int c = client.read();
            if (c < 0) break;
            buffer[idx++] = (char)c;
        }
    }
    buffer[idx] = '\0';

    if (idx == 0) {
        Serial.println("No data after headers (body empty).");
        return false;
    }

    return true;
}

/**
 * @brief Fetch one full feature buffer from the server via HTTP.
 * Expects the server to return *raw binary floats* after HTTP headers.
 */
bool fetch_features_from_server() {
    WiFiClient client;

    Serial.print("Connecting to server ");
    Serial.print(SERVER_HOST);
    Serial.print(":");
    Serial.println(SERVER_PORT);

    if (!client.connect(SERVER_HOST, SERVER_PORT)) {
        Serial.println("Connection failed");
        return false;
    }

    // Simple HTTP GET
    client.print(String("GET ") + FEATURES_PATH + " HTTP/1.1\r\n" +
                 "Host: " + SERVER_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");

    client.setTimeout(5000);
    if (!client.find("\r\n\r\n")) {
        Serial.println("No end-of-headers for features, aborting.");
        client.stop();
        return false;
    }

    // Read exactly sizeof(features) bytes into our float buffer
    size_t bytes_needed = sizeof(features);
    uint8_t* dst = reinterpret_cast<uint8_t*>(features);
    size_t bytes_read = 0;
    unsigned long start = millis();

    while (bytes_read < bytes_needed && (millis() - start < 5000)) {
        if (client.available()) {
            int n = client.read(dst + bytes_read, bytes_needed - bytes_read);
            if (n > 0) {
                bytes_read += n;
            }
        } else {
            delay(1);
        }
    }

    client.stop();

    if (bytes_read != bytes_needed) {
        Serial.print("ERROR: expected ");
        Serial.print(bytes_needed);
        Serial.print(" bytes, got ");
        Serial.println(bytes_read);
        return false;
    }

    Serial.println("Feature buffer received from server.");
    return true;
}

/**
 * @brief Query server for current frame_counter.
 * Returns 0 if something goes wrong.
 */
unsigned long get_frame_counter_from_server() {
    WiFiClient client;

    Serial.print("Checking frame status at ");
    Serial.print(SERVER_HOST);
    Serial.print(":");
    Serial.println(SERVER_PORT);

    if (!client.connect(SERVER_HOST, SERVER_PORT)) {
        Serial.println("Frame status connection failed");
        return 0;
    }

    client.print(String("GET ") + FRAME_STATUS_PATH + " HTTP/1.1\r\n" +
                 "Host: " + SERVER_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");

    char body[32];
    if (!read_http_body(client, body, sizeof(body))) {
        Serial.println("Failed to read frame status body.");
        client.stop();
        return 0;
    }

    client.stop();

    Serial.print("Frame status body: '");
    Serial.print(body);
    Serial.println("'");

    // Convert to unsigned long: look for the first digit sequence
    char *p = body;
    while (*p && !isdigit((unsigned char)*p)) p++;
    if (!*p) {
        Serial.println("No digits in frame status body.");
        return 0;
    }

    unsigned long val = strtoul(p, nullptr, 10);
    return val;
}

/**
 * @brief Check if the server says the video is finished.
 * Server returns plain "0" or "1".
 */
bool check_video_done() {
    WiFiClient client;

    Serial.print("Checking video status at ");
    Serial.print(SERVER_HOST);
    Serial.print(":");
    Serial.println(SERVER_PORT);

    if (!client.connect(SERVER_HOST, SERVER_PORT)) {
        Serial.println("Video status connection failed");
        return false;
    }

    client.print(String("GET ") + VIDEO_STATUS_PATH + " HTTP/1.1\r\n" +
                 "Host: " + SERVER_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");

    char body[16];
    if (!read_http_body(client, body, sizeof(body))) {
        Serial.println("Failed to read video status body, assuming not done.");
        client.stop();
        return false;
    }

    client.stop();

    Serial.print("Video status body: '");
    Serial.print(body);
    Serial.println("'");

    // If first non-space char is '1', video is done
    for (size_t i = 0; body[i] != '\0'; i++) {
        char c = body[i];
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        return (c == '1');
    }

    return false;
}

/**
 * @brief Check latest feedback from server and print it once when new.
 * Server returns "none" or:
 *   "<id>\\t<timestamp>\\t<rating>\\t<comment>"
 */
void check_feedback_from_server() {
    WiFiClient client;

    Serial.print("Checking feedback at ");
    Serial.print(SERVER_HOST);
    Serial.print(":");
    Serial.println(SERVER_PORT);

    if (!client.connect(SERVER_HOST, SERVER_PORT)) {
        Serial.println("Feedback connection failed");
        return;
    }

    client.print(String("GET ") + FEEDBACK_STATUS_PATH + " HTTP/1.1\r\n" +
                 "Host: " + SERVER_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");

    char body[256];
    if (!read_http_body(client, body, sizeof(body))) {
        Serial.println("Failed to read feedback body.");
        client.stop();
        return;
    }

    client.stop();

    Serial.print("Feedback body: '");
    Serial.print(body);
    Serial.println("'");

    if (strncmp(body, "none", 4) == 0) {
        // No feedback yet
        return;
    }

    // Parse "<id>\t<timestamp>\t<rating>\t<comment>"
    char *saveptr;
    char *token = strtok_r(body, "\t\n", &saveptr); // id
    if (!token) return;
    unsigned long id = strtoul(token, nullptr, 10);
    if (id == 0) return;

    // If already seen, ignore
    if (id == lastFeedbackId) {
        return;
    }

    char *timestamp = strtok_r(nullptr, "\t\n", &saveptr);
    char *rating    = strtok_r(nullptr, "\t\n", &saveptr);
    char *comment   = strtok_r(nullptr, "\n",   &saveptr);

    if (!timestamp) timestamp = (char *)"";
    if (!rating)    rating    = (char *)"";
    if (!comment)   comment   = (char *)"";

    lastFeedbackId = id;

    Serial.println("------ NEW FEEDBACK RECEIVED ------");
    Serial.printf("Feedback ID: %lu\n", id);
    Serial.printf("Time (UTC): %s\n", timestamp);
    Serial.printf("Rating: %s\n", rating);
    Serial.printf("Comment: %s\n", comment);
    Serial.println("-----------------------------------");
}

/********* ARDUINO LIFECYCLE *********/

void setup()
{
    Serial.begin(115200);
    while (!Serial) { ; }  // wait for USB serial

    Serial.println("Edge Impulse WiFi inferencing demo (Feather ESP32 V2)");

    pinMode(GOOD_LED_PIN, OUTPUT);
    pinMode(BAD_LED_PIN, OUTPUT);

    // Start with LEDs off
    digitalWrite(GOOD_LED_PIN, LOW);
    digitalWrite(BAD_LED_PIN, LOW);

    connect_wifi();

    Serial.print("EI expects DSP input size = ");
    Serial.println(EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
}

/**
 * @brief      Arduino main function
 */
void loop()
{
    // 0) Check if there is a NEW frame from the server
    unsigned long currentFrameCounter = get_frame_counter_from_server();

    if (currentFrameCounter == 0) {
        // Probably no frames yet, or couldn't read
        check_feedback_from_server();  // still allow feedback to show up
        delay(1000);
        return;
    }

    if (currentFrameCounter == lastFrameCounterServer) {
        // No new frame since last loop; nothing to classify
        Serial.println("No new frame yet, skipping classification.");

        check_feedback_from_server();  // poll feedback even when idle

        // Still check if video has ended (in case we missed it)
        if (check_video_done()) {
            ei_printf("\n\n====================================\n");
            ei_printf("=========== VIDEO SUMMARY ==========\n");
            ei_printf("Frames processed: %d, good frames: %d, bad frames: %d\n",
                      frameCounter, videoGoodCount, videoBadCount);

            if (videoGoodCount > videoBadCount) {
                ei_printf("VIDEO CLASS: GOOD_JUMP\n");
            } else if (videoBadCount > videoGoodCount) {
                ei_printf("VIDEO CLASS: BAD_JUMP\n");
            } else {
                ei_printf("VIDEO CLASS: TIE (equal good/bad)\n");
            }

            // Response time: from first frame to summary
            if (videoInProgress) {
                unsigned long responseMs = millis() - videoStartTime;
                ei_printf("Response time: %lu ms from first frame to summary\n",
                          responseMs);
                videoInProgress = false;
            }

            ei_printf("====================================\n\n");

            // Reset for next video
            frameCounter    = 0;
            videoGoodCount  = 0;
            videoBadCount   = 0;

            ei_printf("Ready for next video.\n");
        }

        delay(1000);
        return;
    }

    // New frame detected
    lastFrameCounterServer = currentFrameCounter;

    // If this is the first frame of a new video, start timer
    if (frameCounter == 0) {
        videoStartTime = millis();
        videoInProgress = true;
        Serial.println("Starting timer for new video.");
    }

    ei_printf("\n\n--- New inference cycle (frame %lu) ---\n", lastFrameCounterServer);

    // 1) Get features over WiFi
    if (!fetch_features_from_server()) {
        ei_printf("Failed to get features from server, retrying...\n");
        delay(1000);
        return;
    }

    // 2) Sanity check
    if (sizeof(features) / sizeof(float) != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        ei_printf("Size mismatch: features[%lu] vs expected %lu\n",
                  sizeof(features) / sizeof(float),
                  (unsigned long)EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
        delay(1000);
        return;
    }

    // 3) Wrap as signal and invoke classifier
    ei_impulse_result_t result = { 0 };
    signal_t features_signal;
    features_signal.total_length = sizeof(features) / sizeof(features[0]);
    features_signal.get_data = &raw_feature_get_data;

    EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", res);
        delay(1000);
        return;
    }

    ei_printf("run_classifier returned: %d\r\n", res);
    print_inference_result(result);

    // 4) Interpret result: decide GOOD vs BAD
    float bad = 0.0f;
    float good = 0.0f;

    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        const char *label = ei_classifier_inferencing_categories[i];
        float value = result.classification[i].value;

        if (strcmp(label, "bad_jump") == 0) {
            bad = value;
        }
        else if (strcmp(label, "good_jump") == 0) {
            good = value;
        }
    }

    ei_printf("Decision: ");
    if (good > bad) {
        ei_printf("GOOD JUMP! (good=%.3f, bad=%.3f)\r\n", good, bad);
        digitalWrite(GOOD_LED_PIN, HIGH);
        digitalWrite(BAD_LED_PIN, LOW);
    } else {
        ei_printf("BAD JUMP! (good=%.3f, bad=%.3f)\r\n", good, bad);
        digitalWrite(GOOD_LED_PIN, LOW);
        digitalWrite(BAD_LED_PIN, HIGH);
    }

    // ===== Majority vote for the whole video =====
    frameCounter++;

    if (good > bad) {
        videoGoodCount++;
    } else {
        videoBadCount++;
    }

    // Ask server if video is finished (for the case where video ends
    // right after this frame)
    if (check_video_done()) {
        ei_printf("\n\n====================================\n");
        ei_printf("=========== VIDEO SUMMARY ==========\n");
        ei_printf("Frames processed: %d, good frames: %d, bad frames: %d\n",
                  frameCounter, videoGoodCount, videoBadCount);

        if (videoGoodCount > videoBadCount) {
            ei_printf("VIDEO CLASS: GOOD_JUMP\n");
        } else if (videoBadCount > videoGoodCount) {
            ei_printf("VIDEO CLASS: BAD_JUMP\n");
        } else {
            ei_printf("VIDEO CLASS: TIE (equal good/bad)\n");
        }

        if (videoInProgress) {
            unsigned long responseMs = millis() - videoStartTime;
            ei_printf("Response time: %lu ms from first frame to summary\n",
                      responseMs);
            videoInProgress = false;
        }

        ei_printf("====================================\n\n");

        // Reset for next video
        frameCounter    = 0;
        videoGoodCount  = 0;
        videoBadCount   = 0;

        ei_printf("Ready for next video.\n");
    }

    // Also check for feedback from user
    check_feedback_from_server();

    // Small pause before next status check
    delay(500);
}

/********* PRINT RESULTS (unchanged from your code) *********/

void print_inference_result(ei_impulse_result_t result) {

    // Print how long it took to perform inference
    ei_printf("Timing: DSP %d ms, inference %d ms, anomaly %d ms\r\n",
            result.timing.dsp,
            result.timing.classification,
            result.timing.anomaly);

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    ei_printf("Object detection bounding boxes:\r\n");
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
    }
#else
    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
        ei_printf("%.5f\r\n", result.classification[i].value);
    }
#endif

#if EI_CLASSIFIER_HAS_ANOMALY
    ei_printf("Anomaly prediction: %.3f\r\n", result.anomaly);
#endif

#if EI_CLASSIFIER_HAS_VISUAL_ANOMALY
    ei_printf("Visual anomalies:\r\n");
    for (uint32_t i = 0; i < result.visual_ad_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.visual_ad_grid_cells[i];
        if (bb.value == 0) {
            continue;
        }
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
    }
#endif
}
