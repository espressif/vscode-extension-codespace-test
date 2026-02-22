# Gemini Project Notes

## Goal
Modify the Matter light project to control an SG90 servo on GPIO 17.
- "On" -> 0 degrees
- "Off" -> 180 degrees

## Project Structure
- `main/app_main.cpp`: Main entry point, Matter initialization.
- `main/app_driver.cpp`: Hardware control logic.
- `main/app_priv.h`: Private header for app-specific declarations.
- `main/CMakeLists.txt`: Build script for the main component.
- `main/idf_component.yml`: Component dependencies.

## Plan
1.  **Add Servo Control Logic:**
    - Use the `ledc` driver for PWM control.
    - Configure GPIO 17 as the output.
    - Create functions to set the servo angle (0 and 180 degrees).
2.  **Integrate with Matter:**
    - Find the on/off attribute update callback in `app_driver.cpp`.
    - Call the servo control functions from the callback.
3.  **Initialization:**
    - Add servo initialization code to `app_driver_init()`.
