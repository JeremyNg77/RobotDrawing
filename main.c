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
int readTextData(char textData[][256], int *numLines);
int calculateScaleFactor(float *scaleFactor, float *userHeight);
int mapTextToFontData(char textData[], fontValue fontData[], fontValue textFontData[], float userHeight, float *currentYOffset);
int generateGCode(fontValue textFontData[], float scaleFactor, char *gCodeCommand[]);
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

    // Load font data 
    if (readFontData(fontData)) 
    {
        printf("Error: Font data could not be loaded.\n");
        return 1;
    }

    // Load text data 
    if (readTextData(textData, &numLines)) 
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

    char *gCodeCommand[4096];  // Array to hold G-code strings
    for (int i = 0; i < 4096; i++) {
        gCodeCommand[i] = malloc(100);  // Allocate memory for each command
        if (!gCodeCommand[i]) {
            printf("Memory allocation failed for gCodeCommand[%d]\n", i);
            return 1;
        }
        gCodeCommand[i][0] = '\0';  // Initialize with an empty string
    }

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
    //char buffer[100];  // Buffer to hold the formatted G-code command
    float currentYOffset = 0.0;

    for (int i = 0; i < numLines; i++) 
    {
        printf("Processing line %d: %s\n", i + 1, textData[i]);

        // Map the text data to font data for the current line
        if (mapTextToFontData(textData[i], fontData, textFontData, userHeight, &currentYOffset)) 
        {
            printf("Error: Failed to map text data to font data for line %d.\n", i + 1);
            return 1;
        }

        currentYOffset=currentYOffset-23;

        // Generate G-codes for the current line
        if (generateGCode(textFontData, scaleFactor, gCodeCommand)) 
        {
            printf("Error: Failed to generate G-code for line %d.\n", i + 1);
            return 1;
        }

        // Send G-codes for the current line
        for (int j = 0; gCodeCommand[j] != NULL; j++) 
        {
            //printf("%s", gCodeCommand[j]);
            sprintf(buffer, "%s", gCodeCommand[j]);
            SendCommands(buffer);
            gCodeCommand[j][0] = '\0';  // Reset string 
           
        }
        
    }

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

int readTextData(char textData[][256], int *numLines) 
{
    FILE *textDataFile = fopen("RobotTesting.txt", "r");

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
    float charSpace = userHeight;  // Space between characters

    // Initialize textFontData to a terminating value
    for (int i = 0; i < 1024; i++) 
    {
        textFontData[i].value1 = -1;  // Use a clear terminating condition
    }

    // Loop through each character in textData
    for (int i = 0; textData[i] != '\0'; i++) 
    {
        ascii = (int)textData[i];
        int totalStrokes = 0;  // Number of strokes for this character
        int strokesProcessed = 0;  // Count of strokes processed for the current character
        
        
        if (textData[i] == '\n') 
        {
            printf("Newline encountered: Moving to next line.\n");
            yOffset += 2 * userHeight + 5;  // Move down by userHeight + 5 for a new line
            xOffset = 0;  // Reset xOffset to start from the beginning of the new line
            continue;  // Skip to the next character, no further processing for '\n'
        }
        

        // Check if xOffset exceeds the threshold and move to the next line only after a word is completed
        if (xOffset > 100 && textData[i] == ' ') 
        {
            yOffset -= 2 * userHeight + 5;  // Move down by userHeight + 5
            xOffset = 0;  // Reset xOffset to start from the beginning of the new line
            
        }

        // Search for the corresponding font data for this character
        for (int j = 0; j < NumberOfLines; j++) 
        {
            if (fontData[j].value1 == 999 && fontData[j].value2 == ascii) 
            {
                totalStrokes = fontData[j].value3;  // Number of strokes for this character
                //printf("Character '%c' (ASCII: %d) mapped to %d strokes with offset X=%.3f, Y=%.3f\n", textData[i], ascii, totalStrokes, xOffset, yOffset);

                // Apply xOffset and yOffset to all strokes for the current character
                for (int k = 0; k < totalStrokes; k++) 
                {
                    textFontData[textIndex] = fontData[j + k + 1];  // Copy the stroke data

                    // Apply offsets to the x and y values of each stroke
                    textFontData[textIndex].value1 += xOffset;
                    textFontData[textIndex].value2 += yOffset;

                    textIndex++;  // Increment the textFontData index
                    strokesProcessed++;  // Increment stroke count
                }

                // Once all strokes for the current character are processed, 
                // update xOffset for the next character
                if (strokesProcessed == totalStrokes) 
                {
                    xOffset += 2*charSpace;  // Move to the next character space
                    strokesProcessed = 0;  // Reset strokesProcessed for the next character
                }

                break;  // Stop searching once we find the font data for the character
            }
        }
    }
    
    *currentYOffset = yOffset;

    return 0;
}

int generateGCode(fontValue textFontData[], float scaleFactor, char *gCodeCommand[]) 
{
    int gCodeIndex = 0;
    
    int lastPenStatus = -1;  // To track the last pen status (initialized to an invalid value)

    // Loop through all font data for the text
    for (int i = 0; textFontData[i].value1 != -1; i++) 
    {
        if (gCodeIndex >= 1024) 
        {
            printf("Error: G-code command array is full.\n");
            return 1;  // Exit if the array is full
        }

        int xValue = textFontData[i].value1;
        int yValue = textFontData[i].value2;
        int penStatus = textFontData[i].value3;
        float xScaled = xValue * scaleFactor;
        float yScaled = yValue * scaleFactor;

        //printf("About to generate G-code[%d]: penStatus=%d, xScaled=%.3f, yScaled=%.3f\n", gCodeIndex, penStatus, xScaled, yScaled);

        // If the pen status is different from the last one, output the corresponding command
        if (penStatus != lastPenStatus) 
        {
            if (penStatus == 0) // Pen up
            {  
                sprintf(gCodeCommand[gCodeIndex++], "S0\n");
            } 
            else if (penStatus == 1) // Pen down
            {  
                sprintf(gCodeCommand[gCodeIndex++], "S1000\n");
            }
            lastPenStatus = penStatus;  // Update the last pen status
        }

        // Generate  G0 or G1 command depending on the pen status
        if (penStatus == 0) 
        {  
            sprintf(gCodeCommand[gCodeIndex++], "G0 X%.3f Y%.3f\n", xScaled, yScaled);
            //printf("%s", gCodeCommand[gCodeIndex]);
            //printf("G0 has succeeded\n");
        } 
        else if (penStatus == 1) 
        {  
            sprintf(gCodeCommand[gCodeIndex++], "G1 X%.3f Y%.3f\n", xScaled, yScaled);
            //printf("%s", gCodeCommand[gCodeIndex]);
            //printf("G1 has succeeded\n");
        }

    }

    // Ensure the last G-code command is a return to the origin
    sprintf(gCodeCommand[gCodeIndex++], "G0 X0.0 Y0.0\n");

    // Null-terminate the G-code array
    gCodeCommand[gCodeIndex] = NULL;  // Proper null-termination

    return 0;
}

void printTextFontData(fontValue textFontData[], int maxSize) {
    for (int i = 0; i < maxSize; i++) {
        if (textFontData[i].value1 == -1) {  // Stop if you encounter a special end value (or any other criteria for your array end)
            break;
        }
        printf("Movement %d: X = %d, Y = %d, Pen Status = %d\n", i, textFontData[i].value1, textFontData[i].value2, textFontData[i].value3);
    }
}