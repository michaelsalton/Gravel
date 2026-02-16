# Feature 8.2: Automated Benchmarking Suite

**Epic**: 8 - Performance Analysis
**Status**: Not Started
**Estimated Time**: 3 hours
**Dependencies**: Feature 8.1 (GPU Timing)

## Goal

Create an automated benchmarking system that reproduces the paper's Table 1 performance results. The suite should run headless, test various configurations, and output CSV data for analysis.

## Implementation

### Step 1: Define Benchmark Configuration Structure

Create `src/profiling/Benchmark.h`:

```cpp
#pragma once
#include <string>
#include <vector>
#include <glm/glm.h>

struct BenchmarkConfig {
    std::string name;
    std::string meshPath;
    uint32_t elementType;      // 0=torus, 1=sphere, 100=pebble, etc.
    uint32_t resolutionM;
    uint32_t resolutionN;
    bool enableCulling;
    bool enableLOD;
    bool enablePebble;
    glm::vec3 cameraPosition;
    glm::vec3 cameraTarget;
    glm::vec3 cameraUp;
    uint32_t warmupFrames;     // Frames to run before measurement
    uint32_t measureFrames;    // Frames to measure
};

struct BenchmarkResult {
    std::string name;

    // Frame timing statistics
    float avgFrameTime;
    float minFrameTime;
    float maxFrameTime;
    float stdDevFrameTime;
    float medianFrameTime;

    // Geometry statistics
    uint32_t avgVertices;
    uint32_t avgPrimitives;
    uint32_t avgRenderedElements;
    uint32_t avgCulledElements;

    // Memory
    float vramUsageMB;

    // Computed metrics
    float getFPS() const { return avgFrameTime > 0.0f ? 1000.0f / avgFrameTime : 0.0f; }
    float getCullingEfficiency() const {
        uint32_t total = avgRenderedElements + avgCulledElements;
        return total > 0 ? 100.0f * avgCulledElements / total : 0.0f;
    }
};

class BenchmarkSuite {
public:
    void addBenchmark(const BenchmarkConfig& config);
    void runAll(class GravelApp* app);
    void exportCSV(const std::string& filename) const;
    void printSummary() const;

    const std::vector<BenchmarkResult>& getResults() const { return results; }

private:
    BenchmarkResult runBenchmark(class GravelApp* app, const BenchmarkConfig& config);
    float computeMean(const std::vector<float>& data) const;
    float computeStdDev(const std::vector<float>& data, float mean) const;
    float computeMedian(std::vector<float> data) const;

    std::vector<BenchmarkConfig> configs;
    std::vector<BenchmarkResult> results;
};
```

### Step 2: Implement Benchmark Runner

Create `src/profiling/Benchmark.cpp`:

```cpp
#include "Benchmark.h"
#include "../GravelApp.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>

void BenchmarkSuite::addBenchmark(const BenchmarkConfig& config) {
    configs.push_back(config);
}

void BenchmarkSuite::runAll(GravelApp* app) {
    results.clear();
    results.reserve(configs.size());

    std::cout << "=== Running Benchmark Suite ===" << std::endl;
    std::cout << "Total benchmarks: " << configs.size() << std::endl << std::endl;

    for (size_t i = 0; i < configs.size(); i++) {
        std::cout << "[" << (i + 1) << "/" << configs.size() << "] ";
        std::cout << configs[i].name << "..." << std::endl;

        BenchmarkResult result = runBenchmark(app, configs[i]);
        results.push_back(result);

        std::cout << "  -> " << std::fixed << std::setprecision(2)
                  << result.avgFrameTime << " ms ("
                  << result.getFPS() << " FPS)" << std::endl << std::endl;
    }

    std::cout << "=== Benchmark Complete ===" << std::endl;
    printSummary();
}

BenchmarkResult BenchmarkSuite::runBenchmark(GravelApp* app, const BenchmarkConfig& config) {
    // Configure application
    app->loadMesh(config.meshPath);
    app->setElementType(config.elementType);
    app->setResolution(config.resolutionM, config.resolutionN);
    app->setCullingEnabled(config.enableCulling);
    app->setLODEnabled(config.enableLOD);
    app->setPebbleEnabled(config.enablePebble);

    // Set camera
    app->setCameraPosition(config.cameraPosition);
    app->setCameraTarget(config.cameraTarget);
    app->setCameraUp(config.cameraUp);

    // Warm-up phase
    for (uint32_t i = 0; i < config.warmupFrames; i++) {
        app->renderFrame();
    }

    // Measurement phase
    std::vector<float> frameTimes;
    std::vector<uint32_t> vertexCounts;
    std::vector<uint32_t> primitiveCounts;
    std::vector<uint32_t> renderedElements;
    std::vector<uint32_t> culledElements;

    frameTimes.reserve(config.measureFrames);
    vertexCounts.reserve(config.measureFrames);
    primitiveCounts.reserve(config.measureFrames);
    renderedElements.reserve(config.measureFrames);
    culledElements.reserve(config.measureFrames);

    for (uint32_t i = 0; i < config.measureFrames; i++) {
        app->renderFrame();

        const auto& stats = app->getPerformanceStats();
        frameTimes.push_back(stats.frameTime);
        vertexCounts.push_back(stats.vertexCount);
        primitiveCounts.push_back(stats.primitiveCount);
        renderedElements.push_back(stats.renderedElements);
        culledElements.push_back(stats.culledElements);
    }

    // Compute statistics
    BenchmarkResult result;
    result.name = config.name;

    float mean = computeMean(frameTimes);
    result.avgFrameTime = mean;
    result.minFrameTime = *std::min_element(frameTimes.begin(), frameTimes.end());
    result.maxFrameTime = *std::max_element(frameTimes.begin(), frameTimes.end());
    result.stdDevFrameTime = computeStdDev(frameTimes, mean);
    result.medianFrameTime = computeMedian(frameTimes);

    result.avgVertices = static_cast<uint32_t>(computeMean(
        std::vector<float>(vertexCounts.begin(), vertexCounts.end())));
    result.avgPrimitives = static_cast<uint32_t>(computeMean(
        std::vector<float>(primitiveCounts.begin(), primitiveCounts.end())));
    result.avgRenderedElements = static_cast<uint32_t>(computeMean(
        std::vector<float>(renderedElements.begin(), renderedElements.end())));
    result.avgCulledElements = static_cast<uint32_t>(computeMean(
        std::vector<float>(culledElements.begin(), culledElements.end())));

    result.vramUsageMB = app->getVRAMUsageMB();

    return result;
}

float BenchmarkSuite::computeMean(const std::vector<float>& data) const {
    if (data.empty()) return 0.0f;
    return std::accumulate(data.begin(), data.end(), 0.0f) / data.size();
}

float BenchmarkSuite::computeStdDev(const std::vector<float>& data, float mean) const {
    if (data.empty()) return 0.0f;

    float variance = 0.0f;
    for (float value : data) {
        float diff = value - mean;
        variance += diff * diff;
    }
    variance /= data.size();

    return std::sqrt(variance);
}

float BenchmarkSuite::computeMedian(std::vector<float> data) const {
    if (data.empty()) return 0.0f;

    std::sort(data.begin(), data.end());
    size_t mid = data.size() / 2;

    if (data.size() % 2 == 0) {
        return (data[mid - 1] + data[mid]) / 2.0f;
    } else {
        return data[mid];
    }
}

void BenchmarkSuite::exportCSV(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << filename << " for writing" << std::endl;
        return;
    }

    // Header
    file << "Name,AvgFrameTime(ms),MinFrameTime(ms),MaxFrameTime(ms),"
         << "StdDev(ms),Median(ms),FPS,AvgVertices,AvgPrimitives,"
         << "RenderedElements,CulledElements,CullingEfficiency(%),VRAM(MB)\n";

    // Data rows
    for (const auto& result : results) {
        file << result.name << ","
             << result.avgFrameTime << ","
             << result.minFrameTime << ","
             << result.maxFrameTime << ","
             << result.stdDevFrameTime << ","
             << result.medianFrameTime << ","
             << result.getFPS() << ","
             << result.avgVertices << ","
             << result.avgPrimitives << ","
             << result.avgRenderedElements << ","
             << result.avgCulledElements << ","
             << result.getCullingEfficiency() << ","
             << result.vramUsageMB << "\n";
    }

    file.close();
    std::cout << "Benchmark results exported to " << filename << std::endl;
}

void BenchmarkSuite::printSummary() const {
    std::cout << "\n=== Benchmark Summary ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::setw(30) << std::left << "Name"
              << std::setw(12) << "Frame(ms)"
              << std::setw(10) << "FPS"
              << std::setw(12) << "Vertices"
              << std::setw(12) << "Primitives"
              << std::setw(10) << "VRAM(MB)" << std::endl;
    std::cout << std::string(86, '-') << std::endl;

    for (const auto& result : results) {
        std::cout << std::setw(30) << std::left << result.name
                  << std::setw(12) << result.avgFrameTime
                  << std::setw(10) << result.getFPS()
                  << std::setw(12) << result.avgVertices
                  << std::setw(12) << result.avgPrimitives
                  << std::setw(10) << result.vramUsageMB << std::endl;
    }
}
```

### Step 3: Define Paper's Benchmark Configurations

Create `src/profiling/PaperBenchmarks.cpp`:

```cpp
#include "Benchmark.h"

std::vector<BenchmarkConfig> createPaperBenchmarks() {
    std::vector<BenchmarkConfig> configs;

    // Configuration 1: Torus 8×8 on 1000 elements
    configs.push_back({
        .name = "Torus 8x8 (1K elements)",
        .meshPath = "assets/icosphere.obj",  // ~1000 faces
        .elementType = 0,  // Torus
        .resolutionM = 8,
        .resolutionN = 8,
        .enableCulling = false,
        .enableLOD = false,
        .enablePebble = false,
        .cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f),
        .cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f),
        .cameraUp = glm::vec3(0.0f, 1.0f, 0.0f),
        .warmupFrames = 60,
        .measureFrames = 300
    });

    // Configuration 2: Torus 16×16 on 1000 elements
    configs.push_back({
        .name = "Torus 16x16 (1K elements)",
        .meshPath = "assets/icosphere.obj",
        .elementType = 0,
        .resolutionM = 16,
        .resolutionN = 16,
        .enableCulling = false,
        .enableLOD = false,
        .enablePebble = false,
        .cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f),
        .cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f),
        .cameraUp = glm::vec3(0.0f, 1.0f, 0.0f),
        .warmupFrames = 60,
        .measureFrames = 300
    });

    // Configuration 3: Sphere 8×8 on 1000 elements
    configs.push_back({
        .name = "Sphere 8x8 (1K elements)",
        .meshPath = "assets/cube.obj",  // Subdivided cube
        .elementType = 1,  // Sphere
        .resolutionM = 8,
        .resolutionN = 8,
        .enableCulling = false,
        .enableLOD = false,
        .enablePebble = false,
        .cameraPosition = glm::vec3(0.0f, 0.0f, 5.0f),
        .cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f),
        .cameraUp = glm::vec3(0.0f, 1.0f, 0.0f),
        .warmupFrames = 60,
        .measureFrames = 300
    });

    // Configuration 4: Pebble subdivision level 3
    configs.push_back({
        .name = "Pebble L3 (1K faces)",
        .meshPath = "assets/bunny.obj",
        .elementType = 100,  // Pebble
        .resolutionM = 0,
        .resolutionN = 0,
        .enableCulling = false,
        .enableLOD = false,
        .enablePebble = true,
        .cameraPosition = glm::vec3(0.0f, 1.0f, 3.0f),
        .cameraTarget = glm::vec3(0.0f, 0.5f, 0.0f),
        .cameraUp = glm::vec3(0.0f, 1.0f, 0.0f),
        .warmupFrames = 60,
        .measureFrames = 300
    });

    // Configuration 5: Dragon with frustum culling
    configs.push_back({
        .name = "Dragon + Culling",
        .meshPath = "assets/dragon.obj",
        .elementType = 0,  // Torus
        .resolutionM = 8,
        .resolutionN = 8,
        .enableCulling = true,
        .enableLOD = false,
        .enablePebble = false,
        .cameraPosition = glm::vec3(2.0f, 1.0f, 2.0f),
        .cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f),
        .cameraUp = glm::vec3(0.0f, 1.0f, 0.0f),
        .warmupFrames = 60,
        .measureFrames = 300
    });

    // Configuration 6: Dragon with LOD
    configs.push_back({
        .name = "Dragon + LOD",
        .meshPath = "assets/dragon.obj",
        .elementType = 0,
        .resolutionM = 8,
        .resolutionN = 8,
        .enableCulling = false,
        .enableLOD = true,
        .enablePebble = false,
        .cameraPosition = glm::vec3(2.0f, 1.0f, 2.0f),
        .cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f),
        .cameraUp = glm::vec3(0.0f, 1.0f, 0.0f),
        .warmupFrames = 60,
        .measureFrames = 300
    });

    return configs;
}
```

### Step 4: Add Command-Line Interface

Modify `main.cpp`:

```cpp
#include "GravelApp.h"
#include "profiling/Benchmark.h"
#include "profiling/PaperBenchmarks.h"
#include <iostream>
#include <cstring>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n"
              << "Options:\n"
              << "  --benchmark         Run automated benchmark suite\n"
              << "  --benchmark-output  Output CSV filename (default: benchmark_results.csv)\n"
              << "  --help              Show this help message\n";
}

int main(int argc, char** argv) {
    bool runBenchmarks = false;
    std::string outputFile = "benchmark_results.csv";

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            runBenchmarks = true;
        } else if (strcmp(argv[i], "--benchmark-output") == 0 && i + 1 < argc) {
            outputFile = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    try {
        GravelApp app;
        app.init();

        if (runBenchmarks) {
            // Benchmark mode
            std::cout << "=== Gravel Benchmark Mode ===" << std::endl;

            BenchmarkSuite suite;
            auto configs = createPaperBenchmarks();
            for (const auto& config : configs) {
                suite.addBenchmark(config);
            }

            suite.runAll(&app);
            suite.exportCSV(outputFile);

            return 0;
        } else {
            // Interactive mode
            app.run();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

### Step 5: Add Benchmark Script

Create `scripts/run_benchmarks.sh`:

```bash
#!/bin/bash

echo "=== Gravel Benchmark Runner ==="
echo ""

# Check if executable exists
if [ ! -f "./build/Gravel" ]; then
    echo "Error: Gravel executable not found at ./build/Gravel"
    echo "Please build the project first with: cmake --build build"
    exit 1
fi

# Create results directory
mkdir -p benchmark_results

# Get timestamp for filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
OUTPUT_FILE="benchmark_results/results_${TIMESTAMP}.csv"

echo "Running benchmarks..."
echo "Output file: ${OUTPUT_FILE}"
echo ""

# Run benchmarks
./build/Gravel --benchmark --benchmark-output "${OUTPUT_FILE}"

if [ $? -eq 0 ]; then
    echo ""
    echo "=== Benchmark Complete ==="
    echo "Results saved to: ${OUTPUT_FILE}"

    # Optionally generate charts if Python script exists
    if [ -f "scripts/analyze_results.py" ]; then
        echo ""
        echo "Generating performance charts..."
        python3 scripts/analyze_results.py "${OUTPUT_FILE}"
    fi
else
    echo "Benchmark failed with exit code $?"
    exit 1
fi
```

Make executable:
```bash
chmod +x scripts/run_benchmarks.sh
```

## Acceptance Criteria

- [ ] Benchmark suite runs headless without window/ImGui
- [ ] All paper configurations (Torus 8×8, 16×16, Sphere, Pebble, Dragon+Culling, Dragon+LOD) execute
- [ ] CSV output contains frame time statistics (avg, min, max, stddev, median)
- [ ] Geometry counts and VRAM usage included in results
- [ ] Warm-up phase prevents measurement skew
- [ ] Results match paper's Table 1 within 20% (accounting for hardware differences)

## Validation Tests

### Test 1: CSV Format
```bash
./build/Gravel --benchmark --benchmark-output test.csv
head -n 1 test.csv  # Should show header row
wc -l test.csv      # Should be 7 lines (1 header + 6 benchmarks)
```

### Test 2: Performance Range
```cpp
// On RTX 3080, expect:
// Torus 8×8: 4-6 ms (~166-250 FPS)
// Torus 16×16: 10-15 ms (~66-100 FPS)
// Pebble L3: 6-10 ms (~100-166 FPS)
```

### Test 3: Statistical Consistency
```cpp
// StdDev should be < 10% of mean for stable frame times
assert(result.stdDevFrameTime < 0.1f * result.avgFrameTime);
```

## Troubleshooting

**Issue**: Benchmark mode shows window
- **Solution**: Disable ImGui rendering in benchmark mode, or run truly headless with offscreen rendering

**Issue**: First few frames much slower than rest
- **Solution**: Increase `warmupFrames` to 120+

**Issue**: High variance in frame times
- **Solution**: Close background applications, disable CPU frequency scaling, lock GPU clocks

**Issue**: Results don't match paper
- **Solution**: Check GPU model, driver version, ensure similar mesh complexities

## Next Steps

Proceed to **[Feature 8.3 - Memory Analysis](feature-03-memory-analysis.md)** to measure VRAM usage.
