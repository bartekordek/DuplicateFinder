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

    void search(const std::string& path);

    ~DuplicateFinder();
protected:
private:

    using FileSize = unsigned;
    using MD5Value = CUL::String;
    using Value = std::map<MD5Value, CUL::FS::Path>;

    void addFile( const std::string& path );
    size_t getCurrentThreadCount();

    std::unique_ptr<CUL::CULInterface> m_culInterface;
    size_t m_maxThreadCount =  64;

    std::mutex m_duplicatesMtx;
    std::map<FileSize, std::map<MD5Value, std::set<CUL::FS::Path>>> m_duplicates;
    std::map<FileSize, Value> m_filesPathsMap;


    std::mutex m_threadsMtx;
    std::map<std::thread::id, std::thread> m_threads;


    std::mutex m_tasksMtx;
    std::vector<std::function<void()>> m_tasks;
    int m_last = 101;
};