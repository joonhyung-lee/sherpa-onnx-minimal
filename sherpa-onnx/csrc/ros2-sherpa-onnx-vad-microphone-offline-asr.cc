// sherpa-onnx/csrc/sherpa-onnx-vad-microphone-offline-asr.cc
//
// Copyright (c)  2022-2023  Xiaomi Corporation

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <iomanip>  // for std::fixed, std::setprecision
#include <chrono>  // Add this at the top with other includes
#include <mutex>  // NOLINT

#include "portaudio.h"  // NOLINT
#include "sherpa-onnx/csrc/circular-buffer.h"
#include "sherpa-onnx/csrc/microphone.h"
#include "sherpa-onnx/csrc/offline-recognizer.h"
#include "sherpa-onnx/csrc/resample.h"
#include "sherpa-onnx/csrc/voice-activity-detector.h"
#include "sherpa-onnx/csrc/vad-model-config.h"

// ############################################################
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>  // for ftruncate, close

// 공유 메모리에 저장될 데이터 구조체
struct SharedData {
    char text[1024];  // 인식된 텍스트
    int index;        // 인덱스
    bool new_data;    // 새로운 데이터가 있는지 표시
    bool is_active;   // STT Activation Flag
    char state[32];   // Current State
};

SharedData* shared_memory = nullptr;
const char* SHM_NAME = "/sherpa_transcription";
// ############################################################

bool stop = false;
std::mutex mutex;
sherpa_onnx::CircularBuffer buffer(16000 * 60);

// 특수 토큰을 필터링하는 함수 추가
bool isSpecialToken(const std::string& text) {
    // 특수 토큰 패턴: <|로 시작하고 |>로 끝나는 문자열
    return (text.length() >= 4 && 
            text.substr(0, 2) == "<|" && 
            text.substr(text.length() - 2) == "|>");
}

// 특수 토큰을 제거하는 함수
std::string removeSpecialTokens(const std::string& text) {
    if (isSpecialToken(text)) {
        return "";  // 특수 토큰인 경우 빈 문자열 반환
    }
    return text;
}

static int32_t RecordCallback(const void *input_buffer,
                              void * /*output_buffer*/,
                              unsigned long frames_per_buffer,  // NOLINT
                              const PaStreamCallbackTimeInfo * /*time_info*/,
                              PaStreamCallbackFlags /*status_flags*/,
                              void * /*user_data*/) {
  std::lock_guard<std::mutex> lock(mutex);
  buffer.Push(reinterpret_cast<const float *>(input_buffer), frames_per_buffer);

  return stop ? paComplete : paContinue;
}

static void Handler(int32_t /*sig*/) {
  stop = true;
  fprintf(stderr, "\nCaught Ctrl + C. Exiting...\n");
}

int32_t main(int32_t argc, char *argv[]) {
  signal(SIGINT, Handler);

  const char *kUsageMessage = R"usage(
This program shows how to use a streaming VAD with non-streaming ASR in
sherpa-onnx.

Please download silero_vad.onnx from
https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx

For instance, use
wget https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx

Please refer to ./sherpa-onnx-microphone-offline.cc
to download models for offline ASR.

(1) Transducer from icefall

  ./bin/sherpa-onnx-vad-microphone-offline-asr \
    --silero-vad-model=/path/to/silero_vad.onnx \
    --tokens=/path/to/tokens.txt \
    --encoder=/path/to/encoder.onnx \
    --decoder=/path/to/decoder.onnx \
    --joiner=/path/to/joiner.onnx

(2) Paraformer from FunASR

  ./bin/sherpa-onnx-vad-microphone-offline-asr \
    --silero-vad-model=/path/to/silero_vad.onnx \
    --tokens=/path/to/tokens.txt \
    --paraformer=/path/to/model.onnx \
    --num-threads=1

(3) Whisper models

  ./bin/sherpa-onnx-vad-microphone-offline-asr \
    --silero-vad-model=/path/to/silero_vad.onnx \
    --whisper-encoder=./sherpa-onnx-whisper-base.en/base.en-encoder.int8.onnx \
    --whisper-decoder=./sherpa-onnx-whisper-base.en/base.en-decoder.int8.onnx \
    --tokens=./sherpa-onnx-whisper-base.en/base.en-tokens.txt \
    --num-threads=1
)usage";

  sherpa_onnx::ParseOptions po(kUsageMessage);
  sherpa_onnx::VadModelConfig vad_config;

  sherpa_onnx::OfflineRecognizerConfig asr_config;

  // ############################################################
  // 기존 공유 메모리가 있다면 제거
  shm_unlink(SHM_NAME);

  // 공유 메모리 생성 및 초기화
  std::cout << "SHM_NAME: " << SHM_NAME << std::endl;
  int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
      fprintf(stderr, "Failed to create shared memory: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
  }

  // 공유 메모리 크기 설정
  if (ftruncate(fd, sizeof(SharedData)) == -1) {
      fprintf(stderr, "Failed to set size of shared memory: %s\n", strerror(errno));
      close(fd);
      shm_unlink(SHM_NAME);
      exit(EXIT_FAILURE);
  }

  // 메모리 매핑
  shared_memory = (SharedData*)mmap(NULL, sizeof(SharedData), 
                                   PROT_READ | PROT_WRITE, 
                                   MAP_SHARED, fd, 0);
  if (shared_memory == MAP_FAILED) {
      fprintf(stderr, "Failed to map shared memory\n");
      exit(EXIT_FAILURE);
  }

  // ############################################################
  // 초기화
  memset(shared_memory, 0, sizeof(SharedData));
  // 초기 상태 설정
  shared_memory->is_active = true;  // 시작할 때는 활성화 상태
  strncpy(shared_memory->state, "recording", sizeof(shared_memory->state) - 1);
  shared_memory->state[sizeof(shared_memory->state) - 1] = '\0';
  
  fprintf(stderr, "[Debug] Initial Shared Memory State - Active: %d, State: %s\n", 
          shared_memory->is_active, shared_memory->state);
  // ############################################################

  vad_config.Register(&po);
  asr_config.Register(&po);

  po.Read(argc, argv);
  if (po.NumArgs() != 0) {
    po.PrintUsage();
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "%s\n", vad_config.ToString().c_str());
  fprintf(stderr, "%s\n", asr_config.ToString().c_str());

  if (!vad_config.Validate()) {
    fprintf(stderr, "Errors in vad_config!\n");
    return -1;
  }

  if (!asr_config.Validate()) {
    fprintf(stderr, "Errors in asr_config!\n");
    return -1;
  }

  fprintf(stderr, "Creating recognizer ...\n");
  sherpa_onnx::OfflineRecognizer recognizer(asr_config);
  fprintf(stderr, "Recognizer created!\n");

  sherpa_onnx::Microphone mic;

  PaDeviceIndex num_devices = Pa_GetDeviceCount();
  fprintf(stderr, "Num devices: %d\n", num_devices);

  int32_t device_index = Pa_GetDefaultInputDevice();

  if (device_index == paNoDevice) {
    fprintf(stderr, "No default input device found\n");
    exit(EXIT_FAILURE);
  }

  const char *pDeviceIndex = std::getenv("SHERPA_ONNX_MIC_DEVICE");
  if (pDeviceIndex) {
    fprintf(stderr, "Use specified device: %s\n", pDeviceIndex);
    device_index = atoi(pDeviceIndex);
  }

  for (int32_t i = 0; i != num_devices; ++i) {
    const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
    fprintf(stderr, " %s %d %s\n", (i == device_index) ? "*" : " ", i,
            info->name);
  }

  PaStreamParameters param;
  param.device = device_index;

  fprintf(stderr, "Use device: %d\n", param.device);

  const PaDeviceInfo *info = Pa_GetDeviceInfo(param.device);
  fprintf(stderr, "  Name: %s\n", info->name);
  fprintf(stderr, "  Max input channels: %d\n", info->maxInputChannels);

  param.channelCount = 1;
  param.sampleFormat = paFloat32;

  param.suggestedLatency = info->defaultLowInputLatency;
  param.hostApiSpecificStreamInfo = nullptr;
  float mic_sample_rate = 16000;
  const char *pSampleRateStr = std::getenv("SHERPA_ONNX_MIC_SAMPLE_RATE");
  if (pSampleRateStr) {
    fprintf(stderr, "Use sample rate %f for mic\n", mic_sample_rate);
    mic_sample_rate = atof(pSampleRateStr);
  }
  float sample_rate = 16000;
  std::unique_ptr<sherpa_onnx::LinearResample> resampler;
  if (mic_sample_rate != sample_rate) {
    float min_freq = std::min(mic_sample_rate, sample_rate);
    float lowpass_cutoff = 0.99 * 0.5 * min_freq;

    int32_t lowpass_filter_width = 6;
    resampler = std::make_unique<sherpa_onnx::LinearResample>(
        mic_sample_rate, sample_rate, lowpass_cutoff, lowpass_filter_width);
  }

  PaStream *stream;
  PaError err =
      Pa_OpenStream(&stream, &param, nullptr, /* &outputParameters, */
                    mic_sample_rate,
                    0,          // frames per buffer
                    paClipOff,  // we won't output out of range samples
                                // so don't bother clipping them
                    RecordCallback, nullptr);
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  err = Pa_StartStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  auto vad = std::make_unique<sherpa_onnx::VoiceActivityDetector>(vad_config);

  // Configure pre/post record times (in seconds)
  float pre_record = 0.5;   // 0.5 seconds before speech
  float post_record = 0.5;  // 0.5 seconds after speech
  vad->SetPreRecordSeconds(pre_record);
  vad->SetPostRecordSeconds(post_record);

  fprintf(stderr, "Started. Please speak\n");

  int32_t window_size = vad_config.silero_vad.window_size;
  int32_t index = 0;
  bool was_speech = false;  // Add this line to track speech state
  auto speech_start_time = std::chrono::high_resolution_clock::now();  // Add this line

  while (!stop) {
      {
        std::lock_guard<std::mutex> lock(mutex);

        // Debug: Print shared memory state changes
        static std::string last_state = "";
        static bool last_active = true;
        if (shared_memory->state != last_state || shared_memory->is_active != last_active) {
            fprintf(stderr, "\n[Debug] Shared Memory State Changed - Active: %d, State: %s\n", 
                  shared_memory->is_active, shared_memory->state);
            last_state = shared_memory->state;
            last_active = shared_memory->is_active;
            
            // 상태가 변경될 때 VAD 버퍼 초기화
            if (!shared_memory->is_active) {
                if (buffer.Size() > 0) {
                    size_t cleared_size = buffer.Size();
                    buffer.Pop(buffer.Size());
                    fprintf(stderr, "[Debug] Cleared audio buffer: %zu samples\n", cleared_size);
                }
                while (!vad->Empty()) {
                    vad->Pop();
                }
                fprintf(stderr, "[Debug] Cleared VAD buffer\n");
            } else {
                // 활성화될 때 speech_start_time 재설정
                speech_start_time = std::chrono::high_resolution_clock::now();
                fprintf(stderr, "[Debug] STT activated - Reset speech timer\n");
            }
        }
        // STT가 비활성화 상태일 때
        if (!shared_memory->is_active) {
            std::cout << "\r[Paused] State: " << shared_memory->state
                      << "                            \r" << std::flush;
            Pa_Sleep(10);
            continue;
        }

        while (buffer.Size() >= window_size && shared_memory->is_active) {  // Added condition
            std::vector<float> samples = buffer.Get(buffer.Head(), window_size);
            buffer.Pop(window_size);

            if (resampler) {
                std::vector<float> tmp;
                resampler->Resample(samples.data(), samples.size(), true, &tmp);
                samples = std::move(tmp);
            }

            vad->AcceptWaveform(samples.data(), samples.size());
            float current_score = vad->GetLastScore();

            if (!vad->IsSpeechDetected()) {
                static float last_printed_score = 0.0f;
                if (std::abs(current_score - last_printed_score) > 0.1f) {
                    std::cout << "\r[Non Recording] VAD: " << std::fixed << std::setprecision(2) 
                            << current_score << "                              \r" << std::flush;
                    last_printed_score = current_score;
                }
            } else {
                auto elapsed = std::chrono::duration<float>(
                    std::chrono::high_resolution_clock::now() - speech_start_time).count();
                
                std::cout << "\r[Recording] VAD: " << std::fixed << std::setprecision(2) 
                        << current_score << " Duration: " << std::setprecision(2) 
                        << elapsed << "s" << std::setw(20) << " " << std::flush;
            }
        }
      }

      // Process VAD segments only if STT is active
      while (!vad->Empty() && shared_memory->is_active) {  // Added condition
          was_speech = false;
          const auto &segment = vad->Front();
          auto s = recognizer.CreateStream();
          s->AcceptWaveform(sample_rate, segment.samples.data(),
                          segment.samples.size());
          auto start_time = std::chrono::high_resolution_clock::now();
          recognizer.DecodeStream(s.get());
          auto end_time = std::chrono::high_resolution_clock::now();
          std::chrono::duration<float> duration = end_time - start_time;

          const auto &result = s->GetResult();
          std::string filtered_text = removeSpecialTokens(result.text);
          if (!filtered_text.empty()) {  // 필터링된 텍스트가 비어있지 않은 경우에만 출력
            std::cout << "\n[" << std::setw(2) << index << "] " << filtered_text 
                      << " (inference time: " << std::fixed << std::setprecision(3) 
                      << duration.count() << "s)\n" << std::endl;

            // 공유 메모리에 결과 저장
            strncpy(shared_memory->text, filtered_text.c_str(), 1023);
            shared_memory->text[1023] = '\0';  // null 종료 보장
            shared_memory->index = index;
            shared_memory->new_data = true;

            ++index;
          }
          vad->Pop();
      }

      Pa_Sleep(10);
  }

  err = Pa_CloseStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  // ############################################################
  // 프로그램 종료 전 정리
  munmap(shared_memory, sizeof(SharedData));
  close(fd);
  shm_unlink(SHM_NAME);
  // ############################################################

  return 0;
}
