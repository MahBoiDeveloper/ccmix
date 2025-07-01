#include "mix_db_gmd.h"
#include <iostream>

GlobalMixDataBase::GlobalMixDataBase() :
TDList(TD),
RAList(RA),
TSList(TS),
RA2List(RA2)
{
    vDataBase.push_back(&TDList);
    vDataBase.push_back(&RAList);
    vDataBase.push_back(&TSList);
    vDataBase.push_back(&RA2List);
}

void GlobalMixDataBase::ReadDB(std::fstream &fh)
{
    uint32_t begin, end, size, offset;
    
    // get file size
    fh.seekg(0, std::ios::beg);
    begin = fh.tellg();
    fh.seekg(0, std::ios::end);
    end = fh.tellg();
    size = end - begin;
    offset = 0;
    
    //read file into data buffer
    //char data[size];
	std::vector<char> data(size);

    fh.seekg(0, std::ios::beg);
    fh.read(&data.at(0), size);
    
    // read file from buffer into respective dbs
    for (uint32_t i = 0; i < vDataBase.size(); i++){
        vDataBase[i]->readDB(&data.at(0), offset);
        offset += vDataBase[i]->getSize();
    }
}

void GlobalMixDataBase::WriteDB(std::fstream& fh)
{
    for (unsigned int i = 0; i < vDataBase.size(); i++){
        if (!vDataBase[i]) continue;
        vDataBase[i]->writeDB(fh);
    }
}

std::string GlobalMixDataBase::GetName(GameKind game, int32_t id)
{
    switch(game){
        case TD:
            return TDList.getName(id);
        case RA:
            return RAList.getName(id);
        case TS:
            return TSList.getName(id);
        case RA2:
            return RA2List.getName(id);
        default:
            return "";
    }
}

bool GlobalMixDataBase::AddName(GameKind game, std::string name, std::string desc = "")
{
    switch(game){
        case TD:
            return TDList.addName(name, desc);
        case RA:
            return RAList.addName(name, desc);
        case TS:
            return TSList.addName(name, desc);
        case RA2:
            return RA2List.addName(name, desc);
        default:
            return false;
    }
}

bool GlobalMixDataBase::DeleteName(GameKind game, std::string name)
{
    switch(game){
        case TD:
            return TDList.deleteName(name);
        case RA:
            return RAList.deleteName(name);
        case TS:
            return TSList.deleteName(name);
        case RA2:
            return RA2List.deleteName(name);
        default:
            return false;
    }
}
