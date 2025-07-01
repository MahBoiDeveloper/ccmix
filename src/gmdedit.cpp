/* 
 * File:   gmdedit.cpp
 * Author: aidan
 *
 * Created on 03 September 2014, 19:33
 */

#include "mix_db_gmd.h"
#include <stdio.h>
#include <iostream>

typedef std::pair<std::string, std::string> t_namepair;
GameKind game;
std::vector<t_namepair> names;

int GmeditMain(int argc, char** argv) {
    
    GlobalMixDataBase gmd;
    std::fstream ifh;
    std::fstream ofh;
    std::vector<t_namepair> names;
    
    ifh.open(argv[1], std::ios_base::in|std::ios_base::binary);
    gmd.ReadDB(ifh);
    ifh.close();
    
    if(argc < 4){
        //menu();
        std::wcout << "Use: gmdedit gmdpath additionspath newgmdpath\n";
        return 1;
    } else {
        game = GameKind::TD;
        
        ifh.open(argv[2], std::ios_base::in);

        while(!ifh.eof()){
            t_namepair entry;
            std::getline(ifh, entry.first, ',');
            std::getline(ifh, entry.second);
            std::cout << entry.first << " - " << entry.second << "\n";
            if(entry.first != ""){
                names.push_back(entry);
            }
        }
    }

    for(unsigned int i = 0; i < names.size(); i++) {
        gmd.AddName(game, names[i].first, names[i].second);
    }
    
    ofh.open(argv[3], std::ios_base::out|std::ios_base::binary);
    gmd.WriteDB(ofh);
    
    return 0;
}
