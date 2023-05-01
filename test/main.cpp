#include "OneButton.h"
OneButton btn_right(47, true);
OneButton btn_left(0, true);
unsigned long firstButtonClickTime = 0;
const unsigned long timeThreshold = 500;
bool rightClicked = false;
bool leftClicked = false;

void rightClick()
{
    rightClicked = true;
    if (firstButtonClickTime == 0)
    {
        firstButtonClickTime = millis();
    }
}
void leftClick()
{
    leftClicked = true;
    if (firstButtonClickTime == 0)
    {
        firstButtonClickTime = millis();
    }
}
void bothButtonsPressed()
{
    Serial.println("Both buttons pressed within 500ms");
}


void setup()
{
    Serial.begin(9600);
    btn_right.attachClick(rightClick);
    btn_left.attachClick(leftClick);
}
void loop()
{
    btn_right.tick();
    btn_left.tick();
    if (rightClicked && leftClicked)
    {
        if (millis() - firstButtonClickTime <= timeThreshold)
        {
            bothButtonsPressed();
        }
        rightClicked = false;
        leftClicked = false;
        firstButtonClickTime = 0;
    }
}
