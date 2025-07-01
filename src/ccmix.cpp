#include "SimpleOpt.h"
#include "mix_file.h"
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include <windows.h>
#include <tchar.h>
#include "win32/dirent.h"

#define DIR_SEPARATOR '\\'

enum { OPT_HELP, OPT_EXTRACT, OPT_CREATE, OPT_GAME, OPT_FILES, OPT_DIR,
       OPT_LIST, OPT_MIX, OPT_ID, OPT_LMD, OPT_ENC, OPT_CHK, OPT_INFO, OPT_ADD, 
       OPT_REM};

enum class ExecutionMode 
{ 
    NONE, 
    EXTRACT, 
    CREATE, 
    ADD, 
    REMOVE, 
    LIST, 
    INFO
};

const std::string games[] = {"td", "ra", "ts", "ra2"};

// Search and test a few locations for a global mix database
// TODO copy gmd if found but not in a home dir config.
std::string FindGMD(const std::string program_dir, const std::string home_dir)
{
    std::string gmd_loc = "global mix database.dat";
    std::vector<std::string> gmd_dir(3);
    gmd_dir[0] = home_dir;
    gmd_dir[1] = "/usr/share/ccmix";
    gmd_dir[2] = program_dir;
    for (unsigned int i = 0; i < gmd_dir.size(); i++)
    {
        std::string gmd_test = gmd_dir[i] + DIR_SEPARATOR + gmd_loc;
        if (FILE *file = fopen(gmd_test.c_str(), "r"))
        {
            fclose(file);
            return gmd_test;
        }
    }
    return gmd_loc;
}

std::string FindKeySource(const std::string program_dir)
{
    std::string key_source_loc = "key.source";
    std::string gmd_test = program_dir + DIR_SEPARATOR + key_source_loc;
    if (FILE *file = fopen(gmd_test.c_str(), "r")) {
        fclose(file);
        return gmd_test;
    }

    return key_source_loc;
}

// This just shows a quick usage guide in case an incorrect parameter was used
// or not all required args were provided.
inline void ShowUsage(TCHAR** argv)
{
    std::wcout << "Usage: " << argv[0] << " [--mode] (--file FILE)"
            "(--directory DIR) [--mix MIXFILE]" << std::endl;
    std::wcout << "Try `" << argv[0] << " -?` or `" << argv[0] <<
            " --help` for more information." << std::endl;
}

// Shows more detailed help if help flags were used in invocation.
void ShowHelp(TCHAR** argv)
{
    std::wcout << "/n***ccmix program usage***\n" << std::endl;
    std::wcout << "Usage: " << argv[0] <<
        " [--mode] (--file FILE) (--directory DIR) (--game "
        "[td|ra|ts|ra2]) [--mix MIXFILE]\n" << std::endl;
    std::wcout << "Modes:\n" << std::endl;
    std::wcout << "--extract\n"
        "Extracts the contents of the specified mix file to the current "
        "directory.\n"
        "--file specifies a single file to extract.\n"
        "--directory specifies an alternative directory to extract to.\n"
        "--game specified the game the mix is from, td covers the\n"
        "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
        "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's "
        "Revenge.\n" << std::endl;
    std::wcout << "--create\n"
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
    std::wcout << "--list\n"
        "Lists the contents of the specified mix file.\n"
        "--game specified the game the mix is from, td covers the\n"
        "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
        "expansions. ts covers Tiberian Sun and Red Alert 2/Yuri's "
        "Revenge.\n" << std::endl;
    std::wcout << "--add\n"
        "Adds the specified file or mix feature.\n"
        "--file specifies a single file to add.\n"
        "--checksum specifies the mix should have a checksum.\n"
        "--game specified the game the mix is from, td covers the\n"
        "orignal C&C and Sole Survivor. ra covers Redalert and its\n"
        "expansions. ts covers Tiberian Sun and ra2 covers Red Alert 2/Yuri's "
        "Revenge.\n" << std::endl;
    std::wcout << "--remove\n"
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
    std::cout << "You cannot specify more than one mode at once." << std::endl;
    ShowUsage(argv);
}

//convert string we got for ID to uint32_t
//This converts a text hex string to a number, its not to hash the filename.
//That is a method of MixFile.
uint32_t StringToID(std::string in_string)
{
    if (in_string.size() > 8) return 0;

    char* p;
    uint32_t n = strtoul(in_string.c_str(), &p, 16);
    if (*p != 0)
        return 0;
    else
        return n;
}

//Function handles the different ways to extract files from a mix.
bool Extraction(MixFile& in_file, std::string filename, std::string outdir, uint32_t id)
{
    if (outdir == "") outdir = "./";
    std::string destination = outdir + DIR_SEPARATOR + filename;

    if (filename == "" && id == 0) {
        if (!in_file.extractAll(outdir)) {
            std::wcout << "Extraction failed" << std::endl;
            return false;
        }
        else {
            return true;
        }
    }
    else if (filename != "" && id == 0) {
        if (!in_file.extractFile(filename, destination)) {
            std::wcout << "Extraction failed" << std::endl;
            return false;
        }
        else {
            return true;
        }
    }
    else if (id != 0) {
        if (filename == "") {
            std::wcout << "You must specify a filename to extract to when giving "
                "an ID" << std::endl;
            return false;
        }
        if (!in_file.extractFile(id, destination)) {
            std::wcout << "Extraction failed" << std::endl;
            return false;
        }
        else {
            return true;
        }
    }
    else {
        std::wcout << "You have used an unsupported combination of options." << std::endl;
        return false;
    }
    return false;
}

//Return a string of the home dir path
std::string GetHomeDir()
{
    char* tmp;
    std::string rv;

#ifdef _WIN32

    tmp = getenv("HOMEDRIVE");
    if (tmp == NULL) {
        return "";
    }
    else {
        rv = std::string(tmp);
    }

    tmp = getenv("HOMEPATH");
    if (tmp == NULL) {
        return "";
    }
    else {
        rv += std::string(tmp);
    }

#else

    tmp = getenv("HOME");
    if (tmp == NULL) {
        rv = string(getpwuid(getuid())->pw_dir);
    }
    else {
        rv = string(tmp);
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
    if (argc <= 1) {
        ShowUsage(argv);
        return 0;
    }

    //initialise some variables used later
    uint32_t file_id = 0;
    std::wstring file = L"";
    std::string dir = "";
    std::string input_mixfile = "";
    const std::wstring program_path(argv[0]);
    std::string user_home_dir = GetHomeDir();
    GameKind game = GameKind::TD;
    ExecutionMode mode = ExecutionMode::NONE;
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
                file = std::wstring(args.OptionArg());
            }
            else {
                _tprintf(_T("--filename option requires a filename.\n"));
                return 1;
            }
            break;
        }
        case OPT_ID:
        {
            if (args.OptionArg() != NULL)
            {
                file_id = StringToID(std::string(args.OptionArg()));
            }
            else
            {
                _tprintf(_T("--id option requires a file id.\n"));
                return 1;
            }
            break;
        }
        case OPT_MIX:
        {
            if (args.OptionArg() != NULL) 
            {
                input_mixfile = std::string(args.OptionArg());
                std::wcout << "Operating on " << input_mixfile.c_str() << std::endl;
            }
            else
            {
                _tprintf(_T("--mix option requires a mix file.\n"));
                return 1;
            }
            break;
        }
        case OPT_DIR:
        {
            if (args.OptionArg() != NULL)
            {
                dir = std::string(args.OptionArg());
            }
            else
            {
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
            std::string gt = std::string(args.OptionArg().c_str());
            if (gt == games[0])
                game = GameKind::TD;
            else if (gt == games[1])
                game = GameKind::RA;
            else if (gt == games[2])
                game = GameKind::TS;
            else if (gt == games[3])
                game = GameKind::RA2;
            else
                std::cout << "--game is either td, ra, ts or ra2." << std::endl;

            break;
        }
        case OPT_CREATE:
        {
            if (mode != ExecutionMode::NONE) { NoMultiMode(argv); return 1; }
            mode = ExecutionMode::CREATE;
            break;
        }
        case OPT_EXTRACT:
        {
            if (mode != ExecutionMode::NONE) { NoMultiMode(argv); return 1; }
            mode = EXTRACT;
            break;
        }
        case OPT_LIST:
        {
            if (mode != ExecutionMode::NONE) { NoMultiMode(argv); return 1; }
            mode = ExecutionMode::LIST;
            break;
        }
        case OPT_INFO:
        {
            if (mode != ExecutionMode::NONE) { NoMultiMode(argv); return 1; }
            mode = ExecutionMode::INFO;
            break;
        }
        case OPT_ADD:
        {
            if (mode != ExecutionMode::NONE) { NoMultiMode(argv); return 1; }
            mode = ExecutionMode::ADD;
            break;
        }
        case OPT_REM:
        {
            if (mode != ExecutionMode::NONE) { NoMultiMode(argv); return 1; }
            mode = ExecutionMode::REMOVE;
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
    if (input_mixfile == "") {
        std::wcout << "You must specify --mix MIXFILE to operate on." << std::endl;
    }

    switch (mode) {
    case ExecutionMode::EXTRACT:
    {
        MixFile in_file(FindGMD(std::filesystem::current_path()),
            user_home_dir), game);

        if (!in_file.open(input_mixfile)) {
            std::wcout << "Cannot open specified mix file" << std::endl;
            return 1;
        }

        if (!Extraction(in_file, file, dir, file_id)) {
            return 1;
        }
        return 0;
        break;
    }
    case ExecutionMode::CREATE:
    {
        MixFile out_file(FindGMD(std::filesystem::current_path()),
            user_home_dir), game);

        if (!out_file.createMix(input_mixfile, dir, local_db,
            encrypt, checksum, FindKeySource(std::filesystem::current_path())))) {
            std::wcout << "Failed to create new mix file" << std::endl;
            return 1;
        }

        return 0;
        break;
    }
    case ExecutionMode::ADD:
    {
        MixFile in_file(FindGMD(std::filesystem::current_path()),
            user_home_dir), game);

        if (!in_file.open(input_mixfile)) {
            std::wcout << "Cannot open specified mix file" << std::endl;
            return 1;
        }

        if (file == "") {
            if (checksum) {
                in_file.addCheckSum();
            }
        }
        else {
            in_file.addFile(file);
        }

        return 0;
        break;
    }
    case ExecutionMode::REMOVE:
    {
        MixFile in_file(FindGMD(std::filesystem::current_path()),
            user_home_dir), game);

        if (!in_file.open(input_mixfile)) {
            std::wcout << "Cannot open specified mix file" << std::endl;
            return 1;
        }

        if (file == L"") {
            if (checksum) {
                in_file.removeCheckSum();
            }
        }
        else {
            in_file.removeFile(file);
        }

        return 0;
        break;
    }
    case ExecutionMode::LIST:
    {
        MixFile in_file(FindGMD(std::filesystem::current_path()), user_home_dir), game);

        if (!in_file.open(input_mixfile)) {
            std::wcout << "Cannot open specified mix file" << std::endl;
            return 1;
        }

        in_file.printFileList();
        return 0;
        break;
    }
    case ExecutionMode::INFO:
    {
        MixFile in_file(FindGMD(std::filesystem::current_path()),
            user_home_dir), game);

        if (!in_file.open(input_mixfile)) 
        {
            std::wcout << "Cannot open specified mix file" << std::endl;
            return 1;
        }

        in_file.printInfo();
        return 0;
        break;
    }
    default:
    {
        std::wcout << "command switch default, this shouldn't happen!!" << std::endl;
        return 1;
    }
    
    return 0;
}
