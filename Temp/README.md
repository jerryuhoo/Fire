# Testing

This is for testing, make sure each commit has the same result of outputs.
`tests/GoldenMasters` stores the "correct" version of output, while `tests/RegressionOutput` stores the current version's output.

## How to use?

Drag GenerateGoldenMasters.cpp to `tests/` for generating `tests/GoldenMasters` wav files.
`cmake -S . -B Builds -G "Ninja" -DCMAKE_BUILD_TYPE=Debug`
`cmake --build Builds --config Debug`
`cd Builds`
`./Tests`