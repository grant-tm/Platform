#ifndef PLATFORM_AUDIO_H
#define PLATFORM_AUDIO_H

#include "../Core/core.h"

typedef Handle64 PlatformAudioDevice;
typedef Handle64 PlatformAudioStream;

typedef enum PlatformAudioDeviceDirection
{
    PLATFORM_AUDIO_DEVICE_DIRECTION_NONE = 0,
    PLATFORM_AUDIO_DEVICE_DIRECTION_INPUT = BIT_U32(0),
    PLATFORM_AUDIO_DEVICE_DIRECTION_OUTPUT = BIT_U32(1),
    PLATFORM_AUDIO_DEVICE_DIRECTION_INPUT_OUTPUT =
        PLATFORM_AUDIO_DEVICE_DIRECTION_INPUT |
        PLATFORM_AUDIO_DEVICE_DIRECTION_OUTPUT,
} PlatformAudioDeviceDirection;

typedef enum PlatformAudioSampleFormat
{
    PLATFORM_AUDIO_SAMPLE_FORMAT_NONE = 0,
    PLATFORM_AUDIO_SAMPLE_FORMAT_F32,
} PlatformAudioSampleFormat;

typedef struct PlatformAudioDeviceInfo
{
    PlatformAudioDevice device;
    String name;
    PlatformAudioDeviceDirection direction;
    u32 input_channel_count;
    u32 output_channel_count;
    u32 preferred_sample_rate;
    u32 preferred_frame_count;
    Nanoseconds default_low_input_latency;
    Nanoseconds default_low_output_latency;
    b32 supports_exclusive;
    b32 is_default_input;
    b32 is_default_output;
} PlatformAudioDeviceInfo;

typedef struct PlatformAudioBuffer
{
    f32 **channels;
    u32 channel_count;
    u32 frame_count;
} PlatformAudioBuffer;

typedef struct PlatformAudioCallbackInfo
{
    u64 input_sample_index;
    u64 output_sample_index;
    Nanoseconds callback_time;
    Nanoseconds input_latency;
    Nanoseconds output_latency;
    b32 input_overflow_occurred;
    b32 output_underflow_occurred;
} PlatformAudioCallbackInfo;

/* Real-time callback contract:
   - no allocation or free
   - no locks or waits
   - no file I/O
   - no logging
   - no general platform calls
   - always fill the full output buffer when output is active */
typedef void PlatformAudioCallback (
    PlatformAudioBuffer input,
    PlatformAudioBuffer output,
    const PlatformAudioCallbackInfo *info,
    void *user_data);

typedef struct PlatformAudioStreamDesc
{
    PlatformAudioDevice input_device;
    PlatformAudioDevice output_device;
    u32 input_channel_count;
    u32 output_channel_count;
    u32 requested_sample_rate;
    u32 requested_frame_count;
    PlatformAudioSampleFormat sample_format;
    PlatformAudioCallback *callback;
    void *user_data;
} PlatformAudioStreamDesc;

typedef struct PlatformAudioStreamInfo
{
    PlatformAudioDevice input_device;
    PlatformAudioDevice output_device;
    u32 input_channel_count;
    u32 output_channel_count;
    u32 actual_sample_rate;
    u32 actual_frame_count;
    PlatformAudioSampleFormat sample_format;
    Nanoseconds input_latency;
    Nanoseconds output_latency;
    b32 is_running;
} PlatformAudioStreamInfo;

static const PlatformAudioDevice PLATFORM_AUDIO_DEVICE_INVALID = HANDLE64_INVALID;
static const PlatformAudioStream PLATFORM_AUDIO_STREAM_INVALID = HANDLE64_INVALID;

usize PlatformAudio_GetDeviceCount (void);
PlatformAudioDevice PlatformAudio_GetDeviceByIndex (usize index);
PlatformAudioDevice PlatformAudio_GetDefaultInputDevice (void);
PlatformAudioDevice PlatformAudio_GetDefaultOutputDevice (void);
b32 PlatformAudio_GetDeviceInfo (PlatformAudioDevice device, MemoryArena *arena, PlatformAudioDeviceInfo *info);

PlatformAudioStream PlatformAudioStream_Create (const PlatformAudioStreamDesc *desc);
void PlatformAudioStream_Destroy (PlatformAudioStream stream);
b32 PlatformAudioStream_IsValid (PlatformAudioStream stream);
b32 PlatformAudioStream_Start (PlatformAudioStream stream);
void PlatformAudioStream_Stop (PlatformAudioStream stream);
b32 PlatformAudioStream_IsRunning (PlatformAudioStream stream);
b32 PlatformAudioStream_GetInfo (PlatformAudioStream stream, PlatformAudioStreamInfo *info);

#endif // PLATFORM_AUDIO_H
