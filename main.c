#include <stdio.h>
#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200               /* 115200 baud */
#define NumberOfLines 1027

typedef struct {
    int value1;
    int value2;
    int value3;
} fontValue;

void SendCommands(char *buffer );
int readFontData(fontValue fontData[]);
int readTextData(char textData[]);
int calculateScaleFactor(float *scaleFactor);
int mapTextToFontData(char textData[], fontValue fontData[], fontValue* textFontData[]);

int main()
{
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    fontValue fontData[NumberOfLines];
    char textData[256];
    float scaleFactor;
    fontValue* textFontData[1024];

    // Load font data 
    if (readFontData(fontData)) 
    {
        printf("Error: Font data could not be loaded.\n");
        return 1;
    }

    // Load text data 
    if (readTextData(textData)) 
    {
        printf("Error: Text data could not be loaded.\n");
        return 1;
    }

    // Calculate the scale factor using user input
    if (calculateScaleFactor(&scaleFactor)) 
    {
        printf("Error: Scale factor calculation failed.\n");
        return 1;
    }

    printf("Scale factor calculated: %.2f\n", scaleFactor);

    // Map the text data to font data
    if (mapTextToFontData(textData, fontData, textFontData)) 
    {
        printf("Error: Failed to map text data to font data.\n");
        return 1;
    }

    printf("Text data mapped to font data successfully.\n");

    return 0;

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);


    // These are sample commands to draw out some information - these are the ones you will be generating.
    sprintf (buffer, "G0 X-13.41849 Y0.000\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y-4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y0.0000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41089 Y4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X-7.17524 Y0\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X0 Y0\n");
    SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
}

int readFontData(fontValue fontData[])
{
    FILE *fontDataFile = fopen("SingleStrokeFont.txt","r");

    if (fontDataFile == NULL) {
        printf("Error: Unable to open font data file.\n");
        return 1; // Failure
    }

    int i = 0;
    while (i < NumberOfLines) 
    {
        if (fscanf(fontDataFile, "%d %d %d", &fontData[i].value1, &fontData[i].value2, &fontData[i].value3) != 3) 
        {
            printf("Error: Failure to read font data from font file at line %d.\n", i + 1);
            fclose(fontDataFile);
            return 1; // Failure
        }
        i++;
    }

    fclose(fontDataFile);

    printf("Font data loaded successfully. Total lines read: %d\n", i);

    return 0;

}

int readTextData(char textData[])
{
    FILE *textDataFile = fopen("TextData.txt","r");

    if (textDataFile == NULL) 
    {
        printf("Error: Unable to open text data file.\n");
        return 1; // Failure
    }   

    if (fgets(textData, 256, textDataFile)) 
    {
        textData[strcspn(textData, "\n")] = '\0'; // Remove trailing newline character
        fclose(textDataFile);
        printf("Text data loaded successfully: %s\n", textData);
        return 0;
    }

    fclose(textDataFile);

    return 1;

}  

int calculateScaleFactor(float *scaleFactor)
{
    int userHeight;

    // Get user input for text height
    printf("Enter the text height (4-10 mm): ");
    scanf("%d", &userHeight);

    if (userHeight < 4 || userHeight > 10) {
        printf("Error: Invalid height. Must be between 4 and 10 mm.\n");
        return 1;
    }

    *scaleFactor = (float)userHeight/18;

    return 0;
}

int mapTextToFontData(char textData[], fontValue fontData[], fontValue *textFontData[]) {
    int i, j;
    int ascii;  // ASCII value of the character
    int lines = 0; // To track the number of lines for a character's font data

    // Loop through each character in textData
    for (i = 0; textData[i] != '\0'; i++) {
        ascii = (int)textData[i];  // Get ASCII value of character

        // Search for the corresponding font data for this character
        for (j = 0; j < NumberOfLines; j++) {
            if (fontData[j].value1 == 999 && fontData[j].value2 == ascii) {
                // The character's data starts at fontData[j+1]
                lines = fontData[j].value3;  // Get the number of lines for this character

                // Store the font data for this character in textFontData
                for (int k = 0; k < lines; k++) {
                    textFontData[i + k] = &fontData[j + k + 1];  // Store font data into the array
                }

                break;
            }
        }

        // If no matching character is found in fontData, return an error
        if (j == NumberOfLines) {
            printf("Error: Font data for character '%c' not found.\n", textData[i]);
            return 1; // Failure
        }
    }

    return 0; // Success
}