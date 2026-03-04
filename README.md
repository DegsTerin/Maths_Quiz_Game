# Maths Quiz Game

An interactive Arduino maths game. The system randomly selects operations, shows the operands on 7-segment displays, displays the operation on an 8x8 matrix, and lets the player answer using three buttons. The difficulty level is controlled by a potentiometer, and the score is shown on a 16x2 LCD.

## Gallery

### Finished build

![Finished maths quiz game enclosure](assets/media/maths-quiz-game-cover.png)

### Prototype during assembly

![Front view of the prototype during assembly](assets/media/assembly-front.jpg)

![Top view of the prototype during assembly](assets/media/assembly-top.jpg)

### Demonstration video

[Open the demonstration video](assets/media/demonstration.mp4)

## Overview

The main sketch is located at:

`arduino/maths-quiz-game/maths-quiz-game.ino`

Features implemented in the code:

- Addition, subtraction, multiplication, and division
- Three difficulty levels: easy, medium, and hard
- Animated operation selection on the 8x8 LED matrix
- Three answer choices shown on TM1637 displays
- Visual feedback with a right LED and a wrong LED
- Right and wrong score tracking on the I2C LCD
- Dedicated reset button to clear the game

## Components used

- 1 Arduino board
- 1 I2C 16x2 LCD at address `0x27`
- 1 8x8 LED matrix with a `LedControl`-compatible driver
- 5 four-digit TM1637 displays
- 3 answer buttons
- 1 reset button
- 1 potentiometer for difficulty selection
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

Note: the code treats `HIGH` as a pressed button, and the sketch comments indicate that the current hardware uses external resistors.

## How the game works

1. The potentiometer defines the current difficulty level.
2. The system selects an operation with an animation on the 8x8 matrix.
3. Two displays show the operands.
4. Three displays show the answer choices.
5. The player presses one of the three answer buttons.
6. The system updates the correct and incorrect counters and shows visual feedback.
7. The reset button clears the score and restarts the cycle.

## Difficulty rules

The code adjusts number ranges according to the selected level:

- Easy: smaller numbers and a higher chance of simpler sums
- Medium: wider ranges and a more balanced mix of operations
- Hard: larger values and a heavier weight for multiplication and division

For division:

- At easy level, division is more likely to be exact
- At medium and hard levels, decimal results may appear

## Libraries

Libraries referenced by the sketch:

- `TM1637Display`
- `LiquidCrystal_I2C`
- `LedControl`

Libraries included in the repository:

- `arduino/libraries/TM1637_Driver`
- `arduino/libraries/LiquidCrystal_I2C`

Note: the sketch includes `LedControl.h`, but that library does not appear to be versioned in this repository. If it is not installed in your Arduino IDE, you will need to add it manually.

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

## How to run

1. Open the Arduino IDE.
2. Ensure the required libraries are installed.
3. Open the `arduino/maths-quiz-game/maths-quiz-game.ino` file.
4. Connect the Arduino board.
5. Compile and upload the sketch.

## How to simulate in Wokwi

You can simulate the project in the Wokwi web editor at:

`https://wokwi.com/projects/new/arduino-uno`

Recommended setup steps:

1. Open a new Arduino Uno project in Wokwi.
2. Replace the default `sketch.ino` content with the contents of `simulation/sketch.ino`, or copy the code from `arduino/maths-quiz-game/maths-quiz-game.ino`.
3. Replace the default `diagram.json` content with the contents of `simulation/diagram.json`.
4. Replace or create `libraries.txt` with the contents of `simulation/libraries.txt`.
5. If needed, use Wokwi's Library Manager to confirm these libraries are installed: `TM1637Display`, `LiquidCrystal_I2C`, and `LedControl`.
6. Start the simulation.

Notes:

- The LCD address used by this project is `0x27`.
- The local Wokwi files for this repository are stored in `simulation/`.
- `simulation/wokwi-project.txt` contains the Wokwi project link or reference information.
- If a required library is missing, the simulation will not compile until it is added to the Wokwi project.

![Wokwi simulation layout](simulation/wokwi.png)

## Supporting materials

The repository also includes:

- Electronics files in `hardware/electronics`
- Vector design files for the enclosure/project in `design/vector`
- An additional document in `docs/project-tutorial.docx`
- Prototype photos in `assets/media`
- Demonstration video in `assets/media/demonstration.mp4`
- Wokwi simulation files in `simulation`

## Notes

- The project appears to have been organised as a complete build package, with code, artwork, and circuit files in the same repository.
- The `maths-quiz-game.txt` file in the electronics folder appears to use inconsistent encoding and was not used as a primary source for this documentation.







