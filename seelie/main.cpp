#include <csignal>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>
#include <unistd.h>
#include <ctime>

#include "CPUSnapshot.h"
#include "classify.h"
#include "ina3221.h"
#include "record_system.h"

struct OutputData {
  uint8_t trigger_count;
  uint8_t latchup_count;
};

inline void latchup_test(Model &classify_model, RecordSystem &system_stats,
                         INA3221 &current_stats, OutputData &output_data) {
  // Test for 3 seconds at 200Hz = 600 samples
  const int target_freq_hz = 200;
  const int test_duration_sec = 3;
  const int total_samples = target_freq_hz * test_duration_sec;
  const int target_interval_us = 1000000 / target_freq_hz;  // 5000μs for 200Hz
  
  std::cout << "Testing for " << test_duration_sec << " seconds at " << target_freq_hz << "Hz (" << total_samples << " samples)!" << std::endl;
  
  int timing_violations = 0;
  struct timespec loop_start, loop_end;
  
  for (int i = 0; i < total_samples; i++) {
    clock_gettime(CLOCK_MONOTONIC_RAW, &loop_start);
    
    classify_model.add_datapoint(current_stats.read_currents(),
                                 system_stats.get_system_info());
    if (classify_model.test_model()) {
      std::cout << "Potential latchup detected!" << std::endl;

      output_data.latchup_count += 1;
      if (output_data.latchup_count > 0b00001111)
        output_data.latchup_count = 0x1;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &loop_end);
    long loop_time_us = (loop_end.tv_sec - loop_start.tv_sec) * 1000000L + 
                        (loop_end.tv_nsec - loop_start.tv_nsec) / 1000;
    
    if (loop_time_us > target_interval_us) {
      timing_violations++;
    }
    
    // Sleep for remaining time to maintain target interval
    long remaining_time_us = target_interval_us - loop_time_us;
    if (remaining_time_us > 0) {
      usleep(remaining_time_us);
    }
  }
  
  std::cout << "Detection completed. Timing violations: " << timing_violations 
            << "/" << total_samples << " (" 
            << (timing_violations * 100.0 / total_samples) << "%)" << std::endl;
}

int main(int argc, char **argv) {
  std::string model_file;
  std::tuple<double, double> predicted, actual;
  OutputData output_data;

  if (argc != 2) {
    printf("Usage: %s MODEL_FILE\n", argv[0]);
    return -1;
  }

  model_file = argv[1];

  Model classify_model(model_file);
  RecordSystem system_stats;
  INA3221 current_stats;

  if (std::filesystem::exists("one_byte_telemetry")) {
    // one_byte_telemetry exists, read in latest data
    std::ifstream one_byte_telemetry_file("one_byte_telemetry",
                                          std::ios::in | std::ios::binary);
    char raw_data;
    one_byte_telemetry_file.read(&raw_data, 1);
    auto data = static_cast<uint8_t>(raw_data);

    output_data.latchup_count = data & 0b00001111;
    output_data.trigger_count = data >> 4;
  } else {
    output_data.latchup_count = 0;
    output_data.trigger_count = 0;
  }

  std::cout << "Counters from previous run, if any: run "
            << static_cast<unsigned int>(output_data.trigger_count)
            << " times, "
            << static_cast<unsigned int>(output_data.latchup_count)
            << " latchup events detected\n";

  // Increase trigger count now that idle is detected
  output_data.trigger_count += 1;
  if (output_data.trigger_count > 0b00001111)
    output_data.trigger_count = 0x1;

  latchup_test(classify_model, system_stats, current_stats, output_data);

  unsigned int one_byte_telemetry = 0x0;
  one_byte_telemetry |= static_cast<unsigned int>(output_data.latchup_count & 0b00001111);
  one_byte_telemetry |= static_cast<unsigned int>(output_data.trigger_count << 4);

  std::cout << "Counters after test: run "
            << static_cast<unsigned int>(output_data.trigger_count)
            << ", latchup detected "
            << static_cast<unsigned int>(output_data.latchup_count)
            << ", one byte telemetry 0x"
            << std::hex << std::uppercase << std::setw(2)
            << one_byte_telemetry
            << "\n";

  // Write output byte
  std::ofstream one_byte_telemetry_file("one_byte_telemetry",
                                        std::ios::out | std::ios::binary);
  one_byte_telemetry_file << static_cast<char>(one_byte_telemetry);

  return 0;
}
