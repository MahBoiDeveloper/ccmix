#pragma once
#include "mix_db_gmd.h"
#include "mix_db_lmd.h"
#include "mix_header.h"
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <stdint.h>

/**
 * @brief Mix Databases.
 * 
 * Global Mix Database holds mappings between file id's and their names as well
 * as additional details about the file such as what it represents in game. The 
 * file consists of several sections that contain a little endian uint32_t count
 * of how many entries are in the DB followed by 0 separated strings of 
 * alternating filenames and descriptions. The ID's themselves are generated on
 * loading database.
 * 
 * Local Mix Databases hold just filenames but have a more complicated header.
 * It consists first of a 32 byte string, the xcc_id which appears to be a kind
 * of digital signature for the XCC tools author.
 */

/// @brief .mix file controller.
/// 
/// Some parts of code and code are taken from XCC mix file specification.
/// @sa TS mix file format specification (http://xhp.xwis.net/documents/MIX_Format.html)
class MixFile
{
protected: // Data
    typedef std::map<uint32_t, uint32_t> SkipMap;
    typedef std::pair<uint32_t, uint32_t> SkipEntry;
    typedef std::map<uint32_t, uint32_t>::const_iterator SkipMapIter;

    MixHeader header; // mix file header
    GlobalMixDataBase m_global_db;
    MixLMD m_local_db;
    SkipMap m_skip;
    bool m_has_lmd;
    std::string m_file_path;
    std::fstream fh; // file handler
    uint8_t m_checksum[20];
public:
    inline static const std::wstring GMD_FILENAME = L"global mix database.dat";

protected: // Methods
    bool WriteCheckSum(std::fstream& fh, int32_t pos = 0);
    std::string BaseName(std::string const& pathname);
    bool Decrypt();
    bool Encrypt();
    bool OverWriteOld(std::string temp);
public:
    MixFile(const std::wstring gmd = GMD_FILENAME, GameKind openGame = GameKind::RA2);
    virtual ~MixFile();
    
    /// @brief Open mix archive
    /// @param path mix file path
    /// @retval true file opened
    /// @retval false file not found
    bool Open(const std::string path);
    
    /// @brief Closes mix file.
    /// Prepare for opening another file.
    void Close();

    /// @brief extract file from mix archive
    /// @param fileID CRC ID of file
    /// @param outPath extracted file path
    /// @retval true file extracted
    /// @retval false file not present in the archive 
    bool ExtractFile(int32_t fileID, std::string outPath);

    /// @brief extract file from mix archive
    /// @param fileName name of file
    /// @param outPath extracted file path
    /// @retval true file extracted
    /// @retval false file not present in the archive 
    bool ExtractFile(std::string fileName, std::string outPath);
     
    /// @brief extract all files from the archive
    /// @param outPath output directory
    /// @param withFileNames try to get file names of the content
    /// @return true if extraction successful
    bool ExtractAll(std::string outPath = ".");
     
    /// @brief Creates a new mix file
    /// @param fileName name and path of mix to create
    /// @param infiles vector containing files to add to the new mix
    /// @param game game the mix should be compatible with
    /// @param in_dir location we should create the new mix at
    /// @param with_lmd should we generate a local mix database for this mix
    /// @param encrypted should we Encrypt the header of this mix
    /// @param checksum should we generate a checksum for this mix
    /// @param key_src string of the path to a key_source to use in encryption
    /// @return true if creation is successful
    bool CreateMix(std::string fileName, std::string in_dir, bool with_lmd = false, 
                   bool encrypted = false, bool checksum = false, 
                   std::string key_src = "");
    /**
     * @brief adds a sha1 checksum to the end of the file and flags it in the
     *        header.
     * @return true if successful
     */
    bool addCheckSum();
    /**
     * @brief removes the sha1 checksum to the end of the file and flags it in the
     *        header.
     * @return true if successful
     */
    bool removeCheckSum();
    /**
     * @brief checks, if file is present in the archive and adds if not
     * @param name file name
     * @return true if successful
     */
    bool addFile(std::string name);
     /**
     * @brief checks, if file is present in the archive and removes if so
     * @param name file name
     * @return true if successful
     */
    bool removeFile(std::string name);
     /**
     * @brief checks, if file is present in the archive and removes if so
     * @param name file name
     * @return true if successful
     */
    bool removeFile(int32_t id);
    /**
     * @brief checks, if file is present in the archive
     * @param id id of filename
     * @return true if present
     */
    bool checkFileName(std::string name);
    /**
     * @brief mix archive header
     * 
     * Prints header in following format:
     * file CRC (hex) || file offset (dec) || file size (dec)
     * @param flags print settings
     * @return file text list
     */
    void printFileList();
    /**
     * @brief mix archive header
     * 
     * Prints information about the mix file:
     * file CRC (hex) || file offset (dec) || file size (dec)
     */
    void printInfo();
    /**
     * @brief save file in decrypted format
     * @param outPath output filename
     * @return true if successful
     */
};
