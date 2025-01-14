// sherpa-onnx/csrc/vad-model-config.h
//
// Copyright (c)  2023  Xiaomi Corporation
#ifndef SHERPA_ONNX_CSRC_VAD_MODEL_CONFIG_H_
#define SHERPA_ONNX_CSRC_VAD_MODEL_CONFIG_H_

#include <string>

#include "sherpa-onnx/csrc/parse-options.h"
#include "sherpa-onnx/csrc/silero-vad-model-config.h"

namespace sherpa_onnx {

struct VadModelConfig {
  SileroVadModelConfig silero_vad;

  int32_t sample_rate = 16000;
  int32_t num_threads = 1;
  std::string provider = "cpu";

  // true to show debug information when loading models
  bool debug = false;

  float pre_record_seconds = 0.5;
  float post_record_seconds = 0.5;
  
  VadModelConfig() = default;

  VadModelConfig(const SileroVadModelConfig &silero_vad, int32_t sample_rate,
                 int32_t num_threads, const std::string &provider, bool debug,
                 float pre_record_seconds, float post_record_seconds)
      : silero_vad(silero_vad),
        sample_rate(sample_rate),
        num_threads(num_threads),
        provider(provider),
        debug(debug),
        pre_record_seconds(pre_record_seconds),
        post_record_seconds(post_record_seconds) {}

  void Register(ParseOptions *po);
  bool Validate() const;

  std::string ToString() const;
};

}  // namespace sherpa_onnx

#endif  // SHERPA_ONNX_CSRC_VAD_MODEL_CONFIG_H_
