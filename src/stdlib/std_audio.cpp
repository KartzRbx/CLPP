// @clpp.audio — sound effects and music: WAV files and synthesized tones, mixed in software.
//
// Sounds are decoded once into 44.1 kHz stereo floats. Playing a sound starts a voice (volume,
// pan, pitch, loop) that a mixer thread adds into the output; on Windows the mix goes to the
// default device through waveOut (winmm, no extra DLLs). Without a device (other systems for now,
// CLPP_HEADLESS, or no sound card) everything still works except that nothing is heard, so
// programs and tests behave the same. Audio.SaveWav writes any sound to a .wav file.

#include "stdlib/host.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

namespace clpp::stdlib::host {

namespace {

constexpr int kRate = 44100;
constexpr double kPi = 3.14159265358979323846;

struct Sound {
  std::vector<float> samples;  // interleaved stereo
  [[nodiscard]] std::size_t frames() const { return samples.size() / 2; }
};

struct Voice {
  int sound{0};
  double position{0};  // in frames, fractional for pitch
  double volume{1};
  double pan{0};
  double pitch{1};
  bool loop{false};
  bool playing{false};
};

struct Mixer {
  std::mutex mutex;
  std::vector<Sound> sounds;  // handle = index + 1
  std::vector<Voice> voices;  // voice id = index + 1
  double master{1};
  bool started{false};
  bool device{false};
  std::atomic<bool> running{false};
  std::thread thread;
#ifdef _WIN32
  HWAVEOUT out{nullptr};
#endif

  ~Mixer() {
    running = false;
    if (thread.joinable()) {
      thread.join();
    }
#ifdef _WIN32
    if (out != nullptr) {
      waveOutReset(out);
      waveOutClose(out);
    }
#endif
  }
};

Mixer& mixer() {
  static Mixer instance;
  return instance;
}

// Adds `frames` frames of every playing voice into `out` (interleaved stereo). Caller holds the lock.
void mix(float* out, const std::size_t frames) {
  Mixer& m = mixer();
  std::fill(out, out + frames * 2, 0.0f);
  for (Voice& voice : m.voices) {
    if (!voice.playing || voice.sound <= 0 || static_cast<std::size_t>(voice.sound) > m.sounds.size()) {
      continue;
    }
    const Sound& sound = m.sounds[static_cast<std::size_t>(voice.sound - 1)];
    const std::size_t length = sound.frames();
    if (length == 0) {
      voice.playing = false;
      continue;
    }
    const double left_gain = voice.volume * m.master * std::min(1.0, 1.0 - voice.pan);
    const double right_gain = voice.volume * m.master * std::min(1.0, 1.0 + voice.pan);
    for (std::size_t frame = 0; frame < frames; ++frame) {
      auto index = static_cast<std::size_t>(voice.position);
      if (index >= length) {
        if (!voice.loop) {
          voice.playing = false;
          break;
        }
        voice.position = std::fmod(voice.position, static_cast<double>(length));
        index = static_cast<std::size_t>(voice.position);
      }
      const std::size_t next = index + 1 < length ? index + 1 : (voice.loop ? 0 : index);
      const double t = voice.position - static_cast<double>(index);
      const double left = static_cast<double>(sound.samples[index * 2]) * (1 - t) + static_cast<double>(sound.samples[next * 2]) * t;
      const double right =
          static_cast<double>(sound.samples[index * 2 + 1]) * (1 - t) + static_cast<double>(sound.samples[next * 2 + 1]) * t;
      out[frame * 2] += static_cast<float>(left * left_gain);
      out[frame * 2 + 1] += static_cast<float>(right * right_gain);
      voice.position += voice.pitch;
    }
  }
}

#ifdef _WIN32
void run_device() {
  Mixer& m = mixer();
  constexpr std::size_t kFrames = 1024;  // ~23 ms per buffer
  constexpr int kBuffers = 4;
  std::vector<std::int16_t> pcm[kBuffers];
  WAVEHDR headers[kBuffers]{};
  std::vector<float> scratch(kFrames * 2);
  for (int index = 0; index < kBuffers; ++index) {
    pcm[index].assign(kFrames * 2, 0);
    headers[index].lpData = reinterpret_cast<LPSTR>(pcm[index].data());
    headers[index].dwBufferLength = static_cast<DWORD>(kFrames * 2 * sizeof(std::int16_t));
    waveOutPrepareHeader(m.out, &headers[index], sizeof(WAVEHDR));
    waveOutWrite(m.out, &headers[index], sizeof(WAVEHDR));
  }
  while (m.running) {
    bool wrote = false;
    for (int index = 0; index < kBuffers; ++index) {
      if ((headers[index].dwFlags & WHDR_DONE) == 0) {
        continue;
      }
      {
        const std::lock_guard<std::mutex> lock(m.mutex);
        mix(scratch.data(), kFrames);
      }
      for (std::size_t sample = 0; sample < kFrames * 2; ++sample) {
        const float value = std::clamp(scratch[sample], -1.0f, 1.0f);
        pcm[index][sample] = static_cast<std::int16_t>(value * 32767.0f);
      }
      waveOutWrite(m.out, &headers[index], sizeof(WAVEHDR));
      wrote = true;
    }
    if (!wrote) {
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }
  waveOutReset(m.out);
  for (int index = 0; index < kBuffers; ++index) {
    waveOutUnprepareHeader(m.out, &headers[index], sizeof(WAVEHDR));
  }
}
#endif

void ensure_started() {
  Mixer& m = mixer();
  if (m.started) {
    return;
  }
  m.started = true;
  if (std::getenv("CLPP_HEADLESS") != nullptr) {
    return;
  }
#ifdef _WIN32
  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = 2;
  format.nSamplesPerSec = kRate;
  format.wBitsPerSample = 16;
  format.nBlockAlign = 4;
  format.nAvgBytesPerSec = kRate * 4;
  if (waveOutOpen(&m.out, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
    m.device = true;
    m.running = true;
    m.thread = std::thread(run_device);
  }
#endif
}

[[nodiscard]] int add_sound(Sound sound) {
  Mixer& m = mixer();
  const std::lock_guard<std::mutex> lock(m.mutex);
  m.sounds.push_back(std::move(sound));
  return static_cast<int>(m.sounds.size());
}

// --- WAV ------------------------------------------------------------------------------------------

[[nodiscard]] std::uint32_t le(const std::vector<std::uint8_t>& data, const std::size_t at, const int bytes) {
  std::uint32_t value = 0;
  for (int index = 0; index < bytes; ++index) {
    value |= static_cast<std::uint32_t>(data[at + static_cast<std::size_t>(index)]) << (8U * static_cast<std::uint32_t>(index));
  }
  return value;
}

bool decode_wav(const std::vector<std::uint8_t>& file, Sound& sound, std::string& error) {
  if (file.size() < 12 || std::string(file.begin(), file.begin() + 4) != "RIFF" ||
      std::string(file.begin() + 8, file.begin() + 12) != "WAVE") {
    error = "not a WAV file";
    return false;
  }
  int format = 0;
  int channels = 0;
  int rate = 0;
  int bits = 0;
  std::size_t data_at = 0;
  std::size_t data_size = 0;
  std::size_t pos = 12;
  while (pos + 8 <= file.size()) {
    const std::string id(file.begin() + static_cast<std::ptrdiff_t>(pos), file.begin() + static_cast<std::ptrdiff_t>(pos + 4));
    const std::size_t size = le(file, pos + 4, 4);
    const std::size_t body = pos + 8;
    if (id == "fmt " && body + 16 <= file.size()) {
      format = static_cast<int>(le(file, body, 2));
      channels = static_cast<int>(le(file, body + 2, 2));
      rate = static_cast<int>(le(file, body + 4, 4));
      bits = static_cast<int>(le(file, body + 14, 2));
      if (format == 0xFFFE && size >= 40 && body + 26 <= file.size()) {
        format = static_cast<int>(le(file, body + 24, 2));  // WAVE_FORMAT_EXTENSIBLE: real format in the GUID
      }
    } else if (id == "data") {
      data_at = body;
      data_size = std::min(size, file.size() - body);
    }
    pos = body + size + (size & 1U);
  }
  if (data_at == 0 || channels < 1 || channels > 8 || rate <= 0 || (format != 1 && format != 3) ||
      (format == 1 && bits != 8 && bits != 16 && bits != 24 && bits != 32) || (format == 3 && bits != 32)) {
    error = "unsupported WAV (use PCM 8/16/24/32-bit or 32-bit float)";
    return false;
  }
  const std::size_t bytes = static_cast<std::size_t>(bits / 8);
  const std::size_t frames = data_size / (bytes * static_cast<std::size_t>(channels));
  const auto sample_at = [&](const std::size_t frame, const int channel) -> double {
    const std::size_t at = data_at + (frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)) * bytes;
    if (format == 3) {
      float value = 0;
      const std::uint32_t raw = le(file, at, 4);
      std::memcpy(&value, &raw, sizeof(value));
      return value;
    }
    switch (bits) {
      case 8:
        return (static_cast<double>(file[at]) - 128.0) / 128.0;
      case 16:
        return static_cast<double>(static_cast<std::int16_t>(le(file, at, 2))) / 32768.0;
      case 24: {
        std::int32_t value = static_cast<std::int32_t>(le(file, at, 3) << 8U) >> 8;
        return static_cast<double>(value) / 8388608.0;
      }
      default:
        return static_cast<double>(static_cast<std::int32_t>(le(file, at, 4))) / 2147483648.0;
    }
  };
  // resample linearly to 44.1 kHz stereo
  const double step = static_cast<double>(rate) / kRate;
  const auto out_frames = static_cast<std::size_t>(static_cast<double>(frames) / step);
  sound.samples.resize(out_frames * 2);
  for (std::size_t frame = 0; frame < out_frames; ++frame) {
    const double source = static_cast<double>(frame) * step;
    const auto index = static_cast<std::size_t>(source);
    const std::size_t next = std::min(index + 1, frames - 1);
    const double t = source - static_cast<double>(index);
    for (int channel = 0; channel < 2; ++channel) {
      const int from = channels == 1 ? 0 : channel;
      const double value = sample_at(index, from) * (1 - t) + sample_at(next, from) * t;
      sound.samples[frame * 2 + static_cast<std::size_t>(channel)] = static_cast<float>(value);
    }
  }
  return true;
}

// --- synthesis -----------------------------------------------------------------------------------

[[nodiscard]] double wave_at(const std::string& wave, const double phase, std::uint32_t& noise) {
  const double cycle = phase - std::floor(phase);
  if (wave == "square") {
    return cycle < 0.5 ? 0.6 : -0.6;
  }
  if (wave == "saw") {
    return (2.0 * cycle - 1.0) * 0.6;
  }
  if (wave == "triangle") {
    return (cycle < 0.5 ? 4.0 * cycle - 1.0 : 3.0 - 4.0 * cycle) * 0.9;
  }
  if (wave == "noise") {
    noise ^= noise << 13U;
    noise ^= noise >> 17U;
    noise ^= noise << 5U;
    return (static_cast<double>(noise & 0xFFFFU) / 32767.5 - 1.0) * 0.5;
  }
  return std::sin(2.0 * kPi * cycle);
}

[[nodiscard]] Sound synthesize(const double from, const double to, const double seconds, const std::string& wave) {
  Sound sound;
  const auto frames = static_cast<std::size_t>(std::clamp(seconds, 0.0, 60.0) * kRate);
  sound.samples.resize(frames * 2);
  double phase = 0;
  std::uint32_t noise = 0x12345678U;
  const double attack = std::min(0.01, seconds / 4);
  const double release = std::min(0.05, seconds / 3);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const double time = static_cast<double>(frame) / kRate;
    const double progress = frames > 1 ? static_cast<double>(frame) / static_cast<double>(frames - 1) : 0;
    const double frequency = from + (to - from) * progress;
    phase += frequency / kRate;
    double envelope = 1.0;
    if (time < attack) {
      envelope = time / attack;
    } else if (time > seconds - release) {
      envelope = std::max(0.0, (seconds - time) / release);
    }
    const auto value = static_cast<float>(wave_at(wave, phase, noise) * envelope * 0.5);
    sound.samples[frame * 2] = value;
    sound.samples[frame * 2 + 1] = value;
  }
  return sound;
}

void write_le(std::ofstream& out, const std::uint32_t value, const int bytes) {
  for (int index = 0; index < bytes; ++index) {
    out.put(static_cast<char>((value >> (8U * static_cast<std::uint32_t>(index))) & 0xFFU));
  }
}

[[nodiscard]] Voice* voice_at(const double id) {
  Mixer& m = mixer();
  const auto index = static_cast<long long>(id) - 1;
  if (index < 0 || static_cast<std::size_t>(index) >= m.voices.size()) {
    return nullptr;
  }
  return &m.voices[static_cast<std::size_t>(index)];
}

#define CLPP_NATIVE(name) bool name(const Value* args, const std::uint8_t arity, Value& out, std::string& error)

CLPP_NATIVE(audio_load) {
  if (arity != 1 || !args[0].is_string()) {
    return fail(error, "Audio.Load: expected a .wav file path");
  }
  const std::string& path = args[0].text;
  if (path.find("..") != std::string::npos) {
    return fail(error, "Audio.Load: paths with '..' are refused");
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return fail(error, "cannot open sound: " + path);
  }
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  Sound sound;
  std::string reason;
  if (!decode_wav(bytes, sound, reason)) {
    return fail(error, "cannot load sound " + path + ": " + reason);
  }
  out = Value::number_of(add_sound(std::move(sound)));
  return true;
}

CLPP_NATIVE(audio_tone) {  // (from Hz, to Hz, seconds, wave)
  if (arity != 4 || !args[0].is_number() || !args[1].is_number() || !args[2].is_number() || !args[3].is_string()) {
    return fail(error, "Audio.Tone: expected (frequency, seconds, wave)");
  }
  out = Value::number_of(add_sound(synthesize(args[0].number, args[1].number, args[2].number, args[3].text)));
  return true;
}

CLPP_NATIVE(audio_play) {  // (sound, volume, pan, pitch, loop) -> voice
  if (!numbers(args, arity, error, "Audio.Play")) {
    return false;
  }
  ensure_started();
  Mixer& m = mixer();
  const std::lock_guard<std::mutex> lock(m.mutex);
  const auto sound = static_cast<int>(args[0].number);
  if (sound <= 0 || static_cast<std::size_t>(sound) > m.sounds.size()) {
    return fail(error, "Audio.Play: invalid sound handle " + std::to_string(sound));
  }
  Voice voice{sound, 0, std::max(0.0, args[1].number), std::clamp(args[2].number, -1.0, 1.0),
              std::clamp(args[3].number, 0.05, 8.0), args[4].number != 0, true};
  if (!m.device) {
    voice.playing = voice.loop;  // nothing to hear: a one-shot sound is over at once, so waits never hang
  }
  for (std::size_t index = 0; index < m.voices.size(); ++index) {  // reuse a finished voice
    if (!m.voices[index].playing) {
      m.voices[index] = voice;
      out = Value::number_of(static_cast<double>(index + 1));
      return true;
    }
  }
  m.voices.push_back(voice);
  out = Value::number_of(static_cast<double>(m.voices.size()));
  return true;
}

CLPP_NATIVE(audio_voice) {  // (voice, which, value): 0 stop, 1 volume, 2 pan, 3 pitch, 4 is playing
  if (!numbers(args, arity, error, "Audio voice")) {
    return false;
  }
  Mixer& m = mixer();
  const std::lock_guard<std::mutex> lock(m.mutex);
  Voice* voice = voice_at(args[0].number);
  const int which = static_cast<int>(args[1].number);
  if (which == 4) {
    out = boolean(voice != nullptr && voice->playing);
    return true;
  }
  if (voice == nullptr) {
    return true;  // a voice that never existed or was reused: nothing to change
  }
  switch (which) {
    case 0:
      voice->playing = false;
      break;
    case 1:
      voice->volume = std::max(0.0, args[2].number);
      break;
    case 2:
      voice->pan = std::clamp(args[2].number, -1.0, 1.0);
      break;
    default:
      voice->pitch = std::clamp(args[2].number, 0.05, 8.0);
      break;
  }
  return true;
}

CLPP_NATIVE(audio_global) {  // (which, value): 0 stop all, 1 master volume, 2 device available, 3 voices playing
  (void)arity;
  (void)error;
  Mixer& m = mixer();
  const std::lock_guard<std::mutex> lock(m.mutex);
  switch (static_cast<int>(num(args[0]))) {
    case 0:
      for (Voice& voice : m.voices) {
        voice.playing = false;
      }
      break;
    case 1:
      m.master = std::clamp(num(args[1]), 0.0, 4.0);
      break;
    case 2:
      out = boolean(m.device);
      break;
    default:
      out = Value::number_of(static_cast<double>(std::count_if(m.voices.begin(), m.voices.end(),
                                                               [](const Voice& voice) { return voice.playing; })));
      break;
  }
  return true;
}

CLPP_NATIVE(audio_duration) {
  if (!numbers(args, arity, error, "Audio.Duration")) {
    return false;
  }
  Mixer& m = mixer();
  const std::lock_guard<std::mutex> lock(m.mutex);
  const auto sound = static_cast<std::size_t>(std::max(0.0, args[0].number));
  if (sound == 0 || sound > m.sounds.size()) {
    return fail(error, "Audio.Duration: invalid sound handle");
  }
  out = Value::number_of(static_cast<double>(m.sounds[sound - 1].frames()) / kRate);
  return true;
}

CLPP_NATIVE(audio_save) {  // (sound, path)
  if (arity != 2 || !args[0].is_number() || !args[1].is_string()) {
    return fail(error, "Audio.SaveWav: expected (sound, string path)");
  }
  if (args[1].text.find("..") != std::string::npos) {
    return fail(error, "Audio.SaveWav: paths with '..' are refused");
  }
  Mixer& m = mixer();
  const std::lock_guard<std::mutex> lock(m.mutex);
  const auto sound = static_cast<std::size_t>(std::max(0.0, args[0].number));
  if (sound == 0 || sound > m.sounds.size()) {
    return fail(error, "Audio.SaveWav: invalid sound handle");
  }
  const std::vector<float>& samples = m.sounds[sound - 1].samples;
  std::ofstream file(args[1].text, std::ios::binary);
  const auto data_size = static_cast<std::uint32_t>(samples.size() * 2);
  file.write("RIFF", 4);
  write_le(file, 36 + data_size, 4);
  file.write("WAVEfmt ", 8);
  write_le(file, 16, 4);
  write_le(file, 1, 2);
  write_le(file, 2, 2);
  write_le(file, kRate, 4);
  write_le(file, kRate * 4, 4);
  write_le(file, 4, 2);
  write_le(file, 16, 2);
  file.write("data", 4);
  write_le(file, data_size, 4);
  for (const float sample : samples) {
    const auto value = static_cast<std::int16_t>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
    write_le(file, static_cast<std::uint16_t>(value), 2);
  }
  out = boolean(static_cast<bool>(file));
  return true;
}

#undef CLPP_NATIVE

constexpr std::string_view kAudioSource = R"clp(<< @clpp.audio: WAV files and synthesized tones, mixed in software (44.1 kHz stereo).
<< Silent but fully working without a sound device (headless runs, tests).

func Load(string path) -> int { return audio::Load(path); }
func Tone(float frequency, float seconds, string wave = "sine") -> int { return audio::Tone(frequency, frequency, seconds, wave); }
func Sweep(float startHz, float endHz, float seconds, string wave = "square") -> int { return audio::Tone(startHz, endHz, seconds, wave); }
func Play(int sound, float volume = 1, float pan = 0, float pitch = 1, bool loop = false) -> int { return audio::Play(sound, volume, pan, pitch, loop); }
func Stop(int voice) { audio::Voice(voice, 0, 0); }
func SetVolume(int voice, float volume) { audio::Voice(voice, 1, volume); }
func SetPan(int voice, float pan) { audio::Voice(voice, 2, pan); }
func SetPitch(int voice, float pitch) { audio::Voice(voice, 3, pitch); }
func IsPlaying(int voice) -> bool { return audio::Voice(voice, 4, 0); }
func StopAll() { audio::Global(0, 0); }
func MasterVolume(float volume) { audio::Global(1, volume); }
func Available() -> bool { return audio::Global(2, 0); }
func Playing() -> int { return audio::Global(3, 0); }
func Duration(int sound) -> float { return audio::Duration(sound); }
func SaveWav(int sound, string path) -> bool { return audio::SaveWav(sound, path); }
)clp";

}  // namespace

void add_audio(std::vector<Entry>& table) {
  table.insert(table.end(), {
                                {"audio::Load", 1, audio_load, true},
                                {"audio::Tone", 4, audio_tone, false},
                                {"audio::Play", 5, audio_play, true},
                                {"audio::Voice", 3, audio_voice, true},
                                {"audio::Global", 2, audio_global, true},
                                {"audio::Duration", 1, audio_duration, false},
                                {"audio::SaveWav", 2, audio_save, true},
                            });
}

std::string_view audio_source() { return kAudioSource; }

}  // namespace clpp::stdlib::host
