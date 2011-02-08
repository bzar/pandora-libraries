
#include <stdio.h>
#include <strings.h>

#include "freedesktop_cats.h"

// folks want to limit categories to official ones, so okay.
// http://standards.freedesktop.org/menu-spec/latest/apa.html
//

freedesktop_cat_t freedesktop_complete[] = {

  // HACK
  { BADCATNAME,         NULL,           "Lazy PXML dev is lazy." },
  { "NoParentCategory", NULL,           "To mean no parent category" },
  { "NoSubcategory",    "NoneParent",   "To mean no subcategory" },
  // HACK

  { "AudioVideo",       NULL,           "A multimedia (audio/video) application" },
  { "Audio",            NULL,           "An audio application" },
  { "Video",            NULL,           "A video application" },
  { "Development",      NULL,           "An application for development" },
  { "Education",        NULL,           "Educational software" },
  { "Game",             NULL,           "A game" },
  { "Graphics",         NULL,           "Graphical application" },
  { "Network",          NULL,           "Network application such as a web browser" },
  { "Office",           NULL,           "An office type application" },
  { "Settings",         NULL,           "Settings applications" },
  { "System",           NULL,           "System application" },
  { "Utility",          NULL,           "Small utility application" },

  { "Building",         "Development",  "A tool to build applications " },
  { "Debugger",         "Development",   "A tool to debug applications" },
  { "IDE",              "Development",   "IDE application " },
  { "GUIDesigner",      "Development",   "A GUI designer application " },
  { "Profiling",        "Development",   "A profiling tool      " },
  { "RevisionControl",  "Development",   "Applications like cvs or subversion " },
  { "Translation",      "Development",   "A translation tool " },
  { "Calendar",         "Office",   "Calendar application " },
  { "ContactManagement",    "Office",   "E.g. an address book " },
  { "Database",         "Office",       "Application to manage a database " },
  { "Database",         "Development",  "Application to manage a database " },
  { "Database",         "AudioVideo",   "Application to manage a database " },
  { "Dictionary",       "Office",   "A dictionary  " },
  { "Chart",            "Office",   "Chart application" },
  { "Email",            "Office",   "Email application" },
  { "Finance",          "Office",   "Application to manage your finance " },
  { "FlowChart",        "Office",   "A flowchart application    " },
  { "PDA",              "Office",   "Tool to manage your PDA    " },
  { "ProjectManagement",     "Office",   "Project management application " },
  { "Presentation",     "Office",   "Presentation software  " },
  { "Spreadsheet",      "Office",   "A spreadsheet  " },
  { "WordProcessor",    "Office",   "A word processor  " },
  { "2DGraphics",       "Graphics",   "2D based graphical application " },
  { "VectorGraphics",   "Graphics",   "Vector based graphical application " },
  { "RasterGraphics",   "Graphics",   "Raster based graphical application " },
  { "3DGraphics",       "Graphics",   "3D based graphical application     " },
  { "Scanning",         "Graphics",   "Tool to scan a file/text      " },
  { "OCR",              "Graphics",   "Optical character recognition application " },
  { "Photography",      "Graphics",   "Camera tools, etc. " },
  { "Publishing",       "Graphics",   "Desktop Publishing applications and Color Management tools " },
  { "Viewer",           "Graphics",   "Tool to view e.g. a graphic or pdf file   " },
  { "TextTools",        "Utility",   "A text tool utiliy  " },
  { "DesktopSettings",  "Settings",   "Configuration tool for the GUI " },
  { "HardwareSettings", "Settings",   "A tool to manage hardware components, like sound cards, video cards or printers " },
  { "Printing",         "Settings",   "A tool to manage printers " },
  { "PackageManager",   "Settings",   "A package manager application " },
  { "Dialup",           "Network",   "A dial-up program " },
  { "InstantMessaging", "Network",   "An instant messaging client " },
  { "Chat",             "Network",   "A chat client" },
  { "IRCClient",        "Network",   "An IRC client" },
  { "FileTransfer",     "Network",   "Tools like FTP or P2P programs " },
  { "HamRadio",         "Network",   "HAM radio software " },
  { "News",             "Network",   "A news reader or a news ticker " },
  { "P2P",              "Network",   "A P2P program " },
  { "RemoteAccess",     "Network",   "A tool to remotely manage your PC " },
  { "Telephony",        "Network",   "Telephony via PC   " },
  { "TelephonyTools",   "Utility",   "Telephony tools, to dial a number, manage PBX, ... " },
  { "VideoConference",  "Network",   "Video Conference software  " },
  { "WebBrowser",       "Network",   "A web browser    " },
  { "WebDevelopment",   "Network",   "A tool for web developers " },
  { "Midi",             "AudioVideo",   "An app related to MIDI    " },
  { "Midi",             "Audio",     "An app related to MIDI    " },
  { "Mixer",            "AudioVideo",   "Just a mixer  " },
  { "Mixer",            "Audio",     "Just a mixer  " },
  { "Sequencer",        "AudioVideo",   "A sequencer  " },
  { "Sequencer",        "Audio",     "A sequencer  " },
  { "Tuner",            "AudioVideo",   "A tuner  " },
  { "Tuner",            "Audio",        "A tuner  " },
  { "TV",               "AudioVideo",   "A TV application" },
  { "TV",               "Video",   "A TV application" },
  { "AudioVideoEditing",    "Audio",   "Application to edit audio/video files " },
  { "AudioVideoEditing",    "Video",   "Application to edit audio/video files " },
  { "AudioVideoEditing",    "AudioVideo",   "Application to edit audio/video files " },
  { "Player",           "Audio",   "Application to play audio/video files " },
  { "Player",           "AudioVideo",   "Application to play audio/video files " },
  { "Player",           "Video",   "Application to play audio/video files " },
  { "Recorder",         "Audio",   "Application to record audio/video files " },
  { "Recorder",         "AudioVideo",   "Application to record audio/video files " },
  { "DiscBurning",      "AudioVideo",   "Application to burn a disc     " },
  { "ActionGame",       "Game",   "An action game " },
  { "AdventureGame",    "Game",   "Adventure style game" },
  { "ArcadeGame",       "Game",   "Arcade style game" },
  { "BoardGame",        "Game",   "A board game" },
  { "BlocksGame",       "Game",   "Falling blocks game" },
  { "CardGame",         "Game",   "A card game  " },
  { "KidsGame",         "Game",   "A game for kids" },
  { "LogicGame",        "Game",   "Logic games like puzzles, etc " },
  { "RolePlaying",      "Game",   "A role playing game     " },
  { "Simulation",       "Game",   "A simulation game     " },
  { "SportsGame",       "Game",   "A sports game     " },
  { "StrategyGame",     "Game",   "A strategy game     " },
  { "Art",              "Education",   "Software to teach arts    " },
  { "Construction",     "Education",   NULL },
  { "Music",            "Audio",  "Musical software" },
  { "Languages",        "Education",   "Software to learn foreign languages " },
  { "Science",          "Education",   "Scientific software     " },
  { "ArtificialIntelligence",     "Education",   "Artificial Intelligence software " },
  { "Astronomy",        "Education",   "Astronomy software   " },
  { "Biology",          "Education",   "Biology software   " },
  { "Chemistry",        "Education",   "Chemistry software   " },
  { "ComputerScience",  "Education",   "ComputerSience software " },
  { "DataVisualization", "Education",   "Data visualization software " },
  { "Economy",          "Education",   "Economy software   " },
  { "Electricity",      "Education",   "Electricity software" },
  { "Geography",        "Education",   "Geography software" },
  { "Geology",          "Education",   "Geology software" },
  { "Geoscience",       "Education",   "Geoscience software" },
  { "History",          "Education",   "History software" },
  { "ImageProcessing",  "Education",   "Image Processing software " },
  { "Literature",       "Education",   "Literature software     " },
  { "Math",             "Education",   "Math software     " },
  { "NumericalAnalysis",   "Education",   "Numerical analysis software " },
  { "MedicalSoftware",     "Education",   "Medical software   " },
  { "Physics",          "Education",   "Physics software   " },
  { "Robotics",         "Education",   "Robotics software  " },
  { "Sports",           "Education",   "Sports software    " },
  { "ParallelComputing",   "Education",   "Parallel computing software " },
  { "Amusement",        NULL, "A simple amusement " },
  { "Archiving",        "Utility",   "A tool to archive/backup data " },
  { "Compression",      "Utility",   "A tool to manage compressed data/archives " },
  { "Electronics",      NULL, "Electronics software, e.g. a circuit designer" },
  { "Emulator",         "Game",   "Emulator of another platform, such as a DOS emulator " },
  { "Engineering",      NULL, "Engineering software, e.g. CAD programs " },
  { "FileTools",        "Utility",   "A file tool utility " },
  { "FileTools",        "System",   "A file tool utility " },
  { "FileManager",      "System",   "A file manager " },
  { "TerminalEmulator", "System",   "A terminal emulator application " },
  { "Filesystem",       "System",   "A file system tool  " },
  { "Monitor",          "System",   "Monitor application/applet that monitors some resource or activity " },
  { "Security",         "System",   "A security tool      " },
  { "Accessibility",    "System",   "Accessibility      " },
  { "Calculator",       "Utility",   "A calculator      " },
  { "Clock",            "Utility",   "A clock application/applet " },
  { "TextEditor",       "Utility",   "A text editor      " },
  { "Documentation",    NULL, "Help or documentation      " },
  { "Core",             NULL, "Important application, core to the desktop such as a file manager or a help browser " },
  { "KDE",              "QT",   "Application based on KDE libraries " },
  { "GNOME",           "GTK",   "Application based on GNOME libraries " },
  { "GTK",              NULL, "Application based on GTK+ libraries" },
  { "Qt",               NULL, "Application based on Qt libraries" },
  { "Motif",            NULL, "Application based on Motif libraries" },
  { "Java",             NULL, "Application based on Java GUI libraries, such as AWT or Swing" },
  { "ConsoleOnly",      NULL, "Application that only works inside a terminal (text-based or command line application)" },
  { NULL,               NULL, NULL }
};

// return the found category, if ..
//   name matches catname AND
//   parent is NULL, or
//     parent is value and matches cats parent.
// ex:
//   If you're looking for Game/NULL, you'll find it.
//   If you're looking for Game/Emulator, you'll find it.
//   If you're looking for Emulator/NULL you WILL NOT find it.
freedesktop_cat_t *freedesktop_category_query ( char *name, char *parentcatname ) {
  freedesktop_cat_t *p = freedesktop_complete;

  while ( p -> cat ) {

    if ( strcasecmp ( p -> cat, name ) == 0 ) {

      if ( parentcatname == NULL && p -> parent_cat == NULL ) {
	return ( p );
      } else if ( parentcatname && p -> parent_cat && strcasecmp ( p -> parent_cat, parentcatname ) == 0 ) {
	return ( p );
      }

    }

    p++;
  }

  return ( NULL );
}

#if 0
int main ( void ) {

  printf ( "check Applet (should be 1) -> %d\n", freedesktop_category_query ( "Applet" ) );
  printf ( "check Education (should be 1) -> %d\n", freedesktop_category_query ( "Education" ) );
  printf ( "check Mofo (should be 0) -> %d\n", freedesktop_category_query ( "Mofo" ) );

  return ( 0 );
}
#endif
