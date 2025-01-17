// sherpa-onnx/csrc/sherpa-onnx-microphone.cc
//
// Copyright (c)  2022-2023  Xiaomi Corporation

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <clocale>
#include <cwctype>

#include "portaudio.h"  // NOLINT
#include "sherpa-onnx/csrc/display.h"
#include "sherpa-onnx/csrc/microphone.h"
#include "sherpa-onnx/csrc/online-recognizer.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <iostream>

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

bool stop = false;
float mic_sample_rate = 16000;

static int32_t RecordCallback(const void *input_buffer,
                              void * /*output_buffer*/,
                              unsigned long frames_per_buffer,  // NOLINT
                              const PaStreamCallbackTimeInfo * /*time_info*/,
                              PaStreamCallbackFlags /*status_flags*/,
                              void *user_data) {
  auto stream = reinterpret_cast<sherpa_onnx::OnlineStream *>(user_data);

  stream->AcceptWaveform(mic_sample_rate,
                         reinterpret_cast<const float *>(input_buffer),
                         frames_per_buffer);

  return stop ? paComplete : paContinue;
}

static void Handler(int32_t /*sig*/) {
  stop = true;
  fprintf(stderr, "\nCaught Ctrl + C. Exiting...\n");
}

static std::string tolowerUnicode(const std::string &input_str) {
  // Use system locale
  std::setlocale(LC_ALL, "");

  // From char string to wchar string
  std::wstring input_wstr(input_str.size() + 1, '\0');
  std::mbstowcs(&input_wstr[0], input_str.c_str(), input_str.size());
  std::wstring lowercase_wstr;

  for (wchar_t wc : input_wstr) {
    if (std::iswupper(wc)) {
      lowercase_wstr += std::towlower(wc);
    } else {
      lowercase_wstr += wc;
    }
  }

  // Back to char string
  std::string lowercase_str(input_str.size() + 1, '\0');
  std::wcstombs(&lowercase_str[0], lowercase_wstr.c_str(),
                lowercase_wstr.size());

  return lowercase_str;
}

int32_t main(int32_t argc, char *argv[]) {
  signal(SIGINT, Handler);


  // ############################################################
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

  // 초기화
  memset(shared_memory, 0, sizeof(SharedData));
  shared_memory->is_active = true;  // 시작할 때는 활성화 상태
  strncpy(shared_memory->state, "recording", sizeof(shared_memory->state) - 1);
  shared_memory->state[sizeof(shared_memory->state) - 1] = '\0';
  // ############################################################
  // ############################################################

  const char *kUsageMessage = R"usage(
This program uses streaming models with microphone for speech recognition.
Usage:

  ./bin/sherpa-onnx-microphone \
    --tokens=/path/to/tokens.txt \
    --encoder=/path/to/encoder.onnx \
    --decoder=/path/to/decoder.onnx \
    --joiner=/path/to/joiner.onnx \
    --provider=cpu \
    --num-threads=1 \
    --decoding-method=greedy_search

Please refer to
https://k2-fsa.github.io/sherpa/onnx/pretrained_models/index.html
for a list of pre-trained models to download.
)usage";

  sherpa_onnx::ParseOptions po(kUsageMessage);
  sherpa_onnx::OnlineRecognizerConfig config;

  config.Register(&po);
  po.Read(argc, argv);
  if (po.NumArgs() != 0) {
    po.PrintUsage();
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "%s\n", config.ToString().c_str());

  if (!config.Validate()) {
    fprintf(stderr, "Errors in config!\n");
    return -1;
  }

  sherpa_onnx::OnlineRecognizer recognizer(config);
  auto s = recognizer.CreateStream();

  sherpa_onnx::Microphone mic;

  PaDeviceIndex num_devices = Pa_GetDeviceCount();
  fprintf(stderr, "Num devices: %d\n", num_devices);

  int32_t device_index = Pa_GetDefaultInputDevice();

  if (device_index == paNoDevice) {
    fprintf(stderr, "No default input device found\n");
    fprintf(stderr, "If you are using Linux, please switch to \n");
    fprintf(stderr, " ./bin/sherpa-onnx-alsa \n");
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
  const char *pSampleRateStr = std::getenv("SHERPA_ONNX_MIC_SAMPLE_RATE");
  if (pSampleRateStr) {
    fprintf(stderr, "Use sample rate %f for mic\n", mic_sample_rate);
    mic_sample_rate = atof(pSampleRateStr);
  }
  float sample_rate = 16000;

  PaStream *stream;
  PaError err =
      Pa_OpenStream(&stream, &param, nullptr, /* &outputParameters, */
                    sample_rate,
                    0,          // frames per buffer
                    paClipOff,  // we won't output out of range samples
                                // so don't bother clipping them
                    RecordCallback, s.get());
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  err = Pa_StartStream(stream);
  fprintf(stderr, "Started\n");

  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  std::string last_text;
  int32_t segment_index = 0;
  sherpa_onnx::Display display(30);
  while (!stop) {
    // STT가 비활성화 상태일 때
    if (!shared_memory->is_active) {
        std::cout << "\r[Paused] State: " << shared_memory->state
                  << "                            \r" << std::flush;
        Pa_Sleep(10);
        continue;
    }

    while (recognizer.IsReady(s.get())) {
      recognizer.DecodeStream(s.get());
    }

    auto text = recognizer.GetResult(s.get()).text;
    bool is_endpoint = recognizer.IsEndpoint(s.get());

    if (is_endpoint && !config.model_config.paraformer.encoder.empty()) {
      // For streaming paraformer models, since it has a large right chunk size
      // we need to pad it on endpointing so that the last character
      // can be recognized
      std::vector<float> tail_paddings(static_cast<int>(1.0 * mic_sample_rate));
      s->AcceptWaveform(mic_sample_rate, tail_paddings.data(),
                        tail_paddings.size());
      while (recognizer.IsReady(s.get())) {
        recognizer.DecodeStream(s.get());
      }
      text = recognizer.GetResult(s.get()).text;
    }

    if (!text.empty() && last_text != text) {
      last_text = text;
      display.Print(segment_index, tolowerUnicode(text));
      fflush(stderr);

      // 공유 메모리에 결과 저장
      strncpy(shared_memory->text, text.c_str(), 1023);
      shared_memory->text[1023] = '\0';  // null 종료 보장
      shared_memory->index = segment_index;
      shared_memory->new_data = true;
    }

    if (is_endpoint) {
      if (!text.empty()) {
        ++segment_index;
      }

      recognizer.Reset(s.get());
    }

    Pa_Sleep(20);  // sleep for 20ms
  }

  err = Pa_CloseStream(stream);
  if (err != paNoError) {
    fprintf(stderr, "portaudio error: %s\n", Pa_GetErrorText(err));
    exit(EXIT_FAILURE);
  }

  munmap(shared_memory, sizeof(SharedData));
  close(fd);
  shm_unlink(SHM_NAME);

  return 0;
}
