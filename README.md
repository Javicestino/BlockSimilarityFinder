# 3D Block Duplicate Detection with Hashing

This project implements an efficient method for detecting duplicate 3D blocks in large datasets. It processes a 3D image file, applies a threshold to convert it into binary data, extracts 3D blocks, and uses a hash table to identify and count duplicate blocks. The solution is optimized for large-scale datasets, ensuring both performance and memory efficiency.

## Features

- **Efficient Duplicate Detection**: The program uses a hash table to detect and count duplicate 3D blocks quickly.
- **3D Data Processing**: Processes 3D data, such as images or volumetric datasets, for block-level comparison.
- **Optimized Performance**: Designed for large datasets with optimizations for memory usage and processing time.
- **Scalable**: Supports parallel processing using MPI for distributed computing.

## Prerequisites

To compile and run the program, you need the following:

- A C compiler (e.g., GCC)
- MPI library for parallel processing (for the MPI version)
- Standard C library for basic operations

## Installation

### 1. Clone the repository:

```bash
git clone https://github.com/yourusername/3d-block-duplicate-detection.git
cd 3d-block-duplicate-detection
