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
int calculateScaleFactor(float *scaleFactor, float *userHeight);
int mapTextToFontData(char textData[], fontValue fontData[], fontValue textFontData[], float userHeight);
int generateGCode(fontValue textFontData[], float scaleFactor, char *gCodeCommand[]);
void printTextFontData(fontValue textFontData[], int maxSize);

int main()
{
    //char mode[]= {'8','N','1',0};
    char buffer[100];

    fontValue fontData[NumberOfLines];
    char textData[256];
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
    if (readTextData(textData)) 
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

    // Map the text data to font data
    if (mapTextToFontData(textData, fontData, textFontData, userHeight)) 
    {
        printf("Error: Failed to map text data to font data.\n");
        return 1;
    }

    printf("Text data mapped to font data successfully.\n");
    
    
    printTextFontData(textFontData, 100);

    char *gCodeCommand[1024];  // Array to hold G-code strings
    for (int i = 0; i < 1024; i++) 
    {
        gCodeCommand[i] = malloc(100);
        if (!gCodeCommand[i]) {
            printf("Memory allocation failed for gCodeCommand[%d]\n", i);
            return 1;
        }
    }

    // Generate G-codes
    if (generateGCode(textFontData, scaleFactor, gCodeCommand)) 
    {
        printf("Error: Failed to generate G-code.\n");
        return 1;
    }

    printf("G-code generation completed.\n");

    // Send G-codes to the robot
    for (int i = 0; gCodeCommand[i] != NULL; i++) 
    {
        printf("Sending G-code: %s", gCodeCommand[i]);
        SendCommands(gCodeCommand[i]);
        free(gCodeCommand[i]);  // Free allocated memory
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
    FILE *textDataFile = fopen("test.txt","r");

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

int mapTextToFontData(char textData[], fontValue fontData[], fontValue textFontData[], float userHeight) 
{
    int textIndex = 0;  // Track the current index in textFontData
    int ascii;

    float xOffset = 0.0;  // Track X offset for characters
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
        
        // Search for the corresponding font data for this character
        for (int j = 0; j < NumberOfLines; j++) 
        {
            if (fontData[j].value1 == 999 && fontData[j].value2 == ascii) 
            {
                totalStrokes = fontData[j].value3;  // Number of strokes for this character
                
                // Copy font data for this character
                for (int k = 0; k < totalStrokes; k++) 
                {
                    textFontData[textIndex] = fontData[j + k + 1];  // Copy the stroke data
                    textIndex++;  // Increment the textFontData index
                    strokesProcessed++;  // Increment stroke count
                }

                // Once all strokes for the character are processed, apply the offset
                if (strokesProcessed == totalStrokes) 
                {
                    xOffset += charSpace;  // Move to the next character space
                }

                break;  // Stop searching once we find the font data for the character
            }
        }
    }

    return 0;
}

int generateGCode(fontValue textFontData[], float scaleFactor, char *gCodeCommand[]) 
{
    int gCodeIndex = 0;
    
    int lastPenStatus = -1;  // To track the last pen status (initialized to an invalid value)

    // Loop through all font data for the text
    for (int i = 0; textFontData[i].value1 != -1; i++) 
    {
        int xValue = textFontData[i].value1;
        int yValue = textFontData[i].value2;
        int penStatus = textFontData[i].value3;
        float xScaled = xValue * scaleFactor;
        float yScaled = yValue * scaleFactor;

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
        } 
        else if (penStatus == 1) 
        {  
            sprintf(gCodeCommand[gCodeIndex++], "G1 X%.3f Y%.3f\n", xScaled, yScaled);
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