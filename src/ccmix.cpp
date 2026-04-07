/* 
 * File:   ccmix.cpp
 * Author: fbsagr
 *
 * Created on April 10, 2014, 1:43 PM
 */

#include "SimpleOpt.hpp"
#include "mix_file.hpp"
//#include "SimpleGlob.h"
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _MSC_VER

#include <windows.h>
#include <tchar.h>
#include "win32/dirent.hpp"

#else

#include <unistd.h>
#include <dirent.h>
#define TCHAR		char
#define _T(x)		x
#define _tprintf	printf
#define _tmain		main

#endif

#ifdef _WIN32
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#include <pwd.h>
#endif

enum { OPT_HELP, OPT_EXTRACT, OPT_CREATE, OPT_GAME, OPT_FILES, OPT_DIR,
       OPT_LIST, OPT_MIX, OPT_ID, OPT_LMD, OPT_ENC, OPT_CHK, OPT_INFO, OPT_ADD, 
       OPT_REM};
typedef enum { NONE, EXTRACT, CREATE, ADD, REMOVE, LIST, INFO} t_mixmode;

namespace
{
    const std::string games[] = {"td", "ra", "ts", "ra2"};

    bool fileExists(const std::string& path)
    {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        return file.is_open();
    }
}

//get program directory from argv[0]
static std::string getProgramDir(const std::string& program_location) 
{
    const std::string::size_type separator = program_location.find_last_of("\\/");
    if (separator == std::string::npos) {
        return ".";
    }

    return program_location.substr(0, separator);
}

//search and test a few locations for a global mix database
//TODO copy gmd if found but not in a home dir config.
std::string findGMD(const std::string& program_dir, const std::string& home_dir)
{
    const std::string gmd_loc = "global mix database.dat";
    std::vector<std::string> gmd_dir(3);
    gmd_dir[0] = home_dir;
    gmd_dir[1] = "/usr/share/ccmix";
    gmd_dir[2] = program_dir;
    for (unsigned int i = 0; i < gmd_dir.size(); i++) {
        const std::string gmd_test = gmd_dir[i] + DIR_SEPARATOR + gmd_loc;
        if (fileExists(gmd_test)) {
            return gmd_test;
        }
    }
    return gmd_loc;
}

std::string findKeySource(const std::string& program_dir)
{
    const std::string key_source_loc = "key.source";
    const std::string gmd_test = program_dir + DIR_SEPARATOR + key_source_loc;
    if (fileExists(gmd_test)) {
        return gmd_test;
    }

    return key_source_loc;
}

// This just shows a quick usage guide in case an incorrect parameter was used
// or not all required args were provided.
inline void ShowUsage(TCHAR** argv)
{
    std::cout << "Usage: " << argv[0] << " [--mode] (--file FILE)"
            "(--directory DIR) [--mix MIXFILE]" << std::endl;
    std::cout << "Try `" << argv[0] << " -?` or `" << argv[0] << 
            " --help` for more information." << std::endl;
}

// Shows more detailed help if help flags were used in invocation.
void ShowHelp(TCHAR** argv)
{
    std::cout << "\n***ccmix program usage***\n" << std::endl;
    std::cout << "Usage: " << argv[0] << 
            " [--mode] (--file FILE) (--directory DIR) (--game "
            "[td|ra|ts|ra2]) [--mix MIXFILE]\n" << std::endl;
    std::cout << "Modes:\n" << std::endl;
    std::cout << "--extract\n"
            "Extracts the contents of the specified mix file to the current "
            "directory.\n"
            "--file specifies a single file to extract.\n"
            "--directory specifies an alternative directory to extract to.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's "
            "Revenge.\n" << std::endl;
    std::cout << "--create\n"
            "Creates a new mix file from the contents of the current folder.\n"
            "--file specifies a single file as the initial file to add to the\n"
            "new mix.\n"
            "--directory specifies an alternative directory to create mix from.\n"
            "--checksum specifies the mix should have a checksum.\n"
            "--encrypted specified the mix header should be encrypted.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's "
            "Revenge.\n" << std::endl;
    std::cout << "--list\n"
            "Lists the contents of the specified mix file.\n" 
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and Red Alert 2/Yuri's "
            "Revenge.\n" << std::endl;
    std::cout << "--add\n"
            "Adds the specified file or mix feature.\n"
            "--file specifies a single file to add.\n"
            "--checksum specifies the mix should have a checksum.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's "
            "Revenge.\n" << std::endl;
    std::cout << "--remove\n"
            "Removes the specified file or mix feature.\n"
            "--file specifies a single file to remove.\n"
            "--checksum specifies the mix should not have a checksum.\n"
            "--game specified the game the mix is from, td covers the\n"
            "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
            "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's "
            "Revenge.\n" << std::endl;
}

//quick inline to respond to more than one command being specified.
inline void NoMultiMode(TCHAR** argv)
{
    _tprintf(_T("You cannot specify more than one mode at once.\n"));
    ShowUsage(argv);
}

//convert string we got for ID to uint32_t
//This converts a text hex string to a number, its not to hash the filename.
//That is a method of MixFile.
uint32_t StringToID(const std::string& in_string)
{
    if (in_string.size() > 8) return 0;
    
    char* p;
    uint32_t n = strtoul( in_string.c_str(), &p, 16 ); 
    if ( * p != 0 ) {  
    	return 0;
    }    else {  
    	return n;
    }
}

//Function handles the different ways to extract files from a mix.
bool Extraction(MixFile& in_file, const std::string& filename, const std::string& outdir, uint32_t id)
{   
    std::string extraction_dir = outdir.empty() ? "." : outdir;
    std::string destination = extraction_dir + DIR_SEPARATOR + filename;
    
    if (filename == "" && id == 0) {
        if (!in_file.extractAll(extraction_dir)) {
            std::cout << "Extraction failed" << std::endl;
            return false;
        } else {
            return true;
        }
    } else if (filename != "" && id == 0) {
        if (!in_file.extractFile(filename, destination)) {
            std::cout << "Extraction failed" << std::endl;
            return false;
        } else {
            return true;
        }
    } else if (id != 0){
        if (filename == ""){
            std::cout << "You must specify a filename to extract to when giving "
                    "an ID" << std::endl;
            return false;
        }
        if (!in_file.extractFile(id, destination)) {
            std::cout << "Extraction failed" << std::endl;
            return false;
        } else {
            return true;
        }
    } else {
        std::cout << "You have used an unsupported combination of options." << std::endl;
        return false;
    }
    return false;
}

//Return a string of the home dir path
std::string GetHomeDir()
{
    const char* tmp = NULL;
    std::string rv;
    
    #ifdef _WIN32

    tmp = std::getenv("HOMEDRIVE");
    if ( tmp == NULL ) {
        return "";
    } else {
        rv = std::string(tmp);
    }
    
    tmp = std::getenv("HOMEPATH");
    if ( tmp == NULL ) {
        return "";
    } else {
        rv += std::string(tmp);
    }

    #else

    tmp = std::getenv("HOME");
    if ( tmp == NULL ) {
        rv = std::string(getpwuid(getuid())->pw_dir);
    } else {
        rv = std::string(tmp);
    }

    #endif
    return rv;
}

//This specifies the various available command line options
CSimpleOpt::SOption g_rgOptions[] = {
    { OPT_EXTRACT,  _T("--extract"),      SO_NONE    },
    { OPT_CREATE,   _T("--create"),       SO_NONE    },
    { OPT_ADD,      _T("--add"),          SO_NONE    },
    { OPT_REM,      _T("--remove"),       SO_NONE    },
    { OPT_LIST,     _T("--list"),         SO_NONE    },
    { OPT_INFO,     _T("--info"),         SO_NONE    },
    { OPT_LMD,      _T("--lmd"),          SO_NONE    },
    { OPT_ENC,      _T("--encrypt"),      SO_NONE    },
    { OPT_CHK,      _T("--checksum"),     SO_NONE    },
    { OPT_FILES,    _T("--file"),         SO_REQ_SEP },
    { OPT_ID,       _T("--id"),           SO_REQ_SEP },
    { OPT_DIR,      _T("--directory"),    SO_REQ_SEP },
    { OPT_MIX,      _T("--mix"),          SO_REQ_SEP },
    { OPT_GAME,     _T("--game"),         SO_REQ_SEP },
    { OPT_HELP,     _T("-?"),             SO_NONE    },
    { OPT_HELP,     _T("--help"),         SO_NONE    },
    SO_END_OF_OPTIONS                       
};
    
int _tmain(int argc, TCHAR** argv)
{
    if(argc <= 1) {
        ShowUsage(argv);
        return 0;
    }
    
    //initialise some variables used later
    uint32_t file_id = 0;
    std::string file = "";
    std::string dir = "";
    std::string input_mixfile = "";
    const std::string program_path(argv[0]);
    const std::string program_dir = getProgramDir(program_path);
    const std::string user_home_dir = GetHomeDir();
    const std::string global_db_path = findGMD(program_dir, user_home_dir);
    const std::string key_source_path = findKeySource(program_dir);
    t_game game = game_td;
    t_mixmode mode = NONE;
    bool local_db = false;
    bool encrypt = false;
    bool checksum = false;
    
    //seed random number generator
    srand(time(NULL));
    
    CSimpleOpt args(argc, argv, g_rgOptions);
    
    //Process the command line args and set the variables
    while (args.Next()) {
        if (args.LastError() != SO_SUCCESS) {
            _tprintf(_T("Invalid argument: %s\n"), args.OptionText());
            ShowUsage(argv);
            return 1;
        }
        
        switch (args.OptionId()) {
            case OPT_HELP:
            {
                ShowHelp(argv);
                return 0;
            }
            case OPT_FILES:
            {
                if (args.OptionArg() != NULL) {
                    file = std::string(args.OptionArg());
                } else {
                    _tprintf(_T("--filename option requires a filename.\n"));
                    return 1;
                }
                break;
            }
            case OPT_ID:
            {
                if (args.OptionArg() != NULL) {
                    file_id = StringToID(std::string(args.OptionArg()));
                } else {
                    _tprintf(_T("--id option requires a file id.\n"));
                    return 1;
                }
                break;
            }
            case OPT_MIX:
            {
                if (args.OptionArg() != NULL) {
                    input_mixfile = std::string(args.OptionArg());
                    std::cout << "Operating on " << input_mixfile << std::endl;
                } else {
                    _tprintf(_T("--mix option requires a mix file.\n"));
                    return 1;
                }
                break;
            }
            case OPT_DIR:
            {
                if (args.OptionArg() != NULL) {
                    dir = std::string(args.OptionArg());
                } else {
                    _tprintf(_T("--directory option requires a directory name.\n"));
                    return 1;
                }
                break;
            }
            case OPT_LMD:
            {
                local_db = true;
                break;
            }
            case OPT_ENC:
            {
                encrypt = true;
                break;
            }
            case OPT_CHK:
            {
                checksum = true;
                break;
            }
            case OPT_GAME:
            {
                std::string gt = std::string(args.OptionArg());
                if(gt == games[0]){
                    game = game_td;
                } else if(gt == games[1]){
                    game = game_ra;
                } else if(gt == games[2]){
                    game = game_ts;
                } else if(gt == games[3]){
                    game = game_ra2;
                } else {
                    _tprintf(_T("--game is either td, ra, ts or ra2.\n"));
                }
                    
                break;
            }
            case OPT_CREATE:
            {
                if (mode != NONE) { NoMultiMode(argv); return 1; }
                mode = CREATE;
                break;
            }
            case OPT_EXTRACT:
            {
                if (mode != NONE) { NoMultiMode(argv); return 1; }
                mode = EXTRACT;
                break;
            }
            case OPT_LIST:
            {
                if (mode != NONE) { NoMultiMode(argv); return 1; }
                mode = LIST;
                break;
            }
            case OPT_INFO:
            {
                if (mode != NONE) { NoMultiMode(argv); return 1; }
                mode = INFO;
                break;
            }
            case OPT_ADD:
            {
                if (mode != NONE) { NoMultiMode(argv); return 1; }
                mode = ADD;
                break;
            }
            case OPT_REM:
            {
                if (mode != NONE) { NoMultiMode(argv); return 1; }
                mode = REMOVE;
                break;
            }
            default:
            {
                if (args.OptionArg()) {
                    _tprintf(_T("option: %2d, text: '%s', arg: '%s'\n"),
                        args.OptionId(), args.OptionText(), args.OptionArg());
                }
                else {
                    _tprintf(_T("option: %2d, text: '%s'\n"),
                        args.OptionId(), args.OptionText());
                }
                break;
            }
        }
    }
    
    //check if we got told a mix file to do something with.
    if (input_mixfile == ""){
        std::cout << "You must specify --mix MIXFILE to operate on." << std::endl;
        return 1;
    }
    
    switch(mode) {
        case EXTRACT:
        {   
            MixFile in_file(global_db_path, game);

            if (!in_file.open(input_mixfile)){
                std::cout << "Cannot open specified mix file" << std::endl;
                return 1;
            }
            
            if (!Extraction(in_file, file, dir, file_id)) {
                return 1;
            }
            return 0;
            break;
        }
        case CREATE:
        {
            MixFile out_file(global_db_path, game);

            if (!out_file.createMix(input_mixfile, dir, local_db, 
                 encrypt, checksum, key_source_path)){
                std::cout << "Failed to create new mix file" << std::endl;
                return 1;
            }
            
            return 0;
            break;
        }
        case ADD:
        {
            MixFile in_file(global_db_path, game);

            if (!in_file.open(input_mixfile)){
                std::cout << "Cannot open specified mix file" << std::endl;
                return 1;
            }
            
            if(file == ""){
                if(checksum){
                    in_file.addCheckSum();
                }
            } else {
                in_file.addFile(file);
            }
            
            return 0;
            break;
        }
        case REMOVE:
        {
            MixFile in_file(global_db_path, game);

            if (!in_file.open(input_mixfile)){
                std::cout << "Cannot open specified mix file" << std::endl;
                return 1;
            }
            
            if(file == ""){
                if(checksum){
                    in_file.removeCheckSum();
                }
            } else {
                in_file.removeFile(file);
            }
            
            return 0;
            break;
        }
        case LIST:
        {
            MixFile in_file(global_db_path, game);

            if (!in_file.open(input_mixfile)){
                std::cout << "Cannot open specified mix file" << std::endl;
                return 1;
            }
            
            in_file.printFileList();
            return 0;
            break;
        }
        case INFO:
        {
            MixFile in_file(global_db_path, game);

            if (!in_file.open(input_mixfile)){
                std::cout << "Cannot open specified mix file" << std::endl;
                return 1;
            }
            
            in_file.printInfo();
            return 0;
            break;
        }
        default:
        {
            std::cout << "command switch default, this shouldn't happen!!" << std::endl;
            return 1;
        }
    }
    
    return 0;
}
