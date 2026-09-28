#pragma once

#include <Arduino.h>

// Implemented by the participants in student_code.cpp.
// Called by the core for every departure that is not cancelled.
// `minutes` is never negative.
String formatMinutes(int minutes);
