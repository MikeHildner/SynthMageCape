// Runs a handful of the Kotlin parser's test packets through the C++ port at
// start-up, so a porting mistake shows up on the serial monitor straight away.
#pragma once

// Prints one line per failure and a summary. Returns true if everything passed.
bool runParserSelfTest();
