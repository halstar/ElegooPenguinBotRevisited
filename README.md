# Elegoo Penguin Bot Revisited

Refactored/cleaned up PenguinBot.ino file (kept other files almost unchanged), in order to:

* Decrease ROM occupation from 22654 bytes (70%) to 19484 bytes (60%),
* Decrease globals  usage from  1614 bytes (78%) to  1330 bytes (64%),
* Decrease lines of code  from  1160 to 1136,
* Keep most of features unchanged,

* Remove useless parts (e.g. uncalled functions, unused globals),
* Try and make code clearer (use english everywhere),
* Try and make code cleaner (use homogeneous naming rules),
* Try and make code easier to sustain/modify, by factorizing things,
  (e.g. by removing disparate calls to servoAttach()/servoDetach(),
  which might break the Bot's runtime, when moving some code around),

* Add voice MP3s announcing mode changes, commands, etc. to help understand current state.

## Notes

* Started from so-called version 1.0.2018.11.07, from Eleogoo website,
* Don't forget to copy MP3s from MusicFiles directory to the Bot's micro SD.
