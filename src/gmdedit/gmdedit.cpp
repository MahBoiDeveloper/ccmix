/* 
 * File:   gmdedit.cpp
 * Author: aidan
 *
 * Created on 03 September 2014, 19:33
 */

#include "../mix_db_gmd.hpp"
#include <stdio.h>
#include <print>

typedef std::pair<std::string, std::string> NamePair;
Game game;
std::vector<NamePair> names;

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

int main(int argc, char** argv) {
    
    MixGmd gmd;
    std::fstream ifh;
    std::fstream ofh;
    std::vector<NamePair> names;
    
    ifh.open(argv[1], std::ios_base::in|std::ios_base::binary);
    gmd.ReadDb(ifh);
    ifh.close();
    
    if(argc < 4){
        //Menu();
        std::print("Use: gmdedit gmdpath additionspath newgmdpath\n");
        return 1;
    } else {
        game = GameTd;
        
        ifh.open(argv[2], std::ios_base::in);

        while(!ifh.eof()){
            NamePair entry;
            std::getline(ifh, entry.first, ',');
            std::getline(ifh, entry.second);
            std::println("{} - {}", entry.first, entry.second);
            if(entry.first != ""){
                names.push_back(entry);
            }
        }
    }

    for(unsigned int i = 0; i < names.size(); i++) {
        gmd.AddName(game, names[i].first, names[i].second);
    }
    
    ofh.open(argv[3], std::ios_base::out|std::ios_base::binary);
    gmd.WriteDb(ofh);
    
    return 0;
}


