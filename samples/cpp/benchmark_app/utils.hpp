// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <chrono>
#include <iomanip>
#include <map>
#include <openvino/openvino.hpp>
#include <samples/slog.hpp>
#include <string>
#include <vector>

typedef std::chrono::high_resolution_clock Time;
typedef std::chrono::nanoseconds ns;

inline uint64_t getDurationInMilliseconds(uint32_t duration) {
    return duration * 1000LL;
}

inline uint64_t getDurationInNanoseconds(uint32_t duration) {
    return duration * 1000000000LL;
}

inline double get_duration_ms_till_now(Time::time_point& startTime) {
    return std::chrono::duration_cast<ns>(Time::now() - startTime).count() * 0.000001;
};

inline std::string double_to_string(const double number) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << number;
    return ss.str();
};

namespace benchmark_app {
struct InputInfo {
    ov::element::Type type;
    ov::PartialShape partialShape;
    ov::Shape dataShape;
    ov::Layout layout;
    std::vector<float> scale;
    std::vector<float> mean;
    bool isImage() const;
    bool isImageInfo() const;
    size_t width() const;
    size_t height() const;
    size_t channels() const;
    size_t batch() const;
    size_t depth() const;
    std::vector<std::string> fileNames;
};
using InputsInfo = std::map<std::string, InputInfo>;
using PartialShapes = std::map<std::string, ngraph::PartialShape>;
}  // namespace benchmark_app

std::vector<std::string> parseDevices(const std::string& device_string);
uint32_t deviceDefaultDeviceDurationInSeconds(const std::string& device);
std::map<std::string, std::string> parseNStreamsValuePerDevice(const std::vector<std::string>& devices,
                                                               const std::string& values_string);
std::string getShapeString(const ov::Shape& shape);
std::string getShapesString(const benchmark_app::PartialShapes& shapes);
size_t getBatchSize(const benchmark_app::InputsInfo& inputs_info);
std::vector<std::string> split(const std::string& s, char delim);
std::map<std::string, std::vector<float>> parseScaleOrMean(const std::string& scale_mean,
                                                           const benchmark_app::InputsInfo& inputs_info);
std::vector<ngraph::Dimension> parsePartialShape(const std::string& partial_shape);
ov::Shape parseDataShape(const std::string& dataShapeStr);
std::pair<std::string, std::vector<std::string>> parseInputFiles(const std::string& file_paths_string);
std::map<std::string, std::vector<std::string>> parseInputArguments(const std::vector<std::string>& args);

std::map<std::string, std::vector<std::string>> parseInputParameters(const std::string& parameter_string,
                                                                     const ov::ParameterVector& input_info);

/// <summary>
/// Parses command line data and data obtained from the function and returns configuration of each input
/// </summary>
/// <param name="shape_string">command-line shape string</param>
/// <param name="layout_string">command-line layout string</param>
/// <param name="batch_size">command-line batch string</param>
/// <param name="tensors_shape_string">command-line tensor_shape string</param>
/// <param name="scale_string">command-line iscale string</param>
/// <param name="mean_string">command-line imean string</param>
/// <param name="input_info">inputs vector obtained from ov::Model</param>
/// <param name="reshape_required">returns true to this parameter if reshape is required</param>
/// <returns>vector of benchmark_app::InputsInfo elements.
/// Each element is a configuration item for every test configuration case
/// (number of cases is calculated basing on tensor_shape and other parameters).
/// Each element is a map (input_name, configuration) containing data for each input</returns>
std::vector<benchmark_app::InputsInfo> getInputsInfo(const std::string& shape_string,
                                                     const std::string& layout_string,
                                                     const size_t batch_size,
                                                     const std::string& data_shapes_string,
                                                     const std::map<std::string, std::vector<std::string>>& fileNames,
                                                     const std::string& scale_string,
                                                     const std::string& mean_string,
                                                     const std::vector<ov::Output<const ov::Node>>& input_info,
                                                     bool& reshape_required);

/// <summary>
/// Parses command line data and data obtained from the function and returns configuration of each input
/// </summary>
/// <param name="shape_string">command-line shape string</param>
/// <param name="layout_string">command-line layout string</param>
/// <param name="batch_size">command-line batch string</param>
/// <param name="tensors_shape_string">command-line tensor_shape string</param>
/// <param name="scale_string">command-line iscale string</param>
/// <param name="mean_string">command-line imean string</param>
/// <param name="input_info">inputs vector obtained from ov::Model</param>
/// <param name="reshape_required">returns true to this parameter if reshape is required</param>
/// <returns>vector of benchmark_app::InputsInfo elements.
/// Each element is a configuration item for every test configuration case
/// (number of cases is calculated basing on tensor_shape and other parameters).
/// Each element is a map (input_name, configuration) containing data for each
/// input</returns>
std::vector<benchmark_app::InputsInfo> getInputsInfo(const std::string& shape_string,
                                                     const std::string& layout_string,
                                                     const size_t batch_size,
                                                     const std::string& data_shapes_string,
                                                     const std::map<std::string, std::vector<std::string>>& fileNames,
                                                     const std::string& scale_string,
                                                     const std::string& mean_string,
                                                     const std::vector<ov::Output<const ov::Node>>& input_info);

#ifdef USE_OPENCV
void dump_config(const std::string& filename, const std::map<std::string, std::map<std::string, std::string>>& config);
void load_config(const std::string& filename, std::map<std::string, std::map<std::string, std::string>>& config);
#endif

extern const std::vector<std::string> supported_image_extensions;
extern const std::vector<std::string> supported_binary_extensions;

bool isBinaryFile(const std::string& filePath);
bool isImageFile(const std::string& filePath);
bool containsBinaries(const std::vector<std::string>& filePaths);
std::vector<std::string> filterFilesByExtensions(const std::vector<std::string>& filePaths,
                                                 const std::vector<std::string>& extensions);
