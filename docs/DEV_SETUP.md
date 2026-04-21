# Dev Setup (macOS)

Local development requires OpenCV and ONNX Runtime.

## Install

```bash
brew install opencv cmake

# ONNX Runtime 1.17.3 (arm64)
ORT_VER="1.17.3"
curl -L "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VER}/onnxruntime-osx-arm64-${ORT_VER}.tgz" -o /tmp/ort.tgz
mkdir -p vendor
tar -xzf /tmp/ort.tgz -C vendor
mv "vendor/onnxruntime-osx-arm64-${ORT_VER}" vendor/onnxruntime
```

## Build

```bash
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DONNXRUNTIME_ROOT="$(pwd)/vendor/onnxruntime"
cmake --build build --config Debug --target Theremin_Standalone
open build/Theremin_artefacts/Debug/Standalone/Theremin.app
```

## Tests

```bash
cmake --build build --target theremin_tests
./build/tests/theremin_tests
```
