void lcdSetup();
void lcdClear();
void lcdSetLine(byte rowIndex, char *line);
void lcdShowBarGraph(int columnIndex, int rowIndex, int charWidth, char activeChar, char inactiveChar, float value);
void lcdSetChar(int columnIndex, int rowIndex, char c);
void lcdSetChars(int columnIndex, int rowIndex, char *c);
void lcdUpdate(unsigned long now);
