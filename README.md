# Elegoo Penguin Bot Revisited

Refactored/cleaned up PenguinBot.ino file (kept other files almost unchanged), in order to:

* Decrease ROM occupation from 22654 bytes (70%) to 20694 bytes (64%),
* Decrease globals  usage from  1614 bytes (78%) to  1580 bytes (77%),
* Keep most of features unchanged.

* Remove useless parts (e.g. uncalled functions, unused globals),
* Try and make code clearer (use english everywhere),
* Try and make code cleaner (use homogeneous naming rules),
* Try and make code easier to sustain/modify, by factorizing things,
  (e.g. by removing disparate calls to servoAttach()/servoDetach(),
  which might break the Bot's runtime, when moving some code around).

* Add voice MP3s announcing mode changes, commands, etc. to help understand current state.

## Note

Started from so-called version 1.0.2018.11.07, from Eleogoo website.
