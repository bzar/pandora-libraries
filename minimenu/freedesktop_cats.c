
#include <stdio.h>
#include <strings.h>

#include "freedesktop_cats.h"

// folks want to limit categories to official ones, so okay.
// http://standards.freedesktop.org/menu-spec/latest/apa.html
//

char *freedesktop_approved_cats[] = {
  "AudioVideo",
  "Audio",
  "Video",
  "Development",
  "Education",
  "Game",
  "Graphics",
  "Network",
  "Office",
  "Settings",
  "System",
  "Utility",
  "Building",
  "Debugger",
  "IDE",
  "GUIDesigner",
  "Profiling",
  "RevisionControl",
  "Translation",
  "Calendar",
  "ContactManagement",
  "Database",
  "Dictionary",
  "Chart",
  "Email",
  "Finance",
  "FlowChart",
  "PDA",
  "ProjectManagement",
  "Presentation",
  "Spreadsheet",
  "WordProcessor",
  "2DGraphics",
  "VectorGraphics",
  "RasterGraphics",
  "3DGraphics",
  "Scanning",
  "OCR",
  "Photography",
  "Publishing",
  "Viewer",
  "TextTools",
  "DesktopSettings",
  "HardwareSettings",
  "Printing",
  "PackageManager",
  "Dialup",
  "InstantMessaging",
  "Chat",
  "IRCClient",
  "FileTransfer",
  "HamRadio",
  "News",
  "P2P",
  "RemoteAccess",
  "Telephony",
  "TelephonyTools",
  "VideoConference",
  "WebBrowser",
  "WebDevelopment",
  "Midi",
  "Mixer",
  "Sequencer",
  "Tuner",
  "TV",
  "AudioVideoEditing",
  "Player",
  "Recorder",
  "DiscBurning",
  "ActionGame",
  "AdventureGame",
  "ArcadeGame",
  "BoardGame",
  "BlocksGame",
  "CardGame",
  "KidsGame",
  "LogicGame",
  "RolePlaying",
  "Simulation",
  "SportsGame",
  "StrategyGame",
  "Art",
  "Construction",
  "Music",
  "Languages",
  "Science",
  "ArtificialIntelligence",
  "Astronomy",
  "Biology",
  "Chemistry",
  "ComputerScience",
  "DataVisualization",
  "Economy",
  "Electricity",
  "Geography",
  "Geology",
  "Geoscience",
  "History",
  "ImageProcessing",
  "Literature",
  "Math",
  "NumericalAnalysis",
  "MedicalSoftware",
  "Physics",
  "Robotics",
  "Sports",
  "ParallelComputing",
  "Amusement",
  "Archiving",
  "Compression",
  "Electronics",
  "Emulator",
  "Engineering",
  "FileTools",
  "FileManager",
  "TerminalEmulator",
  "Filesystem",
  "Monitor",
  "Security",
  "Accessibility",
  "Calculator",
  "Clock",
  "TextEditor",
  "Documentation",
  "Core",
  "KDE",
  "GNOME",
  "GTK",
  "Qt",
  "Motif",
  "Java",
  "ConsoleOnly",
  "Screensaver",
  "TrayIcon",
  "Applet",
  "Shell",
  NULL
};

unsigned char freedesktop_check_cat ( char *name ) {
  char **p = freedesktop_approved_cats;

  while ( *p ) {

    if ( strcasecmp ( *p, name ) == 0 ) {
      return ( 1 );
    }

    p++;
  }

  return ( 0 );
}

#if 0
int main ( void ) {

  printf ( "check Applet (should be 1) -> %d\n", freedesktop_check_cat ( "Applet" ) );
  printf ( "check Education (should be 1) -> %d\n", freedesktop_check_cat ( "Education" ) );
  printf ( "check Mofo (should be 0) -> %d\n", freedesktop_check_cat ( "Mofo" ) );

  return ( 0 );
}
#endif
