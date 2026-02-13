/*
            Auto-mode
 Triggered via the commmand: "auto_mode:mmm" where mmm is the duration in minutes. The code auto-generates a script much like the
 hardcoded "funky" script, and then plays it out. Also, add the command "auto_mode_debug:mmm" which does the same thing except it only outputs
 the generated commands to the terminal, but does not run them. 

 We have a large array of commands at your disposal, and a command processing system that allows us to feed commands in sequentially. 
 For the specified period of time we compute a series of pseudo-randomly generate commands to create visually pleasing effects. 
 I suggest that the system consider the available time duration, and considering human attention span, try to generate a series of 
 commands that approximately fill that time (it doesn't have to be absolutely precise. We have the visual imensions of: 
  - motor speed and direction
  - LED color
  - LED brightness 
  - led cycle direction
  - led tails - color, length and number
  - various led visual effects defined in the command set, ie. rainbow, sine hue, sine pulse 
 
  The sculpture rotates on a base powered by the motor and has a spiral shape consisting of an inner and outer helix that joins
  back at it's starting point to create a closed loop. The helix completes three full turns in space to complete the loop from start
  to finish, and due to its spiral shape it creates a visual illusion of vertical motion. The eye tends to follow the curve up and down,
  and the led cycling serves to augment the visual effect. By default, the leds cycle in the same direction as the motor rotation, and when
  the motor changes speed and direction the led cycling follows suit. But, we can also reverse the direction of led cycling in relation to
  the motor, and also change the speed. This can create effects where the led tails appear to be chasing after the spiral, and maybe overtaking
  it or falling behind. 
  
  Generate the series of commands up front and print them out to the terminal so that we can review what the code created. And then
  proceed into the auto-generated script. Use a random seed based on current time that the auto_mode command is run. Cap the number of generated
  commands to 100. Place all the code for this in this new file, and create functions that main.cpp can call.   

  


*/
#include "auto_generator.h"
#include <Arduino.h>
#include <vector>
#include <string>

// This file can't access the log_t function in main.cpp directly.
// We'll use Serial.printf for logging within this module.
#define AUTO_LOG(format, ...) Serial.printf("%lu ms: [AutoGenerator] " format "\n", millis(), ##__VA_ARGS__)

namespace AutoGenerator {

// A reasonable guess for ramp duration for timing calculations.
const int APPROX_RAMP_DURATION_MS = 4000;

// Helper to create a command string from various parameter types
std::string format_command(const char* cmd, long val) {
    char buffer[32];
    sprintf(buffer, "%s:%ld", cmd, val);
    return std::string(buffer);
}

std::string format_command(const char* cmd, int val1, int val2) {
    char buffer[32];
    sprintf(buffer, "%s:%d,%d", cmd, val1, val2);
    return std::string(buffer);
}

std::string format_command(const char* cmd, int val1, int val2, int val3) {
    char buffer[48];
    sprintf(buffer, "%s:%d,%d,%d", cmd, val1, val2, val3);
    return std::string(buffer);
}

std::vector<std::string> generateScript(int duration_minutes) {
    std::vector<std::string> script;
    if (duration_minutes <= 0) return script;

    randomSeed(millis());

    long total_duration_ms = duration_minutes * 60L * 1000L;
    long accumulated_duration_ms = 0;

    // --- Dynamic Command Limit based on Available Memory ---
    // Instead of a fixed limit, we calculate how many commands we can safely store.
    // This prevents running out of memory on long auto-mode durations.
    const long HEAP_SAFETY_MARGIN = 50 * 1024; // Reserve 50KB of heap for other operations.
    const int AVG_COMMAND_MEMORY_COST = 48;   // A generous estimate for a std::string in a vector.
    const int ABSOLUTE_MAX_COMMANDS = 2000;   // A hard cap to prevent excessively long generation.

    long free_heap = ESP.getFreeHeap();
    long available_for_script = free_heap - HEAP_SAFETY_MARGIN;
    int max_commands = 100; // Default fallback
    if (available_for_script > 0) {
        max_commands = min(ABSOLUTE_MAX_COMMANDS, (int)(available_for_script / AVG_COMMAND_MEMORY_COST));
    }

    AUTO_LOG("Generating auto-script for %d minutes (%ld ms)...", duration_minutes, total_duration_ms);
    AUTO_LOG("Heap: %ldB free. Dynamic max commands set to: %d", free_heap, max_commands);

    script.push_back("system_reset");
    script.push_back("hold:1000");
    accumulated_duration_ms += 1000;

    while (accumulated_duration_ms < total_duration_ms && script.size() < max_commands - 4) {
        long scene_duration_ms = random(15000, 45001); // 15 to 45 seconds per scene

        // --- Color & Effect Generation with Basic Color Theory ---
        // Instead of pure random colors, we'll pick a relationship (e.g., complementary)
        // to create more visually pleasing and intentional scenes.

        uint8_t base_hue = random(256);
        uint8_t tail_hue;
        uint8_t bg_hue;

        int color_strategy = random(100);
        if (color_strategy < 50) { // 50% Complementary: Opposites on the color wheel.
            bg_hue = base_hue;
            tail_hue = (base_hue + 128) % 256;
        } else if (color_strategy < 80) { // 30% Analogous: Colors that are next to each other.
            bg_hue = base_hue;
            tail_hue = (base_hue + random(20, 41)) % 256;
        } else { // 20% Monochromatic: Variations of a single hue.
            bg_hue = base_hue;
            tail_hue = base_hue;
        }

        // --- Command Generation ---
        if (random(100) < 80) script.push_back(format_command("motor_speed", (long)random(400, 1001)));
        if (random(100) < 25) {
            script.push_back("motor_reverse");
            script.push_back(format_command("hold", (long)APPROX_RAMP_DURATION_MS + 1000));
            accumulated_duration_ms += APPROX_RAMP_DURATION_MS + 1000;
        }

        if (random(100) < 70) script.push_back(format_command("led_background", (int)bg_hue, (int)random(10, 41)));
        else script.push_back("led_background:0,0"); // Occasionally have no background

        if (random(100) < 85) script.push_back(format_command("led_tails", (int)tail_hue, (int)random(5, 25), (int)random(1, 6)));
        else script.push_back("led_tails:0,1,0"); // Occasionally have no tails

        int effect_choice = random(100);
        if (effect_choice < 20) {
            script.push_back("led_rainbow"); // Rainbow overrides our careful color choices, which is fine for variety.
        } else if (effect_choice < 40) {
            // Sine Hue will oscillate around the chosen tail hue.
            uint8_t hue_low = (tail_hue - 30 + 256) % 256;
            uint8_t hue_high = (tail_hue + 30) % 256;
            script.push_back(format_command("led_sine_hue", (int)hue_low, (int)hue_high));
        } else if (effect_choice < 60) {
            script.push_back(format_command("led_sine_pulse", (int)random(10, 41), (int)random(70, 101)));
        }

        if (random(100) < 15) script.push_back("led_reverse");
        if (random(100) < 10) script.push_back(format_command("led_brightness", (long)random(50, 101)));

        long remaining_time = total_duration_ms - accumulated_duration_ms;
        if (scene_duration_ms > remaining_time && remaining_time > 1000) scene_duration_ms = remaining_time;
        script.push_back(format_command("hold", scene_duration_ms));
        accumulated_duration_ms += scene_duration_ms;
    }

    script.push_back("motor_speed:500");
    script.push_back("hold:5000");
    script.push_back("system_off");

    AUTO_LOG("Generated %d script commands for a total duration of ~%ld ms.", script.size(), accumulated_duration_ms);
    Serial.println("\n--- BEGIN AUTO-GENERATED SCRIPT ---");
    for (const auto& cmd : script) { Serial.println(cmd.c_str()); }
    Serial.println("--- END AUTO-GENERATED SCRIPT ---");
    Serial.printf("Total script lines generated: %d\n\n", (int)script.size());

    return script;
}

} // namespace AutoGenerator