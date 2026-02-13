#pragma once

#include <Arduino.h>

// This header file contains constants, structs, and function declarations
// shared between main.cpp and auto_generator.cpp to reduce code duplication.

// --- Shared Constants ---

// Default duration for a full motor speed ramp (0 to 1000).
const int DEFAULT_RAMP_DURATION_MS = 4000;

// --- Shared Data Structures ---

// Struct for the speed-to-revolution-time lookup table.
struct SpeedSyncPair {
    int logicalSpeed;
    int revTimeMs;
};

extern const SpeedSyncPair g_speedSyncTable[];
extern const int g_speedSyncTableSize;

// --- Shared Function Declarations ---

// Calculates the estimated revolution time in ms for a given logical speed.
long calculate_rev_time_ms(int speed);