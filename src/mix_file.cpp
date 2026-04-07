#include "mix_file.hpp"

#ifdef _MSC_VER

#include <Windows.h>

#endif

#include "cryptopp/sha.h"
#include "cryptopp/integer.h"
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <algorithm>

#ifdef _MSC_VER
#include "win32/dirent.hpp"
#else
#include <dirent.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

using CryptoPP::SHA1;

#ifdef _WIN32

#define DIR_SEPARATOR '\\'

#else

#define DIR_SEPARATOR '/'

#endif

namespace
{
    const char* const TEMP_FILE_PATH = "~ccmix.tmp";
    const std::streamsize COPY_BUFFER_SIZE = 32 * 1024;

    class DirectoryHandle
    {
    public:
        explicit DirectoryHandle(DIR* handle) : m_handle(handle) {}
        ~DirectoryHandle()
        {
            if (m_handle != NULL) {
                closedir(m_handle);
            }
        }

        bool isOpen() const
        {
            return m_handle != NULL;
        }

        DIR* get() const
        {
            return m_handle;
        }

    private:
        DirectoryHandle(const DirectoryHandle&);
        DirectoryHandle& operator=(const DirectoryHandle&);

        DIR* m_handle;
    };

    std::string joinPath(const std::string& directory, const std::string& name)
    {
        if (directory.empty()) {
            return name;
        }

        if (directory[directory.size() - 1] == DIR_SEPARATOR) {
            return directory + name;
        }

        return directory + DIR_SEPARATOR + name;
    }

    bool readFileStat(const std::string& path, struct stat& st)
    {
        return stat(path.c_str(), &st) == 0;
    }

    bool isRegularFile(const std::string& path, struct stat& st)
    {
        return readFileStat(path, st) && !S_ISDIR(st.st_mode);
    }

    std::streamoff getStreamSize(std::istream& stream)
    {
        stream.clear();
        stream.seekg(0, std::ios::end);
        const std::streamoff size = stream.tellg();
        stream.seekg(0, std::ios::beg);
        return size;
    }

    bool copyExact(std::istream& input, std::ostream& output, std::streamoff size)
    {
        if (size <= 0) {
            return true;
        }

        std::vector<char> buffer(COPY_BUFFER_SIZE);
        std::streamoff remaining = size;

        while (remaining > 0) {
            const std::streamsize chunk = static_cast<std::streamsize>(
                    std::min<std::streamoff>(remaining, COPY_BUFFER_SIZE));
            input.read(&buffer.at(0), chunk);
            const std::streamsize read = input.gcount();

            if (read <= 0) {
                return false;
            }

            output.write(&buffer.at(0), read);
            if (!output) {
                return false;
            }

            remaining -= read;
        }

        return true;
    }

    bool copyToEnd(std::istream& input, std::ostream& output)
    {
        std::vector<char> buffer(COPY_BUFFER_SIZE);

        while (input.good()) {
            input.read(&buffer.at(0), COPY_BUFFER_SIZE);
            const std::streamsize read = input.gcount();
            if (read > 0) {
                output.write(&buffer.at(0), read);
                if (!output) {
                    return false;
                }
            }
        }

        const bool reached_eof = input.eof();
        input.clear();
        return reached_eof && output.good();
    }

    bool copyBodyWithSkips(std::istream& input, std::ostream& output,
            std::streamoff body_offset, std::streamoff body_end,
            const std::map<uint32_t, uint32_t>& skips)
    {
        input.clear();
        input.seekg(body_offset, std::ios::beg);
        if (!input) {
            return false;
        }

        std::streamoff current = body_offset;
        for (std::map<uint32_t, uint32_t>::const_iterator it = skips.begin();
             it != skips.end(); ++it) {
            const std::streamoff skip_begin = body_offset + it->first;
            if (skip_begin > current && !copyExact(input, output, skip_begin - current)) {
                return false;
            }

            current = skip_begin + it->second;
            input.clear();
            input.seekg(current, std::ios::beg);
            if (!input) {
                return false;
            }
        }

        if (body_end > current) {
            return copyExact(input, output, body_end - current);
        }

        return true;
    }

    void removeTempFile()
    {
        std::remove(TEMP_FILE_PATH);
    }
}

std::string MixFile::baseName(const std::string& pathname) const
{
    std::string rv = pathname;
    const std::string::size_type last_slash_idx = rv.find_last_of("\\/");
    if (std::string::npos != last_slash_idx)
    {
        rv.erase(0, last_slash_idx + 1);
    }
    
    return rv;
}

MixFile::MixFile(const std::string& gmd, t_game game) :
m_header(game),
m_global_db(),
m_local_db(game),
m_has_lmd(false),
m_file_path()
{
    std::fill(m_checksum, m_checksum + 20, 0);

    std::fstream gmdfile;
    gmdfile.open(gmd.c_str(), std::fstream::in | std::fstream::binary);
    if(gmdfile.is_open()){
        m_global_db.readDB(gmdfile);
        gmdfile.close();
    } else {
        std::cout << "Could not open global mix database.dat" << std::endl;
    }
}

MixFile::~MixFile()
{
    close();
}

bool MixFile::open(const std::string& path)
{
    t_index_info lmd;
    m_file_path = path;
    m_has_lmd = false;
    std::fill(m_checksum, m_checksum + 20, 0);
    
    if (fh.is_open()) {
        close();
    }
    
    fh.open(path.c_str(), std::fstream::in | std::fstream::out | std::fstream::binary);
    if (!fh.is_open()) {
        std::cout << "File " << path << " failed to open" << std::endl;
        return false;
    }
    
    const std::streamoff fsize = getStreamSize(fh);
    
    if (!m_header.readHeader(fh)) {
        close();
        return false;
    }
    
    if (fsize < static_cast<std::streamoff>(m_header.getHeaderSize())) {
        close();
        return false;
    }

    const std::streamoff available_body = fsize - m_header.getHeaderSize();
    if (m_header.getBodySize() >= available_body) {
        m_header.setBodySize(static_cast<uint32_t>(available_body));
    }
    
    if (m_header.getHasChecksum()) {
        fh.seekg(-20, std::ios::end);
        fh.read(reinterpret_cast<char*>(m_checksum), 20);
    }
    
    //check if we have a local mix db and if its sane-ish
    lmd = m_header.getEntry(MixID::idGen(m_header.getGame(), m_local_db.getDBName()));
    if (lmd.size < m_header.getBodySize()) {
        m_local_db.readDB(fh, lmd.offset + m_header.getHeaderSize(), lmd.size);
        m_has_lmd = true;
    }
    
    return true;
}

bool MixFile::checkFileName(const std::string& name) const
{
    t_index_info rv = m_header.getEntry(MixID::idGen(m_header.getGame(), name));
    //size of a valid name will be > 0 hence true;
    return rv.size != 0;
}

bool MixFile::extractAll(const std::string& outPath) 
{
    std::string fname;
    bool rv = true;
            
    for(t_mix_index_iter it = m_header.getBegin(); it != m_header.getEnd(); it++) {
        
        fname = m_local_db.getName(it->first);
        
        if(fname.substr(0, 4) == "[id]") {
            fname = m_global_db.getName(m_header.getGame(), it->first);
        }
        
        if (it->second.size <= m_header.getBodySize()) {
            rv = extractFile(it->first, joinPath(outPath, fname)) && rv;
        }
    }
    
    return rv;
}

bool MixFile::extractFile(int32_t id, const std::string& out) 
{
    t_index_info entry;

    // find file index entry
    entry = m_header.getEntry(id);
    
    if(entry.size) {
        std::vector<char> buffer(entry.size);
        fh.clear();
        fh.seekg(m_header.getHeaderSize() + entry.offset);
        fh.read(&buffer.at(0), entry.size);
        if (static_cast<uint32_t>(fh.gcount()) != entry.size) {
            fh.clear();
            return false;
        }

        std::ofstream of(out.c_str(), std::ios_base::binary);
        if (!of.is_open()) {
            return false;
        }
        of.write(&buffer.at(0), entry.size);

        fh.clear();

        return of.good();
    }
    
    return false;
}

bool MixFile::extractFile(const std::string& filename, const std::string& outpath) 
{
    return extractFile(MixID::idGen(m_header.getGame(), filename), outpath);
}

bool MixFile::createMix(const std::string& fileName, const std::string& in_dir, 
                        bool with_lmd, bool encrypted, bool checksum, 
                        const std::string& key_src) 
{
    (void)key_src;

    struct stat st;
    std::vector<std::string> filenames; // file names
    
    if(encrypted) m_header.setIsEncrypted();
    if(checksum) m_header.setHasChecksum();
    
    //cout << "Game we are building for is " << m_header.getGame() << endl;
    /*if(m_header.getIsEncrypted()){
        cout << "Header will be encrypted." << endl;
    }*/
    
    //make sure we can open the directory
    DirectoryHandle directory(opendir(in_dir.c_str()));
    if(!directory.isOpen()) {
        std::cout << "Error opening " << in_dir << std::endl;
        return false;
    }
    
    //iterate through entries in directory, ignoring directories
    for (struct dirent *dirp = readdir(directory.get()); dirp != NULL;
         dirp = readdir(directory.get())) {
        const std::string filename(dirp->d_name);
        const std::string file_path = joinPath(in_dir, filename);
        if(isRegularFile(file_path, st)){
            
            //check if we have an ID containing file name, if not add to localdb
            if(!MixID::isIdName(filename)){
                if(!m_local_db.addName(filename)) {
                    std::cout << "Skipping " << filename << ", ID Collision" << std::endl;
                    continue;
                }
            }
            
            //try adding entry to header, skip if failed
            if(!m_header.addEntry(MixID::idGen(m_header.getGame(), 
                                  filename), 
                                  st.st_size)) {
                continue;
            }
            
            //finally add the filename to the list of files we will add
            filenames.push_back(filename);
        }
        
    }
    
    //are we wanting lmd? if so push header info for it here
    if(with_lmd){
        filenames.push_back(m_local_db.getDBName());
        m_header.addEntry(MixID::idGen(m_header.getGame(), m_local_db.getDBName()), 
                          m_local_db.getSize());
    }
    
    //cout << m_header.getBodySize() << " files, total size " << m_header.getFileCount() << " before writing header" << endl;
    
    //if we are encrypted, get a key source
    /*
    if(m_header.getIsEncrypted()){
        ifile.open(key_src.c_str(), ios::binary|ios::in);
        //readKeySource checks if file is actually open
        if(!m_header.readKeySource(ifile)){
            cout << "Could not open a key_source, encryption disabled" << endl;
            m_header.clearIsEncrypted();
        } else {
            cout << "Header will be encrypted" << endl;
        }
        ifile.close();
    }
    */
    
    //time to start writing our new file
    fh.open(fileName.c_str(), std::fstream::in | std::fstream::out |
            std::fstream::binary | std::fstream::trunc);
    
    if(!fh.is_open()){
        std::cout << "Failed to create empty file" << std::endl;
        return false;
    }
    
    //write a header
    if(!m_header.writeHeader(fh)) {
        std::cout << "Failed to write header." << std::endl;
        return false;
    }
    
    //cout << "Writing the body now" << endl;
    
    std::cout << "Writing " << filenames.size() << " files." << std::endl;
    
    //write the body, offset is set in header based on when added so should match
    for(unsigned int i = 0; i < filenames.size(); i++){
        //last entry is lmd if with_lmd so break for special handling
        if(with_lmd && i == filenames.size() - 1) break;
        
        const std::string input_path = joinPath(in_dir, filenames[i]);
        std::cout << input_path << std::endl;
        std::fstream ifile(input_path.c_str(), std::fstream::in | std::fstream::binary);
        if(!ifile.is_open()) {
            std::cout << "Could not open input file " << filenames[i] << std::endl;
            return false;
        }
        if (!copyToEnd(ifile, fh)) {
            return false;
        }
    }
    
    //handle lmd writing here.
    if(with_lmd){
        m_local_db.writeDB(fh);
    }
    
    //just write the checksum, flag is set further up
    if(m_header.getHasChecksum()){
        if (!writeCheckSum(fh)) {
            fh.close();
            return false;
        }
    }
    
    //cout << "Offset to data is " << m_header.getHeaderSize() << endl;
    
    fh.close();
    
    return true;
}

bool MixFile::addFile(const std::string& name)
{
    struct stat st;
    std::fstream ofh;
    t_index_info lmd;
    t_index_info old;
    
    m_skip.clear();
    
    //get filename without path info
    const std::string basename = baseName(name);
    
    std::cout << "Adding " << basename << std::endl;
    
    //save the old data offset from header before we started changing it.
    const std::streamoff old_offset = m_header.getHeaderSize();
    const std::streamoff old_size = old_offset + m_header.getBodySize();
    
    lmd = m_header.getEntry(MixID::idGen(m_header.getGame(), m_local_db.getDBName()));
    old = m_header.getEntry(MixID::idGen(m_header.getGame(), basename)); 
    
    //add skip entry for lmd if we have one and remove from header
    if(lmd.size) {
        m_skip[lmd.offset] = lmd.size;
        m_header.removeEntry(MixID::idGen(m_header.getGame(), 
                             m_local_db.getDBName()), true);
    }
    
    //setup to skip over the file if it is replacing
    if(old.size) {
        m_skip[old.offset] = old.size;
        m_header.removeEntry(MixID::idGen(m_header.getGame(), basename), true);
        std::cout << "A file with the same ID exists and will be replaced." << std::endl;
    }
    
    //stat file to add and check its not a directory
    if(!isRegularFile(name, st)){
        std::cout << "Cannot add directory as a file" << std::endl;
        return false;
    }

    if (!old.size && !MixID::isIdName(basename) && !m_local_db.addName(basename)) {
        return false;
    }
    m_header.addEntry(MixID::idGen(m_header.getGame(), basename), st.st_size);
    
    //if the lmd had a size before (thus existed), add it back to header now
    if(lmd.size) {
        m_header.addEntry(MixID::idGen(m_header.getGame(), m_local_db.getDBName()),
                          m_local_db.getSize());
    }
    
    //open a temp file
    ofh.open(TEMP_FILE_PATH, std::ios::binary | std::ios::out | std::ios::trunc);
    if(!ofh.is_open()){
        std::cout << "Couldn't open a temporary file to buffer the changes" << std::endl;
        return false;
    }
    
    //write our new header
    if (!m_header.writeHeader(ofh)) {
        return false;
    }
    
    //copy the body of the old mix, skipping the old lmd and replaced file
    if (!copyBodyWithSkips(fh, ofh, old_offset, old_size, m_skip)) {
        ofh.close();
        removeTempFile();
        return false;
    }
    
    //open the file to add, if we can't open, bail and delete temp file
    std::fstream ifh(name.c_str(), std::ios::binary | std::ios::in);
    if(!ifh.is_open()){
        std::cout << "Failed to open file to add" << std::endl;
        ofh.close();
        removeTempFile();
        return false;
    }
    
    //add new file to mix body
    if (!copyExact(ifh, ofh, st.st_size)) {
        ifh.close();
        ofh.close();
        removeTempFile();
        return false;
    }
    
    ifh.close();
    
    //write lmd if needed
    if(lmd.size){
        m_local_db.writeDB(ofh);
    }
    
    //write checksum to end of file if required.
    if(m_header.getHasChecksum()){
        if (!writeCheckSum(ofh, 0)) {
            ofh.close();
            removeTempFile();
            return false;
        }
    }
    
    //Replace the old file with our new one.
    ofh.close();
    return overWriteOld(TEMP_FILE_PATH);
    
}

bool MixFile::removeFile(const std::string& name)
{
    return removeFile(MixID::idGen(m_header.getGame(), baseName(name)));
}

bool MixFile::removeFile(int32_t id)
{
    t_index_info lmd;
    t_index_info rem;
    std::fstream ofh;
    
    //empty the skip map
    m_skip.clear();
    
    //save the old data offset from header before we started changing it.
    const std::streamoff old_offset = m_header.getHeaderSize();
    const std::streamoff old_size = old_offset + m_header.getBodySize();
    
    //set up to skip copying the lmd if the mix contains one
    lmd = m_header.getEntry(MixID::idGen(m_header.getGame(), m_local_db.getDBName()));
    rem = m_header.getEntry(id);
    
    if(!rem.size) return false;
            
    //add skip entry for lmd if we have one and remove from header
    if(lmd.size) {
        m_skip[lmd.offset] = lmd.size;
        m_header.removeEntry(MixID::idGen(m_header.getGame(), 
                             m_local_db.getDBName()), true);
    }
    
    //add our file to the skip map and remove it
    m_skip[rem.offset] = rem.size;
    m_header.removeEntry(id, true);
    m_local_db.deleteName(id);
    
    //re-add our lmd entry if we had one will recalc its position
    if(lmd.size) {
        m_header.addEntry(MixID::idGen(m_header.getGame(), m_local_db.getDBName()),
                          m_local_db.getSize());
    }
    
    //open a temp file
    ofh.open(TEMP_FILE_PATH, std::ios::binary | std::ios::out | std::ios::trunc);
    if(!ofh.is_open()){
        std::cout << "Couldn't open a temporary file to buffer the changes" << std::endl;
        return false;
    }
    
    //write our new header
    if (!m_header.writeHeader(ofh)) {
        return false;
    }
    
    //copy the body of the old mix, skipping files in m_skip
    if (!copyBodyWithSkips(fh, ofh, old_offset, old_size, m_skip)) {
        ofh.close();
        removeTempFile();
        return false;
    }
    
    //write lmd if needed
    if(lmd.size){
        m_local_db.writeDB(ofh);
    }
    
    //write checksum to end of file if required. will have to overwrite old one
    if(m_header.getHasChecksum()){
        if (!writeCheckSum(ofh, 0)) {
            ofh.close();
            removeTempFile();
            return false;
        }
    }
    
    ofh.close();
    return overWriteOld(TEMP_FILE_PATH);
}

bool MixFile::addCheckSum()
{
    //check if we think this file is checksummed already
    if(m_header.getHasChecksum()){
        std::cout << "File is already flagged as having a checksum" << std::endl;
        return false;
    }
    
    //toggle flag for checksum and then write it
    m_header.setHasChecksum();
    fh.seekp(0, std::ios::beg);
    m_header.writeHeader(fh);
    
    //write the actual checksum
    return writeCheckSum(fh);
    
}

bool MixFile::removeCheckSum()
{
    std::fstream ofh;
    const std::streamoff old_offset = m_header.getHeaderSize();
    const std::streamoff old_size = old_offset + m_header.getBodySize(); 
    
    //check if we think this file is checksummed already
    if(!m_header.getHasChecksum()){
        std::cout << "File is already flagged as not having a checksum" << std::endl;
        return false;
    }
    
    ofh.open(TEMP_FILE_PATH, std::ios::binary | std::ios::out | std::ios::trunc);
    if(!ofh.is_open()){
        std::cout << "Couldn't open a temporary file to buffer the changes" << std::endl;
        return false;
    }
    
    //toggle flag for checksum and then write it
    m_header.clearHasChecksum();
    m_header.writeHeader(ofh);
 
    fh.clear();
    fh.seekg(old_offset, std::ios::beg);
    if (!copyExact(fh, ofh, old_size - old_offset)) {
        ofh.close();
        removeTempFile();
        return false;
    }
    
    ofh.close();
    return overWriteOld(TEMP_FILE_PATH);
}

bool MixFile::writeCheckSum(std::fstream &fh, int32_t pos) 
{
    SHA1 sha1;
    const std::size_t BufferSize = 144 * 7 * 1024;
    std::vector<uint8_t> buffer(BufferSize);
    uint8_t hash[20];
    
    //read data into sha1 algo from dataoffset
    fh.clear();
    fh.seekg(m_header.getHeaderSize(), std::ios::beg);
    
    while(fh.good()) {
        fh.read(reinterpret_cast<char*>(&buffer.at(0)),
                static_cast<std::streamsize>(BufferSize));
        const std::streamsize numBytesRead = fh.gcount();
        if (numBytesRead > 0) {
            sha1.Update(&buffer.at(0), static_cast<unsigned int>(numBytesRead));
        }
    }
    
    //clear stream
    fh.clear();
    
    // get our hash and print it to console as well
    sha1.Final(hash);
    std::copy(hash, hash + 20, m_checksum);
    std::cout << "Checksum is "
    << MixID::idStr(reinterpret_cast<const char*>(hash), 20);
    std::cout << std::endl;
    
    //write checksum, pos is position from end to start at.
    fh.seekp(pos, std::ios::end);
    fh.write(reinterpret_cast<const char*>(hash), 20);

    return fh.good();
}
    
void MixFile::printFileList() 
{
    std::string fname;
    t_mix_index_iter it = m_header.getBegin();
    while(it != m_header.getEnd()){
        //try to get a filename, if lmd doesn't have it try gmd.
        fname = m_local_db.getName(it->first);
        if(fname.substr(0, 4) == "[id]") {
            fname = m_global_db.getName(m_header.getGame(), it->first);
        }
        
        std::cout << std::setw(24) << fname << std::setw(10) << MixID::idStr(it->first) <<
                std::setw(12) << it->second.offset << std::setw(12) << 
                it->second.size << std::endl;
        it++;
    }
}

void MixFile::printInfo()
{
    if(m_header.getGame()){
        std::cout << "This mix is a new style mix that supports header encryption"
                " and checksums.\nRed Alert onwards can read this type of mix"
                " but the ID's used differ between Red Alert and later games.\n"
                << std::endl;
    } else {
        std::cout << "This mix is an old style mix that doesn't support header"
                " encryption or\nchecksums.\nTiberian Dawn and Sole Survivor"
                " use this format exclusively and Red Alert can\nuse them as well"
                ".\n" << std::endl;
    }
    std::cout << "It contains " << m_header.getFileCount() << " files"
            " which take up " << m_header.getBodySize() << " bytes\n" << std::endl;
    if(m_header.getIsEncrypted()){
        std::cout << "The mix has an encrypted header.\n\nThe blowfish key is " <<
                MixID::idStr(m_header.getKey(), 56) << "\n" << std::endl;
        std::cout << "The key was recovered from the following key source:\n" <<
                MixID::idStr(m_header.getKeySource(), 80) << "\n" <<
                std::endl;
    }
    if(m_header.getHasChecksum()){
        std::cout << "The mix has a SHA1 checksum:\nSHA1: " << 
                MixID::idStr(reinterpret_cast<const char*>(m_checksum), 20) << "\n" << std::endl;
    }
}

bool MixFile::decrypt()
{
    std::fstream ofh;
    
    //are we already decrypted?
    if (!m_header.getIsEncrypted()) return false;
    
    //get some info on original file and then set header to decrypted
    uint32_t dataoffset = m_header.getHeaderSize();
    m_header.clearIsEncrypted();
    
    ofh.open(TEMP_FILE_PATH, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if (!m_header.writeHeader(ofh)) {
        return false;
    }
    
    fh.seekg(dataoffset);
    
    if (!copyToEnd(fh, ofh)) {
        ofh.close();
        removeTempFile();
        return false;
    }
    
    ofh.close();
    
    return overWriteOld(TEMP_FILE_PATH);
}

bool MixFile::encrypt()
{
    std::fstream ofh;
    
    //are we already encrypted?
    if (m_header.getIsEncrypted()) return false;
    
    //get some info on original file and then set header to decrypted
    uint32_t dataoffset = m_header.getHeaderSize();
    m_header.setIsEncrypted();
    
    ofh.open(TEMP_FILE_PATH, std::fstream::out | std::fstream::binary | std::fstream::trunc);
    if (!m_header.writeHeader(ofh)) {
        return false;
    }
    
    fh.seekg(dataoffset);
    
    if (!copyToEnd(fh, ofh)) {
        ofh.close();
        removeTempFile();
        return false;
    }
    
    ofh.close();
    
    return overWriteOld(TEMP_FILE_PATH);
}

bool MixFile::overWriteOld(const std::string& temp)
{
    close();

    if (std::remove(m_file_path.c_str()) != 0) {
        std::cout << "Failed to remove the original mix file" << std::endl;
        return false;
    }

    if (std::rename(temp.c_str(), m_file_path.c_str()) != 0) {
        std::cout << "Failed to rename the temporary mix file" << std::endl;
        return false;
    }
    
    return open(m_file_path);
}

void MixFile::close()
{
    if (fh.is_open()) {
        fh.close();
    }
    fh.clear();
    m_has_lmd = false;
}
