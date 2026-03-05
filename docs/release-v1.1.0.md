# v1.1.0

## Highlights

- Switched button input strategy to `INPUT_PULLUP` and removed the need for external 10k button resistors.
- Updated wiring logic so pressed buttons read as `LOW` and each button is connected between pin and `GND`.
- Improved quiz logic for decimals, wrong-answer generation, and level transitions.
- Added clearer round debug output in Serial Monitor (equation, options, and correct option).
- Split Arduino sketches by language into dedicated sketch folders (`en-gb` and `pt-br`).

## Included in this release

- English sketch: `arduino/maths-quiz-game-en-gb/maths-quiz-game-en-gb.ino`
- Portuguese sketch: `arduino/maths-quiz-game-pt-br/maths-quiz-game-pt-br.ino`
- Updated simulation sketch: `simulation/sketch.ino`
- Updated project documentation in `README.md`

## Important hardware note

This release adopts internal pull-up inputs for answer/reset buttons:

- `pinMode(..., INPUT_PULLUP)`
- pressed = `LOW`
- button wiring: `pin <-> button <-> GND`
- external pull-down resistors are no longer required for these buttons

