#include <ncurses.h>
#include <iostream>
#include <regex>
#include <unistd.h>
#include <locale.h>

static const std::string error = "ERROR";
static const std::string battery_command = "acpi";
static const std::string charging_s = "Charging";
static const std::regex acpi_bat_pattern("\\d{1,3}%");
static const long sleep_time = 10000000L;
static int bars;
static int full_bars;
static bool battery_charging = false;
/* declare and initialize some color types */
// used for COLOR_PAIR(color);
typedef int color;
static color yellow = 1;
static color green = 2;
static color red = 3;
static color white = 4;
static color cyan = 5;
/* unicode chars to be printed as blocks */
static wchar_t block_solid = L'\u2588';
static wchar_t block_shaded = L'\u2592';

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
std::string find_percentage(std::string s)
{
    // base cases: string empty or substring up to ',' matches percentage
    if (s.length() == 0)
    {
        return error;
    }
    // find a token ending in ','
    int comma_loc = s.find(",");
    if (comma_loc == -1)
    {
         comma_loc = s.size() - 1;
    }
    std::string token = s.substr(0, comma_loc);
    // this will be a token in format ("[0-100]%")
    if (std::regex_match(token, acpi_bat_pattern))
    {
        return token;
    }
    return find_percentage(s.substr(1));
}
std::string get_bat_percent()
{
    const char * cmd = battery_command.c_str();
    std::string result = exec(cmd);
    battery_charging = (result.find(charging_s) != std::string::npos);
    return find_percentage(result);
}
void draw_battery(int percent)
{
    double decimal = ((double)percent / 100) * bars;
    full_bars = decimal;
    bool full_and_charging = percent == 100 && battery_charging;
    /* print blocks to make up the bar */
    int pair_n = (battery_charging|| full_and_charging ? green : yellow);
    // if the battery is full or charging, the bar is green.
    // otherwise it is yellow
    attron(COLOR_PAIR(pair_n));
    for (int i = 0; i < full_bars; i++)
    {
        add_wchar(block_solid);
    }
    for (int i = 0; i < (bars - full_bars); i++)
    {
        add_wchar(block_shaded);
    }
    attroff(COLOR_PAIR(pair_n));
    printw(" ");
}
/* draw the bar and the percentage following it */
void draw_window()
{
    std::string bat_percent_string;
    int bat_percent;
    int pair_n;

    bat_percent_string = get_bat_percent();
    bat_percent = std::stoi(bat_percent_string);
    erase();
    draw_battery(bat_percent);
    printw(bat_percent_string.c_str());
    addch('%');
    if (!battery_charging)
        // add a full block of red color that scrolls down from the last full bar
        // to the first full bar, if the battery is discharging
    {
        for (int i = full_bars - 2; i >= 0; i--)
        {
            pair_n = (bat_percent == 100 ? cyan : red);
            // if the battery is full, the block that scrolls down is
            // cyan. Otherwise it is red
            erase();
            draw_battery(bat_percent);
            printw(bat_percent_string.c_str());
            addch('%');
            mvdelch(0,i);
            attron(COLOR_PAIR(pair_n));
            add_wchar(block_solid);
            attroff(COLOR_PAIR(pair_n));
            refresh();
            usleep(sleep_time / full_bars - 1);
        }
    }
    else
        // blink the last full bar and one over if the battery is charging
    {
        for (int i = 0; i < 10; i++)
        {
            int color = (i % 2 == 0 ? white : cyan);
            int last_bar = full_bars;
            if (last_bar >= bars - 1)
            {
                last_bar --;
            }
            erase();
            draw_battery(bat_percent);
            printw(bat_percent_string.c_str());
            addch('%');
            mvdelch(0, last_bar);
            attron(COLOR_PAIR(color));
            add_wchar(block_solid);
            attroff(COLOR_PAIR(color));
            refresh();
            usleep(sleep_time / 10);
        }
    }
}
/* colors which will be used for ncurses display */
// colors in ncurses are always used in fg / bg pairs
void set_color_pairs()
{
    init_pair(yellow, COLOR_YELLOW, COLOR_BLACK);
    init_pair(green, COLOR_GREEN, COLOR_BLACK);
    init_pair(red, COLOR_RED, COLOR_BLACK);
    init_pair(white, COLOR_WHITE, COLOR_BLACK);
    init_pair(cyan, COLOR_CYAN, COLOR_BLACK);
}
void draw_window_loop()
{
    while (true)
    {
        draw_window();
    }
}

int main(int argc, char * argv[])
{
    bars = std::stoi(argv[1]);
    setlocale(LC_ALL, "");
    /* start ncurses mode */
    initscr();
    start_color();
    set_color_pairs();
    curs_set(0); // hide cursor
    cbreak();
    draw_window_loop();
}
