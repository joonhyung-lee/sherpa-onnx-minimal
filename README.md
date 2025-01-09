## Sherpa ONNX Minimal

This is a minimal repository for Sherpa ONNX.


### Installation in MacOS

```bash
git clone https://github.com/joonhyung-lee/sherpa-onnx-minimal
cd sherpa-onnx
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j6
cd ../
```

### Installation in Linux

```bash
git clone https://github.com/joonhyung-lee/sherpa-onnx-minimal
cd sherpa-onnx
mkdir build
cd build
# By default, it builds static libaries and uses static link.
cmake -DCMAKE_BUILD_TYPE=Release ..

# If you have GCC<=10, e.g., use Ubuntu <= 18.04 or use CentOS<=7, please
# use the following command to build shared libs; otherwise, you would
# get link errors from libonnxruntime.a
#
# cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON ..
#
#
make -j6 # You can check the number of threads by `nproc`
cd ../
```


### Download Pre-trained Models

```bash
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
tar xvf sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2

ls -lh sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17
### You can see the result as below:
# total 1.1G
# -rw-r--r-- 1 runner docker   71 Jul 18 13:06 LICENSE
# -rw-r--r-- 1 runner docker  104 Jul 18 13:06 README.md
# -rwxr-xr-x 1 runner docker 5.8K Jul 18 13:06 export-onnx.py
# -rw-r--r-- 1 runner docker 229M Jul 18 13:06 model.int8.onnx
# -rw-r--r-- 1 runner docker 895M Jul 18 13:06 model.onnx
# drwxr-xr-x 2 runner docker 4.0K Jul 18 13:06 test_wavs
# -rw-r--r-- 1 runner docker 309K Jul 18 13:06 tokens.txt

ls -lh sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/test_wavs
### You can see the result as below:
# total 940K
# -rw-r--r-- 1 runner docker 224K Jul 18 13:06 en.wav
# -rw-r--r-- 1 runner docker 226K Jul 18 13:06 ja.wav
# -rw-r--r-- 1 runner docker 145K Jul 18 13:06 ko.wav
# -rw-r--r-- 1 runner docker 161K Jul 18 13:06 yue.wav
# -rw-r--r-- 1 runner docker 175K Jul 18 13:06 zh.wav
```

### Run Demo scripts

Run below commands in the `root` directory of this repository.

- Specify Language ( `--sense-voice-language=ko \` )
    
    ```python
    ./build/bin/sherpa-onnx-offline \
      --tokens=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt \
      --sense-voice-model=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.onnx \
      --num-threads=1 \
      --sense-voice-language=ko \
      --debug=0 \
      ./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/test_wavs/ko.wav
    ```
    
- Speech recognition from a microphone
    
    ```python
    ./build/bin/sherpa-onnx-microphone-offline \
      --tokens=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt \
      --sense-voice-model=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.int8.onnx
    ```
    
- Speech recognition from a microphone with VAD
    
    ```python
    wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx
    
    ./build/bin/sherpa-onnx-vad-microphone-offline-asr \
      --silero-vad-model=./silero_vad.onnx \
      --sense-voice-language=ko \
      --tokens=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt \
      --sense-voice-model=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.int8.onnx
    
    # In Jetson, you can use below command to run the script.
    SHERPA_ONNX_MIC_DEVICE=0 SHERPA_ONNX_MIC_SAMPLE_RATE=44100 ./build/bin/sherpa-onnx-vad-microphone-offline-asr \
      --silero-vad-model=./silero_vad.onnx \
      --sense-voice-language=ko \
      --tokens=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt \
      --sense-voice-model=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.int8.onnx
    ```

- Speech recognition from a microphone with VAD and Shared Memory
    
    ```python
    ./build/bin/ros2-sherpa-onnx-vad-microphone-offline-asr \
      --silero-vad-model=./silero_vad.onnx \
      --sense-voice-language=ko \
      --tokens=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt \
      --sense-voice-model=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.int8.onnx

    # In Jetson, you can use below command to run the script.
    SHERPA_ONNX_MIC_DEVICE=0 SHERPA_ONNX_MIC_SAMPLE_RATE=44100 ./build/bin/ros2-sherpa-onnx-vad-microphone-offline-asr \
      --silero-vad-model=./silero_vad.onnx \
      --sense-voice-language=ko \
      --tokens=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/tokens.txt \
      --sense-voice-model=./sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/model.int8.onnx
    ```
