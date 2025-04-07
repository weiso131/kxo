#define GET_HISTORY 1
#define GET_MOVE(history, i) ((history >> i) & 0x0f)
static char table_form[][3] = {"A1", "A2", "A3", "A4", "B1", "B2", "B3", "B4",
                               "C1", "C2", "C3", "C4", "D1", "D2", "D3", "D4"};
