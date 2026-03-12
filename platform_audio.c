#include "platform_internal.h"

#include <initguid.h>
#include <avrt.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <objbase.h>
#include <propidl.h>

#define PLATFORM_MAX_AUDIO_DEVICES 64
#define PLATFORM_MAX_AUDIO_STREAMS 16
#define PLATFORM_AUDIO_TEXT_CAPACITY 512
#define PLATFORM_AUDIO_MAX_CHANNELS 32

DEFINE_GUID(CLSID_MMDeviceEnumerator,
    0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,
    0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IMMEndpoint,
    0x1be09788, 0x6894, 0x4089, 0x85, 0x86, 0x9a, 0x2a, 0x6c, 0x26, 0x5a, 0xc5);
DEFINE_GUID(IID_IAudioClient,
    0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioRenderClient,
    0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,
    0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM,
    0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

static const PROPERTYKEY PLATFORM_PKEY_DEVICE_FRIENDLY_NAME =
{
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
    14
};

typedef struct PlatformAudioDeviceState
{
    b32 is_used;
    GenerationalHandle64 handle;
    c8 device_id[PLATFORM_AUDIO_TEXT_CAPACITY];
    usize device_id_count;
    c8 name[PLATFORM_AUDIO_TEXT_CAPACITY];
    usize name_count;
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
} PlatformAudioDeviceState;

typedef struct PlatformAudioStreamState
{
    b32 is_used;
    GenerationalHandle64 handle;
    PlatformAudioStreamDesc desc;
    PlatformAudioStreamInfo info;
    HANDLE thread_handle;
    HANDLE stop_event;
    HANDLE started_event;
    HANDLE audio_event;
    volatile LONG start_succeeded;
    volatile LONG thread_is_active;
    u64 output_sample_index;
} PlatformAudioStreamState;

typedef struct PlatformAudioState
{
    b32 com_initialized_by_platform;
    b32 devices_are_loaded;
    PlatformAudioDeviceState devices[PLATFORM_MAX_AUDIO_DEVICES];
    usize device_count;
    PlatformAudioStreamState streams[PLATFORM_MAX_AUDIO_STREAMS];
} PlatformAudioState;

static PlatformAudioState platform_audio_state = {0};

static void Platform_ReleaseCOMObject (IUnknown *unknown)
{
    if (unknown != NULL)
    {
        unknown->lpVtbl->Release(unknown);
    }
}

static String PlatformAudio_CopyFixedStringToArena (MemoryArena *arena, const c8 *source, usize count)
{
    c8 *buffer;

    ASSERT(arena != NULL);

    buffer = MemoryArena_PushArray(arena, c8, count);
    if ((count > 0) && (buffer == NULL))
    {
        return String_Create(NULL, 0);
    }

    if (count > 0)
    {
        Memory_Copy(buffer, source, count);
    }

    return String_Create(buffer, count);
}

static usize PlatformAudio_CopyWideStringToUTF8 (c8 *destination, usize destination_capacity, const wchar_t *source)
{
    i32 count;

    ASSERT(destination != NULL);
    ASSERT(destination_capacity > 0);

    destination[0] = 0;
    if (source == NULL)
    {
        return 0;
    }

    count = WideCharToMultiByte(CP_UTF8, 0, source, -1, destination, (i32) destination_capacity, NULL, NULL);
    if (count <= 0)
    {
        destination[0] = 0;
        return 0;
    }

    return (usize) (count - 1);
}

static usize PlatformAudio_CopyUTF8ToWideString (wchar_t *destination, usize destination_capacity, const c8 *source)
{
    i32 count;

    ASSERT(destination != NULL);
    ASSERT(destination_capacity > 0);

    destination[0] = 0;
    if (source == NULL)
    {
        return 0;
    }

    count = MultiByteToWideChar(CP_UTF8, 0, source, -1, destination, (i32) destination_capacity);
    if (count <= 0)
    {
        destination[0] = 0;
        return 0;
    }

    return (usize) (count - 1);
}

static PlatformAudioDevice PlatformAudio_PackDeviceHandle (GenerationalHandle64 handle)
{
    return GenerationalHandle64_Pack(handle);
}

static PlatformAudioStream PlatformAudio_PackStreamHandle (GenerationalHandle64 handle)
{
    return GenerationalHandle64_Pack(handle);
}

static PlatformAudioDeviceState *PlatformAudio_GetDeviceState (PlatformAudioDevice device)
{
    GenerationalHandle64 handle;
    u32 slot_index;
    PlatformAudioDeviceState *device_state;

    if (!Handle64_IsValid(device))
    {
        return NULL;
    }

    handle = GenerationalHandle64_Unpack(device);
    if (!GenerationalHandle64_IsValid(handle))
    {
        return NULL;
    }

    slot_index = handle.index - 1;
    if (slot_index >= PLATFORM_MAX_AUDIO_DEVICES)
    {
        return NULL;
    }

    device_state = &platform_audio_state.devices[slot_index];
    if (!device_state->is_used)
    {
        return NULL;
    }

    if (!GenerationalHandle64_Equals(device_state->handle, handle))
    {
        return NULL;
    }

    return device_state;
}

static PlatformAudioStreamState *PlatformAudio_GetStreamState (PlatformAudioStream stream)
{
    GenerationalHandle64 handle;
    u32 slot_index;
    PlatformAudioStreamState *stream_state;

    if (!Handle64_IsValid(stream))
    {
        return NULL;
    }

    handle = GenerationalHandle64_Unpack(stream);
    if (!GenerationalHandle64_IsValid(handle))
    {
        return NULL;
    }

    slot_index = handle.index - 1;
    if (slot_index >= PLATFORM_MAX_AUDIO_STREAMS)
    {
        return NULL;
    }

    stream_state = &platform_audio_state.streams[slot_index];
    if (!stream_state->is_used)
    {
        return NULL;
    }

    if (!GenerationalHandle64_Equals(stream_state->handle, handle))
    {
        return NULL;
    }

    return stream_state;
}

static PlatformAudioStreamState *PlatformAudio_AllocateStreamState (void)
{
    usize index;

    for (index = 0; index < ARRAY_COUNT(platform_audio_state.streams); index += 1)
    {
        PlatformAudioStreamState *stream_state;

        stream_state = &platform_audio_state.streams[index];
        if (!stream_state->is_used)
        {
            Memory_Zero(stream_state, sizeof(*stream_state));
            stream_state->is_used = true;
            stream_state->handle.index = (u32) index + 1;
            stream_state->handle.generation += 1;
            if (stream_state->handle.generation == 0)
            {
                stream_state->handle.generation = 1;
            }

            return stream_state;
        }
    }

    return NULL;
}

static void PlatformAudio_ReleaseStreamState (PlatformAudioStreamState *stream_state)
{
    ASSERT(stream_state != NULL);

    if (stream_state->audio_event != NULL)
    {
        CloseHandle(stream_state->audio_event);
    }
    if (stream_state->started_event != NULL)
    {
        CloseHandle(stream_state->started_event);
    }
    if (stream_state->stop_event != NULL)
    {
        CloseHandle(stream_state->stop_event);
    }
    if (stream_state->thread_handle != NULL)
    {
        CloseHandle(stream_state->thread_handle);
    }

    stream_state->is_used = false;
    stream_state->desc = (PlatformAudioStreamDesc) {0};
    stream_state->info = (PlatformAudioStreamInfo) {0};
}

static void PlatformAudio_StopStreamState (PlatformAudioStreamState *stream_state)
{
    ASSERT(stream_state != NULL);

    if (stream_state->thread_handle != NULL)
    {
        SetEvent(stream_state->stop_event);
        WaitForSingleObject(stream_state->thread_handle, INFINITE);
        CloseHandle(stream_state->thread_handle);
        stream_state->thread_handle = NULL;
    }

    if (stream_state->stop_event != NULL)
    {
        ResetEvent(stream_state->stop_event);
    }

    stream_state->info.is_running = false;
    InterlockedExchange(&stream_state->thread_is_active, 0);
}

static HRESULT PlatformAudio_CreateEnumerator (IMMDeviceEnumerator **enumerator)
{
    ASSERT(enumerator != NULL);
    *enumerator = NULL;

    return CoCreateInstance(
        &CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        &IID_IMMDeviceEnumerator,
        (void **) enumerator);
}

static Nanoseconds PlatformAudio_ReferenceTimeToNanoseconds (REFERENCE_TIME reference_time)
{
    return (Nanoseconds) (reference_time * 100);
}

static DWORD PlatformAudio_GetChannelMask (u32 channel_count)
{
    switch (channel_count)
    {
        case 1: return SPEAKER_FRONT_CENTER;
        case 2: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        case 3: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER;
        case 4: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
        case 5: return SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
        case 6: return KSAUDIO_SPEAKER_5POINT1;
        case 7: return KSAUDIO_SPEAKER_5POINT1 | SPEAKER_BACK_CENTER;
        case 8: return KSAUDIO_SPEAKER_7POINT1;
        default: return 0;
    }
}

static b32 PlatformAudio_BuildExclusiveOutputFormat (
    u32 channel_count,
    u32 sample_rate,
    u32 bits_per_sample,
    const GUID *sub_format,
    WAVEFORMATEXTENSIBLE *format)
{
    ASSERT(format != NULL);
    ASSERT(sub_format != NULL);

    if ((channel_count == 0) || (channel_count > PLATFORM_AUDIO_MAX_CHANNELS))
    {
        return false;
    }

    Memory_Zero(format, sizeof(*format));
    format->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format->Format.nChannels = (WORD) channel_count;
    format->Format.nSamplesPerSec = sample_rate;
    format->Format.wBitsPerSample = (WORD) bits_per_sample;
    format->Format.nBlockAlign = (WORD) (channel_count * (bits_per_sample / 8));
    format->Format.nAvgBytesPerSec = format->Format.nSamplesPerSec * format->Format.nBlockAlign;
    format->Format.cbSize = sizeof(*format) - sizeof(format->Format);
    format->Samples.wValidBitsPerSample = (WORD) bits_per_sample;
    format->dwChannelMask = PlatformAudio_GetChannelMask(channel_count);
    format->SubFormat = *sub_format;
    return true;
}

static b32 PlatformAudio_FindSupportedExclusiveOutputFormat (
    IAudioClient *audio_client,
    u32 channel_count,
    u32 requested_sample_rate,
    u32 fallback_sample_rate,
    WAVEFORMATEXTENSIBLE *format)
{
    static const u32 common_sample_rates[] = {48000, 44100, 96000, 88200, 192000};
    static const struct
    {
        u32 bits_per_sample;
        const GUID *sub_format;
    } candidate_formats[] =
    {
        {32, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT},
        {32, &KSDATAFORMAT_SUBTYPE_PCM},
        {24, &KSDATAFORMAT_SUBTYPE_PCM},
        {16, &KSDATAFORMAT_SUBTYPE_PCM},
    };
    u32 sample_rates[8];
    usize sample_rate_count;
    usize sample_rate_index;
    usize format_index;

    ASSERT(audio_client != NULL);
    ASSERT(format != NULL);

    sample_rate_count = 0;
    if (requested_sample_rate > 0)
    {
        sample_rates[sample_rate_count] = requested_sample_rate;
        sample_rate_count += 1;
    }

    if ((fallback_sample_rate > 0) && (fallback_sample_rate != requested_sample_rate))
    {
        sample_rates[sample_rate_count] = fallback_sample_rate;
        sample_rate_count += 1;
    }

    for (sample_rate_index = 0; sample_rate_index < ARRAY_COUNT(common_sample_rates); sample_rate_index += 1)
    {
        u32 sample_rate;
        b32 already_added;
        usize existing_index;

        sample_rate = common_sample_rates[sample_rate_index];
        already_added = false;
        for (existing_index = 0; existing_index < sample_rate_count; existing_index += 1)
        {
            if (sample_rates[existing_index] == sample_rate)
            {
                already_added = true;
                break;
            }
        }

        if (!already_added)
        {
            sample_rates[sample_rate_count] = sample_rate;
            sample_rate_count += 1;
        }
    }

    for (sample_rate_index = 0; sample_rate_index < sample_rate_count; sample_rate_index += 1)
    {
        for (format_index = 0; format_index < ARRAY_COUNT(candidate_formats); format_index += 1)
        {
            HRESULT result;

            if (!PlatformAudio_BuildExclusiveOutputFormat(
                    channel_count,
                    sample_rates[sample_rate_index],
                    candidate_formats[format_index].bits_per_sample,
                    candidate_formats[format_index].sub_format,
                    format))
            {
                continue;
            }

            result = audio_client->lpVtbl->IsFormatSupported(
                audio_client,
                AUDCLNT_SHAREMODE_EXCLUSIVE,
                (WAVEFORMATEX *) format,
                NULL);
            if (result == S_OK)
            {
                return true;
            }
        }
    }

    return false;
}

static b32 PlatformAudio_IsFloatFormat (const WAVEFORMATEX *format)
{
    const WAVEFORMATEXTENSIBLE *extensible_format;

    ASSERT(format != NULL);

    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
        return true;
    }

    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        extensible_format = (const WAVEFORMATEXTENSIBLE *) format;
        return IsEqualGUID(&extensible_format->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    }

    return false;
}

static b32 PlatformAudio_IsPCMFormat (const WAVEFORMATEX *format)
{
    const WAVEFORMATEXTENSIBLE *extensible_format;

    ASSERT(format != NULL);

    if (format->wFormatTag == WAVE_FORMAT_PCM)
    {
        return true;
    }

    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        extensible_format = (const WAVEFORMATEXTENSIBLE *) format;
        return IsEqualGUID(&extensible_format->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM);
    }

    return false;
}

static u32 PlatformAudio_GetValidBitsPerSample (const WAVEFORMATEX *format)
{
    ASSERT(format != NULL);

    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
        return ((const WAVEFORMATEXTENSIBLE *) format)->Samples.wValidBitsPerSample;
    }

    return format->wBitsPerSample;
}

static void PlatformAudio_ConvertPlanarF32ToFormat (
    byte *destination,
    const WAVEFORMATEX *format,
    f32 **source_channels,
    u32 channel_count,
    u32 frame_count)
{
    u32 frame_index;
    u32 channel_index;
    u32 bytes_per_sample;

    ASSERT(destination != NULL);
    ASSERT(format != NULL);
    ASSERT(source_channels != NULL);

    bytes_per_sample = format->wBitsPerSample / 8;

    if (PlatformAudio_IsFloatFormat(format) && (format->wBitsPerSample == 32))
    {
        f32 *float_destination;

        float_destination = (f32 *) destination;
        for (frame_index = 0; frame_index < frame_count; frame_index += 1)
        {
            for (channel_index = 0; channel_index < channel_count; channel_index += 1)
            {
                float_destination[(frame_index * channel_count) + channel_index] =
                    source_channels[channel_index][frame_index];
            }
        }

        return;
    }

    if (PlatformAudio_IsPCMFormat(format) &&
        ((format->wBitsPerSample == 16) || (format->wBitsPerSample == 24) || (format->wBitsPerSample == 32)))
    {
        u32 valid_bits;
        i64 max_integer;
        i64 min_integer;
        u32 shift_amount;

        valid_bits = PlatformAudio_GetValidBitsPerSample(format);
        max_integer = (((i64) 1) << (valid_bits - 1)) - 1;
        min_integer = -(((i64) 1) << (valid_bits - 1));
        shift_amount = format->wBitsPerSample - valid_bits;

        for (frame_index = 0; frame_index < frame_count; frame_index += 1)
        {
            for (channel_index = 0; channel_index < channel_count; channel_index += 1)
            {
                f32 sample;
                i64 integer_sample;
                byte *sample_destination;
                u32 byte_index;

                sample = source_channels[channel_index][frame_index];
                if (sample > 1.0f)
                {
                    sample = 1.0f;
                }
                else if (sample < -1.0f)
                {
                    sample = -1.0f;
                }

                if (sample <= -1.0f)
                {
                    integer_sample = min_integer;
                }
                else if (sample >= 1.0f)
                {
                    integer_sample = max_integer;
                }
                else
                {
                    integer_sample = (i64) (sample * (f64) max_integer);
                }

                integer_sample <<= shift_amount;
                sample_destination = destination + ((((usize) frame_index * channel_count) + channel_index) * bytes_per_sample);
                for (byte_index = 0; byte_index < bytes_per_sample; byte_index += 1)
                {
                    sample_destination[byte_index] = (byte) ((u64) integer_sample >> (byte_index * 8));
                }
            }
        }

        return;
    }

    Memory_Zero(destination, (usize) format->nBlockAlign * frame_count);
}

static HRESULT PlatformAudio_CreateDeviceFromUTF8ID (const c8 *device_id, IMMDevice **device)
{
    HRESULT result;
    IMMDeviceEnumerator *enumerator;
    wchar_t device_id_wide[PLATFORM_AUDIO_TEXT_CAPACITY];

    ASSERT(device_id != NULL);
    ASSERT(device != NULL);

    *device = NULL;
    if (PlatformAudio_CopyUTF8ToWideString(device_id_wide, ARRAY_COUNT(device_id_wide), device_id) == 0)
    {
        return E_FAIL;
    }

    enumerator = NULL;
    result = PlatformAudio_CreateEnumerator(&enumerator);
    if (FAILED(result))
    {
        return result;
    }

    result = enumerator->lpVtbl->GetDevice(enumerator, device_id_wide, device);
    Platform_ReleaseCOMObject((IUnknown *) enumerator);
    return result;
}

static void PlatformAudio_RunOutputCallback (
    PlatformAudioStreamState *stream_state,
    byte *backend_output,
    const WAVEFORMATEX *backend_format,
    f32 *planar_storage,
    f32 **planar_channels,
    u32 frame_count,
    b32 output_underflow_occurred)
{
    PlatformAudioBuffer input_buffer;
    PlatformAudioBuffer output_buffer;
    PlatformAudioCallbackInfo callback_info;
    u32 channel_index;

    ASSERT(stream_state != NULL);
    ASSERT(backend_output != NULL);
    ASSERT(backend_format != NULL);
    ASSERT(planar_storage != NULL);
    ASSERT(planar_channels != NULL);

    for (channel_index = 0; channel_index < stream_state->info.output_channel_count; channel_index += 1)
    {
        planar_channels[channel_index] = planar_storage + ((usize) channel_index * frame_count);
        Memory_Zero(planar_channels[channel_index], sizeof(f32) * frame_count);
    }

    input_buffer.channels = NULL;
    input_buffer.channel_count = 0;
    input_buffer.frame_count = frame_count;

    output_buffer.channels = planar_channels;
    output_buffer.channel_count = stream_state->info.output_channel_count;
    output_buffer.frame_count = frame_count;

    callback_info.input_sample_index = 0;
    callback_info.output_sample_index = stream_state->output_sample_index;
    callback_info.callback_time = Platform_QueryTimestamp();
    callback_info.input_latency = 0;
    callback_info.output_latency = stream_state->info.output_latency;
    callback_info.input_overflow_occurred = false;
    callback_info.output_underflow_occurred = output_underflow_occurred;

    stream_state->desc.callback(input_buffer, output_buffer, &callback_info, stream_state->desc.user_data);
    PlatformAudio_ConvertPlanarF32ToFormat(
        backend_output,
        backend_format,
        planar_channels,
        stream_state->info.output_channel_count,
        frame_count);

    stream_state->output_sample_index += frame_count;
}

static DWORD WINAPI PlatformAudio_OutputThreadProc (LPVOID parameter)
{
    PlatformAudioStreamState *stream_state;
    PlatformAudioDeviceState *output_device_state;
    HRESULT result;
    IMMDevice *device;
    IAudioClient *audio_client;
    IAudioRenderClient *render_client;
    HANDLE mmcss_handle;
    DWORD task_index;
    DWORD buffer_frame_count;
    REFERENCE_TIME default_period;
    REFERENCE_TIME minimum_period;
    REFERENCE_TIME stream_latency;
    WAVEFORMATEX *mix_format;
    WAVEFORMATEXTENSIBLE wave_format;
    WAVEFORMATEX *active_format;
    f32 *planar_storage;
    f32 **planar_channels;
    b32 co_initialized;

    stream_state = (PlatformAudioStreamState *) parameter;
    ASSERT(stream_state != NULL);

    device = NULL;
    audio_client = NULL;
    render_client = NULL;
    mmcss_handle = NULL;
    task_index = 0;
    mix_format = NULL;
    active_format = NULL;
    planar_storage = NULL;
    planar_channels = NULL;
    co_initialized = false;
    InterlockedExchange(&stream_state->start_succeeded, 0);

    result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(result))
    {
        co_initialized = true;
    }
    else if (result != RPC_E_CHANGED_MODE)
    {
        goto startup_failed;
    }

    output_device_state = PlatformAudio_GetDeviceState(stream_state->info.output_device);
    if (output_device_state == NULL)
    {
        goto startup_failed;
    }

    result = PlatformAudio_CreateDeviceFromUTF8ID(output_device_state->device_id, &device);
    if (FAILED(result))
    {
        goto startup_failed;
    }

    result = device->lpVtbl->Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **) &audio_client);
    if (FAILED(result))
    {
        goto startup_failed;
    }

    result = audio_client->lpVtbl->GetMixFormat(audio_client, &mix_format);
    if (FAILED(result) || (mix_format == NULL))
    {
        goto startup_failed;
    }

    result = audio_client->lpVtbl->GetDevicePeriod(audio_client, &default_period, &minimum_period);
    if (FAILED(result))
    {
        goto startup_failed;
    }

    if (mix_format->nChannels != stream_state->info.output_channel_count)
    {
        goto startup_failed;
    }

    if (!PlatformAudio_FindSupportedExclusiveOutputFormat(
            audio_client,
            stream_state->info.output_channel_count,
            stream_state->info.actual_sample_rate,
            mix_format->nSamplesPerSec,
            &wave_format))
    {
        result = AUDCLNT_E_UNSUPPORTED_FORMAT;
        goto startup_failed;
    }

    active_format = (WAVEFORMATEX *) &wave_format;
    stream_state->info.actual_sample_rate = active_format->nSamplesPerSec;

    if (active_format == NULL)
    {
        goto startup_failed;
    }

    result = audio_client->lpVtbl->Initialize(
        audio_client,
        AUDCLNT_SHAREMODE_EXCLUSIVE,
        0,
        minimum_period,
        minimum_period,
        active_format,
        NULL);
    if (FAILED(result))
    {
        goto startup_failed;
    }

    result = audio_client->lpVtbl->GetBufferSize(audio_client, &buffer_frame_count);
    if (FAILED(result) || (buffer_frame_count == 0))
    {
        goto startup_failed;
    }

    result = audio_client->lpVtbl->GetStreamLatency(audio_client, &stream_latency);
    if (FAILED(result))
    {
        stream_latency = minimum_period;
    }

    result = audio_client->lpVtbl->GetService(audio_client, &IID_IAudioRenderClient, (void **) &render_client);
    if (FAILED(result))
    {
        goto startup_failed;
    }

    planar_storage = (f32 *) HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(f32) * (usize) buffer_frame_count * (usize) stream_state->info.output_channel_count);
    planar_channels = (f32 **) HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(f32 *) * (usize) stream_state->info.output_channel_count);
    if ((planar_storage == NULL) || (planar_channels == NULL))
    {
        goto startup_failed;
    }

    mmcss_handle = AvSetMmThreadCharacteristicsA("Pro Audio", &task_index);

    stream_state->info.actual_frame_count = buffer_frame_count;
    stream_state->info.actual_sample_rate = active_format->nSamplesPerSec;
    stream_state->info.output_latency = PlatformAudio_ReferenceTimeToNanoseconds(stream_latency);
    stream_state->output_sample_index = 0;

    {
        BYTE *render_buffer;

        result = render_client->lpVtbl->GetBuffer(render_client, buffer_frame_count, &render_buffer);
        if (FAILED(result))
        {
            goto startup_failed;
        }

        PlatformAudio_RunOutputCallback(
            stream_state,
            render_buffer,
            active_format,
            planar_storage,
            planar_channels,
            buffer_frame_count,
            false);

        result = render_client->lpVtbl->ReleaseBuffer(render_client, buffer_frame_count, 0);
        if (FAILED(result))
        {
            goto startup_failed;
        }
    }

    result = audio_client->lpVtbl->Start(audio_client);
    if (FAILED(result))
    {
        goto startup_failed;
    }

    stream_state->info.is_running = true;
    InterlockedExchange(&stream_state->thread_is_active, 1);
    InterlockedExchange(&stream_state->start_succeeded, 1);
    SetEvent(stream_state->started_event);

    for (;;)
    {
        DWORD wait_result;
        Milliseconds sleep_duration;
        UINT32 padding;
        UINT32 frames_available;
        BYTE *render_buffer;

        sleep_duration = MAX(1, Milliseconds_FromNanoseconds(PlatformAudio_ReferenceTimeToNanoseconds(minimum_period) / 2));
        wait_result = WaitForSingleObject(stream_state->stop_event, (DWORD) sleep_duration);
        if (wait_result == WAIT_OBJECT_0)
        {
            break;
        }

        if (wait_result == WAIT_TIMEOUT)
        {
            result = audio_client->lpVtbl->GetCurrentPadding(audio_client, &padding);
            if (FAILED(result))
            {
                break;
            }

            frames_available = buffer_frame_count - padding;
            if (frames_available > 0)
            {
                result = render_client->lpVtbl->GetBuffer(render_client, frames_available, &render_buffer);
                if (FAILED(result))
                {
                    break;
                }

                PlatformAudio_RunOutputCallback(
                    stream_state,
                    render_buffer,
                    active_format,
                    planar_storage,
                    planar_channels,
                    frames_available,
                    false);

                result = render_client->lpVtbl->ReleaseBuffer(render_client, frames_available, 0);
                if (FAILED(result))
                {
                    break;
                }
            }

            continue;
        }

        break;
    }

    audio_client->lpVtbl->Stop(audio_client);

cleanup:
    stream_state->info.is_running = false;
    InterlockedExchange(&stream_state->thread_is_active, 0);

    if (mmcss_handle != NULL)
    {
        AvRevertMmThreadCharacteristics(mmcss_handle);
    }
    if (planar_channels != NULL)
    {
        HeapFree(GetProcessHeap(), 0, planar_channels);
    }
    if (planar_storage != NULL)
    {
        HeapFree(GetProcessHeap(), 0, planar_storage);
    }
    if (mix_format != NULL)
    {
        CoTaskMemFree(mix_format);
    }
    Platform_ReleaseCOMObject((IUnknown *) render_client);
    Platform_ReleaseCOMObject((IUnknown *) audio_client);
    Platform_ReleaseCOMObject((IUnknown *) device);
    if (co_initialized)
    {
        CoUninitialize();
    }

    return 0;

startup_failed:
    SetEvent(stream_state->started_event);
    goto cleanup;
}

static b32 PlatformAudio_LoadDeviceState (PlatformAudioDeviceState *device_state, IMMDevice *device, b32 is_default_input, b32 is_default_output)
{
    HRESULT result;
    LPWSTR device_id_wide;
    IMMEndpoint *endpoint;
    EDataFlow data_flow;
    IPropertyStore *property_store;
    PROPVARIANT property_value;
    IAudioClient *audio_client;
    WAVEFORMATEX *mix_format;
    REFERENCE_TIME default_period;
    REFERENCE_TIME minimum_period;

    ASSERT(device_state != NULL);
    ASSERT(device != NULL);

    Memory_Zero(device_state, sizeof(*device_state));

    device_id_wide = NULL;
    endpoint = NULL;
    property_store = NULL;
    PropVariantInit(&property_value);
    audio_client = NULL;
    mix_format = NULL;

    result = device->lpVtbl->GetId(device, &device_id_wide);
    if (FAILED(result))
    {
        goto cleanup;
    }

    device_state->device_id_count = PlatformAudio_CopyWideStringToUTF8(
        device_state->device_id,
        ARRAY_COUNT(device_state->device_id),
        device_id_wide);
    if (device_state->device_id_count == 0)
    {
        goto cleanup;
    }

    result = device->lpVtbl->OpenPropertyStore(device, STGM_READ, &property_store);
    if (FAILED(result))
    {
        goto cleanup;
    }

    result = property_store->lpVtbl->GetValue(property_store, &PLATFORM_PKEY_DEVICE_FRIENDLY_NAME, &property_value);
    if (FAILED(result) || (property_value.vt != VT_LPWSTR))
    {
        goto cleanup;
    }

    device_state->name_count = PlatformAudio_CopyWideStringToUTF8(
        device_state->name,
        ARRAY_COUNT(device_state->name),
        property_value.pwszVal);
    if (device_state->name_count == 0)
    {
        goto cleanup;
    }

    result = device->lpVtbl->QueryInterface(device, &IID_IMMEndpoint, (void **) &endpoint);
    if (FAILED(result))
    {
        goto cleanup;
    }

    result = endpoint->lpVtbl->GetDataFlow(endpoint, &data_flow);
    if (FAILED(result))
    {
        goto cleanup;
    }

    if (data_flow == eCapture)
    {
        device_state->direction = PLATFORM_AUDIO_DEVICE_DIRECTION_INPUT;
    }
    else if (data_flow == eRender)
    {
        device_state->direction = PLATFORM_AUDIO_DEVICE_DIRECTION_OUTPUT;
    }
    else
    {
        goto cleanup;
    }

    result = device->lpVtbl->Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **) &audio_client);
    if (FAILED(result))
    {
        goto cleanup;
    }

    result = audio_client->lpVtbl->GetMixFormat(audio_client, &mix_format);
    if (FAILED(result) || (mix_format == NULL))
    {
        goto cleanup;
    }

    result = audio_client->lpVtbl->GetDevicePeriod(audio_client, &default_period, &minimum_period);
    if (FAILED(result))
    {
        goto cleanup;
    }

    device_state->preferred_sample_rate = mix_format->nSamplesPerSec;
    device_state->preferred_frame_count = (u32) (((u64) minimum_period * (u64) mix_format->nSamplesPerSec) / 10000000ull);
    if (device_state->preferred_frame_count == 0)
    {
        device_state->preferred_frame_count = 1;
    }

    if (device_state->direction == PLATFORM_AUDIO_DEVICE_DIRECTION_INPUT)
    {
        device_state->input_channel_count = mix_format->nChannels;
        device_state->default_low_input_latency = PlatformAudio_ReferenceTimeToNanoseconds(minimum_period);
    }
    else
    {
        device_state->output_channel_count = mix_format->nChannels;
        device_state->default_low_output_latency = PlatformAudio_ReferenceTimeToNanoseconds(minimum_period);
    }

    device_state->supports_exclusive = true;
    device_state->is_default_input = is_default_input;
    device_state->is_default_output = is_default_output;
    return true;

cleanup:
    if (mix_format != NULL)
    {
        CoTaskMemFree(mix_format);
    }

    Platform_ReleaseCOMObject((IUnknown *) audio_client);
    PropVariantClear(&property_value);
    Platform_ReleaseCOMObject((IUnknown *) property_store);
    Platform_ReleaseCOMObject((IUnknown *) endpoint);
    if (device_id_wide != NULL)
    {
        CoTaskMemFree(device_id_wide);
    }

    return false;
}

static usize PlatformAudio_GetDeviceIDUTF8 (IMMDevice *device, c8 *buffer, usize buffer_capacity)
{
    HRESULT result;
    LPWSTR device_id_wide;
    usize count;

    ASSERT(device != NULL);
    ASSERT(buffer != NULL);
    ASSERT(buffer_capacity > 0);

    device_id_wide = NULL;
    buffer[0] = 0;

    result = device->lpVtbl->GetId(device, &device_id_wide);
    if (FAILED(result))
    {
        return 0;
    }

    count = PlatformAudio_CopyWideStringToUTF8(buffer, buffer_capacity, device_id_wide);
    CoTaskMemFree(device_id_wide);
    return count;
}

static void PlatformAudio_ClearDevices (void)
{
    usize index;

    for (index = 0; index < ARRAY_COUNT(platform_audio_state.devices); index += 1)
    {
        PlatformAudioDeviceState *device_state;

        device_state = &platform_audio_state.devices[index];
        device_state->is_used = false;
        device_state->handle.index = (u32) index + 1;
        device_state->handle.generation += 1;
        if (device_state->handle.generation == 0)
        {
            device_state->handle.generation = 1;
        }
    }

    platform_audio_state.device_count = 0;
    platform_audio_state.devices_are_loaded = false;
}

static void PlatformAudio_RefreshDevices (void)
{
    HRESULT result;
    IMMDeviceEnumerator *enumerator;
    IMMDeviceCollection *collection;
    IMMDevice *default_input_device;
    IMMDevice *default_output_device;
    c8 default_input_id[PLATFORM_AUDIO_TEXT_CAPACITY];
    c8 default_output_id[PLATFORM_AUDIO_TEXT_CAPACITY];
    usize default_input_id_count;
    usize default_output_id_count;
    UINT count;
    UINT index;

    PlatformAudio_ClearDevices();

    enumerator = NULL;
    collection = NULL;
    default_input_device = NULL;
    default_output_device = NULL;
    default_input_id_count = 0;
    default_output_id_count = 0;

    result = PlatformAudio_CreateEnumerator(&enumerator);
    if (FAILED(result))
    {
        goto cleanup;
    }

    enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eCapture, eConsole, &default_input_device);
    enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &default_output_device);
    if (default_input_device != NULL)
    {
        default_input_id_count = PlatformAudio_GetDeviceIDUTF8(
            default_input_device,
            default_input_id,
            ARRAY_COUNT(default_input_id));
    }
    if (default_output_device != NULL)
    {
        default_output_id_count = PlatformAudio_GetDeviceIDUTF8(
            default_output_device,
            default_output_id,
            ARRAY_COUNT(default_output_id));
    }

    result = enumerator->lpVtbl->EnumAudioEndpoints(enumerator, eAll, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(result))
    {
        goto cleanup;
    }

    result = collection->lpVtbl->GetCount(collection, &count);
    if (FAILED(result))
    {
        goto cleanup;
    }

    for (index = 0; index < count; index += 1)
    {
        IMMDevice *device;
        PlatformAudioDeviceState loaded_state;
        PlatformAudioDeviceState *device_state;
        b32 is_default_input;
        b32 is_default_output;

        if (platform_audio_state.device_count >= ARRAY_COUNT(platform_audio_state.devices))
        {
            break;
        }

        device = NULL;
        result = collection->lpVtbl->Item(collection, index, &device);
        if (FAILED(result))
        {
            continue;
        }

        if (PlatformAudio_LoadDeviceState(&loaded_state, device, false, false))
        {
            is_default_input =
                (default_input_id_count > 0) &&
                (loaded_state.device_id_count == default_input_id_count) &&
                (Memory_Compare(loaded_state.device_id, default_input_id, default_input_id_count) == 0);
            is_default_output =
                (default_output_id_count > 0) &&
                (loaded_state.device_id_count == default_output_id_count) &&
                (Memory_Compare(loaded_state.device_id, default_output_id, default_output_id_count) == 0);
            loaded_state.is_default_input = is_default_input;
            loaded_state.is_default_output = is_default_output;
            device_state = &platform_audio_state.devices[platform_audio_state.device_count];
            loaded_state.is_used = true;
            loaded_state.handle = device_state->handle;
            *device_state = loaded_state;
            platform_audio_state.device_count += 1;
        }

        Platform_ReleaseCOMObject((IUnknown *) device);
    }

    platform_audio_state.devices_are_loaded = true;

cleanup:
    Platform_ReleaseCOMObject((IUnknown *) default_output_device);
    Platform_ReleaseCOMObject((IUnknown *) default_input_device);
    Platform_ReleaseCOMObject((IUnknown *) collection);
    Platform_ReleaseCOMObject((IUnknown *) enumerator);
}

static void PlatformAudio_EnsureDevicesLoaded (void)
{
    if (!platform_audio_state.devices_are_loaded)
    {
        PlatformAudio_RefreshDevices();
    }
}

void PlatformAudio_Initialize (void)
{
    HRESULT result;

    result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(result))
    {
        platform_audio_state.com_initialized_by_platform = true;
    }

    PlatformAudio_RefreshDevices();
}

void PlatformAudio_Shutdown (void)
{
    usize index;

    for (index = 0; index < ARRAY_COUNT(platform_audio_state.streams); index += 1)
    {
        if (platform_audio_state.streams[index].is_used)
        {
            PlatformAudio_ReleaseStreamState(&platform_audio_state.streams[index]);
        }
    }

    PlatformAudio_ClearDevices();

    if (platform_audio_state.com_initialized_by_platform)
    {
        CoUninitialize();
    }

    Memory_Zero(&platform_audio_state, sizeof(platform_audio_state));
}

usize PlatformAudio_GetDeviceCount (void)
{
    PlatformAudio_EnsureDevicesLoaded();
    return platform_audio_state.device_count;
}

PlatformAudioDevice PlatformAudio_GetDeviceByIndex (usize index)
{
    PlatformAudio_EnsureDevicesLoaded();

    if (index >= platform_audio_state.device_count)
    {
        return PLATFORM_AUDIO_DEVICE_INVALID;
    }

    return PlatformAudio_PackDeviceHandle(platform_audio_state.devices[index].handle);
}

PlatformAudioDevice PlatformAudio_GetDefaultInputDevice (void)
{
    usize index;

    PlatformAudio_EnsureDevicesLoaded();

    for (index = 0; index < platform_audio_state.device_count; index += 1)
    {
        if (platform_audio_state.devices[index].is_default_input)
        {
            return PlatformAudio_PackDeviceHandle(platform_audio_state.devices[index].handle);
        }
    }

    return PLATFORM_AUDIO_DEVICE_INVALID;
}

PlatformAudioDevice PlatformAudio_GetDefaultOutputDevice (void)
{
    usize index;

    PlatformAudio_EnsureDevicesLoaded();

    for (index = 0; index < platform_audio_state.device_count; index += 1)
    {
        if (platform_audio_state.devices[index].is_default_output)
        {
            return PlatformAudio_PackDeviceHandle(platform_audio_state.devices[index].handle);
        }
    }

    return PLATFORM_AUDIO_DEVICE_INVALID;
}

b32 PlatformAudio_GetDeviceInfo (PlatformAudioDevice device, MemoryArena *arena, PlatformAudioDeviceInfo *info)
{
    PlatformAudioDeviceState *device_state;

    ASSERT(arena != NULL);
    ASSERT(info != NULL);

    PlatformAudio_EnsureDevicesLoaded();

    device_state = PlatformAudio_GetDeviceState(device);
    if (device_state == NULL)
    {
        return false;
    }

    Memory_Zero(info, sizeof(*info));
    info->device = device;
    info->name = PlatformAudio_CopyFixedStringToArena(arena, device_state->name, device_state->name_count);
    info->direction = device_state->direction;
    info->input_channel_count = device_state->input_channel_count;
    info->output_channel_count = device_state->output_channel_count;
    info->preferred_sample_rate = device_state->preferred_sample_rate;
    info->preferred_frame_count = device_state->preferred_frame_count;
    info->default_low_input_latency = device_state->default_low_input_latency;
    info->default_low_output_latency = device_state->default_low_output_latency;
    info->supports_exclusive = device_state->supports_exclusive;
    info->is_default_input = device_state->is_default_input;
    info->is_default_output = device_state->is_default_output;
    return !String_IsEmpty(info->name);
}

PlatformAudioStream PlatformAudioStream_Create (const PlatformAudioStreamDesc *desc)
{
    PlatformAudioDeviceState *input_device_state;
    PlatformAudioDeviceState *output_device_state;
    PlatformAudioStreamState *stream_state;

    ASSERT(desc != NULL);

    if (desc->callback == NULL)
    {
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    if (desc->sample_format != PLATFORM_AUDIO_SAMPLE_FORMAT_F32)
    {
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    if ((desc->input_channel_count == 0) && (desc->output_channel_count == 0))
    {
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    if (desc->input_channel_count > 0)
    {
        /* Input and duplex are not implemented in this first streaming slice. */
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    if ((desc->requested_sample_rate == 0) || (desc->requested_frame_count == 0))
    {
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    PlatformAudio_EnsureDevicesLoaded();

    input_device_state = NULL;
    output_device_state = NULL;

    if (desc->input_channel_count > 0)
    {
        input_device_state = PlatformAudio_GetDeviceState(desc->input_device);
        if ((input_device_state == NULL) ||
            ((input_device_state->direction & PLATFORM_AUDIO_DEVICE_DIRECTION_INPUT) == 0))
        {
            return PLATFORM_AUDIO_STREAM_INVALID;
        }
    }

    if (desc->output_channel_count > 0)
    {
        output_device_state = PlatformAudio_GetDeviceState(desc->output_device);
        if ((output_device_state == NULL) ||
            ((output_device_state->direction & PLATFORM_AUDIO_DEVICE_DIRECTION_OUTPUT) == 0))
        {
            return PLATFORM_AUDIO_STREAM_INVALID;
        }
    }

    stream_state = PlatformAudio_AllocateStreamState();
    if (stream_state == NULL)
    {
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    stream_state->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    stream_state->started_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    stream_state->audio_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if ((stream_state->stop_event == NULL) ||
        (stream_state->started_event == NULL) ||
        (stream_state->audio_event == NULL))
    {
        PlatformAudio_ReleaseStreamState(stream_state);
        return PLATFORM_AUDIO_STREAM_INVALID;
    }

    stream_state->desc = *desc;
    stream_state->info.input_device = desc->input_device;
    stream_state->info.output_device = desc->output_device;
    stream_state->info.input_channel_count = desc->input_channel_count;
    stream_state->info.output_channel_count = desc->output_channel_count;
    stream_state->info.actual_sample_rate = desc->requested_sample_rate;
    stream_state->info.actual_frame_count = desc->requested_frame_count;
    stream_state->info.sample_format = desc->sample_format;
    stream_state->info.input_latency = (input_device_state != NULL) ? input_device_state->default_low_input_latency : 0;
    stream_state->info.output_latency = (output_device_state != NULL) ? output_device_state->default_low_output_latency : 0;
    stream_state->info.is_running = false;

    return PlatformAudio_PackStreamHandle(stream_state->handle);
}

void PlatformAudioStream_Destroy (PlatformAudioStream stream)
{
    PlatformAudioStreamState *stream_state;

    stream_state = PlatformAudio_GetStreamState(stream);
    if (stream_state == NULL)
    {
        return;
    }

    PlatformAudio_StopStreamState(stream_state);
    PlatformAudio_ReleaseStreamState(stream_state);
}

b32 PlatformAudioStream_IsValid (PlatformAudioStream stream)
{
    return PlatformAudio_GetStreamState(stream) != NULL;
}

b32 PlatformAudioStream_Start (PlatformAudioStream stream)
{
    PlatformAudioStreamState *stream_state;
    DWORD thread_id;

    stream_state = PlatformAudio_GetStreamState(stream);
    if (stream_state == NULL)
    {
        return false;
    }

    if (stream_state->info.is_running || (InterlockedCompareExchange(&stream_state->thread_is_active, 1, 1) != 0))
    {
        return false;
    }

    ResetEvent(stream_state->stop_event);
    ResetEvent(stream_state->started_event);
    InterlockedExchange(&stream_state->start_succeeded, 0);

    stream_state->thread_handle = CreateThread(
        NULL,
        0,
        PlatformAudio_OutputThreadProc,
        stream_state,
        0,
        &thread_id);
    if (stream_state->thread_handle == NULL)
    {
        return false;
    }

    if (WaitForSingleObject(stream_state->started_event, 5000) != WAIT_OBJECT_0)
    {
        PlatformAudio_StopStreamState(stream_state);
        return false;
    }

    if (InterlockedCompareExchange(&stream_state->start_succeeded, 0, 0) == 0)
    {
        PlatformAudio_StopStreamState(stream_state);
        return false;
    }

    return true;
}

void PlatformAudioStream_Stop (PlatformAudioStream stream)
{
    PlatformAudioStreamState *stream_state;

    stream_state = PlatformAudio_GetStreamState(stream);
    if (stream_state == NULL)
    {
        return;
    }

    PlatformAudio_StopStreamState(stream_state);
}

b32 PlatformAudioStream_IsRunning (PlatformAudioStream stream)
{
    PlatformAudioStreamState *stream_state;

    stream_state = PlatformAudio_GetStreamState(stream);
    if (stream_state == NULL)
    {
        return false;
    }

    return stream_state->info.is_running;
}

b32 PlatformAudioStream_GetInfo (PlatformAudioStream stream, PlatformAudioStreamInfo *info)
{
    PlatformAudioStreamState *stream_state;

    ASSERT(info != NULL);

    stream_state = PlatformAudio_GetStreamState(stream);
    if (stream_state == NULL)
    {
        return false;
    }

    *info = stream_state->info;
    return true;
}
