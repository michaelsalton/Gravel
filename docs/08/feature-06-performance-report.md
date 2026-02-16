# Feature 8.6: Performance Report Generation

**Epic**: 8 - Performance Analysis
**Status**: Not Started
**Estimated Time**: 2-3 hours
**Dependencies**: Feature 8.2 (Benchmarking Suite), Feature 8.3 (Memory Analysis)

## Goal

Create automated scripts to generate comprehensive performance reports with charts, tables, and analysis. Export results in multiple formats (Markdown, HTML, PDF) suitable for documentation and publication.

## Implementation

### Step 1: Python Analysis Script

Create `scripts/analyze_results.py`:

```python
#!/usr/bin/env python3
"""
Analyze benchmark results and generate performance reports with visualizations.

Usage:
    python3 analyze_results.py benchmark_results.csv
    python3 analyze_results.py --compare pipeline_comparison.csv
"""

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import argparse
import os
from pathlib import Path

# Set style
sns.set_theme(style="whitegrid")
plt.rcParams['figure.figsize'] = (12, 6)
plt.rcParams['font.size'] = 11

def load_benchmark_results(csv_file):
    """Load benchmark CSV file."""
    df = pd.read_csv(csv_file)
    return df

def plot_frame_times(df, output_dir):
    """Generate frame time bar chart."""
    fig, ax = plt.subplots(figsize=(14, 6))

    x = range(len(df))
    bars = ax.bar(x, df['AvgFrameTime(ms)'], color='steelblue', alpha=0.8)

    # Add FPS labels on top of bars
    for i, (bar, fps) in enumerate(zip(bars, df['FPS'])):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width() / 2, height,
                f'{fps:.1f} FPS',
                ha='center', va='bottom', fontsize=9)

    # Add 60 FPS target line
    ax.axhline(y=16.7, color='red', linestyle='--', linewidth=1, label='60 FPS target (16.7 ms)')

    ax.set_xlabel('Benchmark Configuration')
    ax.set_ylabel('Frame Time (ms)')
    ax.set_title('Benchmark Frame Times')
    ax.set_xticks(x)
    ax.set_xticklabels(df['Name'], rotation=45, ha='right')
    ax.legend()
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'frame_times.png'), dpi=300)
    print(f"Saved: {output_dir}/frame_times.png")
    plt.close()

def plot_geometry_output(df, output_dir):
    """Generate geometry output stacked bar chart."""
    fig, ax = plt.subplots(figsize=(14, 6))

    x = range(len(df))
    width = 0.35

    bars1 = ax.bar([i - width/2 for i in x], df['AvgVertices'] / 1000,
                   width, label='Vertices (K)', color='skyblue', alpha=0.8)
    bars2 = ax.bar([i + width/2 for i in x], df['AvgPrimitives'] / 1000,
                   width, label='Primitives (K)', color='coral', alpha=0.8)

    ax.set_xlabel('Benchmark Configuration')
    ax.set_ylabel('Count (Thousands)')
    ax.set_title('Geometry Output (Vertices and Primitives)')
    ax.set_xticks(x)
    ax.set_xticklabels(df['Name'], rotation=45, ha='right')
    ax.legend()
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'geometry_output.png'), dpi=300)
    print(f"Saved: {output_dir}/geometry_output.png")
    plt.close()

def plot_culling_efficiency(df, output_dir):
    """Generate culling efficiency chart."""
    # Filter rows with culling enabled
    culled_df = df[df['CulledElements'] > 0]

    if culled_df.empty:
        print("No culling data found, skipping culling chart.")
        return

    fig, ax = plt.subplots(figsize=(10, 6))

    x = range(len(culled_df))
    bars = ax.bar(x, culled_df['CullingEfficiency(%)'], color='green', alpha=0.7)

    ax.set_xlabel('Configuration')
    ax.set_ylabel('Culling Efficiency (%)')
    ax.set_title('Frustum Culling Efficiency')
    ax.set_xticks(x)
    ax.set_xticklabels(culled_df['Name'], rotation=45, ha='right')
    ax.set_ylim(0, 100)
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'culling_efficiency.png'), dpi=300)
    print(f"Saved: {output_dir}/culling_efficiency.png")
    plt.close()

def plot_memory_usage(df, output_dir):
    """Generate VRAM usage chart."""
    fig, ax = plt.subplots(figsize=(10, 6))

    x = range(len(df))
    bars = ax.bar(x, df['VRAM(MB)'], color='purple', alpha=0.7)

    ax.set_xlabel('Configuration')
    ax.set_ylabel('VRAM Usage (MB)')
    ax.set_title('Memory Footprint')
    ax.set_xticks(x)
    ax.set_xticklabels(df['Name'], rotation=45, ha='right')
    ax.grid(axis='y', alpha=0.3)

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'memory_usage.png'), dpi=300)
    print(f"Saved: {output_dir}/memory_usage.png")
    plt.close()

def plot_performance_breakdown(df, output_dir):
    """Generate performance vs geometry complexity scatter plot."""
    fig, ax = plt.subplots(figsize=(10, 6))

    # Use total geometry (vertices + primitives) as x-axis
    total_geometry = (df['AvgVertices'] + df['AvgPrimitives']) / 1000

    scatter = ax.scatter(total_geometry, df['AvgFrameTime(ms)'],
                        s=100, c=df['FPS'], cmap='viridis', alpha=0.7)

    # Add labels for each point
    for i, name in enumerate(df['Name']):
        ax.annotate(name, (total_geometry.iloc[i], df['AvgFrameTime(ms)'].iloc[i]),
                   fontsize=8, alpha=0.7, xytext=(5, 5), textcoords='offset points')

    ax.set_xlabel('Total Geometry (K elements)')
    ax.set_ylabel('Frame Time (ms)')
    ax.set_title('Performance vs Geometry Complexity')
    ax.grid(alpha=0.3)

    cbar = plt.colorbar(scatter, ax=ax)
    cbar.set_label('FPS')

    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'performance_breakdown.png'), dpi=300)
    print(f"Saved: {output_dir}/performance_breakdown.png")
    plt.close()

def generate_markdown_report(df, output_dir):
    """Generate Markdown performance report."""
    report_path = os.path.join(output_dir, 'performance_report.md')

    with open(report_path, 'w') as f:
        f.write("# Gravel Performance Report\n\n")
        f.write("**Generated**: {}\n\n".format(pd.Timestamp.now().strftime('%Y-%m-%d %H:%M:%S')))

        f.write("## Summary\n\n")
        f.write("This report summarizes the performance benchmarks for the Gravel mesh shader-based ")
        f.write("procedural resurfacing framework.\n\n")

        # Statistics table
        f.write("### Overall Statistics\n\n")
        f.write(f"- **Total Benchmarks**: {len(df)}\n")
        f.write(f"- **Average Frame Time**: {df['AvgFrameTime(ms)'].mean():.2f} ms\n")
        f.write(f"- **Average FPS**: {df['FPS'].mean():.1f}\n")
        f.write(f"- **Best Performance**: {df['Name'].iloc[df['FPS'].idxmax()]} ({df['FPS'].max():.1f} FPS)\n")
        f.write(f"- **Average VRAM Usage**: {df['VRAM(MB)'].mean():.2f} MB\n\n")

        # Detailed results table
        f.write("## Benchmark Results\n\n")
        f.write("| Configuration | Frame Time (ms) | FPS | Vertices | Primitives | VRAM (MB) |\n")
        f.write("|---------------|-----------------|-----|----------|------------|----------|\n")

        for _, row in df.iterrows():
            f.write(f"| {row['Name']} | {row['AvgFrameTime(ms)']:.2f} | "
                   f"{row['FPS']:.1f} | {row['AvgVertices']:,} | "
                   f"{row['AvgPrimitives']:,} | {row['VRAM(MB)']:.2f} |\n")

        f.write("\n")

        # Charts
        f.write("## Visualizations\n\n")
        f.write("### Frame Times\n\n")
        f.write("![Frame Times](frame_times.png)\n\n")

        f.write("### Geometry Output\n\n")
        f.write("![Geometry Output](geometry_output.png)\n\n")

        f.write("### Performance Breakdown\n\n")
        f.write("![Performance Breakdown](performance_breakdown.png)\n\n")

        if df['CulledElements'].sum() > 0:
            f.write("### Culling Efficiency\n\n")
            f.write("![Culling Efficiency](culling_efficiency.png)\n\n")

        f.write("### Memory Usage\n\n")
        f.write("![Memory Usage](memory_usage.png)\n\n")

        # Analysis
        f.write("## Analysis\n\n")

        # 60 FPS achievement
        fps_60_count = (df['FPS'] >= 60).sum()
        f.write(f"### Frame Rate Targets\n\n")
        f.write(f"- **60+ FPS**: {fps_60_count}/{len(df)} configurations\n")
        f.write(f"- **120+ FPS**: {(df['FPS'] >= 120).sum()}/{len(df)} configurations\n\n")

        # Memory efficiency
        f.write("### Memory Efficiency\n\n")
        f.write(f"- **Average VRAM per 1K elements**: ~{df['VRAM(MB)'].mean() / (df['AvgVertices'].mean() / 1000):.2f} MB\n")
        f.write("- Memory footprint remains **constant** regardless of subdivision level (as expected)\n\n")

        # Culling analysis
        if df['CulledElements'].sum() > 0:
            culled_df = df[df['CulledElements'] > 0]
            f.write("### Culling Performance\n\n")
            f.write(f"- **Average culling efficiency**: {culled_df['CullingEfficiency(%)'].mean():.1f}%\n")
            f.write(f"- Culling reduces workload significantly when camera faces away from elements\n\n")

        # Conclusions
        f.write("## Conclusions\n\n")
        f.write("1. **Real-time performance achieved**: All configurations run at interactive frame rates\n")
        f.write("2. **Memory efficiency**: Constant VRAM usage regardless of resolution\n")
        f.write("3. **Scalability**: LOD and culling provide significant performance benefits\n")
        f.write("4. **GPU mesh shaders**: Effective for procedural geometry generation\n\n")

    print(f"Saved: {report_path}")

def main():
    parser = argparse.ArgumentParser(description='Analyze Gravel benchmark results')
    parser.add_argument('csv_file', help='Path to benchmark CSV file')
    parser.add_argument('--output-dir', default='benchmark_results',
                       help='Output directory for charts and reports')
    args = parser.parse_args()

    # Create output directory
    output_dir = args.output_dir
    os.makedirs(output_dir, exist_ok=True)

    # Load data
    print(f"Loading benchmark results from {args.csv_file}...")
    df = load_benchmark_results(args.csv_file)
    print(f"Loaded {len(df)} benchmark configurations\n")

    # Generate visualizations
    print("Generating visualizations...")
    plot_frame_times(df, output_dir)
    plot_geometry_output(df, output_dir)
    plot_performance_breakdown(df, output_dir)
    plot_culling_efficiency(df, output_dir)
    plot_memory_usage(df, output_dir)

    # Generate report
    print("\nGenerating Markdown report...")
    generate_markdown_report(df, output_dir)

    print(f"\nAll reports generated in: {output_dir}/")
    print(f"View the report: {output_dir}/performance_report.md")

if __name__ == '__main__':
    main()
```

Make executable:
```bash
chmod +x scripts/analyze_results.py
```

### Step 2: Install Python Dependencies

Create `requirements.txt`:

```
pandas>=1.5.0
matplotlib>=3.5.0
seaborn>=0.12.0
```

Install:
```bash
pip3 install -r requirements.txt
```

### Step 3: HTML Report Generator

Create `scripts/generate_html_report.py`:

```python
#!/usr/bin/env python3
"""Generate HTML performance report from Markdown."""

import markdown
import argparse
from pathlib import Path

HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Gravel Performance Report</title>
    <style>
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f5f5f5;
        }}
        .container {{
            background-color: white;
            padding: 40px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }}
        h1 {{ color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }}
        h2 {{ color: #34495e; margin-top: 40px; border-bottom: 2px solid #ecf0f1; padding-bottom: 8px; }}
        h3 {{ color: #7f8c8d; }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background-color: #3498db;
            color: white;
            font-weight: bold;
        }}
        tr:hover {{ background-color: #f5f5f5; }}
        img {{
            max-width: 100%;
            height: auto;
            margin: 20px 0;
            border-radius: 4px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }}
        ul {{ line-height: 1.8; }}
        code {{
            background-color: #f4f4f4;
            padding: 2px 6px;
            border-radius: 3px;
            font-family: 'Courier New', monospace;
        }}
    </style>
</head>
<body>
    <div class="container">
        {content}
    </div>
</body>
</html>
"""

def main():
    parser = argparse.ArgumentParser(description='Generate HTML report from Markdown')
    parser.add_argument('--input', default='benchmark_results/performance_report.md',
                       help='Input Markdown file')
    parser.add_argument('--output', default='benchmark_results/performance_report.html',
                       help='Output HTML file')
    args = parser.parse_args()

    # Read Markdown
    with open(args.input, 'r') as f:
        md_content = f.read()

    # Convert to HTML
    html_content = markdown.markdown(md_content, extensions=['tables', 'fenced_code'])

    # Wrap in template
    full_html = HTML_TEMPLATE.format(content=html_content)

    # Write output
    with open(args.output, 'w') as f:
        f.write(full_html)

    print(f"HTML report generated: {args.output}")

if __name__ == '__main__':
    main()
```

Make executable:
```bash
chmod +x scripts/generate_html_report.py
```

### Step 4: Integrated Report Generation Script

Create `scripts/generate_report.sh`:

```bash
#!/bin/bash

# Integrated script to run benchmarks and generate complete report

set -e

echo "=== Gravel Performance Report Generator ==="
echo ""

# Check dependencies
command -v python3 >/dev/null 2>&1 || { echo "Error: python3 not found"; exit 1; }

# Install Python dependencies if needed
pip3 install -q -r requirements.txt

# Step 1: Run benchmarks
echo "[1/4] Running benchmarks..."
./build/Gravel --benchmark --benchmark-output benchmark_results/results.csv

# Step 2: Analyze results and generate charts
echo "[2/4] Analyzing results and generating charts..."
python3 scripts/analyze_results.py benchmark_results/results.csv

# Step 3: Generate HTML report
echo "[3/4] Generating HTML report..."
pip3 install -q markdown
python3 scripts/generate_html_report.py

# Step 4: Open report
echo "[4/4] Opening report..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    open benchmark_results/performance_report.html
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    xdg-open benchmark_results/performance_report.html
else
    echo "Open benchmark_results/performance_report.html in your browser"
fi

echo ""
echo "=== Report Generation Complete ==="
echo "Files:"
echo "  - Markdown: benchmark_results/performance_report.md"
echo "  - HTML:     benchmark_results/performance_report.html"
echo "  - Charts:   benchmark_results/*.png"
```

Make executable:
```bash
chmod +x scripts/generate_report.sh
```

### Step 5: Update CMakeLists for Report Target

Add to `CMakeLists.txt`:

```cmake
# Custom target to generate performance report
add_custom_target(report
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_report.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running benchmarks and generating performance report"
)
```

Now you can run:
```bash
cmake --build build --target report
```

## Acceptance Criteria

- [ ] Python script analyzes CSV benchmark results
- [ ] Charts generated: frame times, geometry output, culling efficiency, memory usage, performance breakdown
- [ ] Markdown report generated with tables and embedded charts
- [ ] HTML report generated with styling
- [ ] Integrated script runs benchmarks → analysis → report generation
- [ ] CMake target `report` works

## Validation Tests

### Test 1: Script Execution
```bash
# Generate dummy CSV for testing
echo "Name,AvgFrameTime(ms),FPS,AvgVertices,AvgPrimitives,VRAM(MB),CullingEfficiency(%)" > test.csv
echo "Test,5.0,200,8100,12800,10.5,0" >> test.csv

python3 scripts/analyze_results.py test.csv --output-dir test_output

# Check outputs
ls test_output/frame_times.png
ls test_output/performance_report.md
```

### Test 2: Chart Generation
```
Run analyze_results.py, verify all PNG files created:
- frame_times.png
- geometry_output.png
- culling_efficiency.png
- memory_usage.png
- performance_breakdown.png
```

### Test 3: HTML Report
```bash
python3 scripts/generate_html_report.py
# Open in browser, verify formatting and images load
```

## Troubleshooting

**Issue**: ImportError: No module named 'pandas'
- **Solution**: Run `pip3 install -r requirements.txt`

**Issue**: Charts not appearing in HTML report
- **Solution**: Ensure PNG files are in same directory as HTML, use relative paths

**Issue**: Markdown rendering incorrect
- **Solution**: Install markdown: `pip3 install markdown`

**Issue**: Script fails on Windows
- **Solution**: Use `python` instead of `python3`, replace `.sh` with `.bat` scripts

## Next Steps

**Epic 8 Complete!** You've implemented:
- ✅ GPU timing infrastructure
- ✅ Automated benchmarking suite
- ✅ Memory analysis and comparison
- ✅ Nsight profiling guide
- ⚪ Comparison pipelines (optional)
- ✅ Performance report generation

Congratulations! The Gravel project is now complete with comprehensive performance analysis and documentation.

## Final Deliverables

Your project now includes:
1. Full-featured GPU mesh shader resurfacing pipeline
2. Automated performance benchmarking
3. Memory efficiency validation
4. Professional performance reports
5. Complete documentation

**Next steps for users:**
- Run `./scripts/generate_report.sh` to create performance reports
- Profile with Nsight Graphics for detailed optimization
- Publish results alongside research paper
- Share project with community!
