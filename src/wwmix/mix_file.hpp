#pragma once
#include "mix_db_gmd.hpp"
#include "mix_db_lmd.hpp"
#include "mix_header.hpp"
#include <cstdint>
#include <string>
#include <fstream>
#include <vector>
#include <map>

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
 * 
 * 
 */

/**
 * @brief mix file controller
 * 
 * Some parts of code and code are taken from XCC mix file specification.
 * @sa TS mix file format specification (http://xhp.xwis.net/documents/MIX_Format.html)
 */
class MixFile
{
  public:
    /// @brief Construct a mix-file controller with a global database and default game.
    MixFile(const std::string &gmd = "global mix database.dat",
            Game openGame = GameTd);

    /// @brief Close any open archive state on destruction.
    virtual ~MixFile();
    /// @brief Open a mix archive.
    /// @param path Mix file path.
    /// @retval true The file was opened.
    /// @retval false The file was not found.
    bool Open(const std::string &path);
    /// @brief Extract a file from the mix archive by CRC ID.
    /// @param fileID CRC ID of the file.
    /// @param outPath Extracted file path.
    /// @retval true The file was extracted.
    /// @retval false The file is not present in the archive.
    bool ExtractFile(int32_t fileID, const std::string &outPath);
    /// @brief Extract a file from the mix archive by file name.
    /// @param fileName Name of the file.
    /// @param outPath Extracted file path.
    /// @retval true The file was extracted.
    /// @retval false The file is not present in the archive.
    bool ExtractFile(const std::string &fileName, const std::string &outPath);
    /// @brief Extract all files from the archive.
    /// @param outPath Output directory.
    /// @return `true` if extraction was successful.
    bool ExtractAll(const std::string &outPath = ".");
    /// @brief Create a new mix file.
    /// @param fileName Name and path of the mix to create.
    /// @param in_dir Directory whose contents should be packed into the mix.
    /// @param with_lmd Generate a local mix database for the mix.
    /// @param encrypted Encrypt the mix header.
    /// @param checksum Generate a checksum for the mix.
    /// @param key_src Path to a key source to use for encryption.
    /// @return `true` if creation was successful.
    bool CreateMix(const std::string &fileName, const std::string &in_dir, bool with_lmd = false,
                   bool encrypted = false, bool checksum = false,
                   const std::string &key_src = "");
    /// @brief Add a SHA1 checksum to the end of the file and flag it in the header.
    /// @return `true` if the checksum was added successfully.
    bool AddChecksum();
    /// @brief Remove the SHA1 checksum from the file and clear the header flag.
    /// @return `true` if the checksum was removed successfully.
    bool RemoveChecksum();
    /// @brief Add a file to the archive when it is not already present.
    /// @param name File name.
    /// @return `true` if the file was added successfully.
    bool AddFile(const std::string &name);
    /// @brief Remove a file from the archive by name when it is present.
    /// @param name File name.
    /// @return `true` if the file was removed successfully.
    bool RemoveFile(const std::string &name);
    /// @brief Remove a file from the archive by ID when it is present.
    /// @param id File ID.
    /// @return `true` if the file was removed successfully.
    bool RemoveFile(int32_t id);
    /// @brief Check whether a file name is present in the archive.
    /// @param name File name to test.
    /// @return `true` if the file is present.
    bool CheckFileName(const std::string &name) const;
    /// @brief Print the mix archive file list.
    /// Prints file CRC, file offset, and file size.
    void PrintFileList();
    /// @brief Print information about the mix file.
    void PrintInfo();
    /// @brief Close the mix file.
    /// Prepare the object for opening another file.
    void Close();

  protected:
    typedef std::map<uint32_t, uint32_t> SkipMap;
    typedef std::pair<uint32_t, uint32_t> SkipEntry;
    typedef std::map<uint32_t, uint32_t>::const_iterator SkipMapIterator;

    /// @brief Recompute and write the archive checksum.
    bool WriteChecksum(std::fstream &fh, int32_t pos = 0);

    /// @brief Return the file name component of a path.
    std::string BaseName(const std::string &pathname) const;

    /// @brief Rewrite the current archive with an unencrypted header.
    bool Decrypt();

    /// @brief Rewrite the current archive with an encrypted header.
    bool Encrypt();

    /// @brief Replace the original archive with a temporary output file.
    bool OverwriteOld(const std::string &temp);
    MixHeader m_header; // mix file header
    MixGmd m_global_db;
    MixLmd m_local_db;
    SkipMap m_skip;
    bool m_has_lmd;
    std::string m_file_path;
    std::fstream fh; // file handler
    uint8_t m_checksum[20];
};
