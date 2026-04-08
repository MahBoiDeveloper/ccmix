/* 
 * File:   gmdedit.cpp
 * Author: aidan
 *
 * Created on 03 September 2014, 19:33
 */

#include "../mix_gmd.hpp"
#include <fstream>
#include <print>

typedef std::pair<std::string, std::string> NamePair;
Game game;
std::vector<NamePair> names;

namespace
{
void PrintUsage()
{
    std::println("Use: gmdedit gmdpath additionspath newgmdpath");
}
} // namespace

/*
 * 
 */
/*bool gameChoice()
{
    std::string choice = "";
    while(choice == ""){
        
        std::cout << "Which game do you want to add entries for?\n"
                  << "td|ra|ts|ra2|quit\n"
                  << ">:";
        std::cin >> choice;
        if(choice == "td"){
            game = GameTd;
            return false;
        } else if(choice == "ra") {
            game = GameRa;
            return false;
        } else if(choice == "ts") {
            game = GameTs;
            return false;
        } else if(choice == "ra2") {
            game = GameRa2;
            return false;
        } else if(choice == "quit") {
            return true;
        } else {
            choice = "";
        }
    }
}

bool AddEntry()
{
    bool another = true;
    std::string input;
    NamePair name;
    while(another){
        std::cout << "Filename >: ";
        std::cin >> input;
        name.first = tolower(input);
        std::cout << "Description >: ";
        std::cin >> input;
        name.second = tolower(input);
        names.push_back(name);
        std::cout << "Add Another?(YES|no) >: ";
        std::cin >> input;
        if(tolower(input) == "no" | tolower(input) == "n"){
            another = false;
        }
    }
}

void Menu()
{
    bool quit = false
    game = GameTd;
    
    while(!quit){
        quit = gameChoice();
    } 
}*/

int main(int argc, char **argv)
{

    MixGmd gmd;
    std::fstream ifh;
    std::fstream ofh;
    std::vector<NamePair> names;

    if (argc < 4)
    {
        //Menu();
        PrintUsage();
        return 1;
    }

    ifh.open(argv[1], std::ios_base::in | std::ios_base::binary);
    if (!ifh.is_open())
    {
        std::println("Failed to open global mix database: {}", argv[1]);
        return 1;
    }

    gmd.ReadDb(ifh);
    ifh.close();

    game = GameTd;

    ifh.open(argv[2], std::ios_base::in);
    if (!ifh.is_open())
    {
        std::println("Failed to open additions file: {}", argv[2]);
        return 1;
    }

    for (std::string line; std::getline(ifh, line);)
    {
        if (line.empty())
        {
            continue;
        }

        const std::size_t separator = line.find(',');
        NamePair entry;
        if (separator == std::string::npos)
        {
            entry.first = line;
        }
        else
        {
            entry.first = line.substr(0, separator);
            entry.second = line.substr(separator + 1);
        }

        std::println("{} - {}", entry.first, entry.second);
        if (!entry.first.empty())
        {
            names.push_back(entry);
        }
    }
    ifh.close();

    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (!gmd.AddName(game, names[i].first, names[i].second))
        {
            std::println("Failed to add entry: {}", names[i].first);
            return 1;
        }
    }

    ofh.open(argv[3], std::ios_base::out | std::ios_base::binary);
    if (!ofh.is_open())
    {
        std::println("Failed to open output file: {}", argv[3]);
        return 1;
    }

    gmd.WriteDb(ofh);
    ofh.close();

    return 0;
}
