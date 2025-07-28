# Multi-attribute Linear-hashed Files Database System

## Overview

This project implements a Multi-attribute Linear-hashed Files database system for COMP9315 Assignment 2. The system provides basic database operations including creating relations, inserting data, querying with selection and projection, and managing hash-based file storage.

## Features

- **Relation Creation**: Create new database relations with configurable attributes and pages
- **Data Insertion**: Insert tuples into relations
- **Query Processing**: Execute queries with selection and projection operations
- **Hash-based Storage**: Multi-attribute linear hashing for efficient data storage
- **Statistics**: View relation statistics and storage information
- **Data Generation**: Generate test data for system testing
- **Data Dumping**: Export relation data for inspection

## Project Structure

```
├── Core Components
│   ├── create.c      # Relation creation utility
│   ├── insert.c      # Data insertion utility
│   ├── query.c       # Query processing utility
│   ├── dump.c        # Data export utility
│   ├── stats.c       # Statistics utility
│   └── gendata.c     # Test data generator
├── Database Engine
│   ├── reln.c/h      # Relation management
│   ├── page.c/h      # Page management
│   ├── tuple.c/h     # Tuple operations
│   ├── select.c/h    # Selection operations
│   ├── project.c/h   # Projection operations
│   ├── hash.c/h      # Hash functions
│   ├── chvec.c/h     # Choice vector operations
│   └── bits.c/h      # Bit manipulation utilities
├── Utilities
│   ├── util.c/h      # General utilities
│   └── defs.h        # Global definitions
├── Test Files
│   ├── test01.sh     # Basic functionality test
│   ├── test02.sh     # Advanced functionality test
│   ├── test03.sh     # Comprehensive query test
│   ├── data0.txt     # Small test dataset
│   └── data1.txt     # Large test dataset
└── Makefile          # Build configuration
```

## Building the Project

To build all executables:

```bash
make
```

To clean build artifacts:

```bash
make clean
```

## Usage

### 1. Creating a Relation

```bash
./create [-v] RelName #attrs #pages ChoiceVector
```

**Parameters:**
- `RelName`: Name of the relation
- `#attrs`: Number of attributes (2-10)
- `#pages`: Initial number of pages (1-64)
- `ChoiceVector`: Hash function configuration (format: "attr,bit:attr,bit:...")
- `-v`: Verbose mode (optional)

**Example:**
```bash
./create R 3 5 "0,1:1,1:2,1:3,1:4,1"
```

### 2. Inserting Data

```bash
./insert RelName < data_file
```

**Example:**
```bash
./insert R < data0.txt
```

### 3. Querying Data

```bash
./query [-v] 'attributes' from RelName where 'conditions'
```

**Parameters:**
- `attributes`: Comma-separated attribute list or '*' for all attributes
- `RelName`: Name of the relation to query
- `conditions`: Query conditions with support for:
  - `?`: Unknown value (wildcard)
  - `%`: Pattern matching (zero or more characters)
  - `value`: Exact value matching

**Examples:**
```bash
# Query all attributes where first attribute is '1042'
./query '*' from R where '1042,?,?'

# Query with pattern matching
./query '*' from R where '101%,?,?'

# Query specific attributes
./query '1,2' from R where '101%,?,?'
```

### 4. Viewing Statistics

```bash
./stats RelName
```

### 5. Dumping Data

```bash
./dump RelName
```

### 6. Generating Test Data

```bash
./gendata num_tuples num_attrs seed
```

## Test Scripts

The project includes three test scripts to verify functionality:

### Test 1 (Basic)
```bash
./test01.sh
```
Tests basic relation creation and data insertion.

### Test 2 (Advanced)
```bash
./test02.sh
```
Tests more complex scenarios with statistics.

### Test 3 (Comprehensive)
```bash
./test03.sh
```
Tests various query patterns including:
- Exact value matching
- Pattern matching with wildcards
- Attribute projection
- Complex selection conditions

## Data Format

### Input Data Format
Tuples are comma-separated values, one tuple per line:
```
1,kaleidoscope,hieroglyph,navy,hieroglyph
2,floodlight,fork,drill,sunglasses
3,bridge,torch,yellow,festival
```

### Query Conditions
- `?`: Matches any value
- `%`: Pattern matching (e.g., `101%` matches strings starting with "101")
- `value`: Exact value matching

## Technical Details

### System Constants
- **Page Size**: 1024 bytes
- **Maximum Tuple Length**: 200 characters
- **Maximum Relation Name**: 200 characters
- **Maximum Attributes**: 10 per relation

### Hash Function
The system uses multi-attribute linear hashing with configurable choice vectors that determine which attributes and bits are used for hash computation.

### File Structure
Each relation consists of three files:
- `R.data`: Main data file
- `R.info`: Relation metadata
- `R.ovflow`: Overflow pages for hash collisions

## Error Handling

The system provides comprehensive error handling for:
- Invalid command-line arguments
- Non-existent relations
- Invalid query syntax
- File I/O errors
- Memory allocation failures

## Dependencies

- GCC compiler with C99 support
- Standard C libraries (stdlib, stdio, string, assert)
- Math library (-lm)

## Author
Ziyi
