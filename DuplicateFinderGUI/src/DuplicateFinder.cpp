#include "DuplicateFinder.hpp"

#include "gameengine/IGameEngine.hpp"
#include "gameengine/Camera.hpp"
#include "gameengine/Sprite.hpp"
#include "gameengine/Primitives/Quad.hpp"
#include "gameengine/Cube.hpp"
#include "gameengine/Components/TransformComponent.hpp"

#include "LOGLWAdditionalDeps/ImportImgui.hpp"

#include "CUL/IMPORT_sqlite3.hpp"
#include "CUL/Filesystem/FileFactory.hpp"
#include "CUL/Filesystem/FSApi.hpp"
#include "CUL/GenericUtils/ConsoleUtilities.hpp"
#include "CUL/GenericUtils/ScopeExit.hpp"
#include "CUL/Threading/ThreadUtil.hpp"
#include "CUL/GenericUtils/ScopeExit.hpp"
#include "CUL/Threading/MultiWorkerSystem.hpp"
#include "CUL/Threading/TaskCallback.hpp"
#include "CUL/IMPORT_tracy.hpp"

#include "CUL/STL_IMPORTS/STD_future.hpp"
#include "CUL/STL_IMPORTS/STD_codecvt.hpp"
#include "CUL/STL_IMPORTS/STD_future.hpp"

#define NFD_NATIVE
#include "nfd.h"

#if 0 // DEBUG_THIS_FILE
    #define DEBUG_THIS_FILE 1
    #ifdef _MSC_VER
        #pragma optimize( "", off )
    #else
        #pragma clang optimize off
    #endif
#endif


CApp* CApp::s_instance = nullptr;

const CUL::String WorkerStatus(int8_t workerId, const CUL::String& status)
{
    return "[" + CUL::String( workerId ) + "] " + status;
}

CApp::CApp( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath )
    : LOGLW::IGameEngineApp( fullscreen, width, height, x, y, winName, configPath, false )
{
    m_outputFile = "D:\\out.txt";
    m_maxThreadCount = 3;
    m_minFileSizeBytes = 512;
    m_maxTasksInQueue = 64;
    m_maxFileGroups = 4u;
}

void CApp::onInit()
{
    ZoneScoped;
    s_instance = this;
    m_continousSearch = false;

    m_searchPaths.push_back( CUL::String( "D:\\" ) );
    m_searchPaths.push_back( CUL::String( "E:\\" ) );
    m_searchPaths.push_back( CUL::String( "F:\\" ) );
    m_searchPaths.push_back( CUL::String( "P:\\" ) );

    m_culInterface = m_oglw->getCul();

    m_oglw->drawDebugInfo( true );
    m_oglw->setBackgroundColor( LOGLW::ColorE::BLUE );


    const CUL::MATH::Angle pitch( 90.f, CUL::MATH::Angle::Type::DEGREE );

    m_oglw->guiFrameDelegate.addDelegate( [this](){guiIteration();} );

    m_mouseData = m_oglw->getMouseData();

    m_camera = &m_oglw->getCamera();

    m_oglw->setFpsLimit( 60 );
    m_thread.setBody( [this]() {
        timerThread();
    } );

    m_thread.run();
}

int CApp::callback( void*, int argc, char** argv, char** azColName )
{
    ZoneScoped;
    m_culInterface->getLogger()->log( "\n\n---------------------------------------------------------------------------------" );
    for( int i = 0; i < argc; ++i )
    {
        char* colName = azColName[i];
        CUL::String logVal = CUL::String( colName ) + CUL::String( " = " ) + CUL::String( argv[i] ? argv[i] : "NULL" );
        m_culInterface->getLogger()->log( logVal );
    }
    m_culInterface->getLogger()->log( "---------------------------------------------------------------------------------\n\n" );
    return 0;
}

void CApp::onWindowEvent( const SDL2W::WindowEvent::Type e )
{
    if( e == SDL2W::WindowEvent::Type::CLOSE )
    {
        m_runTimer = false;
        close();
    }
}

void CApp::onKeyBoardEvent( const SDL2W::KeyboardState& keys )
{
    if( keys.at("Q") )
    {
        m_runTimer = false;
        close();
    }
}

void CApp::timerThread()
{
    while( m_runTimer )
    {
        m_time += 0.01f;
        CUL::ITimer::sleepMiliSeconds(16);
    }
}

void CApp::customFrame()
{
}

#if defined(CUL_WINDOWS)
std::string wstring_to_utf8( const std::wstring& str )
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes( str );
}
#endif // #if defined(CUL_WINDOWS)

void CApp::guiIteration()
{
    ZoneScoped;
    CUL::String name = "MAIN";
    ImGui::Begin( name.cStr(), nullptr,
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar );

    auto winSize = IGameEngineApp::m_sdlw->getMainWindow()->getSize();

    ImGui::SetWindowPos( { (float)winSize.w * 0.2f, 0 } );

    const auto targetWidht = (float)winSize.w * 0.8f;
    const auto targetHeight = (float)winSize.h * 1.f;

    ImGui::SetWindowSize( { targetWidht, targetHeight } );

    ImGui::TextUnformatted( "Dirs to check:" );
    if( ImGui::Button( "+" ) )
    {
        addSearchDir();
    }

    ImGui::SameLine();

    if( ImGui::Button( "-" ) )
    {
        removeDir();
    }

    const size_t pathsCount = m_searchPaths.size();
    if( pathsCount > 0 )
    {
        for( size_t i = 0; i < pathsCount; ++i )
        {
            ImGui::TextUnformatted( m_searchPaths[i].cStr() );
        }
    }

    if( ImGui::Button( "Output file" ) )
    {
        chooseResultFile();
    }

    ImGui::SameLine();

    ImGui::TextUnformatted( m_outputFile.cStr() );

    if( ImGui::Button( "Add worker" ) )
    {
        ++m_maxThreadCount;

        CUL::MultiWorkerSystem::getInstance().addWorker( CUL::EPriority::Medium );
    }

    if( ImGui::Button( "Remove worker" ) )
    {
        --m_maxThreadCount;

        CUL::MultiWorkerSystem::getInstance().removeWorker( CUL::EPriority::Medium );
    }

    int value = m_minFileSizeBytes;
    if( ImGui::DragInt( "Minimum file size - bytes.", &value, 1.f, 0, 1024 * 1024 ) )
    {
        m_minFileSizeBytes = value;
    }

    if( ImGui::Button( "Increase minimum file size" ) )
    {
        m_minFileSizeBytes += 32;
    }
    ImGui::SameLine();
    if( ImGui::Button( "Decrease minimum file size" ) )
    {
        m_minFileSizeBytes -= 32;
    }

    static ImGuiTableFlags flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

    if( ImGui::BeginTable( "split", 3, flags ) )
    {
        ImGui::TableSetupColumn( "", ImGuiTableColumnFlags_WidthStretch, 16.f );
        constexpr float width = 270.f;
        ImGui::TableSetupColumn( "Done:", ImGuiTableColumnFlags_WidthStretch, width );
        ImGui::TableSetupColumn( "Current:", ImGuiTableColumnFlags_WidthStretch, width );

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        if( ImGui::Button( "Run" ) )
        {
            if( m_searchThread.joinable() )
            {
                m_searchThread.join();
            }

            m_searchThread = std::thread( &CApp::searchOneTime, this );
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex( 0 );
        if( ImGui::Button( "Run background" ) )
        {
            if( m_searchThread.joinable() )
            {
                m_searchThread.join();
            }

            m_searchThread = std::thread( &CApp::searchBackground, this );
        }

        CUL::String taskDoneAsString = CUL::String( m_filesDone );
        float percentage = 100.f * ( m_filesDone / m_filesCount );
        if( m_filesCount < 1.f )
        {
            percentage = 0.f;
        }

        if( percentage > 100.f )
        {
            percentage = 100.f;
        }

        const CUL::String percentageAsString = CUL::String( percentage );
        const CUL::String logText = percentageAsString + " done. ";

        ImGui::TableSetColumnIndex( 1 );
        ImGui::TextUnformatted( logText.cStr() );

        ImGui::TableSetColumnIndex( 2 );
        ImGui::TextUnformatted( m_currentFileText.cStr() );

        ImGui::EndTable();
    }

    {
        std::lock_guard<std::mutex> locker( m_fileGroupsMtx );

        if( m_fileGroups.empty() == false )
        {
            static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

            if( ImGui::BeginTable( "3ways", 4, flags ) )
            {
                ImGui::TableSetupColumn( "MD5", ImGuiTableColumnFlags_NoHide, 300.f );
                ImGui::TableSetupColumn( "Size", ImGuiTableColumnFlags_WidthFixed, 120.f );
                ImGui::TableSetupColumn( "Mod time", ImGuiTableColumnFlags_WidthFixed, 120.f );
                ImGui::TableSetupColumn( "Action", ImGuiTableColumnFlags_WidthFixed, 120.f );
                ImGui::TableHeadersRow();

                for( const auto& group : m_fileGroups )
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    bool open = ImGui::TreeNodeEx( group.MD5.cStr(), ImGuiTreeNodeFlags_SpanFullWidth );
                    ImGui::TableNextColumn();

                    const float size = group.Size.toFloat();

                    constexpr float BytesInMegs = 1024.f * 1024.f;
                    const float sizeInMB = size / BytesInMegs;

                    ImGui::TextDisabled( "%6.1f MB", sizeInMB );

                    if( open )
                    {
                        for( const auto& file : group.Files )
                        {
                            ImGui::TableNextRow();
                            ImGui::TreeNodeEx( file.Path.cStr(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet |
                                                               ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth );
                            ImGui::TableNextColumn();
                            ImGui::Text( "%s", file.Path.cStr() );
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( "--" );
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( file.ModTime.cStr() );
                        }

                        ImGui::TreePop();
                    }
                }
                ImGui::EndTable();
            }
        }


    }

    if( m_searchStarted )
    {
        const auto statuses = CUL::MultiWorkerSystem::getInstance().getWorkersStatuses();

        for( const auto& status : statuses )
        {
            ImGui::TextUnformatted( status.cStr() );
        }
    }

    ImGui::End();
}

void CApp::addSearchDir()
{
    NFD_Init();

    nfdchar_t* outPath = nullptr;
    // Unused for now.
    //nfdfilteritem_t filterItem = {};
    nfdresult_t result = NFD_PickFolder( &outPath, nullptr );
    if( result == NFD_OKAY )
    {
        CUL::String path = outPath;
        m_searchPaths.push_back( path );
        NFD_FreePath( outPath );
    }
    else if( result == NFD_CANCEL )
    {
    }
    else
    {
    }

    NFD_Quit();


    m_logger->log( "found?" );
}

void CApp::removeDir()
{
    m_searchPaths.pop_back();
}

void CApp::chooseResultFile()
{
    NFD_Init();

    nfdchar_t* outPath = nullptr;
#if defined(CUL_WINDOWS)
    nfdfilteritem_t filter = { L"*.txt", L"*.txt" };
#else // #if defined(CUL_WINDOWS)
    nfdfilteritem_t filter = { "*.txt", "*.txt" };
#endif // #if defined(CUL_WINDOWS)
    nfdresult_t result = NFD_SaveDialog( &outPath, &filter, 1, nullptr, nullptr );
    if( result == NFD_OKAY )
    {
        m_outputFile = outPath;
        NFD_FreePath( outPath );
    }
    else if( result == NFD_CANCEL )
    {
    }
    else
    {
    }

    NFD_Quit();

    m_logger->log( "found?" );
}

void CApp::onMouseEvent( const SDL2W::MouseData& )
{
}

void CApp::searchOneTime()
{
    ZoneScoped;
    m_culInterface->getLogger()->log( CUL::String( "Start search." ) );
    m_searchStarted = true;

    auto culFF = m_culInterface->getFS();

    startWorkers();

    for( const auto& path : m_searchPaths )
    {
        culFF->ListAllFiles( path, [this] ( const CUL::FS::Path& path ){
            m_filesCount = m_filesCount + 1.f;
            addTask( [this, path] ( int8_t workerId){
                CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, path.getPath() ) );
                addFile( path.getPath().string(), workerId );
                CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "IDLE" ) );

                m_filesDone = m_filesDone + 1.f;
            } );
        } );
    }

    m_workersEnabled = false;

    m_searchStarted = false;

    m_culInterface->getLogger()->log( "\nDONE.");
}

void CApp::searchBackground()
{
    ZoneScoped;
    CUL::TaskCallback* loadDbTask = new CUL::TaskCallback();
    loadDbTask->Callback = [this]( int8_t workerId ) {
        ZoneScoped;
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "loading data from db..." ) );
        m_loadingDb = true;
        m_fileDb.loadFilesFromDatabase();
        m_loadingDb = false;
        m_initialDbFilesUpdated = true;
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "IDLE" ) );
    };
    loadDbTask->Type = CUL::ITask::EType::DeleteAfterExecute;
    loadDbTask->Priority = CUL::EPriority::High;
    CUL::MultiWorkerSystem::getInstance().registerTask( loadDbTask );

    CUL::TaskCallback* searchTask = new CUL::TaskCallback();
    searchTask->Type = CUL::ITask::EType::DeleteAfterExecute;
    searchTask->Priority = CUL::EPriority::High;
    searchTask->Callback = [this, searchTask]( int8_t workerId ) {
        ZoneScoped;
        if( m_run == true )
        {
            searchAllFiles();
            CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "IDLE" ) );
        }
        else
        {
            searchTask->Type = CUL::ITask::EType::DeleteAfterExecute;
        }
    };
    CUL::MultiWorkerSystem::getInstance().registerTask( searchTask );

    CUL::TaskCallback* saveTask = new CUL::TaskCallback();
    saveTask->Type = CUL::ITask::EType::Loop;
    saveTask->Callback = [this, saveTask]( int8_t workerId ) {
        ZoneScoped;
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "Saving duplicates..." ) );

        if( m_loadingDb == false )
        {
            m_culInterface->getLogger()->log( "saveDuplicatesToFile::START" );
            fetchDuplicates();
            m_culInterface->getLogger()->log( "saveDuplicatesToFile::STOP" );
        }
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "Idle." ) );
    };
    CUL::MultiWorkerSystem::getInstance().registerTask( saveTask );

    //CUL::TaskCallback* deleteDeletedFileFromBase = new CUL::TaskCallback();
    //deleteDeletedFileFromBase->Type = CUL::ITask::EType::Loop;
    //deleteDeletedFileFromBase->Callback = [this, saveTask]( int8_t /*workerId*/ ) {
    //    if( m_loadingDb == false )
    //    {
    //        m_fileDb.deleteRemnants();
    //    }
    //};
    //CUL::MultiWorkerSystem::getInstance().registerTask( deleteDeletedFileFromBase );


    startWorkers();
    m_searchStarted = true;


    while( m_runBackground )
    {
        //std::list<FileGroup>::iterator it;

        //{
        //    std::lock_guard<std::mutex> lock( m_filesMtx );
        //    it = m_files.begin();
        //}

        //while( it != m_files.end() )
        //{
        //    FileGroup& filePath = *it;

        //    for( const auto& file: filePath.files )
        //    {
        //        if( file.exists() )
        //        {
        //            addTask( [this, file] ( size_t workerId ){
        //                m_currentFiles[workerId] = file.getPath();
        //                addFile( file.getPath().string() );
        //                m_currentFiles[workerId] = "";

        //                m_filesDone = m_filesDone + 1.f;
        //            } );
        //        }
        //        else
        //        {
        //            // here add to list for removal.
        //        }
        //    }
        //    std::lock_guard<std::mutex> lock( m_filesMtx );
        //    ++it;
        //}
    }
}

void CApp::setMainStatus( const CUL::String& status )
{
    ZoneScoped;
    std::lock_guard<std::mutex> lock( m_statusMutex );
    m_statusText = status;
}

void CApp::searchAllFiles()
{
    ZoneScoped;
    const int8_t currentThreadWorkerId = CUL::MultiWorkerSystem::getInstance().getCurrentThreadWorkerId();

    auto culFF = m_culInterface->getFS();
    for( const auto& m_searchPath : m_searchPaths )
    {
        ZoneScoped;
        culFF->ListAllFiles( m_searchPath, [this, currentThreadWorkerId]( const CUL::FS::Path& path ) {
            if( !path.getIsDir() && path != m_outputFile )
            {
                ++m_filesTotalCount;
                CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( currentThreadWorkerId, "Found: " + path.getPath() ) );
                addTask( [this, path]( int8_t workerId ) {
                    ZoneScoped;
                    const CUL::String pathAsString = path.getPath();
                    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "Adding file: " + pathAsString ) );
                    addFile( pathAsString, workerId );
                    ++m_readFilesCount;
                    m_percentage = 100.f * m_readFilesCount / ( 1.f * (float)m_filesTotalCount );
                } );
            }
        } );
    }
    setMainStatus( "Searching files... done." );
}

void CApp::showList()
{
    ZoneScoped;
    const auto workerId = CUL::MultiWorkerSystem::getInstance().getCurrentThreadWorkerId();
    std::vector<uint64_t> listOfSizes;
    m_fileDb.getListOfSizes( listOfSizes );
    const std::int64_t listOfSizesSize = static_cast<std::int64_t>( listOfSizes.size() );
    std::int64_t md5It = 0;
    std::int64_t maxMd5s = 4;
    bool exitLoop = false;
    CUL::String status;
    for( std::int64_t i = listOfSizesSize - 1; i >= 0; --i )
    {
        const auto size = listOfSizes[i];
        if( size < m_minFileSizeBytes )
        {
            continue;
        }

        if( exitLoop )
        {
            exitLoop = false;
            break;
        }

        std::vector<CUL::FS::FileDatabase::FileInfo> sameSizeFiles;
        m_fileDb.getFiles( size, sameSizeFiles );
        if( sameSizeFiles.size() > 1 )
        {
            const auto md5s = getListOfMd5s( sameSizeFiles );
            for( const auto& md5 : md5s )
            {
                std::vector<CUL::FS::FileDatabase::FileInfo> duplicatesList;
                m_fileDb.getFiles( size, md5, duplicatesList );
                if( duplicatesList.size() > 1 )
                {
                    const CUL::String current = "Size: " + CUL::String( size ) + ", MD5: " + md5;
                    if( ImGui::TreeNode( current.cStr() ) )
                    {
                        for( const auto fileInfo : duplicatesList )
                        {
                            ImGui::TextUnformatted( fileInfo.FilePath.getPath().cStr() );
                        }
                        ++md5It;

                        if( md5It > maxMd5s )
                        {
                            md5It = 0;
                            exitLoop = true;
                            break;
                        }
                    }
                }
            }
        }

        status = CUL::String( "Saved duplicate: " ) +
                 CUL::String( ( 100.f * static_cast<float>( i ) ) / ( 1.f * (float)listOfSizesSize ) ) + "%, ";
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status ) );
    }
}

void CApp::fetchDuplicates()
{
    ZoneScoped;
    std::vector<uint64_t> listOfSizes;
    m_fileDb.getListOfSizes( listOfSizes );
    const std::int64_t listOfSizesSize = static_cast<std::int64_t>( listOfSizes.size() );
    CUL::String status;

    for( std::int64_t i = listOfSizesSize - 1; i >= 0; --i )
    {
        if( m_fileGroups.size() > m_maxFileGroups )
        {
            return;
        }

        const auto size = listOfSizes[i];

        std::vector<CUL::FS::FileDatabase::FileInfo> sameSizeFiles;
        m_fileDb.getFiles( size, sameSizeFiles );
        if( sameSizeFiles.empty() )
        {
            continue;
        }

        const auto md5s = getListOfMd5s( sameSizeFiles );

        for( const auto& md5 : md5s )
        {
            std::vector<CUL::FS::FileDatabase::FileInfo> duplicatesList;
            m_fileDb.getFiles( size, md5, duplicatesList );
            if( duplicatesList.size() < 2u )
            {
                continue;
            }

            SameFilesGroup sfg;
            sfg.MD5 = md5;
            sfg.Size = size;
            for( const auto& fi : duplicatesList )
            {
                FileEntry fe;
                fe.Path = fi.FilePath;
                fe.ModTime = fi.ModTime;

                sfg.Files.push_back( fe );
            }

            std::lock_guard<std::mutex> locker( m_fileGroupsMtx );
            m_fileGroups.push_back( sfg );
        }
    }
}

void CApp::saveDuplicates()
{
    ZoneScoped;
    const auto workerId = CUL::MultiWorkerSystem::getInstance().getCurrentThreadWorkerId();
    CUL::String status;

    auto file = m_culInterface->getFF()->createRegularFileRawPtr( CUL::FS::Path( m_outputFile ) );
    file->toggleCache( false );

    CUL::String text = CUL::String( "Files: " );
    m_culInterface->getLogger()->log( text );
    file->addLine( text );

    std::vector<std::uint64_t> listOfSizes;
    m_fileDb.getListOfSizes( listOfSizes );
    listOfSizes.erase( std::remove_if( listOfSizes.begin(), listOfSizes.end(),
                                       [this]( uint64_t val ) {
                                           return val < m_minFileSizeBytes;
                             } ),
                       listOfSizes.end() );

    const size_t listOfSizesSize = listOfSizes.size();

    bool save = false;
    std::size_t md5It = 0;
    bool exitLoop = false;

    float wholePercentage = 0.f;
    float currentMd5Percentage = 0.f;

    for( std::int64_t i = listOfSizesSize - 1; i>=0; --i )
    {
        currentMd5Percentage = 0.f;
        const auto size = listOfSizes[i];

        if( exitLoop )
        {
            exitLoop = false;
            break;
        }

        std::vector<CUL::FS::FileDatabase::FileInfo> sameSizeFiles;
        m_fileDb.getFiles( size, sameSizeFiles );
        if( sameSizeFiles.size() > 1 )
        {
            const auto md5s = getListOfMd5s( sameSizeFiles );
            const std::size_t md5Count = md5s.size();

            md5It = 0u;
            for( const auto& md5 : md5s )
            {
                std::vector<CUL::FS::FileDatabase::FileInfo> duplicatesList;
                m_fileDb.getFiles( size, md5, duplicatesList );
                if( duplicatesList.size() > 1 )
                {
                    const float MegaBytes = 1.f * static_cast<float>( size ) / ( 1.f * bytesINMegabyte );
                    text = CUL::String( "Size: " ) + CUL::String( MegaBytes ) + CUL::String( "MB" );
                    file->addLine( text );

                    text = CUL::String( "MD5: " ) + md5;
                    file->addLine( text );

                    for( const auto& fileInfo : duplicatesList )
                    {
                        file->addLine( fileInfo.FilePath );
                        save = true;
                    }
                    ++md5It;
                    currentMd5Percentage = 100.f * md5It / static_cast<float>(md5Count);
                    wholePercentage = 100.f * ( listOfSizesSize - i ) / static_cast<float>( listOfSizesSize );
                    status = CUL::String( "Saved duplicate: " ) + CUL::String( wholePercentage ) +
                             "%, md5:" + CUL::String( currentMd5Percentage ) + "%";
                    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status ) );
                }
            }
        }

        if( save )
        {
            file->saveFile();
            save = false;
        }

        wholePercentage = 100.f * ( listOfSizesSize - i ) / static_cast<float>( listOfSizesSize );
        status = CUL::String( "Saved duplicate: " ) + CUL::String( wholePercentage ) +"%, md5: " + CUL::String( currentMd5Percentage ) + "%";
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status ) );
    }

    delete file;
}

std::set<CUL::String> CApp::getListOfMd5s( const std::vector<CUL::FS::FileDatabase::FileInfo>& fiList )
{
    ZoneScoped;
    std::set<CUL::String> result;

    for( const auto& fi : fiList )
    {
        result.insert( fi.MD5 );
    }

    return result;
}

void CApp::startWorkers()
{
    m_workersEnabled = true;
    setWorkersCount( m_maxThreadCount );
}

void CApp::setWorkersCount( uint8_t targetCount )
{
    ZoneScoped;
    uint8_t workersCount = CUL::MultiWorkerSystem::getInstance().getCurrentWorkersCount();
    while( workersCount != targetCount )
    {
        if( workersCount < targetCount )
        {
            CUL::MultiWorkerSystem::getInstance().addWorker( CUL::EPriority::Medium );
            workersCount = CUL::MultiWorkerSystem::getInstance().getCurrentWorkersCount();
        }
        else if( workersCount > targetCount )
        {
            CUL::MultiWorkerSystem::getInstance().removeWorker( CUL::EPriority::Medium );
            workersCount = CUL::MultiWorkerSystem::getInstance().getCurrentWorkersCount();
        }
    }
}

void CApp::addTask( std::function<void( int8_t )> task )
{
    ZoneScoped;
    while( CUL::MultiWorkerSystem::getInstance().getTasksLeft(CUL::EPriority::Medium) > m_maxTasksInQueue )
    {
        CUL::ITimer::sleepMiliSeconds( 64 );
    }

    CUL::TaskCallback* taskPtr = new CUL::TaskCallback();
    taskPtr->Callback = task;
    taskPtr->Type = CUL::ITask::EType::DeleteAfterExecute;

    CUL::MultiWorkerSystem::getInstance().registerTask( taskPtr );
}

void CApp::addFile( const CUL::String& path, int8_t workerId )
{
    ZoneScoped;
    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[START] " + path ) );

    CUL::GUTILS::ScopeExit se( [this, workerId]() {
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "IDLE" ) );
    } );


    CUL::FS::Path fp = path;
    if( fp.exists() == false )
    {
        return;
    }

    std::unique_ptr<CUL::FS::IFile> file;
    file.reset( m_culInterface->getFF()->createRegularFileRawPtr( path ) );
    CUL::String fileSize = file->getSizeBytes();
    const auto sizeBytes = fileSize.toUint64();
    if( sizeBytes < m_minFileSizeBytes )
    {
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[File too small, aborting.]" ) );
        return;
    }

    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[Get last mod time] " + path ) );
    CUL::Time modTimeFromFS;
    file->getLastModificationTime( modTimeFromFS );
    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[Get DB info] " + path ) );
    auto info = m_fileDb.getFileInfo( path );
    CUL::String modTimeFromDb;
    CUL::String md5;
    if( info.Found )
    {
        modTimeFromDb = info.ModTime;
        md5 = info.MD5;
    }
    else
    {
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[Get DB info done] " + path ) );
    }

    const CUL::String currentModTime = modTimeFromFS.toString();
    if( currentModTime == modTimeFromDb )
    {
    }
    else
    {
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[File changed, calcualte md5...] " + path ) );
        md5 = file->getMD5();
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[File changed, calcualte md5 done.] " + path ) );

        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[File adding to db...] " + path ) );
        m_fileDb.addFile( md5, path, CUL::String( sizeBytes ), modTimeFromFS.toString() );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "[File adding to db... done.] " + path ) );
    }
}

void CApp::addFileToList( const CUL::String )
{
}

void CApp::addDuplicate( const FileSize, const MD5Value&, const CUL::FS::Path& )
{
}

CUL::String CApp::getModTimeFromDb( const CUL::String& filePath )
{
    ZoneScoped;
    auto info = m_fileDb.getFileInfo( filePath );

    if( !info.MD5.empty() )
    {
        CUL::Time currentTime;
        info.FilePath.getLastModificationTime( currentTime );
        return currentTime.toString();
    }

    return "";
}

void CApp::addForCheckForDeletionList( const CUL::String& inPath )
{
    ZoneScoped;
    const auto it =  std::find_if( m_deletionList.begin(), m_deletionList.end(), [inPath] ( const CUL::String& path ){
        return path == inPath;
    } );

    if( it == m_deletionList.end() )
    {
        m_deletionList.push_back( inPath );
    }
}

void CApp::getList()
{
}

void CApp::printCurrentMean()
{
}

CApp::~CApp()
{
    // BYE!
}

int main( int argc, char* args[] )
{
    std::setlocale( LC_ALL, "" );              // for C and C++ where synced with stdio
    std::locale::global( std::locale( "" ) );  // for C++

    CUL::GUTILS::ConsoleUtilities cu;
    cu.setArgs( argc, args );
    auto width = cu.getFlagValue( "-w" );
    auto height = cu.getFlagValue( "-h" );

    if( width.empty() || height.empty() )
    {
        width = "1280";
        height = "900";
    }

    const CUL::String winName = cu.getArgs().getArgsValVec()[0];
    const char* winNameStr = winName.cStr();
    CApp app( false, std::stoul( width.string() ), std::stoul( height.string() ), 256, 127, winNameStr,
             "Config.txt" );
    app.run();

    return 0;
}

#if defined( DEBUG_THIS_FILE )
    #ifdef _MSC_VER
        #pragma optimize( "", on )
    #else
        #pragma clang optimize on
    #endif
#endif  // #if defined(DEBUG_THIS_FILE)