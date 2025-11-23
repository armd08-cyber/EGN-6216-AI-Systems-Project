import SwiftUI
import AVFoundation
import PhotosUI
import UIKit   // <- needed for UIImage

struct ContentView: View {

    // CHANGE this to your Mac's IP if needed
    let imageUploadURL = URL(string: "http://192.168.86.246:5055/image/upload")!
    let videoEndURL    = URL(string: "http://192.168.86.246:5055/video/end")!
    let feedbackURL    = URL(string: "http://192.168.86.246:5055/feedback")!

    @State private var selectedVideoItem: PhotosPickerItem?
    @State private var isProcessing = false
    @State private var statusMessage = "Idle"

    // Feedback state
    @State private var hasCompletedVideo = false
    @State private var feedbackSent = false
    @State private var selectedFeedback: String? = nil
    @State private var comment: String = ""

    var body: some View {
        VStack(spacing: 20) {
            Text("JumpSafe iPhone Client")
                .font(.title2)
                .multilineTextAlignment(.center)

            Text("""
                 Pick a short jump video.
                 Frames will be sent to the Mac server,
                 then ESP32 will classify + majority vote.
                 """)
                .font(.subheadline)
                .multilineTextAlignment(.center)
                .padding(.horizontal)

            PhotosPicker(
                selection: $selectedVideoItem,
                matching: .videos,
                photoLibrary: .shared()
            ) {
                Text("Select Jump Video")
            }
            .buttonStyle(.borderedProminent)
            .onChange(of: selectedVideoItem) { newItem in
                if let item = newItem {
                    Task {
                        await handlePickedVideo(item)
                    }
                }
            }

            if isProcessing {
                ProgressView("Processing video...")
            }

            Text(statusMessage)
                .font(.footnote)
                .foregroundColor(.gray)

            // Feedback UI
            if hasCompletedVideo && !feedbackSent {
                VStack(spacing: 12) {
                    Text("How was the prediction?")
                        .font(.headline)

                    HStack(spacing: 24) {
                        Button(action: {
                            sendFeedback(rating: "thumbs_up", comment: "")
                        }) {
                            HStack {
                                Text("👍")
                                Text("Good")
                            }
                            .padding(.horizontal, 12)
                            .padding(.vertical, 6)
                            .background(Color.blue.opacity(0.1))
                            .cornerRadius(8)
                        }

                        Button(action: {
                            sendFeedback(rating: "thumbs_down", comment: "")
                        }) {
                            HStack {
                                Text("👎")
                                Text("Bad")
                            }
                            .padding(.horizontal, 12)
                            .padding(.vertical, 6)
                            .background(Color.red.opacity(0.1))
                            .cornerRadius(8)
                        }
                    }

                    TextField("Optional comment (e.g., model was wrong here)...",
                              text: $comment)
                        .textFieldStyle(RoundedBorderTextFieldStyle())
                        .padding(.horizontal)

                    Button("Submit Feedback") {
                        sendFeedback(rating: "comment", comment: comment)
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(comment.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
                }
                .padding(.top, 8)
            } else if hasCompletedVideo && feedbackSent {
                Text("Thanks for the feedback! 🙌")
                    .font(.footnote)
                    .foregroundColor(.green)
            }

            Spacer()
        }
        .padding()
    }

    // MARK: - Handle picked video

    func handlePickedVideo(_ item: PhotosPickerItem) async {
        DispatchQueue.main.async {
            isProcessing = true
            statusMessage = "Loading video..."

            // Reset feedback state for a new video
            hasCompletedVideo = false
            feedbackSent = false
            selectedFeedback = nil
            comment = ""
        }

        do {
            if let movieData = try await item.loadTransferable(type: Data.self) {
                let tmpURL = FileManager.default.temporaryDirectory
                    .appendingPathComponent("jump-\(UUID().uuidString).mov")
                try movieData.write(to: tmpURL)
                await processVideo(at: tmpURL)
            } else {
                DispatchQueue.main.async {
                    statusMessage = "Could not load video data"
                    isProcessing = false
                }
            }
        } catch {
            DispatchQueue.main.async {
                statusMessage = "Error: \(error.localizedDescription)"
                isProcessing = false
            }
        }
    }

    // MARK: - Process video and extract frames

    func processVideo(at url: URL) async {
        DispatchQueue.main.async {
            statusMessage = "Analyzing video..."
        }

        let asset = AVAsset(url: url)
        let duration = CMTimeGetSeconds(asset.duration)

        guard let track = asset.tracks(withMediaType: .video).first else {
            DispatchQueue.main.async {
                statusMessage = "No video track found"
                isProcessing = false
            }
            return
        }

        let fps = track.nominalFrameRate > 0 ? track.nominalFrameRate : 30
        let frameIntervalSeconds = Double(2.0 / fps)  // every 2 frames, like EI

        print("Video FPS:", fps)
        print("Frame interval (s):", frameIntervalSeconds)

        let imageGen = AVAssetImageGenerator(asset: asset)
        imageGen.appliesPreferredTrackTransform = true

        var times: [NSValue] = []
        var current = 0.0
        while current < duration {
            let time = CMTime(seconds: current, preferredTimescale: 600)
            times.append(NSValue(time: time))
            current += frameIntervalSeconds
        }

        var uploaded = 0

        for t in times {
            do {
                let cg = try imageGen.copyCGImage(at: t.timeValue, actualTime: nil)
                let uiImage = UIImage(cgImage: cg)

                if let jpegData = uiImage.jpegData(compressionQuality: 0.8) {
                    uploaded += 1
                    DispatchQueue.main.async {
                        statusMessage = "Uploading frame \(uploaded)..."
                    }
                    uploadImageData(jpegData)
                }
            } catch {
                print("Error grabbing frame:", error)
            }
        }

        // All frames uploaded -> tell server "video is done"
        sendVideoEndSignal()

        DispatchQueue.main.async {
            statusMessage = "Done. Uploaded \(uploaded) frames (video end sent)."
            isProcessing = false
            hasCompletedVideo = true
        }
    }

    // MARK: - Upload one frame to the server

    func uploadImageData(_ data: Data) {
        var request = URLRequest(url: imageUploadURL)
        request.httpMethod = "POST"
        request.setValue("image/jpeg", forHTTPHeaderField: "Content-Type")
        request.httpBody = data

        URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                print("Error uploading frame:", error.localizedDescription)
                return
            }
            if let httpResp = response as? HTTPURLResponse {
                print("Frame upload status:", httpResp.statusCode)
            }
            if let data = data,
               let body = String(data: data, encoding: .utf8) {
                print("Frame response:", body)
            }
        }.resume()
    }

    // MARK: - Tell server video is finished

    func sendVideoEndSignal() {
        var request = URLRequest(url: videoEndURL)
        request.httpMethod = "POST"

        URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                print("Error sending video end:", error.localizedDescription)
                return
            }
            if let httpResp = response as? HTTPURLResponse {
                print("Video end status:", httpResp.statusCode)
            }
            if let data = data,
               let body = String(data: data, encoding: .utf8) {
                print("Video end response:", body)
            }
        }.resume()
    }

    // MARK: - Send feedback to server

    func sendFeedback(rating: String, comment: String) {
        var request = URLRequest(url: feedbackURL)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let trimmedComment = comment.trimmingCharacters(in: .whitespacesAndNewlines)
        let body: [String: Any] = [
            "rating": rating,
            "comment": trimmedComment
        ]

        guard let jsonData = try? JSONSerialization.data(withJSONObject: body) else {
            print("Failed to encode feedback JSON")
            return
        }

        request.httpBody = jsonData

        URLSession.shared.dataTask(with: request) { data, response, error in
            if let error = error {
                print("Error sending feedback:", error.localizedDescription)
                return
            }
            if let httpResp = response as? HTTPURLResponse {
                print("Feedback status:", httpResp.statusCode)
            }
            if let data = data,
               let body = String(data: data, encoding: .utf8) {
                print("Feedback response:", body)
            }

            DispatchQueue.main.async {
                selectedFeedback = rating
                feedbackSent = true
            }
        }.resume()
    }
}

#Preview {
    ContentView()
}
