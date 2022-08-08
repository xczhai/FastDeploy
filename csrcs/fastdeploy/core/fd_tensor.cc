// Copyright (c) 2022 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fastdeploy/core/fd_tensor.h"
#include "fastdeploy/utils/utils.h"

#ifdef WITH_GPU
#include <cuda_runtime_api.h>
#endif

namespace fastdeploy {

void* FDTensor::MutableData() {
  if (external_data_ptr != nullptr) {
    return external_data_ptr;
  }
  return data.data();
}

void* FDTensor::Data() {
  if (external_data_ptr != nullptr) {
    if (device == Device::GPU) {
#ifdef WITH_GPU
      // need to copy cuda mem to cpu first
      temporary_cpu_buffer.resize(Nbytes());
      FDASSERT(cudaMemcpy(temporary_cpu_buffer.data(), external_data_ptr,
                          Nbytes(), cudaMemcpyDeviceToHost) == 0,
               "[ERROR] Error occurs while copy memory from GPU to CPU");
      return temporary_cpu_buffer.data();
#else
      FDASSERT(false,
               "The FastDeploy didn't compile under -DWITH_GPU=ON, so this is "
               "an unexpected problem happend.");
#endif
    } else {
      return external_data_ptr;
    }
  }
  return data.data();
}

const void* FDTensor::Data() const {
  if (external_data_ptr != nullptr) {
    return external_data_ptr;
  }
  return data.data();
}

void FDTensor::SetExternalData(const std::vector<int>& new_shape,
                               const FDDataType& data_type, void* data_buffer) {
  dtype = data_type;
  shape.assign(new_shape.begin(), new_shape.end());
  external_data_ptr = data_buffer;
}

void FDTensor::Allocate(const std::vector<int>& new_shape,
                        const FDDataType& data_type,
                        const std::string& tensor_name) {
  dtype = data_type;
  name = tensor_name;
  shape.assign(new_shape.begin(), new_shape.end());
  int unit = FDDataTypeSize(data_type);
  int total_size =
      std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<int>());
  data.resize(total_size * unit);
}

int FDTensor::Nbytes() const { return Numel() * FDDataTypeSize(dtype); }

int FDTensor::Numel() const {
  return std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<int>());
}

template <typename T>
void CalculateStatisInfo(void* src_ptr, int size, double* mean, double* max,
                         double* min) {
  T* ptr = static_cast<T*>(src_ptr);
  *mean = 0;
  *max = -99999999;
  *min = 99999999;
  for (int i = 0; i < size; ++i) {
    if (*(ptr + i) > *max) {
      *max = *(ptr + i);
    }
    if (*(ptr + i) < *min) {
      *min = *(ptr + i);
    }
    *mean += *(ptr + i);
  }
  *mean = *mean / size;
}

void FDTensor::PrintInfo(const std::string& prefix) {
  double mean = 0;
  double max = -99999999;
  double min = 99999999;
  if (dtype == FDDataType::FP32) {
    CalculateStatisInfo<float>(Data(), Numel(), &mean, &max, &min);
  } else if (dtype == FDDataType::FP64) {
    CalculateStatisInfo<double>(Data(), Numel(), &mean, &max, &min);
  } else if (dtype == FDDataType::INT8) {
    CalculateStatisInfo<int8_t>(Data(), Numel(), &mean, &max, &min);
  } else if (dtype == FDDataType::UINT8) {
    CalculateStatisInfo<uint8_t>(Data(), Numel(), &mean, &max, &min);
  } else if (dtype == FDDataType::INT32) {
    CalculateStatisInfo<int32_t>(Data(), Numel(), &mean, &max, &min);
  } else if (dtype == FDDataType::INT64) {
    CalculateStatisInfo<int64_t>(Data(), Numel(), &mean, &max, &min);
  } else {
    FDASSERT(false,
             "PrintInfo function doesn't support current situation, maybe you "
             "need enhance this function now.")
  }
  std::cout << prefix << ": shape=";
  for (int i = 0; i < shape.size(); ++i) {
    std::cout << shape[i] << " ";
  }
  std::cout << ", dtype=" << Str(dtype) << ", mean=" << mean << ", max=" << max
            << ", min=" << min << std::endl;
}

FDTensor::FDTensor(const std::string& tensor_name) { name = tensor_name; }
}  // namespace fastdeploy