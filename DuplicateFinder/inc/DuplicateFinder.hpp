#pragma once

#include "CUL/Filesystem/Path.hpp"
#include "CUL/STL_IMPORTS/STD_memory.hpp"
#include "CUL/STL_IMPORTS/STD_thread.hpp"
#include "CUL/STL_IMPORTS/STD_map.hpp"
#include "CUL/STL_IMPORTS/STD_mutex.hpp"
#include "CUL/STL_IMPORTS/STD_vector.hpp"
#include "CUL/STL_IMPORTS/STD_set.hpp"
#include "CUL/STL_IMPORTS/STD_functional.hpp"

NAMESPACE_BEGIN( CUL )
class CULInterface;
NAMESPACE_END( CUL )

class DuplicateFinder final
{
public:
    DuplicateFinder();

    void search( const std::string& path, const std::string& summaryFilePath );
    int callback( void* NotUsed, int argc, char** argv, char** azColName );
    static DuplicateFinder* s_instance;

    void addFileFromDb( const CUL::String& path, const CUL::String& size, const CUL::String& md, const CUL::String& modTime );

    ~DuplicateFinder();
protected:
private:
    struct FileDb
    {
        CUL::String size;
        CUL::String md5;
        CUL::String modTime;
    };
    using FileSize = CUL::String;
    using MD5Value = CUL::String;
    using Value = std::map<MD5Value, CUL::FS::Path>;

    void addFile( const CUL::String& path );

    void addTask( std::function<void()> task);
    std::function<void()> getTask();
    void workerThreadMethod();

    unsigned getTasksLeft();
    void initDb();
    CUL::String getModTimeFromDb( const CUL::String& filePath );
    void addFileToDb( MD5Value md5, const CUL::String& filePath, const CUL::String& fileSize, const CUL::String& modTime );
    void getParametersFromDb( const CUL::String& filePath );
    void printCurrentMean();

    std::unique_ptr<CUL::CULInterface> m_culInterface;
    size_t m_maxThreadCount =  10;

    std::mutex m_duplicatesMtx;
    std::map<FileSize, std::map<MD5Value, std::set<CUL::FS::Path>>> m_duplicates;

    std::mutex m_filesPathsMapMtx;
    std::map<FileSize, Value> m_filesPathsMap;


    std::mutex m_workersMtx;
    std::map<std::thread::id, std::thread> m_workers;


    std::mutex m_tasksMtx;
    std::vector<std::function<void()>> m_tasks;

    std::mutex m_filesFromDbMtx;
    std::map<CUL::String, FileDb> m_filesFromDb;

    std::mutex m_sqliteMtx;

    float m_filesCount = 0.f;
    float m_filesLeft = 0.f;


    int m_last = 101;

    struct sqlite3* m_db = nullptr;
    CUL::String m_empty;

    std::mutex m_fileAddTasksDurationMtx;
    std::vector<unsigned> m_fileAddTasksDuration;
};