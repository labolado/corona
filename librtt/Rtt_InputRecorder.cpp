//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Solar2D game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"
#include "Rtt_InputRecorder.h"
#include "Rtt_Runtime.h"
#include "Rtt_Event.h"
#include "Rtt_MPlatform.h"
#include "Display/Rtt_Display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef Rtt_WIN_ENV
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

namespace Rtt
{

// Global flag for replay diagnostics — set during InjectTouchEvent dispatch
int g_InputRecorderReplayDiag = 0;

// ============================================================================
// InputRecorder Implementation
// ============================================================================

// Auto-save interval: 10 seconds (in milliseconds)
const double InputRecorder::kAutoSaveInterval = 10000.0;

InputRecorder::InputRecorder(Runtime& runtime)
    : fRuntime(runtime)
    , fMode(kModeNone)
    , fStartTime(0)
    , fPlaybackFinished(false)
    , fLastSaveTime(0)
    , fPlaybackIndex(0)
    , fPlaybackStartTime(0)
{
}

InputRecorder::~InputRecorder()
{
    if (fMode == kModeRecording)
    {
        SaveRecording();
    }
}

void InputRecorder::StartRecording(const char* recordingDir)
{
    if (fMode != kModeNone)
    {
        Rtt_Log("InputRecorder: Already active, stop first\n");
        return;
    }

    fMode = kModeRecording;
    fStartTime = fRuntime.GetElapsedMS();
    fRecordedEvents.clear();
    fPlaybackFinished = false;

    // Record meta info — use screen pixel size (not content size)
    const Display& display = fRuntime.GetDisplay();
    fMetaInfo.screenWidth = display.WindowWidth();
    fMetaInfo.screenHeight = display.WindowHeight();
    fMetaInfo.platform = "unknown";  // Platform name not directly available
    fMetaInfo.backend = fRuntime.GetBackend() ? fRuntime.GetBackend() : "unknown";

    time_t now = time(NULL);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    fMetaInfo.timestamp = buf;

    // Generate recording filename — use provided dir if available, else default
    if (recordingDir && *recordingDir)
    {
        mkdir(recordingDir, 0755);
        char filename[256];
        snprintf(filename, sizeof(filename), "rec_%s.json", fMetaInfo.timestamp.c_str());
        fRecordingFilename = std::string(recordingDir) + "/" + filename;
    }
    else
    {
        fRecordingFilename = GenerateFilename();
    }
    fLastSaveTime = fStartTime;

    Rtt_Log("InputRecorder: Started recording to %s\n", fRecordingFilename.c_str());
}

void InputRecorder::StartPlayback(const char* filename)
{
    if (fMode != kModeNone)
    {
        Rtt_Log("InputRecorder: Already active, stop first\n");
        return;
    }

    if (!LoadRecording(filename))
    {
        Rtt_Log("InputRecorder: Failed to load recording: %s\n", filename);
        return;
    }

    fMode = kModeReplaying;
    fPlaybackStartTime = fRuntime.GetElapsedMS();
    fPlaybackIndex = 0;
    fPlaybackFinished = false;

    Rtt_Log("InputRecorder: Started playback of %s (%zu events)\n", filename, fPlaybackEvents.size());
}

void InputRecorder::Stop()
{
    if (fMode == kModeRecording)
    {
        SaveRecording();
    }

    fMode = kModeNone;
    fRecordedEvents.clear();
    fPlaybackEvents.clear();
    fPlaybackIndex = 0;
    fPlaybackFinished = false;

    Rtt_Log("InputRecorder: Stopped\n");
}

void InputRecorder::RecordTouchEvent(const TouchEvent& event, const void* touchId)
{
    if (fMode != kModeRecording)
        return;

    TouchRecord record;
    record.timestamp = fRuntime.GetElapsedMS() - fStartTime;
    record.phase = (int)event.GetPhase();
    // Use screen coordinates (not content) — content coords are only valid after Dispatch
    record.x = Rtt_RealToFloat(event.ScreenX());
    record.y = Rtt_RealToFloat(event.ScreenY());
    record.xStart = Rtt_RealToFloat(event.ScreenX());
    record.yStart = Rtt_RealToFloat(event.ScreenY());

    // Use touch id pointer as unique id
    record.id = (unsigned int)(uintptr_t)touchId;

    // Debug: log first few events to see what data we're getting
    {
        static int sRecLog = 0;
        if (sRecLog < 10)
        {
            Rtt_Log("InputRecorder: REC[%d] phase=%d x=%.1f y=%.1f id=%u t=%.0f\n",
                sRecLog, record.phase, record.x, record.y, record.id, record.timestamp);
            sRecLog++;
        }
    }

    fRecordedEvents.push_back(record);
}

void InputRecorder::Update(double currentTime)
{
    if (fMode == kModeRecording)
    {
        // Auto-save: check if it's time to save
        double elapsedSinceLastSave = currentTime - fLastSaveTime;
        if (elapsedSinceLastSave >= kAutoSaveInterval && !fRecordedEvents.empty())
        {
            SaveRecordingToFile(fRecordingFilename);
            fLastSaveTime = currentTime;
        }
    }
    else if (fMode == kModeReplaying && !fPlaybackFinished)
    {
        double elapsed = currentTime - fPlaybackStartTime;

        static int sUpdateLog = 0;
        if (sUpdateLog < 5 && fPlaybackIndex < fPlaybackEvents.size())
        {
            Rtt_Log("REPLAY_UPDATE: elapsed=%.0f nextEvt=%.0f idx=%zu/%zu\n",
                elapsed, fPlaybackEvents[fPlaybackIndex].timestamp, fPlaybackIndex, fPlaybackEvents.size());
            sUpdateLog++;
        }

        // Inject all events that should happen by now
        while (fPlaybackIndex < fPlaybackEvents.size() &&
               fPlaybackEvents[fPlaybackIndex].timestamp <= elapsed)
        {
            InjectTouchEvent(fPlaybackEvents[fPlaybackIndex]);
            fPlaybackIndex++;
        }

        // Check if playback is finished
        if (fPlaybackIndex >= fPlaybackEvents.size())
        {
            fPlaybackFinished = true;
            Rtt_Log("InputRecorder: Playback finished\n");
        }
    }
}

void InputRecorder::InjectTouchEvent(const TouchRecord& record)
{
    TouchEvent::Phase phase = (TouchEvent::Phase)record.phase;

    // Recorded values are in screen coordinates (pixels).
    // TouchEvent constructor expects screen coords; ScreenToContent runs during Dispatch.
    float x = record.x;
    float y = record.y;
    float xStart = record.xStart;
    float yStart = record.yStart;

    static int sInjectLog = 0;
    if (sInjectLog < 10)
    {
        const char* phaseNames[] = {"began", "moved", "stationary", "ended", "cancelled"};
        const char* phaseName = (record.phase >= 0 && record.phase < 5) ? phaseNames[record.phase] : "?";

        // Log Display transform parameters for diagnosis
        const Display& display = fRuntime.GetDisplay();
        Rtt_Log("REPLAY_INJECT[%d] phase=%s screen=(%.1f,%.1f) id=%u t=%.0f\n",
            sInjectLog, phaseName, x, y, record.id, record.timestamp);
        Rtt_Log("  Display: WindowSize=%dx%d Sx=%.6f Sy=%.6f OffsetX=%.1f OffsetY=%.1f\n",
            display.WindowWidth(), display.WindowHeight(),
            Rtt_RealToFloat(display.GetSx()), Rtt_RealToFloat(display.GetSy()),
            Rtt_RealToFloat(display.GetXOriginOffset()), Rtt_RealToFloat(display.GetYOriginOffset()));
        Rtt_Log("  Content: W=%.0f H=%.0f ScreenW=%d ScreenH=%d\n",
            Rtt_RealToFloat(display.ActualContentWidth()), Rtt_RealToFloat(display.ActualContentHeight()),
            display.ScreenWidth(), display.ScreenHeight());

        // Compute what ScreenToContent would produce
        float contentX = x * Rtt_RealToFloat(display.GetSx()) - Rtt_RealToFloat(display.GetXOriginOffset());
        float contentY = y * Rtt_RealToFloat(display.GetSy()) - Rtt_RealToFloat(display.GetYOriginOffset());
        Rtt_Log("  Expected content coords: (%.1f, %.1f)\n", contentX, contentY);

        sInjectLog++;
    }

    TouchEvent event(
        Rtt_FloatToReal(x),
        Rtt_FloatToReal(y),
        Rtt_FloatToReal(xStart),
        Rtt_FloatToReal(yStart),
        phase,
        TouchEvent::kPressureInvalid
    );

    // Set a fake touch id based on record id
    static unsigned int sBaseTouchId = 0;
    unsigned int touchId = sBaseTouchId + record.id;
    event.SetId((const void*)(uintptr_t)touchId);

    // Log the constructed event's actual field values
    if (sInjectLog <= 10)
    {
        Rtt_Log("  TouchEvent: ScreenX=%.1f ScreenY=%.1f Id=%p\n",
            Rtt_RealToFloat(event.ScreenX()), Rtt_RealToFloat(event.ScreenY()), event.GetId());
    }

    // Dispatch as bare TouchEvent — same path as original single-touch input
    // (MultitouchEvent wrapper uses per-id focus which differs from original global focus path)
    g_InputRecorderReplayDiag = 1;
    fRuntime.DispatchEvent(event);
    g_InputRecorderReplayDiag = 0;
}

std::string InputRecorder::GetRecordingDirectory()
{
    String path(fRuntime.GetAllocator());
    fRuntime.Platform().PathForFile("recordings", MPlatform::kDocumentsDir, MPlatform::kDefaultPathFlags, path);
    return path.GetString() ? path.GetString() : "";
}

std::string InputRecorder::GenerateFilename()
{
    std::string dir = GetRecordingDirectory();
    if (!dir.empty())
    {
        // Ensure directory exists
        mkdir(dir.c_str(), 0755);
    }
    
    char filename[256];
    snprintf(filename, sizeof(filename), "rec_%s.json", fMetaInfo.timestamp.c_str());
    
    if (dir.empty())
        return filename;
    
    return dir + "/" + filename;
}

void InputRecorder::SaveRecording()
{
    if (fRecordedEvents.empty())
    {
        Rtt_Log("InputRecorder: No events to save\n");
        return;
    }

    std::string filename = fRecordingFilename.empty() ? GenerateFilename() : fRecordingFilename;
    SaveRecordingToFile(filename);
}

void InputRecorder::SaveRecordingToFile(const std::string& filename)
{
    if (fRecordedEvents.empty())
    {
        return;
    }

    FILE* fp = fopen(filename.c_str(), "w");
    if (!fp)
    {
        Rtt_Log("InputRecorder: Failed to create file: %s\n", filename.c_str());
        return;
    }

    // Write JSON header with meta info
    fprintf(fp, "{\n");
    fprintf(fp, "  \"meta\": {\n");
    fprintf(fp, "    \"version\": 1,\n");
    fprintf(fp, "    \"platform\": \"%s\",\n", fMetaInfo.platform.c_str());
    fprintf(fp, "    \"backend\": \"%s\",\n", fMetaInfo.backend.c_str());
    fprintf(fp, "    \"screenWidth\": %d,\n", fMetaInfo.screenWidth);
    fprintf(fp, "    \"screenHeight\": %d,\n", fMetaInfo.screenHeight);
    fprintf(fp, "    \"timestamp\": \"%s\"\n", fMetaInfo.timestamp.c_str());
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"events\": [\n");

    // Write events
    const char* phaseNames[] = {"began", "moved", "stationary", "ended", "cancelled"};
    for (size_t i = 0; i < fRecordedEvents.size(); i++)
    {
        const TouchRecord& rec = fRecordedEvents[i];
        const char* phaseName = (rec.phase >= 0 && rec.phase < 5) ? phaseNames[rec.phase] : "unknown";
        
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"time\": %.3f,\n", rec.timestamp);
        fprintf(fp, "      \"phase\": \"%s\",\n", phaseName);
        fprintf(fp, "      \"x\": %.2f,\n", rec.x);
        fprintf(fp, "      \"y\": %.2f,\n", rec.y);
        fprintf(fp, "      \"id\": %u\n", rec.id);
        fprintf(fp, "    }%s\n", (i < fRecordedEvents.size() - 1) ? "," : "");
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    Rtt_Log("InputRecorder: Saved %zu events to %s\n", fRecordedEvents.size(), filename.c_str());
}

// Simple JSON parsing helper for playback
static const char* FindJsonString(const char* json, const char* key, char* out, size_t outSize)
{
    char search[64];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return NULL;
    
    p += strlen(search);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    
    if (*p == '\"')
    {
        p++;
        size_t i = 0;
        while (*p && *p != '\"' && i < outSize - 1)
        {
            out[i++] = *p++;
        }
        out[i] = '\0';
        return p;
    }
    else if (*p == '-' || (*p >= '0' && *p <= '9'))
    {
        size_t i = 0;
        while ((*p == '-' || *p == '.' || (*p >= '0' && *p <= '9')) && i < outSize - 1)
        {
            out[i++] = *p++;
        }
        out[i] = '\0';
        return p;
    }
    return NULL;
}

bool InputRecorder::LoadRecording(const char* filename)
{
    FILE* fp = fopen(filename, "r");
    if (!fp)
    {
        Rtt_Log("InputRecorder: Failed to open file: %s\n", filename);
        return false;
    }

    // Read file
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    std::vector<char> buffer(size + 1);
    if (fread(buffer.data(), 1, size, fp) != (size_t)size)
    {
        fclose(fp);
        return false;
    }
    fclose(fp);
    buffer[size] = '\0';

    fPlaybackEvents.clear();

    // Parse meta info for coordinate scaling
    char value[256];
    if (FindJsonString(buffer.data(), "screenWidth", value, sizeof(value)))
        fMetaInfo.screenWidth = atoi(value);
    if (FindJsonString(buffer.data(), "screenHeight", value, sizeof(value)))
        fMetaInfo.screenHeight = atoi(value);
    Rtt_Log("InputRecorder: Meta screenSize=%dx%d\n", fMetaInfo.screenWidth, fMetaInfo.screenHeight);

    // Parse events array
    const char* eventsStart = strstr(buffer.data(), "\"events\":");
    if (!eventsStart)
    {
        Rtt_Log("InputRecorder: No events array found\n");
        return false;
    }

    const char* p = eventsStart;

    while ((p = strstr(p, "{")) != NULL)
    {
        TouchRecord rec;
        
        // timestamp
        if (FindJsonString(p, "time", value, sizeof(value)))
            rec.timestamp = atof(value);
        else
            break;
            
        // phase
        if (FindJsonString(p, "phase", value, sizeof(value)))
        {
            if (strcmp(value, "began") == 0) rec.phase = 0;
            else if (strcmp(value, "moved") == 0) rec.phase = 1;
            else if (strcmp(value, "stationary") == 0) rec.phase = 2;
            else if (strcmp(value, "ended") == 0) rec.phase = 3;
            else if (strcmp(value, "cancelled") == 0) rec.phase = 4;
            else rec.phase = 0;
        }
        else
            break;
            
        // x
        if (FindJsonString(p, "x", value, sizeof(value)))
            rec.x = (float)atof(value);
        else
            break;
            
        // y
        if (FindJsonString(p, "y", value, sizeof(value)))
            rec.y = (float)atof(value);
        else
            break;
            
        // id (optional, defaults to 0)
        if (FindJsonString(p, "id", value, sizeof(value)))
            rec.id = (unsigned int)atoi(value);
        else
            rec.id = 0;
            
        // xStart, yStart (optional)
        rec.xStart = rec.x;
        rec.yStart = rec.y;

        fPlaybackEvents.push_back(rec);
        
        // Move past this object
        p++;
        const char* nextObj = strstr(p, "{");
        if (!nextObj) break;
        p = nextObj - 1;
    }

    // Normalize timestamps: subtract first event's time so playback starts immediately
    if (!fPlaybackEvents.empty())
    {
        double firstTime = fPlaybackEvents[0].timestamp;
        if (firstTime > 0)
        {
            Rtt_Log("InputRecorder: Normalizing timestamps (first event at %.0fms, subtracting)\n", firstTime);
            for (size_t i = 0; i < fPlaybackEvents.size(); i++)
            {
                fPlaybackEvents[i].timestamp -= firstTime;
            }
        }
    }

    Rtt_Log("InputRecorder: Loaded %zu events\n", fPlaybackEvents.size());
    return !fPlaybackEvents.empty();
}

// Helper: check if a trigger file exists at the given path
static bool CheckTriggerFileAtPath(const char* dirPath, const char* triggerName,
                                    std::string& outDir)
{
    if (!dirPath) return false;

    std::string triggerFile = std::string(dirPath) + "/" + triggerName;
    struct stat st;
    if (stat(triggerFile.c_str(), &st) == 0)
    {
        remove(triggerFile.c_str());
        outDir = dirPath;
        return true;
    }
    return false;
}

// Helper: build Android external storage recordings path from package name.
// On Android, kDocumentsDir is internal (inaccessible via adb on release builds).
// External path /sdcard/Android/data/<package>/files/recordings/ is adb-writable.
static std::string BuildExternalRecordingsDir(const MPlatform& platform)
{
#ifdef Rtt_ANDROID_ENV
    // Extract package name from documents dir path:
    // /data/data/<package>/files/Sandbox/Documents/ → <package>
    String docsDir(&platform.GetAllocator());
    platform.PathForFile(NULL, MPlatform::kDocumentsDir, MPlatform::kDefaultPathFlags, docsDir);
    if (docsDir.GetString())
    {
        std::string path(docsDir.GetString());
        // Pattern: /data/data/<package>/files/...  or  /data/user/0/<package>/files/...
        const char* markers[] = { "/data/data/", "/data/user/0/" };
        for (const char* marker : markers)
        {
            size_t pos = path.find(marker);
            if (pos != std::string::npos)
            {
                size_t pkgStart = pos + strlen(marker);
                size_t pkgEnd = path.find('/', pkgStart);
                if (pkgEnd != std::string::npos)
                {
                    std::string pkg = path.substr(pkgStart, pkgEnd - pkgStart);
                    return "/sdcard/Android/data/" + pkg + "/files/recordings";
                }
            }
        }
    }
#endif
    (void)platform;
    return "";
}

bool InputRecorder::CheckRecordingTriggerFile(const MPlatform& platform, std::string& outRecordingPath)
{
    // Check internal storage (kDocumentsDir)
    String recordingsDir(&platform.GetAllocator());
    platform.PathForFile("recordings", MPlatform::kDocumentsDir, MPlatform::kDefaultPathFlags, recordingsDir);

    bool found = false;
    if (recordingsDir.GetString() &&
        CheckTriggerFileAtPath(recordingsDir.GetString(), "RECORD", outRecordingPath))
    {
        found = true;
    }

    // Check external storage (Android: /sdcard/Android/data/<pkg>/files/recordings/)
    std::string extDir = BuildExternalRecordingsDir(platform);
    if (!found && !extDir.empty() && CheckTriggerFileAtPath(extDir.c_str(), "RECORD", outRecordingPath))
    {
        found = true;
    }

    // Always use external storage for recording output on Android
    // (internal storage is inaccessible via adb on release builds)
    if (found && !extDir.empty())
    {
        mkdir(extDir.c_str(), 0755);
        outRecordingPath = extDir;
    }

    return found;
}

// Helper: read REPLAY trigger file and return the replay filename
static bool ReadReplayTrigger(const char* dirPath, std::string& outReplayFilename)
{
    if (!dirPath) return false;

    std::string triggerFile = std::string(dirPath) + "/REPLAY";
    FILE* fp = fopen(triggerFile.c_str(), "r");
    if (!fp) return false;

    char filename[256] = {0};
    if (fgets(filename, sizeof(filename), fp))
    {
        size_t len = strlen(filename);
        if (len > 0 && filename[len-1] == '\n') filename[len-1] = '\0';
        len = strlen(filename);
        if (len > 0 && filename[len-1] == '\r') filename[len-1] = '\0';
        outReplayFilename = std::string(dirPath) + "/" + filename;
    }
    fclose(fp);
    remove(triggerFile.c_str());
    return !outReplayFilename.empty();
}

bool InputRecorder::CheckPlaybackTriggerFile(const MPlatform& platform, std::string& outReplayFilename)
{
    // Check internal storage (kDocumentsDir)
    String recordingsDir(&platform.GetAllocator());
    platform.PathForFile("recordings", MPlatform::kDocumentsDir, MPlatform::kDefaultPathFlags, recordingsDir);

    if (recordingsDir.GetString() && ReadReplayTrigger(recordingsDir.GetString(), outReplayFilename))
    {
        return true;
    }

    // Check external storage (Android)
    std::string extDir = BuildExternalRecordingsDir(platform);
    if (!extDir.empty() && ReadReplayTrigger(extDir.c_str(), outReplayFilename))
    {
        Rtt_LogException("InputRecorder: REPLAY trigger found at external path: %s\n", extDir.c_str());
        return true;
    }

    return false;
}

// ============================================================================

} // namespace Rtt
