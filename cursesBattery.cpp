#include <ncurses.h>
#include <string>
#include <regex>
#include <iostream>
#include <unistd.h>

static const std::string batteryCommand = "acpi";
static const std::regex batteryPercentPattern("\\d{1,3}%");
static const std::regex number("\\d+");
// e.g. "2%," or "80%," or "100%,"
static const std::string error = "NOT_FOUND";
static const std::string chargingString = "Charging";
static const std::string disChargingString = "Discharging";
static const char * numberBarsFlag = "-b";
static const char comma = ',';
static wchar_t fullBlock = L'\u2588';
static wchar_t mediumShadeFullBlock = L'\u2592';
static const long sleepTime = 10000000L;
// ten seconds
static int numberBars = 60;
static int numberFullBars;
static bool batteryCharging = false;
static bool batterySaysDischarging = false;
// declare and initialize some color types
// used for COLOR_PAIR(color);
typedef int color;
static color yellow = 1;
static color green = 2;
static color red = 3;
static color white = 4;
static color cyan = 5;

/* taken from stack overflow */
std::string exec(const char* cmd)
{
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return error;
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
}
/* loop through and handle arguments */
void handleAruments(int argc, char * argv[])
{
    // loop through arguments
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], numberBarsFlag) == 0)
        {
            if (i + 2 <= argc)
            {
                std::string numberArgument = argv[i + 1];
                if (std::regex_match(numberArgument, number))
                {
                    numberBars = std::stoi(numberArgument);
                }
                else
                {
                    std::cout << "token following -b flag must be a number!" << std::endl;
                }
            }
            else
            {
                std::cout << "No flags found after -b (number of flags argument)" << std::endl;
            }
        }
        else
        {
            std::cout << "Not a proper argument or flag: " << argv[i] << std::endl;
            i = argc;
            // end the loop
        }
    }
}
/* recursive method: finds token with percentage in it */
std::string findPercentage(std::string subjectString)
{
    // base cases: string empty or substring up to ',' matches percentage
    if (subjectString.length() == 0)
    {
        return error;
    }
    // find a token ending in ','
    int commaLocation = subjectString.find(comma);
    if (commaLocation == -1)
    {
         commaLocation = subjectString.size() - 1;
    }
    std::string token = subjectString.substr(0, commaLocation);
    // this will be a token in format ("[0-100]%")
    if (std::regex_match(token, batteryPercentPattern))
    {
        return token;
    }
    return findPercentage(subjectString.substr(1));
}
/* determines if the battery is discharging or not */
bool containsString(std::string subjectString, std::string searchFor)
{
    bool containsString = false;
    for (int i = 0; i < subjectString.size(); i++)
    {
        if (subjectString[i] == searchFor[0])
            // find the first character that would start the
            // word "Charging" in the subject string
        {
            bool matches = true;
            int wordLength = searchFor.length();
            int initialIndex = i;
            while (i < initialIndex + wordLength)
                // matches will stay true if all the characters
                // in the word that started with 'C' matches "Charging"
            {
                matches &= searchFor[i - initialIndex] == subjectString[i];
                i++;
            }
            if (matches)
            {
                i = subjectString.size();
                containsString = matches;
            }
        }
    }
    return containsString;
}
/* convert wchar_t to cchar_t */
// taken from David X at StackOverflow
// needed to display unicode
int add_wchar(int c)
    {
    cchar_t t = {
        0 , // .attr
        {c,0} // not sure how .chars works, so best guess
        };
    return add_wch(&t);
    }
std::string getBatteryPercent()
{
    std::string command(batteryCommand);
    const char * cmd = command.c_str();
    std::string result = exec(cmd);
    batteryCharging = containsString(result, chargingString);
    batterySaysDischarging = containsString(result, disChargingString);
    return findPercentage(result);
}
void drawBattery(int percent)
{
    double decimal = ((double)percent / 100) * numberBars;
    numberFullBars = decimal;
    bool fullAndCharging = percent == 100 && !batterySaysDischarging;
    /* print blocks to make up the bar */
    int pairNumber = (batteryCharging || fullAndCharging ? green : yellow);
    // if the battery is full or charging, the bar is green.
    // otherwise it is yellow
    attron(COLOR_PAIR(pairNumber));
    for (int i = 0; i < numberFullBars; i++)
    {
        add_wchar(fullBlock);
    }
    for (int i = 0; i < (numberBars - numberFullBars); i++)
    {
        add_wchar(mediumShadeFullBlock);
    }
    attroff(COLOR_PAIR(pairNumber));
    printw(" ");
    if (batteryCharging)
    {
        printw("Charging: ");
    }
}
/* draw the bar and the percentage following it */
void drawWindow()
{
    std::string batteryPercentageString;
    int batteryPercentage;
    int pairNumber;

    batteryPercentageString = getBatteryPercent();
    batteryPercentage = std::stoi(getBatteryPercent());
    erase();
    drawBattery(batteryPercentage);
    printw(batteryPercentageString.c_str());
    addch('%');
    if (!batteryCharging)
        // add a full block of red color that scrolls down from the last full bar
        // to the first full bar, if the battery is discharging
    {
        for (int i = numberFullBars - 2; i >= 0; i--)
        {
            pairNumber = (batteryPercentage == 100 ? cyan : red);
            // if the battery is full, the block that scrolls down is
            // cyan. Otherwise it is red
            erase();
            drawBattery(batteryPercentage);
            printw(batteryPercentageString.c_str());
            addch('%');
            mvdelch(0,i);
            attron(COLOR_PAIR(pairNumber));
            add_wchar(fullBlock);
            attroff(COLOR_PAIR(pairNumber));
            refresh();
            usleep(sleepTime / numberFullBars - 1);
        }
    }
    else
        // blink the last full bar and one over if the battery is charging
    {
        for (int i = 0; i < 10; i++)
        {
            int color = (i % 2 == 0 ? white : cyan);
            int lastBarPlusOne = numberFullBars;
            if (lastBarPlusOne >= numberBars - 1)
            {
                lastBarPlusOne--;
            }
            erase();
            drawBattery(batteryPercentage);
            printw(batteryPercentageString.c_str());
            addch('%');
            mvdelch(0, lastBarPlusOne);
            attron(COLOR_PAIR(color));
            add_wchar(fullBlock);
            attroff(COLOR_PAIR(color));
            refresh();
            usleep(sleepTime / 10);
        }
    }
}
int main(int argc, char * argv[])
{
    handleAruments(argc, argv);
    setlocale(LC_ALL, "");
    /* start ncurses mode */
    initscr(); // create stdscr
    start_color();
    curs_set(0); // hide the cursor
    cbreak();
    std::string batteryPercentageString;
    /* set some of the color types to represent color pairs using init_pair */
    init_pair(yellow, COLOR_YELLOW, COLOR_BLACK);
    init_pair(green, COLOR_GREEN, COLOR_BLACK);
    init_pair(red, COLOR_RED, COLOR_BLACK);
    init_pair(white, COLOR_WHITE, COLOR_BLACK);
    init_pair(cyan, COLOR_CYAN, COLOR_BLACK);

    /* main loop */
    while (true)
    {
        drawWindow();
    }
    getch();
    endwin();
    return 0;
}
