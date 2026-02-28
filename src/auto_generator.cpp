/*
   ------------------------------------ "auto_mode:MMM" -----------------------------

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
    INTRODUCTION: You could start with motor off, and gradually ramp up lighting activity. Introduce some led effects that benefit from low
    or zero motor speeds. Ramp up towards the VIBE phase. 
    VIBE: Hang onto the vibe for a minute or so. Some variation to spark interest. Favor 1 or 2 led cycles more often. But also explore
    longer led cycle times such that the cycle time is up to twice as long as estimated motor revolution time. 
    TENSION: Really start to ramp things up. led cycles can step up towards 3 to 5 and should mostly have a cycle time equal to or greater
    than motor revolution time.  
    CLIMAX: Go crazy! More led cycles. Rapid led cycle direction changes. Rapid color changes. Hang onto higher motor speeds. Occasionally
    introduce much more rapid led cycle direction changes, creating a see-saw effect. Break up high motor speeds with motor speed zero and 
    lighting effects like marquee. 
    COOL_DOWN: Taper off activity. Bring the audience down gently. 
    With the introduction of additional commands utilizing more of FastLED's sophisticated patterns I think we could take advantage of those 
    to stop motor rotation fully on occasion in order to showcase them. Perlin noise, fire effects, Twinkle/sparkle might benefit from
    being shown during motor ramp downs from higher speed to zero, and then let them showcase for 7 seconds or so before moving on. Or, 
    the reverse of that - kick off a pattern and then ramp the motor up. Those might be most effective during intro and cool down when
    the motor is most likely turning more slowly. And occasionally, a dramatic CLIMAX scene where the motor is still and we go crazy 
    with the light show. So generally I am suggesting that led cycling works best in combination with motor rotation, wherease these more
    advanced light patterns would benefit from or more the min 500 motor speed. 

    On the subject of brightness, it is very important, regardless of what is specified in any script, that I can scale down the brightness
    of everything by invoking the led_global_brightness command. All brightness settings should scale off the global master brightness. This 
    allows any script including auto-generated ones to run exactly as they wish, but if a command tries to set brightness at 100% that is
    still scaled by the global master brightness. That's why we call it "global master" brightness.


  - Some suggestions for phase durations:
    INTRODUCTION: About 30 seconds.
    VIBE: About 2 minutes.  
    TENSION: About 1 minutes.
    CLIMAX: 90 seconds.
    COOL_DOWN: About 1 minute.

  - Overall structure of the musical piece:
    - INTRODUCTION and COOL_DOWN only happen at beginning and end. 
    - VIBE, TENSION, and CLIMAX can cycle through to fill in the duration. 
    - Any composition should at least have one pass through everything, which implies a min duration of 5 minutes for auto. That's 
      about the length of modern pop song. 
    - How about using motor_reverse to signal phase transitions and reserving the usage of that for that purpose?
    - led_display_brightness should naturally start low with the introduction, ramp up to vibe, steadily increase through tension to
      towards max brightness during climax. And then of course, ramp down with cool down. 
    - call out clearly in terminal output " -------------- BEGIN: <phase name> ----------------"
    - Upon completion of an auto-generated script, compose another script of the same duration and execute.

    Upon completion of an auto-generated script, compose another script of the same duration and execute. 

   ------------------------------------ "auto_steady_rotate:MMM" -----------------------------
    Similar to auto_mode, if the generated script runs it's course the system should generate another set of commands and run it again.
    
    This mode runs the motor in the default direction and the default speed, and maintains the motor in that speed and direction. 
    This mode uses comet and marquee led effects only. 
    The intention of this mode is to focus on lighting effects that accentuate the steady motor rotation. To start off, let's try
    creating a cycle where the led effect rotation time starts off at 2x motor speed rotation time - which means the led effect rotation
    is slower than motor speed, and then slowly increases to 0.5x motor speed - which means the led effect rotation is now faster
    than the motor speed. Then, reverse the process back down to 2x motor speed. Provide constants for setting in code the 
    max and min led effect rotation vs motor rotation values. Provide constants for setting in code the number of steps taken
    to increase LED effect speed between min and max, and also the amount of time spent at each step. For example:
    AUTO_STEADY_ROTATE_LED_MOTOR_MAX = 2.0;
    AUTO_STEADY_ROTATE_LED_MOTOR_MIN = 0.5; 
    AUTO_STEADY_ROTATE_LED_EFFECT_STEPS = 10;
    AUTO_STEADY_ROTATE_LED_EFFECT_STEP_DURATION = 2.0; //seconds
        
    With the above initial settings, since we have 10 steps between min and max, each step held for 2 seconds, that would be 
    20 seconds to ramp from MAX TO MIN, and then 20 seconds to ramp from MIN to MAX, creating a total cycle length of 40 seconds 
    with the chosen led effect, before the next effect is chosen.    

    The direction of led effects can be chosen at random. So, sometimes WITH motor rotation, and sometimes AGAINST.

    For each cycle, choose a new lighting effect using either comets or marquee. Then, choose new parameters for that effect, and then
    hold those parameters steady for that cycle. 
    
    For comets you can vary number and length of comets, and foreground and background colors. Perhaps there are other parameters as well.

    For marquee you can vary the length of a marquee unit, and foreground and background colors. Are there other parameters to explore?

    This mode should respect the led_global_brightness and motor_speed commands sent via bluetooth. 

    If the end of the script is encountered for this mode, similiar to auto_mode, generate a new script and run it automatically. 

*/
#include "auto_generator.h"
#include "shared.h"
#include <Arduino.h>
#include <vector>
#include <string>

// This file can't access the log_t function in main.cpp directly.
// We'll use Serial.printf for logging within this module.
#define AUTO_LOG(format, ...) Serial.printf("%lu ms: [AutoGenerator] " format "\n", millis(), ##__VA_ARGS__)

namespace AutoGenerator {

const std::vector<const char*> calm_noise_palettes = {"cloud", "ocean", "forest"};
const std::vector<const char*> energetic_noise_palettes = {"lava", "party", "rainbow"};

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

std::string format_phase_comment(const char* phase_name) {
    char buffer[64];
    sprintf(buffer, "[---------- %s ----------]", phase_name);
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

    // --- Musical Structure Durations & Overview ---
    long intro_duration_ms = 30 * 1000L;
    long cool_down_duration_ms = 60 * 1000L;
    long min_full_show_ms = intro_duration_ms + 120 * 1000L + cool_down_duration_ms; // Approx 3.5 mins for one cycle

    // If requested duration is shorter than a full show, scale down phase times.
    if (total_duration_ms < min_full_show_ms) {
        float scale_factor = (float)total_duration_ms / (float)min_full_show_ms;
        if (scale_factor < 0.5) scale_factor = 0.5; // Don't make them too short
        intro_duration_ms *= scale_factor;
        cool_down_duration_ms *= scale_factor;
    }

    long main_body_duration_ms = total_duration_ms - intro_duration_ms - cool_down_duration_ms;
    if (main_body_duration_ms < 0) main_body_duration_ms = 0;

    const long avg_vibe_ms = (20000 + 30001) / 2; // Keep scenes moving
    const long avg_tension_ms = (15000 + 25001) / 2;
    const long avg_climax_ms = (75000 + 90001) / 2; // Longer, multi-part climax
    const long avg_cycle_ms = avg_vibe_ms + avg_tension_ms + avg_climax_ms;
    int num_cycles = (avg_cycle_ms > 0) ? (main_body_duration_ms / avg_cycle_ms) : 0;

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

    AUTO_LOG("Composition Overview for %d minutes:", duration_minutes);
    AUTO_LOG("  - INTRODUCTION: ~%lds", intro_duration_ms / 1000);
    AUTO_LOG("  - MAIN BODY:    ~%ldm", main_body_duration_ms / 60000);
    if (num_cycles > 0) {
        AUTO_LOG("      - %d cycles of:", num_cycles);
        AUTO_LOG("        - VIBE:    ~%lds", avg_vibe_ms / 1000);
        AUTO_LOG("        - TENSION: ~%lds", avg_tension_ms / 1000);
        AUTO_LOG("        - CLIMAX:  ~%lds", avg_climax_ms / 1000);
    } else if (main_body_duration_ms > 0) {
        AUTO_LOG("      - Partial cycle");
    }
    AUTO_LOG("  - COOL_DOWN:    ~%lds", cool_down_duration_ms / 1000);

    script.push_back("led_reset"); // Use led_reset to clear effects without touching global brightness or motor.
    script.push_back("hold:1000");
    accumulated_duration_ms += 1000;

    // --- 1. INTRODUCTION ---
    if (intro_duration_ms > 1000) {
        script.push_back(format_phase_comment("INTRODUCTION"));
        long intro_remaining_ms = intro_duration_ms;
        script.push_back("led_reset"); // Reset LED state first.
        script.push_back(format_command("led_display_brightness", (long)random(30, 51))); // Start dim

        // Per guidance, showcase a full-strip effect with motor off.
        if (random(100) < 50) { // 50% chance to start with a noise effect
            script.push_back("motor_speed:0");
            const char* palette = calm_noise_palettes[random(calm_noise_palettes.size())];
            char buffer[64];
            sprintf(buffer, "led_effect:noise,%s,5,30", palette);
            script.push_back(buffer);
            script.push_back("hold:7000"); intro_remaining_ms -= 7000;
        } else {
            script.push_back("motor_speed:0");
            script.push_back("hold:2000"); intro_remaining_ms -= 2000;
            script.push_back(format_command("led_background", (int)random(256), 5));
        }

        long hold1 = min(5000L, intro_remaining_ms / 2L);
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
                script.push_back(format_phase_comment("VIBE"));
                script.push_back(format_command("led_display_brightness", (long)random(60, 81))); // Vibe brightness
                scene_duration_ms = random(20000, 30001); // Shorter scenes to keep things moving

                // Colors: Harmonious (Analogous/Monochromatic)
                if (random(100) < 70) { // 70% Analogous
                    bg_hue = base_hue;
                    tail_hue = (base_hue + random(20, 41)) % 256;
                } else { // 30% Monochromatic
                    bg_hue = base_hue;
                    tail_hue = base_hue;
                }

                // Per guidance, occasionally break up the vibe with a full-strip effect
                if (random(100) < 15) {
                    script.push_back(format_command("motor_speed", (long)random(500, 601)));
                    const char* palette = calm_noise_palettes[random(calm_noise_palettes.size())];
                    char buffer[64];
                    sprintf(buffer, "led_effect:noise,%s,8,40", palette);
                    script.push_back(buffer);
                } else {
                    long motor_speed = random(500, 701);
                    script.push_back(format_command("motor_speed", motor_speed));
                    script.push_back(format_command("led_background", (int)bg_hue, (int)random(15, 31)));
                    script.push_back(format_command("led_tails", (int)tail_hue, (int)random(10, 25), (int)random(1, 3)));

                    if (random(100) < 40) { // 40% chance to set a custom cycle time
                        long est_rev_time = calculate_rev_time_ms(motor_speed);
                        float multiplier = (float)random(100, 201) / 100.0f; // 1.0x to 2.0x
                        script.push_back(format_command("led_cycle_time", (long)(est_rev_time * multiplier)));
                    }
                }
                if (random(100) < 50) { // Gentle sine hue effect
                    uint8_t hue_low = (tail_hue - 20 + 256) % 256;
                    uint8_t hue_high = (tail_hue + 20) % 256;
                    script.push_back(format_command("led_sine_hue", (int)hue_low, (int)hue_high));
                }
                currentPhase = TENSION; // Transition to next phase
                break;
            }

            case TENSION: {
                script.push_back(format_phase_comment("TENSION"));
                script.push_back(format_command("led_display_brightness", (long)random(80, 96))); // Tension brightness
                scene_duration_ms = random(15000, 25001); // Medium length, building scenes

                // Colors: High-contrast (Complementary)
                bg_hue = base_hue;
                tail_hue = (base_hue + 128) % 256;

                if (random(100) < 20) { // 20% chance for a marquee effect
                    script.push_back(format_command("motor_speed", (long)random(600, 801)));
                    char buffer[64];
                    // led_effect:marquee,H,LW,DW,S
                    sprintf(buffer, "led_effect:marquee,%d,%d,%d,%d", tail_hue, (int)random(2,5), (int)random(4,10), (int)random(25, 76));
                    script.push_back(buffer);
                } else {
                    long motor_speed = random(750, 951);
                    script.push_back(format_command("motor_speed", motor_speed)); // Ramp up speed
                    script.push_back(format_command("led_background", (int)bg_hue, (int)random(25, 41)));
                    script.push_back(format_command("led_tails", (int)tail_hue, (int)random(5, 15), (int)random(3, 6)));

                    // Per guidance, cycle time >= motor revolution time
                    if (random(100) < 75) { // 75% chance to set a custom cycle time
                        long est_rev_time = calculate_rev_time_ms(motor_speed);
                        float multiplier = (float)random(100, 151) / 100.0f; // 1.0x to 1.5x
                        script.push_back(format_command("led_cycle_time", (long)(est_rev_time * multiplier)));
                    }
                }

                if (random(100) < 60) { // Pulsing brightness effect, kept within the tension range
                    script.push_back(format_command("led_sine_pulse", (int)random(50, 71), (int)random(90, 96)));
                }

                // Per guidance, use motor_reverse to signal the transition to CLIMAX
                if (random(100) < 75) { // 75% chance to reverse into the climax
                    script.push_back("motor_reverse");
                    accumulated_duration_ms += DEFAULT_RAMP_DURATION_MS + 1000;
                }
                currentPhase = CLIMAX; // Transition to next phase
                break;
            }

            case CLIMAX: {
                script.push_back(format_phase_comment("CLIMAX"));
                script.push_back("led_display_brightness:100"); // Max brightness

                // A longer, multi-part climax with 2-3 scenes.
                long climax_total_duration_ms = random(75000, 90001);
                int num_scenes = random(2, 4); // 2 or 3 scenes
                long duration_per_scene = climax_total_duration_ms / num_scenes;

                for (int i = 0; i < num_scenes; ++i) {
                    long current_scene_duration = duration_per_scene;
                    long remaining_total_time = (total_duration_ms - cool_down_duration_ms) - accumulated_duration_ms;
                    if (current_scene_duration > remaining_total_time && remaining_total_time > 1000) {
                        current_scene_duration = remaining_total_time;
                    }
                    if (current_scene_duration <= 1000) break;

                    int effect_choice = random(100);
                    long hold_time = current_scene_duration;

                    // Per guidance, increase use of marquee effect.
                    if (effect_choice < 40) { // 40% marquee (still or high speed)
                        uint8_t marquee_hue = random(256);
                        char buffer[64];
                        if (random(100) < 40) { // motor still
                            script.push_back("motor_speed:0");
                            script.push_back("hold:2000"); // wait for motor to stop
                            hold_time = max(1000L, hold_time - 2000L);
                            sprintf(buffer, "led_effect:marquee,%d,%d,%d,%d", marquee_hue, (int)random(2,5), (int)random(4,10), (int)random(75, 121));
                            script.push_back(buffer);

                            // If hold is long, switch to a different static effect to add variety
                            if (hold_time > 12000) {
                                long half_hold = hold_time / 2;
                                script.push_back(format_command("hold", half_hold));
                                hold_time -= half_hold;

                                if (random(100) < 50) {
                                    script.push_back("led_effect:fire");
                                } else {
                                    const char* palette = energetic_noise_palettes[random(energetic_noise_palettes.size())];
                                    char buffer2[64];
                                    sprintf(buffer2, "led_effect:noise,%s,25,15", palette);
                                    script.push_back(buffer2);
                                }
                            }
                        } else { // motor high speed
                            script.push_back(format_command("motor_speed", (long)random(900, 1001)));
                            sprintf(buffer, "led_effect:marquee,%d,%d,%d,%d", marquee_hue, (int)random(2,5), (int)random(4,10), (int)random(25, 76));
                            script.push_back(buffer);

                            // Break up the long hold with motor speed changes
                            int num_changes = random(2, 4); // 2 or 3 changes
                            if (num_changes > 1) {
                                long hold_per_change = hold_time / num_changes;
                                if (hold_per_change > 4000) { // Only if chunks are meaningful
                                    for (int j = 0; j < num_changes - 1; j++) {
                                        script.push_back(format_command("hold", hold_per_change));
                                        script.push_back(format_command("motor_speed", (long)random(850, 1001)));
                                        hold_time -= hold_per_change;
                                    }
                                }
                            }
                        }

                    } else if (effect_choice < 75) { // 35% Rainbow + see-saw
                        script.push_back(format_command("motor_speed", (long)random(900, 1001)));
                        script.push_back("led_rainbow");
                        script.push_back(format_command("led_tails", 0, (int)random(10, 20), (int)random(4, 7)));

                        int num_reverses = random(2, 5); // 2 to 4 reverses
                        long hold_per_reverse = hold_time / (num_reverses + 1);

                        if (hold_per_reverse > 500) {
                            for(int j=0; j<num_reverses; j++) {
                                script.push_back(format_command("hold", hold_per_reverse));
                                script.push_back("led_reverse");
                                hold_time -= hold_per_reverse;
                            }
                        }
                    } else { // 25% fire/noise or blink
                        if (random(100) < 50) { // fire/noise
                            script.push_back("motor_speed:0");
                            script.push_back("hold:4000");
                            hold_time = max(1000L, hold_time - 4000L);

                            bool use_fire_first = (random(100) < 50);
                            if (use_fire_first) script.push_back("led_effect:fire");
                            else {
                                const char* palette = energetic_noise_palettes[random(energetic_noise_palettes.size())];
                                char buffer[64];
                                sprintf(buffer, "led_effect:noise,%s,25,15", palette);
                                script.push_back(buffer);
                            }

                            // If the hold is long, switch to the other effect halfway through
                            if (hold_time > 12000) {
                                long half_hold = hold_time / 2;
                                script.push_back(format_command("hold", half_hold));
                                hold_time -= half_hold;

                                if (use_fire_first) { // switch to noise
                                    const char* palette = energetic_noise_palettes[random(energetic_noise_palettes.size())];
                                    char buffer[64];
                                    sprintf(buffer, "led_effect:noise,%s,25,15", palette);
                                    script.push_back(buffer);
                                } else { // switch to fire
                                    script.push_back("led_effect:fire");
                                }
                            }
                        } else { // blink
                            script.push_back(format_command("motor_speed", (long)random(950, 1001)));
                            uint8_t blink_hue = random(256);
                            script.push_back(format_command("led_blink", (int)blink_hue, 100, 80, 150, 0 /* loop */));

                            // Break up the long hold with motor speed changes
                            int num_changes = random(2, 4); // 2 or 3 changes
                            if (num_changes > 1) {
                                long hold_per_change = hold_time / num_changes;
                                if (hold_per_change > 4000) { // Only if chunks are meaningful
                                    for (int j = 0; j < num_changes - 1; j++) {
                                        script.push_back(format_command("hold", hold_per_change));
                                        script.push_back(format_command("motor_speed", (long)random(900, 1001)));
                                        hold_time -= hold_per_change;
                                    }
                                }
                            }
                        }
                    }

                    if (hold_time > 0) {
                        script.push_back(format_command("hold", hold_time));
                    }
                    accumulated_duration_ms += current_scene_duration;
                }

                // Per guidance, use motor_reverse to signal the transition out of CLIMAX
                if (random(100) < 40) { // 40% chance to reverse out of the climax
                    script.push_back("motor_reverse");
                    accumulated_duration_ms += DEFAULT_RAMP_DURATION_MS + 1000;
                }
                currentPhase = VIBE; // Transition back to start
                scene_duration_ms = 0; // We handled holds inside this phase
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
        script.push_back(format_phase_comment("COOL_DOWN"));
        script.push_back(format_command("led_display_brightness", (long)random(20, 41))); // Dim for cooldown
        script.push_back("led_reset");

        int cooldown_effect = random(100);
        if (cooldown_effect < 40) { // 40% chance for a final noise effect
            script.push_back(format_command("motor_speed", (long)random(200, 301)));
            const char* palette = calm_noise_palettes[random(calm_noise_palettes.size())];
            char buffer[64];
            sprintf(buffer, "led_effect:noise,%s,4,50", palette); // very slow and smooth
            script.push_back(buffer);
            script.push_back(format_command("hold", (long)cool_down_duration_ms));
        } else if (cooldown_effect < 70) { // 30% chance for a twinkle effect
            script.push_back(format_command("motor_speed", (long)random(200, 301)));
            char buffer[64];
            sprintf(buffer, "led_effect:twinkle,%d,80", (int)random(256));
            script.push_back(buffer);
            script.push_back(format_command("hold", (long)cool_down_duration_ms));
        } else {
            script.push_back(format_command("motor_speed", (long)random(400, 501)));
            script.push_back(format_command("led_background", (int)random(256), (int)random(5, 15))); // Dim background
            script.push_back(format_command("led_tails", (int)random(256), (int)random(20, 30), 1)); // One long tail
            script.push_back(format_command("hold", (long)cool_down_duration_ms / 2));
            script.push_back(format_command("motor_speed", (long)random(200, 301)));
            script.push_back(format_command("hold", (long)cool_down_duration_ms / 2));
        }
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

std::vector<std::string> generateSteadyRotateScript(int duration_minutes) {
    std::vector<std::string> script;
    if (duration_minutes <= 0) return script;

    // --- Configuration for auto_steady_rotate mode ---
    const float AUTO_STEADY_ROTATE_LED_MOTOR_MAX_RATIO = 4.0;
    const float AUTO_STEADY_ROTATE_LED_MOTOR_MIN_RATIO = 1.0;
    const int AUTO_STEADY_ROTATE_LED_EFFECT_STEPS = 10;
    const float AUTO_STEADY_ROTATE_LED_EFFECT_STEP_DURATION_S = 2.0;
    const long STEADY_MOTOR_SPEED = 500; // Default speed for this mode

    randomSeed(millis());
    long total_duration_ms = duration_minutes * 60L * 1000L;
    long accumulated_duration_ms = 0;

    AUTO_LOG("Generating auto_steady_rotate script for %d minutes...", duration_minutes);

    script.push_back(format_command("motor_speed", STEADY_MOTOR_SPEED));
    script.push_back("hold:3000"); // Give motor time to spin up to steady speed
    accumulated_duration_ms += 3000;

    long step_duration_ms = (long)(AUTO_STEADY_ROTATE_LED_EFFECT_STEP_DURATION_S * 1000.0);
    long one_way_ramp_duration_ms = AUTO_STEADY_ROTATE_LED_EFFECT_STEPS * step_duration_ms;
    long full_cycle_duration_ms = 2 * (AUTO_STEADY_ROTATE_LED_EFFECT_STEPS + 1) * step_duration_ms;

    // Dynamic command limit
    const long HEAP_SAFETY_MARGIN = 50 * 1024;
    const int AVG_COMMAND_MEMORY_COST = 48;
    const int ABSOLUTE_MAX_COMMANDS = 2000;
    long free_heap = ESP.getFreeHeap();
    long available_for_script = free_heap - HEAP_SAFETY_MARGIN;
    int max_commands = 100;
    if (available_for_script > 0) {
        max_commands = min(ABSOLUTE_MAX_COMMANDS, (int)(available_for_script / AVG_COMMAND_MEMORY_COST));
    }
    AUTO_LOG("Heap: %ldB free. Dynamic max commands set to: %d", free_heap, max_commands);

    while (accumulated_duration_ms < total_duration_ms && script.size() < max_commands - 25) {
        script.push_back(format_phase_comment("NEW STEADY CYCLE"));

        int effect_choice = random(100);
        script.push_back("led_reset"); // Clear previous effects

        if (effect_choice < 15) { // 15% chance for a Noise effect
            script.push_back(format_phase_comment("NOISE EFFECT"));
            const char* palette;
            if (random(100) < 50) {
                palette = calm_noise_palettes[random(calm_noise_palettes.size())];
            } else {
                palette = energetic_noise_palettes[random(energetic_noise_palettes.size())];
            }
            int speed = random(5, 21); // 5-20
            int scale = random(20, 71); // 20-70
            char buffer[64];
            sprintf(buffer, "led_effect:noise,%s,%d,%d", palette, speed, scale);
            script.push_back(buffer);

            // Hold for the duration of a normal rotational cycle
            script.push_back(format_command("hold", full_cycle_duration_ms));
            accumulated_duration_ms += full_cycle_duration_ms;

        } else { // 85% chance for a rotational effect (Comet or Marquee)
            bool use_comet = random(100) < 50;
            uint8_t fg_hue = random(256);
            uint8_t bg_hue = (fg_hue + random(80, 177)) % 256; // Contrasting bg

            if (use_comet) {
                script.push_back(format_phase_comment("COMET EFFECT"));
                int length = random(15, 41);
                int num_tails = random(1, 6);
                script.push_back(format_command("led_tails", (int)fg_hue, length, num_tails));

                // Add layering for more color variety
                int color_mod_choice = random(100);
                if (color_mod_choice < 33) {
                    script.push_back("led_rainbow");
                } else if (color_mod_choice < 66) {
                    uint8_t hue_low = random(256);
                    uint8_t hue_high = (hue_low + random(60, 120)) % 256;
                    script.push_back(format_command("led_sine_hue", (int)hue_low, (int)hue_high));
                }
                // else: plain comet color
            } else { // marquee
                script.push_back(format_phase_comment("MARQUEE EFFECT"));
                int light_width = random(2, 6);
                int dark_width = random(4, 11);
                char buffer[64];
                sprintf(buffer, "led_effect:marquee,%d,%d,%d", fg_hue, light_width, dark_width);
                script.push_back(buffer);
            }
            script.push_back(format_command("led_background", (int)bg_hue, (int)random(10, 26)));

            // Randomly set LED direction for this cycle
            if (random(100) < 50) {
                script.push_back("led_reverse");
            }

            long est_rev_time = calculate_rev_time_ms(STEADY_MOTOR_SPEED);

            // Ramp from slow to fast (MAX_RATIO to MIN_RATIO)
            script.push_back(format_phase_comment("Ramp Up LED Speed"));
            for (int i = 0; i <= AUTO_STEADY_ROTATE_LED_EFFECT_STEPS; i++) {
                float ratio = map(i, 0, AUTO_STEADY_ROTATE_LED_EFFECT_STEPS, (long)(AUTO_STEADY_ROTATE_LED_MOTOR_MAX_RATIO * 100), (long)(AUTO_STEADY_ROTATE_LED_MOTOR_MIN_RATIO * 100)) / 100.0f;
                long cycle_time = (long)(est_rev_time * ratio);
                script.push_back(format_command("led_cycle_time", cycle_time));
                script.push_back(format_command("hold", step_duration_ms));
            }
            accumulated_duration_ms += (AUTO_STEADY_ROTATE_LED_EFFECT_STEPS + 1) * step_duration_ms;

            // Ramp from fast to slow (MIN_RATIO to MAX_RATIO)
            script.push_back(format_phase_comment("Ramp Down LED Speed"));
            for (int i = 0; i <= AUTO_STEADY_ROTATE_LED_EFFECT_STEPS; i++) {
                float ratio = map(i, 0, AUTO_STEADY_ROTATE_LED_EFFECT_STEPS, (long)(AUTO_STEADY_ROTATE_LED_MOTOR_MIN_RATIO * 100), (long)(AUTO_STEADY_ROTATE_LED_MOTOR_MAX_RATIO * 100)) / 100.0f;
                long cycle_time = (long)(est_rev_time * ratio);
                script.push_back(format_command("led_cycle_time", cycle_time));
                script.push_back(format_command("hold", step_duration_ms));
            }
            accumulated_duration_ms += (AUTO_STEADY_ROTATE_LED_EFFECT_STEPS + 1) * step_duration_ms;
        }
    }

    script.push_back("system_off");

    AUTO_LOG("Generated %d script commands for auto_steady_rotate.", script.size());
    Serial.println("\n--- BEGIN AUTO-STEADY-ROTATE SCRIPT ---");
    for (const auto& cmd : script) { Serial.println(cmd.c_str()); }
    Serial.println("--- END AUTO-STEADY-ROTATE SCRIPT ---");
    Serial.printf("Total script lines generated: %d\n\n", (int)script.size());

    return script;
}

} // namespace AutoGenerator