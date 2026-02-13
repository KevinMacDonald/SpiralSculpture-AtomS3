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

  Some general guidance on command generation:
  - Keep the motor speed at 500 at the minimum, because below that the motor might stall and not rotate.
  - Limit the holds on a particular command to about 15 seconds before moving on. 
  - It is more dramatic when motor speed changes more often across the full range. Place a bias on changing motor speed more aggressively.
  - Place a bias (not a strict one) towards matching led cycling more closely to motor speed, so that when the motor is turning faster 
  - animation speeds up, and vice versa. But you can depart from this. What auto mode should attempt to do is establish a rythm, but then
  - break from the rythm over time to create interest. 
  - Overall, we want to establish a vibe, build some tension, do something flashy, and then re-establish a vibe. Repeated tension, resolution.
    Like a musical piece. 
  - Some ideas for the different phases:
    INTRODUCTION: You could start with motor off, and gradually ramp up lighting activity, leading into the VIBE.
    VIBE: Hang onto the vibe for a minute or so. Some variation to spark interest. Favor 1 or 2 led cycles more often.
    TENSION: Really start to ramp things up. led cycles can step up towards 3 to 5. 
    CLIMAX: Go crazy! More led cycles. Rapid led cycle direction changes. Rapid color changes. Hang onto higher motor speeds. 
    COOL_DOWN: Taper off activity. Bring the audience down gently. 

  - Some suggestions for phase durations:
    INTRODUCTION: About 30 seconds.
    VIBE: About 2 minutes.  
    TENSION: About 1 minutes.
    CLIMAX: 30 seconds.
    COOL_DOWN: About 1 minute.

  - Overall structure of the musical piece:
    - INTRODUCTION and COOL_DOWN only happen at beginning and end. 
    - VIBE, TENSION, and CLIMAX can cycle through to fill in the duration. 
    - Any composition should at least have one pass through everything, which implies a min duration of 5 minutes for auto. That's 
      about the length of modern pop song. 

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

std::string format_command(const char* cmd, int val1, int val2, int val3, int val4, int val5) {
    char buffer[64];
    sprintf(buffer, "%s:%d,%d,%d,%d,%d", cmd, val1, val2, val3, val4, val5);
    return std::string(buffer);
}

// Per guidance, the generator now follows a musical structure.
enum MusicalPhase {
    INTRODUCTION,
    VIBE,
    TENSION,
    CLIMAX,
    COOL_DOWN
};

std::vector<std::string> generateScript(int duration_minutes) {
    std::vector<std::string> script;
    if (duration_minutes <= 0) return script;

    randomSeed(millis());

    long total_duration_ms = duration_minutes * 60L * 1000L;

    // --- Musical Structure Durations ---
    long intro_duration_ms = 30 * 1000L;
    // The main body (Vibe/Tension/Climax) will fill the time between intro and outro.
    long cool_down_duration_ms = 60 * 1000L;
    long min_full_show_ms = intro_duration_ms + 120 * 1000L + cool_down_duration_ms; // Approx 3.5 mins for one cycle

    // If requested duration is shorter than a full show, scale down phase times.
    if (total_duration_ms < min_full_show_ms) {
        float scale_factor = (float)total_duration_ms / (float)min_full_show_ms;
        if (scale_factor < 0.5) scale_factor = 0.5; // Don't make them too short
        intro_duration_ms *= scale_factor;
        cool_down_duration_ms *= scale_factor;
    }

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

    // --- 1. INTRODUCTION ---
    if (intro_duration_ms > 1000) {
        AUTO_LOG("Generating Phase: INTRODUCTION");
        long intro_remaining_ms = intro_duration_ms;
        script.push_back("motor_speed:0");
        script.push_back("led_reset");
        script.push_back("hold:2000"); intro_remaining_ms -= 2000;
        script.push_back(format_command("led_background", (int)random(256), 5));
        long hold1 = min(5000L, intro_remaining_ms / 3L);
        if (hold1 > 0) { script.push_back(format_command("hold", hold1)); intro_remaining_ms -= hold1; }
        script.push_back(format_command("led_tails", (int)random(256), 10, 1));
        long hold2 = min(5000L, intro_remaining_ms / 2L);
        if (hold2 > 0) { script.push_back(format_command("hold", hold2)); intro_remaining_ms -= hold2; }
        script.push_back("motor_speed:500");
        script.push_back(format_command("led_background", (int)random(256), 15));
        if (intro_remaining_ms > 1000) script.push_back(format_command("hold", intro_remaining_ms));
        accumulated_duration_ms += intro_duration_ms;
    }

    // --- 2. MAIN BODY (VIBE -> TENSION -> CLIMAX loop) ---
    MusicalPhase currentPhase = VIBE;

    while (accumulated_duration_ms < (total_duration_ms - cool_down_duration_ms) && script.size() < max_commands - 10) { // Leave room for cooldown and finale
        long scene_duration_ms = 0;
        uint8_t base_hue = random(256);
        uint8_t tail_hue;
        uint8_t bg_hue;

        // --- Generate a scene based on the current musical phase ---
        switch (currentPhase) {
            case VIBE: {
                AUTO_LOG("Generating Phase: VIBE");
                scene_duration_ms = random(25000, 45001); // Longer, more stable scenes

                // Colors: Harmonious (Analogous/Monochromatic)
                if (random(100) < 70) { // 70% Analogous
                    bg_hue = base_hue;
                    tail_hue = (base_hue + random(20, 41)) % 256;
                } else { // 30% Monochromatic
                    bg_hue = base_hue;
                    tail_hue = base_hue;
                }

                script.push_back(format_command("motor_speed", (long)random(500, 701)));
                script.push_back(format_command("led_background", (int)bg_hue, (int)random(15, 31)));
                script.push_back(format_command("led_tails", (int)tail_hue, (int)random(10, 25), (int)random(1, 3))); // Favor 1 or 2 tails

                if (random(100) < 50) { // Gentle sine hue effect
                    uint8_t hue_low = (tail_hue - 20 + 256) % 256;
                    uint8_t hue_high = (tail_hue + 20) % 256;
                    script.push_back(format_command("led_sine_hue", (int)hue_low, (int)hue_high));
                }
                currentPhase = TENSION; // Transition to next phase
                break;
            }

            case TENSION: {
                AUTO_LOG("Generating Phase: TENSION");
                scene_duration_ms = random(15000, 25001); // Medium length, building scenes

                // Colors: High-contrast (Complementary)
                bg_hue = base_hue;
                tail_hue = (base_hue + 128) % 256;

                script.push_back(format_command("motor_speed", (long)random(750, 951))); // Ramp up speed
                script.push_back(format_command("led_background", (int)bg_hue, (int)random(25, 41)));
                script.push_back(format_command("led_tails", (int)tail_hue, (int)random(5, 15), (int)random(3, 6)));

                if (random(100) < 60) { // Pulsing brightness effect
                    script.push_back(format_command("led_sine_pulse", (int)random(20, 51), (int)random(80, 101)));
                }
                if (random(100) < 20) script.push_back("led_reverse");
                currentPhase = CLIMAX; // Transition to next phase
                break;
            }

            case CLIMAX: {
                AUTO_LOG("Generating Phase: CLIMAX");
                scene_duration_ms = random(10000, 20001); // Shorter, punchier scenes
                script.push_back(format_command("motor_speed", (long)random(900, 1001))); // Max speed
                
                int climax_effect = random(100);
                if (climax_effect < 50) { // Big rainbow finish
                    script.push_back("led_rainbow");
                    script.push_back(format_command("led_tails", 0, (int)random(10, 20), (int)random(3, 6)));
                } else { // Strobing blink finish
                    uint8_t blink_hue = random(256);
                    int blink_count = random(5, 11);
                    script.push_back(format_command("led_blink", (int)blink_hue, 100, 100, 200, blink_count));
                    scene_duration_ms = 300 * blink_count + 1000; // Adjust hold for blink duration
                }

                if (random(100) < 50) {
                    script.push_back("motor_reverse");
                    accumulated_duration_ms += APPROX_RAMP_DURATION_MS + 1000;
                }
                currentPhase = VIBE; // Transition back to start
                break;
            }
        }

        long remaining_time = total_duration_ms - accumulated_duration_ms;
        if (scene_duration_ms > remaining_time && remaining_time > 1000) scene_duration_ms = remaining_time;
        script.push_back(format_command("hold", scene_duration_ms));
        accumulated_duration_ms += scene_duration_ms;
    }

    // --- 3. COOL DOWN ---
    if (cool_down_duration_ms > 1000) {
        AUTO_LOG("Generating Phase: COOL_DOWN");
        script.push_back("led_reset");
        script.push_back(format_command("motor_speed", (long)random(400, 501)));
        script.push_back(format_command("led_background", (int)random(256), (int)random(5, 15))); // Dim background
        script.push_back(format_command("led_tails", (int)random(256), (int)random(20, 30), 1)); // One long tail
        script.push_back(format_command("hold", (long)cool_down_duration_ms / 2));
        script.push_back(format_command("motor_speed", (long)random(200, 301)));
        script.push_back(format_command("hold", (long)cool_down_duration_ms / 2));
    }

    // --- 4. FINALE ---
    script.push_back("system_off");

    AUTO_LOG("Generated %d script commands for a total duration of ~%ld ms.", script.size(), accumulated_duration_ms);
    Serial.println("\n--- BEGIN AUTO-GENERATED SCRIPT ---");
    for (const auto& cmd : script) { Serial.println(cmd.c_str()); }
    Serial.println("--- END AUTO-GENERATED SCRIPT ---");
    Serial.printf("Total script lines generated: %d\n\n", (int)script.size());

    return script;
}

} // namespace AutoGenerator