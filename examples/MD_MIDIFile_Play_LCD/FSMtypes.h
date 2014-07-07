// Enumerated types for the FSM(s)
// Need to be defined in Header file for Arduino file mashing process to work
//
enum lcd_state	{ LSBegin, LSSelect, LSShowFile, LSGotFile };
enum midi_state { MSBegin, MSLoad, MSOpen, MSProcess, MSClose };
enum seq_state	{ LCDSeq, MIDISeq };
