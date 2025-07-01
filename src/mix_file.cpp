#include "mix_file.h"
#include "win32/dirent.h"

#include <Windows.h>

#include "../cryptopp/sha.h"
#include "../cryptopp/integer.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
using CryptoPP::SHA1;

#ifdef _WIN32

#define DIR_SEPARATOR '\\'

#else

#define DIR_SEPARATOR '/'

#endif

string MixFile::BaseName(string const& pathname)
{
    string rv = pathname;
    const size_t last_slash_idx = rv.find_last_of("\\/");
    if (string::npos != last_slash_idx)
    {
        rv.erase(0, last_slash_idx + 1);
    }
    
    return rv;
}

MixFile::MixFile(const string gmd, GameKind game) : header(game), m_global_db(), m_local_db(game)
{
    fstream gmdfile;
    gmdfile.open(gmd.c_str(), fstream::in | fstream::binary);
    if(gmdfile.is_open())
    {
        m_global_db.ReadDB(gmdfile);
        gmdfile.close();
    }
    else
    {
        cout << "Could not Open global mix database.dat" << endl;
    }
}

MixFile::~MixFile()
{
    Close();
}

bool MixFile::Open(const string path)
{
    IndexInfo lmd;
    m_file_path = path;
    int32_t fsize;
    
    if (fh.is_open())
        Close();
    
    fh.open(path.c_str(), fstream::in | fstream::out | fstream::binary);
    if (!fh.is_open()) {
        cout << "File " << path << " failed to Open" << endl;
        return false;
    }
    
    fh.seekg(0, ios::end);
    fsize = fh.tellg();
    fh.seekg(0, ios::beg);
    
    if (!header.readHeader(fh)) {
        return false;
    }
    
    if (header.getBodySize() >= fsize - header.getHeaderSize()) {
        header.setBodySize(fsize - header.getHeaderSize());
    }
    
    if (header.getHasChecksum()) {
        fh.seekg(-20, ios::end);
        fh.read(reinterpret_cast<char*>(m_checksum), 20);
    }
    
    //check if we have a local mix db and if its sane-ish
    lmd = header.getEntry(MixID::GenerateID(header.getGame(), m_local_db.getDBName()));
    if (lmd.size < header.getBodySize()) {
        m_local_db.readDB(fh, lmd.offset + header.getHeaderSize(), lmd.size);
        m_has_lmd = true;
    }
    
    return true;
}

bool MixFile::checkFileName(string name) 
{
    IndexInfo rv = header.getEntry(MixID::GenerateID(header.getGame(), name));
    //size of a valid name will be > 0 hence true;
    return rv.size != 0;
}

bool MixFile::ExtractAll(string outPath) 
{
    fstream ofile;
    string fname;
    bool rv;
            
    for(t_mix_index_iter it = header.getBegin(); it != header.getEnd(); it++) {
        
        fname = m_local_db.getName(it->first);
        
        if(fname.substr(0, 4) == "[id]") {
            fname = m_global_db.GetName(header.getGame(), it->first);
        }
        
        if (it->second.size <= header.getBodySize()) {
            rv = ExtractFile(it->first, outPath + DIR_SEPARATOR + fname);
        }
    }
    
    return rv;
}

bool MixFile::ExtractFile(int32_t id, string out) 
{
    ofstream of;
    char * buffer;
    IndexInfo entry;

    // find file index entry
    entry = header.getEntry(id);
    
    if(entry.size) {
        buffer = new char[entry.size];
        fh.seekg(header.getHeaderSize() + entry.offset);
        fh.read(buffer, entry.size);

        of.open(out.c_str(), ios_base::binary);
        of.write(buffer, entry.size);

        of.close();
        delete[] buffer;

        return true;
    }
    
    return false;
}

bool MixFile::ExtractFile(string filename, string outpath) 
{
    return ExtractFile(MixID::GenerateID(header.getGame(), filename), outpath);
}

bool MixFile::CreateMix(string fileName, string in_dir, 
                        bool with_lmd, bool encrypted, bool checksum, 
                        string key_src) 
{
    
    DIR* dp;
    struct dirent *dirp;
    struct stat st;
    //IndexInfo finfo;
    //use mix_head.size uint32_t offset = 0;
    fstream ifile;
    //int32_t file_id;
    std::vector<std::string> filenames; // file names
    
    if(encrypted) header.setIsEncrypted();
    if(checksum) header.setHasChecksum();
    
    //cout << "Game we are building for is " << header.getGame() << endl;
    /*if(header.getIsEncrypted()){
        cout << "Header will be encrypted." << endl;
    }*/
    
    //make sure we can Open the directory
    if((dp = opendir(in_dir.c_str())) == NULL) {
        cout << "Error opening " << in_dir << endl;
        return false;
    }
    
    //iterate through entries in directory, ignoring directories
    while ((dirp = readdir(dp)) != NULL) {
        stat((in_dir + DIR_SEPARATOR + dirp->d_name).c_str(), &st);
        if(!S_ISDIR(st.st_mode)){
            
            //check if we have an ID containing file name, if not add to localdb
            if(!MixID::IsIDExists(string(dirp->d_name))){
                if(!m_local_db.addName(string(dirp->d_name))) {
                    cout << "Skipping " << dirp->d_name << ", ID Collision" << endl;
                    continue;
                }
            }
            
            //try adding entry to header, skip if failed
            stat((in_dir + DIR_SEPARATOR + dirp->d_name).c_str(), &st);
            
            if(!header.addEntry(MixID::GenerateID(header.getGame(), 
                                  string(dirp->d_name)), 
                                  st.st_size)) {
                continue;
            }
            
            //finally add the filename to the list of files we will add
            filenames.push_back(string(dirp->d_name));
        }
        
    }
    closedir(dp);
    
    //are we wanting lmd? if so push header info for it here
    if(with_lmd){
        filenames.push_back(m_local_db.getDBName());
        header.addEntry(MixID::GenerateID(header.getGame(), m_local_db.getDBName()), 
                          m_local_db.getSize());
    }
    
    //cout << header.getBodySize() << " files, total size " << header.getFileCount() << " before writing header" << endl;
    
    //if we are encrypted, get a key source
    /*
    if(header.getIsEncrypted()){
        ifile.Open(key_src.c_str(), ios::binary|ios::in);
        //readKeySource checks if file is actually Open
        if(!header.readKeySource(ifile)){
            cout << "Could not Open a key_source, encryption disabled" << endl;
            header.clearIsEncrypted();
        } else {
            cout << "Header will be encrypted" << endl;
        }
        ifile.Close();
    }
    */
    
    //time to start writing our new file
    fh.open(fileName.c_str(), fstream::in | fstream::out | fstream::binary | 
            fstream::trunc);
    
    if(!fh.is_open()){
        cout << "Failed to create empty file" << endl;
        return false;
    }
    
    //write a header
    if(!header.writeHeader(fh)) {
        cout << "Failed to write header." << endl;
        return false;
    }
    
    //cout << "Writing the body now" << endl;
    
    cout << "Writing " << filenames.size() << " files." << endl;
    
    //write the body, offset is set in header based on when added so should match
    for(unsigned int i = 0; i < filenames.size(); i++){
        char c;
        //last entry is lmd if with_lmd so break for special handling
        if(with_lmd && i == filenames.size() - 1) break;
        
        cout << in_dir + DIR_SEPARATOR + filenames[i] << endl;
        ifile.open((in_dir + DIR_SEPARATOR + filenames[i]).c_str(), 
                    fstream::in | fstream::binary);
        if(!ifile.is_open()) {
            cout << "Could not Open input file " << filenames[i] << endl;
            return false;
        }
        while(ifile.get(c)) {
            fh.write(&c, 1);
        }
        ifile.close();
    }
    
    //handle lmd writing here.
    if(with_lmd){
        m_local_db.writeDB(fh);
    }
    
    //just write the checksum, flag is set further up
    if(header.getHasChecksum()){
        WriteCheckSum(fh);
    }
    
    //cout << "Offset to data is " << header.getHeaderSize() << endl;
    
    fh.close();
    
    return true;
}

bool MixFile::addFile(string name)
{
    struct stat st;
    //use mix_head.size uint32_t offset = 0;
    fstream ifh;
    fstream ofh;
    IndexInfo lmd;
    IndexInfo old;
    std::vector<IndexInfo> removals;
    std::vector<std::string> filenames; // file names
    
    m_skip.clear();
    //int location;
    
    //get filename without path info
    string basename = BaseName(name);
    
    cout << "Adding " << basename << endl;
    
    //save the old data offset from header before we started changing it.
    uint32_t old_offset = header.getHeaderSize();
    uint32_t old_size = old_offset + header.getBodySize();
    
    lmd = header.getEntry(MixID::GenerateID(header.getGame(), m_local_db.getDBName()));
    old = header.getEntry(MixID::GenerateID(header.getGame(), basename)); 
    
    //add skip entry for lmd if we have one and remove from header
    if(lmd.size) {
        m_skip[lmd.offset] = lmd.size;
        header.removeEntry(MixID::GenerateID(header.getGame(), 
                             m_local_db.getDBName()), true);
    }
    
    //setup to skip over the file if it is replacing
    if(old.size) {
        m_skip[old.offset] = old.size;
        header.removeEntry(MixID::GenerateID(header.getGame(), basename), true);
        cout << "A file with the same ID exists and will be replaced." << endl;
    }
    
    //stat file to add and check its not a directory
    stat(name.c_str(), &st);
    if(!S_ISDIR(st.st_mode)){
        m_local_db.addName(basename);
        filenames.push_back(basename);
        header.addEntry(MixID::GenerateID(header.getGame(), basename), st.st_size);
    } else {
        cout << "Cannot add directory as a file" << endl;
        return false;
    }
    
    //if the lmd had a size before (thus existed), add it back to header now
    if(lmd.size) {
        header.addEntry(MixID::GenerateID(header.getGame(), m_local_db.getDBName()),
                          m_local_db.getSize());
    }
    
    //Open a temp file
    ofh.open("~ccmix.tmp", ios::binary|ios::out);
    if(!ofh.is_open()){
        cout << "Couldn't Open a temporary file to buffer the changes" << endl;
        return false;
    }
    
    //write our new header
    header.writeHeader(ofh);
    
    //copy the body of the old mix, skipping the old lmd and replaced file
    fh.seekg(old_offset, ios::beg);
    
    if(m_skip.size()){
        for(SkipMapIter it = m_skip.begin(); it != m_skip.end(); it++) {
            while(fh.tellg() < it->first + old_offset){
                ofh.put(fh.get());
            }
            fh.seekp(old_offset + it->first + it->second);
        }
    }
    
    while(fh.tellg() < old_size){
        ofh.put(fh.get());
    }
    
    //Open the file to add, if we can't Open, bail and delete temp file
    ifh.open(name.c_str(), ios::binary|ios::in);
    if(!ifh.is_open()){
        cout << "Failed to Open file to add" << endl;
        remove("~ccmix.tmp");
        return false;
    }
    
    //add new file to mix body
    while(ifh.tellg() < st.st_size){
        ofh.put(ifh.get());
    }
    
    ifh.close();
    
    //write lmd if needed
    if(lmd.size){
        m_local_db.writeDB(ofh);
    }
    
    //write checksum to end of file if required.
    if(header.getHasChecksum()){
        WriteCheckSum(ofh, 0);
    }
    
    //Replace the old file with our new one.
    OverWriteOld("~ccmix.tmp");
    
    return true;
}

bool MixFile::removeFile(std::string name)
{
    return removeFile(MixID::GenerateID(header.getGame(), BaseName(name)));
}

bool MixFile::removeFile(int32_t id)
{
    IndexInfo lmd;
    IndexInfo rem;
    fstream ofh;
    
    //empty the skip map
    m_skip.clear();
    
    //save the old data offset from header before we started changing it.
    uint32_t old_offset = header.getHeaderSize();
    uint32_t old_size = old_offset + header.getBodySize();
    
    //set up to skip copying the lmd if the mix contains one
    lmd = header.getEntry(MixID::GenerateID(header.getGame(), m_local_db.getDBName()));
    rem = header.getEntry(id);
    
    if(!rem.size) return false;
            
    //add skip entry for lmd if we have one and remove from header
    if(lmd.size) {
        m_skip[lmd.offset] = lmd.size;
        header.removeEntry(MixID::GenerateID(header.getGame(), 
                             m_local_db.getDBName()), true);
    }
    
    //add our file to the skip map and remove it
    m_skip[rem.offset] = rem.size;
    header.removeEntry(id, true);
    m_local_db.deleteName(id);
    
    //re-add our lmd entry if we had one will recalc its position
    if(lmd.size) {
        header.addEntry(MixID::GenerateID(header.getGame(), m_local_db.getDBName()),
                          m_local_db.getSize());
    }
    
    //Open a temp file
    ofh.open("~ccmix.tmp", ios::binary|ios::out);
    if(!ofh.is_open()){
        cout << "Couldn't Open a temporary file to buffer the changes" << endl;
        return false;
    }
    
    //write our new header
    header.writeHeader(ofh);
    
    //copy the body of the old mix, skipping files in m_skip
    fh.seekg(old_offset, ios::beg);
    
    for(SkipMapIter it = m_skip.begin(); it != m_skip.end(); it++) {
        while(fh.tellg() < it->first + old_offset){
            ofh.put(fh.get());
        }
        fh.seekp(old_offset + it->first + it->second);
    }
    
    while(fh.tellg() < old_size){
        ofh.put(fh.get());
    }
    
    //write lmd if needed
    if(lmd.size){
        m_local_db.writeDB(ofh);
    }
    
    //write checksum to end of file if required. will have to overwrite old one
    if(header.getHasChecksum()){
        WriteCheckSum(ofh, 0);
    }
    
    OverWriteOld("~ccmix.tmp");
    
    return true;
}

bool MixFile::addCheckSum()
{
    //check if we think this file is checksummed already
    if(header.getHasChecksum()){
        cout << "File is already flagged as having a checksum" << endl;
        return false;
    }
    
    //toggle flag for checksum and then write it
    header.setHasChecksum();
    fh.seekp(0, ios::beg);
    header.writeHeader(fh);
    
    //write the actual checksum
    WriteCheckSum(fh);
    
    return true;
}

bool MixFile::removeCheckSum()
{
    fstream ofh;
    uint32_t old_offset = header.getHeaderSize();
    uint32_t old_size = old_offset + header.getBodySize(); 
    
    //check if we think this file is checksummed already
    if(!header.getHasChecksum()){
        cout << "File is already flagged as not having a checksum" << endl;
        return false;
    }
    
    ofh.open("~ccmix.tmp", ios::binary|ios::out);
    if(!ofh.is_open()){
        cout << "Couldn't Open a temporary file to buffer the changes" << endl;
        return false;
    }
    
    //toggle flag for checksum and then write it
    header.clearHasChecksum();
    header.writeHeader(ofh);
 
    fh.seekg(old_offset, ios::beg);
    
    while(fh.tellg() < old_size){
        ofh.put(fh.get());
    }
    
    OverWriteOld("~ccmix.tmp");
    
    return true;
}

bool MixFile::WriteCheckSum(fstream &fh, int32_t pos) 
{
    SHA1 sha1;
    const size_t BufferSize = 144*7*1024;
	std::vector<uint8_t> bufvector(144*7*1024);
    uint8_t* buffer = &bufvector.at(0);
    //int blocks = mix_head.size / BufferSize;
    //int rem = mix_head.size % BufferSize;
    uint8_t hash[20];
    ofstream testout;
    
    //read data into sha1 algo from dataoffset
    fh.seekg(header.getHeaderSize(), ios::beg);
    
    while(!fh.eof()) {//#include "MixData.h"
        fh.read(reinterpret_cast<char*>(buffer), BufferSize);
        std::size_t numBytesRead = size_t(fh.gcount());
        sha1.Update(buffer, numBytesRead);
    }
    
    //clear stream
    fh.clear();
    
    // get our hash and print it to console as well
    sha1.Final(hash);
    cout << "Checksum is "
    << MixID::ToHexString(reinterpret_cast<char*>(hash), 20);
    cout << endl;
    
    //write checksum, pos is position from end to start at.
    fh.seekp(pos, ios::end);
    fh.write(reinterpret_cast<const char*>(hash), 20);
    
    //delete[] buffer;
    
    return false;
}
    
void MixFile::printFileList() 
{
    string fname;
    t_mix_index_iter it = header.getBegin();
    while(it != header.getEnd()){
        //try to get a filename, if lmd doesn't have it try gmd.
        fname = m_local_db.getName(it->first);
        if(fname.substr(0, 4) == "[id]") {
            fname = m_global_db.GetName(header.getGame(), it->first);
        }
        
        cout << setw(24) << fname << setw(10) << MixID::ToHexString(it->first) <<
                setw(12) << it->second.offset << setw(12) << 
                it->second.size << endl;
        it++;
    }
}

void MixFile::printInfo()
{
    if(header.getGame()){
        cout << "This mix is a new style mix that supports header encryption"
                " and checksums.\nRed Alert onwards can read this type of mix"
                " but the ID's used differ between Red Alert and later games.\n"
                << endl;
    } else {
        cout << "This mix is an old style mix that doesn't support header"
                " encryption or\nchecksums.\nTiberian Dawn and Sole Survivor"
                " use this format exclusively and Red Alert can\nuse them as well"
                ".\n" << endl;
    }
    cout << "It contains " << header.getFileCount() << " files"
            " which take up " << header.getBodySize() << " bytes\n" << endl;
    if(header.getIsEncrypted()){
        cout << "The mix has an encrypted header.\n\nThe blowfish key is " <<
                MixID::ToHexString(header.getKey(), 56) << "\n" << endl;
        cout << "The key was recovered from the following key source:\n" <<
                MixID::ToHexString(header.getKeySource(), 80) << "\n" <<
                endl;
    }
    if(header.getHasChecksum()){
        cout << "The mix has a SHA1 checksum:\nSHA1: " << 
                MixID::ToHexString(reinterpret_cast<char*>(m_checksum), 20) << "\n" << endl;
    }
}

bool MixFile::Decrypt()
{
    fstream ofh;
    
    //are we already decrypted?
    if (!header.getIsEncrypted()) return false;
    
    //get some info on original file and then set header to decrypted
    uint32_t dataoffset = header.getHeaderSize();
    header.clearIsEncrypted();
    
    ofh.open("~ccmix.tmp", fstream::out | fstream::binary | fstream::trunc);
    header.writeHeader(ofh);
    
    fh.seekg(dataoffset);
    
    while(!fh.eof()){
        ofh.put(fh.get());
    }
    
    ofh.close();
    
    OverWriteOld("~ccmix.tmp");

    return true;
}

bool MixFile::Encrypt()
{
    fstream ofh;
    
    //are we already encrypted?
    if (header.getIsEncrypted()) return false;
    
    //get some info on original file and then set header to decrypted
    uint32_t dataoffset = header.getHeaderSize();
    header.setIsEncrypted();
    
    ofh.open("~ccmix.tmp", fstream::out | fstream::binary | fstream::trunc);
    header.writeHeader(ofh);
    
    fh.seekg(dataoffset);
    
    while(!fh.eof()){
        ofh.put(fh.get());
    }
    
    ofh.close();
    
    //OverWriteOld("~ccmix.tmp");
    
    return true;
}

bool MixFile::OverWriteOld(std::string temp)
{
    fstream ifh;
    string newname = m_file_path;
    
    remove(m_file_path.c_str());
    rename(temp.c_str(), m_file_path.c_str());
    
    return true;
}

void MixFile::Close()
{
    fh.close();   
}
