//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_InputRecorder_H__
#define _Rtt_InputRecorder_H__

#include "Core/Rtt_Build.h"
#include "Core/Rtt_Real.h"
#include <vector>
#include <string>

struct lua_State;

namespace Rtt
{

class Runtime;
class TouchEvent;
class MPlatform;

// ============================================================================
// InputRecorder - Touch Event Recording and Playback
// ============================================================================

class InputRecorder
{
public:
    enum Mode
    {
        kModeNone = 0,
        kModeRecording,
        kModeReplaying
    };

    struct TouchRecord
    {
        double timestamp;   // ms since start
        int phase;          // 0=began, 1=moved, 2=ended, 3=cancelled
        float x;
        float y;
        float xStart;
        float yStart;
        unsigned int id;
    };

    struct MetaInfo
    {
        int screenWidth;
        int screenHeight;
        std::string platform;
        std::string backend;
        std::string timestamp;
    };

public:
    InputRecorder(Runtime& runtime);
    ~InputRecorder();

    // Mode control
    void StartRecording(const char* recordingDir = NULL);
    void StartPlayback(const char* filename);
    void Stop();
    Mode GetMode() const { return fMode; }

    // Recording
    void RecordTouchEvent(const TouchEvent& event, const void* touchId);

    // Playback (call from update loop)
    void Update(double currentTime);
    bool IsPlaybackFinished() const { return fMode == kModeReplaying && fPlaybackFinished; }

    // Trigger file check (for mobile platforms)
    static bool CheckRecordingTriggerFile(const MPlatform& platform, std::string& outRecordingPath);
    static bool CheckPlaybackTriggerFile(const MPlatform& platform, std::string& outReplayFilename);

private:
    void SaveRecording();
    void SaveRecordingToFile(const std::string& filename);  // Save to specific file
    bool LoadRecording(const char* filename);
    void InjectTouchEvent(const TouchRecord& record);
    std::string GetRecordingDirectory();
    std::string GenerateFilename();

private:
    Runtime& fRuntime;
    Mode fMode;
    double fStartTime;
    bool fPlaybackFinished;

    // Recording data
    std::vector<TouchRecord> fRecordedEvents;
    MetaInfo fMetaInfo;
    std::string fRecordingFilename;  // Current recording file for auto-save
    double fLastSaveTime;            // Last auto-save timestamp
    static const double kAutoSaveInterval;  // Auto-save interval in ms (10 seconds)

    // Playback data
    std::vector<TouchRecord> fPlaybackEvents;
    size_t fPlaybackIndex;
    double fPlaybackStartTime;
};

// ============================================================================

} // namespace Rtt

#endif // _Rtt_InputRecorder_H__
