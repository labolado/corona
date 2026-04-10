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

// ============================================================================
// InputRecorder Implementation
// ============================================================================

InputRecorder::InputRecorder(Runtime& runtime)
    : fRuntime(runtime)
    , fMode(kModeNone)
    , fStartTime(0)
    , fPlaybackFinished(false)
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

void InputRecorder::StartRecording()
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

    // Record meta info
    const Display& display = fRuntime.GetDisplay();
    fMetaInfo.screenWidth = display.ViewableContentWidth();
    fMetaInfo.screenHeight = display.ViewableContentHeight();
    fMetaInfo.platform = "unknown";  // Platform name not directly available
    fMetaInfo.backend = fRuntime.GetBackend() ? fRuntime.GetBackend() : "unknown";
    
    time_t now = time(NULL);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", localtime(&now));
    fMetaInfo.timestamp = buf;

    Rtt_Log("InputRecorder: Started recording\n");
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
    record.x = Rtt_RealToFloat(event.X());
    record.y = Rtt_RealToFloat(event.Y());
    
    // Get start position from the event
    record.xStart = Rtt_RealToFloat(event.X()); // Use current as start (simplified)
    record.yStart = Rtt_RealToFloat(event.Y());
    
    // Use touch id pointer as unique id
    record.id = (unsigned int)(uintptr_t)touchId;

    fRecordedEvents.push_back(record);
}

void InputRecorder::Update(double currentTime)
{
    if (fMode != kModeReplaying || fPlaybackFinished)
        return;

    double elapsed = currentTime - fPlaybackStartTime;

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

void InputRecorder::InjectTouchEvent(const TouchRecord& record)
{
    TouchEvent::Phase phase = (TouchEvent::Phase)record.phase;
    
    TouchEvent event(
        Rtt_FloatToReal(record.x),
        Rtt_FloatToReal(record.y),
        Rtt_FloatToReal(record.xStart),
        Rtt_FloatToReal(record.yStart),
        phase,
        TouchEvent::kPressureInvalid
    );
    
    // Set a fake touch id based on record id
    static unsigned int sBaseTouchId = 0;
    unsigned int touchId = sBaseTouchId + record.id;
    event.SetId((const void*)(uintptr_t)touchId);

    fRuntime.DispatchEvent(event);
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

    std::string filename = GenerateFilename();
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

    // Parse events array
    const char* eventsStart = strstr(buffer.data(), "\"events\":");
    if (!eventsStart)
    {
        Rtt_Log("InputRecorder: No events array found\n");
        return false;
    }

    const char* p = eventsStart;
    char value[256];
    
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

    Rtt_Log("InputRecorder: Loaded %zu events\n", fPlaybackEvents.size());
    return !fPlaybackEvents.empty();
}

bool InputRecorder::CheckRecordingTriggerFile(const MPlatform& platform, std::string& outRecordingPath)
{
    String recordingsDir(&platform.GetAllocator());
    platform.PathForFile("recordings", MPlatform::kDocumentsDir, MPlatform::kDefaultPathFlags, recordingsDir);
    
    if (!recordingsDir.GetString())
        return false;

    std::string triggerFile = std::string(recordingsDir.GetString()) + "/RECORD";
    
    struct stat st;
    if (stat(triggerFile.c_str(), &st) == 0)
    {
        // Trigger file exists, delete it and return true
        remove(triggerFile.c_str());
        outRecordingPath = recordingsDir.GetString();
        return true;
    }
    
    return false;
}

bool InputRecorder::CheckPlaybackTriggerFile(const MPlatform& platform, std::string& outReplayFilename)
{
    String recordingsDir(&platform.GetAllocator());
    platform.PathForFile("recordings", MPlatform::kDocumentsDir, MPlatform::kDefaultPathFlags, recordingsDir);
    
    if (!recordingsDir.GetString())
        return false;

    std::string triggerFile = std::string(recordingsDir.GetString()) + "/REPLAY";
    
    FILE* fp = fopen(triggerFile.c_str(), "r");
    if (!fp)
        return false;

    char filename[256] = {0};
    if (fgets(filename, sizeof(filename), fp))
    {
        // Remove newline
        size_t len = strlen(filename);
        if (len > 0 && filename[len-1] == '\n')
            filename[len-1] = '\0';
        if (len > 1 && filename[len-2] == '\r')
            filename[len-2] = '\0';
            
        outReplayFilename = std::string(recordingsDir.GetString()) + "/" + filename;
    }
    fclose(fp);

    // Delete trigger file
    remove(triggerFile.c_str());

    return !outReplayFilename.empty();
}

// ============================================================================

} // namespace Rtt
