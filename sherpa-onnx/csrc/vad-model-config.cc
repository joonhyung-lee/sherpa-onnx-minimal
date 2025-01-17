// sherpa-onnx/csrc/vad-model-config.cc
//
// Copyright (c)  2023  Xiaomi Corporation

#include "sherpa-onnx/csrc/vad-model-config.h"

#include <sstream>
#include <string>

namespace sherpa_onnx {

void VadModelConfig::Register(ParseOptions *po) {
  silero_vad.Register(po);

  po->Register("vad-sample-rate", &sample_rate,
               "Sample rate expected by the VAD model");

  po->Register("vad-num-threads", &num_threads,
               "Number of threads to run the VAD model");

  po->Register("vad-provider", &provider,
               "Specify a provider to run the VAD model. Supported values: "
               "cpu, cuda, coreml");

  po->Register("vad-debug", &debug,
               "true to display debug information when loading vad models");

  po->Register("vad-pre-record-seconds", &pre_record_seconds,
               "Pre-record seconds before speech starts");
  po->Register("vad-post-record-seconds", &post_record_seconds,
               "Post-record seconds after speech ends");
}

bool VadModelConfig::Validate() const { return silero_vad.Validate(); }

std::string VadModelConfig::ToString() const {
  std::ostringstream os;

  os << "VadModelConfig(";
  os << "silero_vad=" << silero_vad.ToString() << ", ";
  os << "sample_rate=" << sample_rate << ", ";
  os << "num_threads=" << num_threads << ", ";
  os << "provider=\"" << provider << "\", ";
  os << "debug=" << (debug ? "True" : "False") << ", ";
  os << "pre_record_seconds=" << pre_record_seconds << ", ";
  os << "post_record_seconds=" << post_record_seconds << ")";

  return os.str();
}

}  // namespace sherpa_onnx
