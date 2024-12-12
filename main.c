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
int readTextData(const char *fileName, char textData[][256], int *numLines);
int calculateScaleFactor(float *scaleFactor, float *userHeight);
int mapTextToFontData(char textData[], fontValue fontData[], fontValue textFontData[], float userHeight, float *currentYOffset);
void sendGCode(fontValue textFontData[], float scaleFactor);
void printTextFontData(fontValue textFontData[], int maxSize);

int main()
{
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    fontValue fontData[NumberOfLines];
    char textData[1024][256];  // 2D array to hold multiple lines of text
    int numLines;  // Total number of lines read
    float scaleFactor;
    float userHeight;
    fontValue textFontData[1024];
    char fileName[256];

    printf("Enter the name of the text data file: ");
    scanf("%255s", fileName);

    // Load font data 
    if (readFontData(fontData)) 
    {
        printf("Error: Font data could not be loaded.\n");
        return 1;
    }

    // Load text data 
    if (readTextData(fileName, textData, &numLines)) 
    {
        printf("Error: Text data could not be loaded.\n");
        return 1;
    }

    // Calculate the scale factor using user input
    if (calculateScaleFactor(&scaleFactor, &userHeight)) 
    {
        printf("Error: Scale factor calculation failed.\n");
        return 1;
    }

    printf("Scale factor calculated: %.2f\n", scaleFactor);

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

    // Send G-codes to the robot
    float currentYOffset = 0.0;
    for (int i = 0; i < numLines; i++) 
    {
        printf("Processing line %d: %s\n", i + 1, textData[i]);

        // Map the text data to font data for the current line
        if (mapTextToFontData(textData[i], fontData, textFontData, userHeight, &currentYOffset)) {
            printf("Error: Failed to map text data to font data for line %d.\n", i + 1);
            return 1;
        }

        // Generate and send G-codes directly for the current line
        sendGCode(textFontData, scaleFactor);

        currentYOffset -= userHeight + 20; 

    }

    sprintf(buffer, "G0 X0.0 Y0.0\n");
    SendCommands(buffer);
    Sleep(7000);

/*
    for (int i = 0; gCodeCommand[i] != NULL; i++) 
    {
        printf("Sending G-code: %s", gCodeCommand[i]);

        // Transfer info from gCodeCommand[] to sprintf() with buffer
        sprintf(buffer, "%s", gCodeCommand[i]);

        // Send the formatted G-code command to the robot
        SendCommands(buffer);

        // Free the allocated memory after sending the command
        free(gCodeCommand[i]);
    }
*/

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

int readTextData(const char *fileName, char textData[][256], int *numLines)
{
    FILE *textDataFile = fopen(fileName, "r");

    if (textDataFile == NULL) 
    {
        printf("Error: Unable to open text data file.\n");
        return 1; // Failure
    }

    *numLines = 0;  // Initialize the line counter

    // Read each line into the textData array
    while (fgets(textData[*numLines], 256, textDataFile) && *numLines < 1024) 
    {
        textData[*numLines][strcspn(textData[*numLines], "\n")] = '\0'; // Remove trailing newline
        printf("Loaded line %d: %s\n", *numLines, textData[*numLines]);
        (*numLines)++;  // Increment line counter
    }

    fclose(textDataFile);

    printf("Text data loaded successfully. Total lines: %d\n", *numLines);

    return 0;
}

int calculateScaleFactor(float *scaleFactor, float *userHeight)
{
    // Get user input for text height
    printf("Enter the text height (4-10 mm): ");
    scanf("%f", userHeight);

    if (*userHeight < 4 || *userHeight > 10) {
        printf("Error: Invalid height. Must be between 4 and 10 mm.\n");
        return 1;
    }

    *scaleFactor = *userHeight/18.0;

    return 0;
}

int mapTextToFontData(char textData[], fontValue fontData[], fontValue textFontData[], float userHeight, float *currentYOffset) 
{
    int textIndex = 0;  // Track the current index in textFontData
    int ascii;
    float xOffset = 0.0;  // Track X offset for characters
    float yOffset = *currentYOffset;  // Track Y offset (for moving to the next line)
    float charSpace = userHeight + 10;  // Space between characters

    // Initialize textFontData to a terminating value
    for (int i = 0; i < 1024; i++) 
    {
        textFontData[i].value1 = -1;  // Use a clear terminating condition
    }

    int i = 0;
    while (textData[i] != '\0') 
    {
        // Collect the current word and calculate its width
        int wordStart = i;
        float wordWidth = 0.0;
        while (textData[i] != '\0' && textData[i] != ' ' && textData[i] != '\n') 
        {
            ascii = (int)textData[i];
            // Find the character width
            for (int j = 0; j < NumberOfLines; j++) 
            {
                if (fontData[j].value1 == 999 && fontData[j].value2 == ascii) 
                {
                    wordWidth += fontData[j + fontData[j].value3].value1 * ((userHeight + 10)/ 18.0) ;  // Character width scaled
                    printf("Word completed: Final xOffset = %.2f, wordWidth = %.2f\n", xOffset, wordWidth);

                    break;
                }
            }
            i++;
        }

        // If the next word will exceed the limit, adjust yOffset
        if ((xOffset + wordWidth) > 200.0) 
        {
            xOffset = 0.0;  // Reset xOffset to the start of the new line
            yOffset -= userHeight + 20;  // Move to the next line
        }

        // Write the word
        for (int j = wordStart; j < i; j++) 
        {
            ascii = (int)textData[j];
            int totalStrokes = 0;

            // Search for the corresponding font data for this character
            for (int k = 0; k < NumberOfLines; k++) 
            {
                if (fontData[k].value1 == 999 && fontData[k].value2 == ascii) 
                {
                    totalStrokes = fontData[k].value3;
                    for (int m = 0; m < totalStrokes; m++) 
                    {
                        textFontData[textIndex] = fontData[k + m + 1];
                        // Apply offsets to the strokes
                        textFontData[textIndex].value1 += xOffset;
                        textFontData[textIndex].value2 += yOffset;

                        textIndex++;
                    }
                    xOffset += charSpace;  // Update xOffset with character width
                    break;
                }
            }
        }

        // If there was a space, move the xOffset for the space
        if (textData[i] == ' ') 
        {
            xOffset += charSpace;  // Add space width
            i++;  // Skip the space
        }

        // Handle newline
        if (textData[i] == '\n') 
        {
            xOffset = 0.0;  // Reset xOffset for a new line
            yOffset -= userHeight + 20;  // Move to the next line
            i++;  // Skip the newline character
        }
    }

    *currentYOffset = yOffset;  // Update currentYOffset for the next call
    return 0;
}


void sendGCode(fontValue textFontData[], float scaleFactor) 
{
    char gCodeBuffer[100];
    int lastPenStatus = -1;

    for (int i = 0; textFontData[i].value1 != -1; i++) 
    {
        int xValue = textFontData[i].value1;
        int yValue = textFontData[i].value2;
        int penStatus = textFontData[i].value3;

        float xScaled = xValue * scaleFactor;
        float yScaled = yValue * scaleFactor;

        // Check if the pen state has changed
        if (penStatus != lastPenStatus) 
        {
            if (penStatus == 1) 
            {
                sprintf(gCodeBuffer, "S1000"); // Pen down
            } 
            else 
            {
                sprintf(gCodeBuffer, "S0");    // Pen up
            }
            SendCommands(gCodeBuffer);
            lastPenStatus = penStatus;
        }

        // Generate G0 or G1 command depending on the pen status
        if (penStatus == 0) 
        {
            sprintf(gCodeBuffer, "G0 X%.3f Y%.3f", xScaled, yScaled);
        } 
        else 
        {
            sprintf(gCodeBuffer, "G1 X%.3f Y%.3f", xScaled, yScaled);
        }
        SendCommands(gCodeBuffer);
    }
    
}

void printTextFontData(fontValue textFontData[], int maxSize) 
{
    for (int i = 0; i < maxSize; i++) 
    {
        if (textFontData[i].value1 == -1) // Stop if you encounter a special end value (or any other criteria for your array end)
        {  
            break;
        }
        printf("Movement %d: X = %d, Y = %d, Pen Status = %d\n", i, textFontData[i].value1, textFontData[i].value2, textFontData[i].value3);
    }
}