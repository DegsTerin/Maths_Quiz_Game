# Maths Quiz Game

An interactive Arduino-based maths quiz system with adaptive difficulty, multiple display modules, physical answer buttons, score tracking, and a complete Wokwi simulation setup.

## Project summary

This project presents arithmetic challenges using dedicated hardware modules:

- two TM1637 displays for the operands
- three TM1637 displays for the answer options
- one 8x8 LED matrix for the selected operation
- one 16x2 I2C LCD for level and score feedback
- three answer buttons, one reset button, and one potentiometer for difficulty selection

The main sketch is located at `arduino/maths-quiz-game/maths-quiz-game.ino`.

## Gallery

### Finished build

![Finished maths quiz game enclosure](assets/media/maths-quiz-game-cover.png)

### Prototype during assembly

![Front view of the prototype during assembly](assets/media/assembly-front.jpg)

![Top view of the prototype during assembly](assets/media/assembly-top.jpg)

### Demonstration video

[Open the demonstration video](assets/media/demonstration.mp4)

## Features

- Addition, subtraction, multiplication, and division
- Three difficulty levels: easy, medium, and hard
- Animated operation selection on the 8x8 LED matrix
- Three multiple-choice answers shown on TM1637 displays
- Right and wrong feedback using dedicated LEDs
- Right and wrong score tracking on the I2C LCD
- Potentiometer-based difficulty selection
- Browser-based simulation support with Wokwi

## Hardware used

- 1 Arduino board
- 1 I2C 16x2 LCD at address `0x27`
- 1 8x8 LED matrix using a `LedControl`-compatible driver
- 5 four-digit TM1637 displays
- 3 answer buttons
- 1 reset button
- 1 potentiometer
- 2 feedback LEDs
- Resistors and wiring according to the assembled circuit

## Pin mapping

Configuration defined in the sketch:

- 8x8 matrix
  - `DIN`: pin `7`
  - `CLK`: pin `8`
  - `CS`: pin `9`
- TM1637 displays
  - Shared DIO: pin `10`
  - CLK display 1: pin `2`
  - CLK display 2: pin `3`
  - CLK display 3: pin `4`
  - CLK display 4: pin `5`
  - CLK display 5: pin `6`
- Answer buttons
  - Button 1: `A1`
  - Button 2: `A2`
  - Button 3: `A3`
- Reset button: pin `11`
- Right LED: pin `12`
- Wrong LED: pin `13`
- Potentiometer: `A0`

Note: the sketch treats `HIGH` as a pressed button, matching the current hardware configuration with external resistors.

## How the game works

1. The potentiometer sets the current difficulty level.
2. The system selects an operation and animates it on the 8x8 matrix.
3. Two displays show the operands.
4. Three displays show the answer options.
5. The player presses one of the three answer buttons.
6. The system updates the score and shows right or wrong feedback.
7. The reset button clears the score and restarts the cycle.

## Difficulty behaviour

The generated values change according to the selected level:

- Easy: smaller values and a higher chance of simpler calculations
- Medium: wider ranges and a more balanced mix of operations
- Hard: larger values and a stronger weighting towards multiplication and division

Division rules:

- At easy level, division is more likely to be exact
- At medium and hard levels, decimal answers may appear

## Libraries

Libraries used by the sketch:

- `TM1637Display`
- `LiquidCrystal_I2C`
- `LedControl`

Libraries included in the repository:

- `arduino/libraries/TM1637_Driver`
- `arduino/libraries/LiquidCrystal_I2C`

Note: the sketch includes `LedControl.h`, but that library is not versioned in this repository. If it is missing in your Arduino environment, install it manually.

## Repository structure

```text
arduino/
|-- libraries/
|   |-- LiquidCrystal_I2C/
|   `-- TM1637_Driver/
`-- maths-quiz-game/
    `-- maths-quiz-game.ino

assets/
`-- media/
    |-- maths-quiz-game-cover.png
    |-- assembly-front.jpg
    |-- assembly-top.jpg
    `-- demonstration.mp4

design/
`-- vector/
    |-- maths-box.ai
    `-- maths-box-v8.ai

docs/
`-- project-tutorial.docx

hardware/
`-- electronics/
    |-- maths-quiz-game.pdf
    |-- maths-quiz-game.psd
    `-- supporting files

simulation/
|-- diagram.json
|-- libraries.txt
|-- sketch.ino
|-- wokwi.png
`-- wokwi-project.txt
```

## Run on hardware

1. Open the Arduino IDE.
2. Install the required libraries if needed.
3. Open `arduino/maths-quiz-game/maths-quiz-game.ino`.
4. Connect the Arduino board.
5. Compile and upload the sketch.

## Simulate in Wokwi

You can start from the Arduino Uno template:

`https://wokwi.com/projects/new/arduino-uno`

Recommended setup:

1. Create a new Arduino Uno project in Wokwi.
2. Replace the default `sketch.ino` with `simulation/sketch.ino`, or copy the code from `arduino/maths-quiz-game/maths-quiz-game.ino`.
3. Replace the default `diagram.json` with `simulation/diagram.json`.
4. Replace or create `libraries.txt` using `simulation/libraries.txt`.
5. Confirm that `TM1637Display`, `LiquidCrystal_I2C`, and `LedControl` are available in the project.
6. Start the simulation.

Notes:

- The LCD address used by this project is `0x27`.
- All local Wokwi files are stored in `simulation/`.
- `simulation/wokwi-project.txt` stores the Wokwi project link or reference details.
- If a required library is missing, the simulation will not compile.

![Wokwi simulation layout](simulation/wokwi.png)

## Supporting materials

The repository also includes:

- electronics files in `hardware/electronics`
- vector design files in `design/vector`
- a project tutorial document in `docs/project-tutorial.docx`
- prototype photos and media in `assets/media`
- complete Wokwi simulation files in `simulation`

## Notes

- The repository combines source code, design assets, electronics documentation, and simulation files in one place.
- The file `hardware/electronics/maths-quiz-game.txt` appears to use inconsistent encoding and was not treated as a primary documentation source.
