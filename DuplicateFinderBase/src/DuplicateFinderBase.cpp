#include "DuplicateFinderBase.hpp"
#include "CUL/CULInterface.hpp"

#include "CUL/Profiling/Profiler.hpp"
#include "CUL/Threading/MultiWorkerSystem.hpp"
#include "CUL/Threading/TaskCallback.hpp"
#include "CUL/Threading/ThreadUtil.hpp"
#include <CUL/STL_IMPORTS/STD_algorithm.hpp>

const std::string BWorkerStatus( int8_t workerId, const std::string& status )
{
    return "[" + std::to_string( workerId ) + "] " + status;
}

DuplicateFinderBase::DuplicateFinderBase():
    m_interface( *CUL::CULInterface::createInstance() )
{
    m_fileDb = std::make_unique<CUL::FS::FileDatabase>();
}


void DuplicateFinderBase::addPath( const CUL::StringWr& inPath )
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );
    m_searchPaths.push_back(inPath);
    std::sort( m_searchPaths.begin(), m_searchPaths.end() );
}

void DuplicateFinderBase::removePath(const CUL::StringWr& inPath)
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );

    const auto it = std::find( m_searchPaths.begin(), m_searchPaths.end(), inPath );
    if (it != m_searchPaths.end())
    {
        m_searchPaths.erase(it);
    }
}

void DuplicateFinderBase::removeLast()
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );
    m_searchPaths.pop_back();
}

const PathList DuplicateFinderBase::getPaths() const
{
    std::lock_guard<std::mutex> locker( m_searchPathsMtx );
    return m_searchPaths;
}

void DuplicateFinderBase::startDBLoad()
{
    ProfilerScope( "CApp::searchBackground::startDBLoad" );
    CUL::TaskCallback* loadDbTask = new CUL::TaskCallback();
    loadDbTask->Callback = [this]( int8_t workerId )
    {
        ProfilerScope( "CApp::searchBackground::startDBLoad::callback" );
        CUL::ThreadUtil::getInstance().setThreadStatus( BWorkerStatus( workerId, "loading data from db..." ) );
        m_loadingDb = true;
        m_fileDb->loadFilesFromDatabase();
        m_loadingDb = false;
        m_initialDbFilesUpdated = true;
        CUL::ThreadUtil::getInstance().setThreadStatus( BWorkerStatus( workerId, "IDLE" ) );
    };
    loadDbTask->Type = CUL::ITask::EType::DeleteAfterExecute;
    loadDbTask->Priority = CUL::EPriority::High;
    CUL::MultiWorkerSystem::getInstance().registerTask( loadDbTask );
}

String DuplicateFinderBase::getModTimeFromDb( const String& filePath )
{
    ProfilerScope( "CApp::getModTimeFromDb" );
    std::optional<CUL::FS::FileInfo> info = m_fileDb->getFileInfo( filePath.getValue() );

    if( info->MD5.empty() == false )
    {
        CUL::Time currentTime;
        CUL::FS::Path path;
        path.createFrom( info->FilePath );
        path.getLastModificationTime( currentTime );
        return currentTime.cStr();
    }

    return "";
}

void DuplicateFinderBase::getListOfSizes( std::vector<uint64_t>& out ) const
{
    m_fileDb->getListOfSizes( out );
}

void DuplicateFinderBase::getFiles( uint64_t inSize, std::vector<FileInfo>& out ) const
{
    m_fileDb->getFiles( inSize, out );
}

void DuplicateFinderBase::getFiles( uint64_t size, const CUL::StringWr& md5, std::vector<FileInfo>& out ) const
{
    m_fileDb->getFiles( size, md5, out );
}

void DuplicateFinderBase::removeFileFromDB( const String& pathRaw )
{
    m_fileDb->removeFileFromDB( pathRaw );
}

bool DuplicateFinderBase::isLoadingDb() const
{
    return m_loadingDb;
}

CUL::FS::CacheUsage DuplicateFinderBase::getCacheUsage() const
{
    return m_fileDb->getCacheUsage();
}

DuplicateFinderBase::~DuplicateFinderBase()
{
}