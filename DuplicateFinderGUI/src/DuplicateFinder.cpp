#include "DuplicateFinder.hpp"

#include "gameengine/IGameEngine.hpp"
#include "gameengine/Camera.hpp"
#include "gameengine/Sprite.hpp"
#include "gameengine/Primitives/Quad.hpp"
#include "gameengine/Cube.hpp"
#include "gameengine/Components/TransformComponent.hpp"

#include "LOGLWAdditionalDeps/ImportImgui.hpp"

#include "CUL/Filesystem/FileFactory.hpp"
#include "CUL/Filesystem/FSApi.hpp"
#include "CUL/Filesystem/PathDialog.hpp"
#include "CUL/Filesystem/RegularFile.hpp"
#include "CUL/GenericUtils/ConsoleUtilities.hpp"
#include "CUL/GenericUtils/ScopeExit.hpp"
#include "CUL/Hardware/DiskInfo.hpp"
#include "CUL/Threading/ThreadUtil.hpp"
#include "CUL/Threading/MultiWorkerSystem.hpp"
#include "CUL/Threading/TaskCallback.hpp"
#include "CUL/Proifling/Profiler.hpp"

#include "CUL/STL_IMPORTS/STD_future.hpp"
#include "CUL/STL_IMPORTS/STD_codecvt.hpp"
#include "CUL/STL_IMPORTS/STD_future.hpp"

#if 0  // DEBUG_THIS_FILE
#define DEBUG_THIS_FILE 1
#if defined( _MSC_VER )
#pragma optimize( "", off )
#elif defined( __clang__ )
#pragma clang optimize off
#endif
#endif

CApp* CApp::s_instance = nullptr;

const std::string WorkerStatus( int8_t workerId, const std::string& status )
{
    return "[" + std::to_string( workerId ) + "] " + status;
}

CApp::CApp( bool fullscreen, unsigned width, unsigned height, int x, int y, const char* winName, const char* configPath )
    : LOGLW::IGameEngineApp( fullscreen, width, height, x, y, winName, configPath, false )
{
    m_outputFile = "D:\\out.txt";
    m_maxThreadCount = 6u;
    m_maxFileGroups = 4u;

    m_timer.reset( CUL::TimerFactory::getChronoTimerPtr( nullptr ) );
}

void CApp::onInit()
{
    ProfilerScope( "CApp::onInit" );

    s_instance = this;
    m_continousSearch = false;

#if defined( CUL_WINDOWS )
    // m_duplicateFinderBase.addPath( String( "C:\\" ) );
    m_duplicateFinderBase.addPath( String( "D:\\" ) );
    m_duplicateFinderBase.addPath( String( "E:\\" ) );
    m_duplicateFinderBase.addPath( String( "F:\\" ) );
#else   // #if defined(CUL_WINDOWS)
    m_duplicateFinderBase.addPath( String( "/mnt/d/" ) );
    m_duplicateFinderBase.addPath( String( "/mnt/e/" ) );
    m_duplicateFinderBase.addPath( String( "/mnt/f/" ) );
#endif  // #if defined(CUL_WINDOWS)

    m_culInterface = m_oglw->getCul();

    m_oglw->drawDebugInfo( true );
    m_oglw->toggleDrawDebugInfo( false );
    m_oglw->setBackgroundColor( LOGLW::ColorE::BLUE );

    const CUL::MATH::Angle pitch( 90.f, CUL::MATH::Angle::Type::DEGREE );

    m_oglw->guiFrameDelegate.addDelegate(
        [this]( float x, float y )
        {
            guiIteration( x, y );
        } );

    m_mouseData = m_oglw->getMouseData();

    m_camera = &m_oglw->getCamera();

    m_oglw->setFpsLimit( 60 );
    m_thread.setBody(
        [this]()
        {
            timerThread();
        } );

    m_thread.run();
    CUL::CDiskInfo::getInstance().start( true );
}

int CApp::callback( void*, int argc, char** argv, char** azColName ) const
{
    ProfilerScope( "CApp::callback" );

    m_culInterface->getLogger()->log( "\n\n---------------------------------------------------------------------------------" );
    for( int i = 0; i < argc; ++i )
    {
        char* colName = azColName[i];
        String logVal = String( colName ) + String( " = " ) + String( argv[i] ? argv[i] : "NULL" );
        m_culInterface->getLogger()->log( logVal.getUtfChar() );
    }
    m_culInterface->getLogger()->log( "---------------------------------------------------------------------------------\n\n" );
    return 0;
}

void CApp::onWindowEvent( const LOGLW::WindowEvent::Type e )
{
    if( e == LOGLW::WindowEvent::Type::CLOSE )
    {
        m_runTimer = false;
        close();
    }
}

void CApp::onKeyBoardEvent( const LOGLW::KeyboardState& keys )
{
    if( keys.at( "Q" ) )
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
        CUL::ITimer::sleepMiliSeconds( 16 );
    }
}

void CApp::customFrame()
{
}

#if defined( CUL_WINDOWS )
void setClipboardText( const std::wstring& inChar )
{
    const wchar_t* cwdBuffer = inChar.c_str();

    DWORD len = inChar.length();

    // Allocate string for cwd
    HGLOBAL hdst = GlobalAlloc( GMEM_MOVEABLE | GMEM_DDESHARE, ( len + 1 ) * sizeof( WCHAR ) );
    LPWSTR dst = (LPWSTR)GlobalLock( hdst );
    memcpy( dst, cwdBuffer, len * sizeof( WCHAR ) );
    dst[len] = 0;
    GlobalUnlock( hdst );

    // Set clipboard data
    if( !OpenClipboard( NULL ) )
        return;
    EmptyClipboard();
    if( !SetClipboardData( CF_UNICODETEXT, hdst ) )
        return;
    CloseClipboard();
}
#else   // #if defined( CUL_WINDOWS )
void setClipboardText( const std::wstring& /*inChar*/ )
{
}
#endif  // #if defined( CUL_WINDOWS )

void setClipboardText( const char* inChar )
{
    ImGui::SetClipboardText( inChar );
}

#if defined( CUL_WINDOWS )
std::string wstring_to_utf8( const std::wstring& str )
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes( str );
}
#endif  // #if defined(CUL_WINDOWS)

void CApp::guiIteration( float x, float /*y*/ )
{
    ProfilerScope( "CApp::guiIteration" );

    String name = "MAIN";
    ImGui::Begin( name.getUtfChar(), nullptr,
                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar );

    auto winSize = IGameEngineApp::m_sdlw->getMainWindow()->getSize();

    ImGui::SetWindowPos( { x, 0 } );

    const auto targetWidht = (float)winSize.W - x;
    const auto targetHeight = (float)winSize.H * 1.f;

    ImGui::SetWindowSize( { targetWidht, targetHeight } );

    const auto u8Str = u8"ĄĘó";
    const std::string tmpStr = reinterpret_cast<const char*>( u8Str );
    ImGui::TextUnformatted( tmpStr.c_str() );
    ImGui::TextUnformatted( reinterpret_cast<const char*>( u8"ąęó" ) );
    ImGui::TextUnformatted( reinterpret_cast<const char*>( u8"日本語" ) );

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

    {
        ProfilerScope( "DrawDuplicates" );
        const size_t pathsCount = m_duplicateFinderBase.getPaths().size();
        if( pathsCount > 0 )
        {
            for( const String& searchPath : m_duplicateFinderBase.getPaths() )
            {
                ImGui::TextUnformatted( searchPath.getUtfChar() );
            }
        }
    }

    if( ImGui::Button( "Output file" ) )
    {
        chooseResultFile();
    }

    ImGui::SameLine();

    ImGui::TextUnformatted( m_outputFile.getPath().getUtfChar() );

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

    const std::int64_t processedFloat = m_processedFiles.load();
    const std::int64_t foundFiles = m_foundFiles.load();

    if( foundFiles != 0 )
    {
        m_percentage = 100.f * static_cast<float>( processedFloat ) / static_cast<float>( foundFiles );
    }

    ImGui::BeginGroup();

#if defined( CUL_WINDOWS )
    ImGui::Text( "Files:\n%8d processed\n%8d found\n%11.2f%% completed", processedFloat, foundFiles, m_percentage );
#else   // #if defined( CUL_WINDOWS )
    ImGui::Text( "Files:\n%8ld processed\n%8ld found\n%11.2f%% completed", processedFloat, foundFiles, m_percentage );
#endif  // #if defined( CUL_WINDOWS )

    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    const auto percentageInfo = m_fileDb.getCacheUsage();
#if defined( CUL_WINDOWS )
    ImGui::Text( "Cache usage: %4.2f %% (%4.2f MB) (%d/%d)", percentageInfo.Percentage * 100.f, percentageInfo.MBUsed, percentageInfo.Curr,
                 percentageInfo.Max );
#else   // defined( CUL_WINDOWS )
    ImGui::Text( "Cache usage: %4.2f %% (%4.2f MB) (%ld/%ld)", percentageInfo.Percentage * 100.f, percentageInfo.MBUsed,
                 percentageInfo.Curr, percentageInfo.Max );
#endif  // defined( CUL_WINDOWS )
    ImGui::EndGroup();

    {
        ProfilerScope( "DrawPrioDiskUsage" );
        const std::vector<std::pair<std::string, float>> disksUsage = CUL::CDiskInfo::getInstance().getDisksUsage();
        for( const std::pair<std::string, float>& currentPair : disksUsage )
        {
            ImGui::Text( "Disk %s usage: %3.0f%%", currentPair.first.c_str(), currentPair.second );
        }
    }

    ImGui::TextUnformatted( "Dirs to Skip:" );
    if( ImGui::Button( "Add skip dir" ) )
    {
        addSkipDir();
    }

    ImGui::SameLine();

    if( ImGui::Button( "Remove skip dir" ) )
    {
        removeSkipDir();
    }

    {
        ProfilerScope( "DrawSkipped" );
        const size_t pathsCount = m_skippedDirs.size();
        if( pathsCount > 0 )
        {
            for( size_t i = 0; i < pathsCount; ++i )
            {
                ImGui::TextUnformatted( m_skippedDirs[i].getPath().getUtfChar() );
            }
        }
    }

    ImGui::TextUnformatted( "Words to exclude:" );

    if( ImGui::Button( "Add exclude word" ) )
    {
        addExcludeWord( "Enter word" );
    }

    ImGui::SameLine();

    if( ImGui::Button( "Remove exclude word" ) )
    {
        removeLastExcludeWord();
    }

    if( ImGui::CollapsingHeader( "Skipped words" ) )
    {
        ProfilerScope( "DrawSkippedWords" );
        std::size_t inputId{ 0u };
        for( LOGLW::String& currentExcludeWord : m_excludeWords )
        {
            constexpr std::size_t labelBufferSize{ 128u };
            static char labelBuffer[labelBufferSize];

#if defined( CUL_WINDOWS )
            snprintf( labelBuffer, labelBufferSize, "Word %d", inputId++ );
#else   // #if defined( CUL_WINDOWS )
            snprintf( labelBuffer, labelBufferSize, "Word %ld", inputId++ );
#endif  // #if defined( CUL_WINDOWS )

            constexpr std::size_t valueBufferSize{ 512u };
            static char valueBuffer[valueBufferSize];
            const char* wordCstr = currentExcludeWord.getUtfChar();
            snprintf( valueBuffer, valueBufferSize, "%s", wordCstr );

            if( ImGui::InputText( labelBuffer, valueBuffer, valueBufferSize ) )
            {
                currentExcludeWord = valueBuffer;
            }
        }
    }

    std::int32_t value = m_minFileSizeMB;
    if( ImGui::DragInt( "Minimum file size - MB.", &value, 1.f, 0 ) )
    {
        m_minFileSizeMB = value;
    }
    value = m_maxFileSizeMB;
    if( ImGui::DragInt( "Maximum file size - MB.", &value, 1.f, 0 ) )
    {
        m_maxFileSizeMB = value;
    }

    static ImGuiTableFlags flags =
        ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

    if( ImGui::BeginTable( "split", 3, flags ) )
    {
        ProfilerScope( "DrawMainTable" );
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

        const String taskDoneAsString = String::createFromPrintf( "%f", m_filesDone.load() );
        float percentage = 100.f * ( m_filesDone / m_filesCount );
        if( m_filesCount < 1.f )
        {
            percentage = 0.f;
        }

        if( percentage > 100.f )
        {
            percentage = 100.f;
        }

        const String percentageAsString = String::createFromPrintf( "%f", percentage );
        const String logText = percentageAsString + " done. ";

        ImGui::TableSetColumnIndex( 1 );
        ImGui::TextUnformatted( logText.getUtfChar() );

        ImGui::TableSetColumnIndex( 2 );
        ImGui::TextUnformatted( m_currentFileText.getUtfChar() );

        ImGui::EndTable();
    }

    for( std::uint8_t currentPrio = static_cast<std::uint8_t>( CUL::EPriority::Low );
         currentPrio < static_cast<std::uint8_t>( CUL::EPriority::COUNT ); ++currentPrio )
    {
        ProfilerScope( "DrawPrioN" );
        const std::uint64_t currentTasksCount =
            CUL::MultiWorkerSystem::getInstance().getTasksLeft( static_cast<CUL::EPriority>( currentPrio ) );
        const std::uint64_t maxTasks = CUL::MultiWorkerSystem::getInstance().getMaxTasksCount( static_cast<CUL::EPriority>( currentPrio ) );
#if defined( CUL_WINDOWS )
        ImGui::Text( "Prio: %s, tasks: %d/%d", CUL::EPriorityToChar( static_cast<CUL::EPriority>( currentPrio ) ), currentTasksCount,
                     maxTasks );
#else   // #if defined( CUL_WINDOWS )
        ImGui::Text( "Prio: %s, tasks: %ld/%ld", CUL::EPriorityToChar( static_cast<CUL::EPriority>( currentPrio ) ), currentTasksCount,
                     maxTasks );
#endif  // #if defined( CUL_WINDOWS )
    }

    {
        ProfilerScope( "m_fileGroupsMtx" );
        std::lock_guard<std::mutex> locker( m_fileGroupsMtx );

        std::set<SameFilesGroup>::iterator itToBeDeleted = m_fileGroups.end();

        if( m_fileGroups.empty() == false )
        {
            ProfilerScope( "DrawFilesTable" );
            static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;

            constexpr std::size_t maxGroups = 10u;

            auto doesGroupContainExcluded = [this]( const SameFilesGroup& inGroup )
            {
                ProfilerScope( "doesGroupContainExcluded" );
                for( const FileEntry& currentEntry : inGroup.Files )
                {
                    for( const auto& currentWord : m_excludeWords )
                    {
                        if( currentEntry.Path.contains( currentWord ) )
                        {
                            return true;
                        }
                    }
                }
                return false;
            };

            auto doesGroupExist = []( const SameFilesGroup& inGroup )
            {
                ProfilerScope( "doesGroupExist" );
                for( const FileEntry& currentEntry : inGroup.Files )
                {
                    CUL::FS::FSApi* fsApi = CUL::CULInterface::getInstance()->getFS();
                    if( fsApi->fileExist( currentEntry.Path ) == false )
                    {
                        return false;
                    }
                }
                return true;
            };

            if( ImGui::BeginTable( "3ways", 4, flags ) )
            {
                ProfilerScope( "DrawTable" );

                ImGui::TableSetupColumn( "MD5", ImGuiTableColumnFlags_WidthFixed, 330.f );
                ImGui::TableSetupColumn( "Size", ImGuiTableColumnFlags_WidthFixed, 60.f );
                ImGui::TableSetupColumn( "Mod time", ImGuiTableColumnFlags_WidthFixed, 70.f );
                ImGui::TableSetupColumn( "Action", ImGuiTableColumnFlags_WidthFixed, 110.f );
                ImGui::TableHeadersRow();

                std::size_t j{ 0 };

                for( std::set<SameFilesGroup>::iterator it = m_fileGroups.begin(); it != m_fileGroups.end(); ++it )
                {
                    SameFilesGroup& group = const_cast<SameFilesGroup&>( *it );

                    if( group.Skip )
                    {
                        continue;
                    }

                    if( doesGroupContainExcluded( group ) )
                    {
                        continue;
                    }

                    if( doesGroupExist( group ) == false )
                    {
                        continue;
                    }

                    if( j >= maxGroups )
                    {
                        break;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::SetNextItemOpen( true );

                    bool open = ImGui::TreeNodeEx( group.MD5.getUtfChar(), ImGuiTreeNodeFlags_SpanFullWidth );
                    ImGui::TableNextColumn();

                    const float size = static_cast<float>( group.Size );

                    constexpr float BytesInMegs = 1024.f * 1024.f;
                    const float sizeInMB = size / BytesInMegs;

                    ImGui::TextDisabled( "%5.2f MB", sizeInMB );
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    {
                        char pathStr[512];
                        sprintf( pathStr, "Skip %li", j );
                        if( ImGui::Button( pathStr ) )
                        {
                            group.Skip = true;
                        }

                        ++j;
                    }

                    if( open )
                    {
                        std::vector<std::size_t> lowestValues;
                        getEarlisestFiles( lowestValues, group.Files );

                        const std::int32_t filesCount = group.Files.size();

                        for( std::int32_t fileId = filesCount - 1; fileId >= 0; --fileId )
                        {
                            ProfilerScope( "DrawFiles - 2" );
                            const bool isNewest = std::find( lowestValues.begin(), lowestValues.end(), fileId ) != lowestValues.end();

                            ImVec4 color( 0.0f, 1.0f, 0.0f, 1.0f );
                            if( isNewest == false && lowestValues.empty() == false )
                            {
                                color.x = 1.f;
                                color.y = 1.f;
                            }

                            const FileEntry& fe = group.Files[fileId];
                            const std::u8string filePathChar = fe.Path.toU8String();
                            const char* pathValue = reinterpret_cast<const char*>( filePathChar.c_str() );

                            ImGui::TableNextRow();
                            ImGui::TreeNodeEx( pathValue, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet |
                                                              ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth );
                            ImGui::TableNextColumn();

                            ImGui::TextColored( color, "%s", pathValue );
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted( "--" );
                            ImGui::TableNextColumn();
                            const String ModTimeStr = fe.ModTime.toString().getValue();
                            ImGui::TextUnformatted( ModTimeStr.getUtfChar() );
                            ImGui::TableNextColumn();

                            char pathStr[512];
                            {
                                sprintf( pathStr, "Copy %d", fileId );
                                if( ImGui::Button( pathStr ) )
                                {
#if defined( CUL_WINDOWS )
                                    setClipboardText( fe.Path.getValue() );
#endif  // #if defined(CUL_WINDOWS)
                                }
                            }
                            ImGui::SameLine();
                            {
                                sprintf( pathStr, "Delete %d", fileId );
                                if( ImGui::Button( pathStr ) )
                                {
                                    m_culInterface->getFS()->deleteFile( fe.Path );
                                    m_fileDb.removeFileFromDB( fe.Path.getValue() );
                                    group.Files.erase( group.Files.begin() + fileId );
                                    if( group.Files.size() < 2 )
                                    {
                                        itToBeDeleted = it;
                                    }
                                }
                            }

                            ImGui::SameLine();
                            {
                                sprintf( pathStr, "Exclude %d", fileId );
                                if( ImGui::Button( pathStr ) )
                                {
                                    const String pathAsString = fe.Path.getValue();
                                    addExcludeWord( pathAsString );
                                }
                            }
                        }

                        ImGui::TreePop();
                    }
                }
            }

            ImGui::EndTable();

            if( itToBeDeleted != m_fileGroups.end() )
            {
                m_fileGroups.erase( itToBeDeleted );
            }
        }
    }

    if( m_searchStarted )
    {
        const std::vector<String> statuses = CUL::MultiWorkerSystem::getInstance().getWorkersStatuses();

        for( const String& status : statuses )
        {
            const std::u8string tmp = status.toU8String();
            ImGui::TextUnformatted( status.getUtfChar() );
        }
    }

    ImGui::End();
}

void CApp::getEarlisestFiles( std::vector<std::size_t>& outValue, const std::vector<FileEntry>& files )
{
    ProfilerScope( "CApp::getEarlisestFiles" );

    if( files.empty() )
    {
        return;
    }

    const std::size_t elementsCount = files.size();

    if( elementsCount == 0u )
    {
        outValue.push_back( 0u );
        return;
    }

    CUL::Time lowest( files[0].ModTime );

    for( std::size_t i = 1u; i < elementsCount; ++i )
    {
        const CUL::Time current = files[i].ModTime;
        if( lowest > current )
        {
            outValue.clear();
            outValue.push_back( i );
        }
        else if( lowest == current )
        {
            outValue.push_back( i );
        }
    }

    if( outValue.empty() )
    {
        outValue.push_back( 0u );
    }
}

void CApp::addSearchDir()
{
    const String choosenDir = CUL::FS::PathDialog::getInstance().pickFolder().getString();

    if( choosenDir.empty() == false )
    {
        m_duplicateFinderBase.addPath( choosenDir );
    }
}

void CApp::removeDir()
{
    m_duplicateFinderBase.removeLast();
}

void CApp::addSkipDir()
{
    const String choosenDir = CUL::FS::PathDialog::getInstance().pickFolder().getString();

    if( choosenDir.empty() == false )
    {
        m_skippedDirs.push_back( choosenDir );
    }
}

void CApp::removeSkipDir()
{
    m_skippedDirs.pop_back();
}

void CApp::addExcludeWord( const String& inString )
{
    m_excludeWords.push_back( inString );
}

void CApp::removeLastExcludeWord()
{
    m_excludeWords.pop_back();
}

void CApp::chooseResultFile()
{
    CUL::FS::PathDialog::Filter filter;
    filter.Name = "*.txt";
    filter.Spec = "*.txt";
    const String choosenDir = CUL::FS::PathDialog::getInstance().saveDialog( filter ).getString();

    if( choosenDir.empty() == false )
    {
        m_outputFile = choosenDir;
    }
}

void CApp::onMouseEvent( const LOGLW::MouseData& )
{
}

void CApp::searchOneTime()
{
    ProfilerScope( "CApp::searchOneTime" );

    startDBLoad();
    startFileSearch();
}

void CApp::searchBackground()
{
    ProfilerScope( "CApp::searchBackground" );

    startDBLoad();
    startFileSearch();

    CUL::TaskCallback* saveTask = new CUL::TaskCallback();
    saveTask->Type = CUL::ITask::EType::Loop;
    saveTask->Priority = CUL::EPriority::Low;
    saveTask->Callback = [this, saveTask]( int8_t workerId )
    {
        ProfilerScope( "CApp::searchBackground::lambda" );

        if( m_loadingDb == false )
        {
            m_culInterface->getLogger()->log( "saveDuplicatesToFile::START" );
            fetchDuplicates();
            m_culInterface->getLogger()->log( "saveDuplicatesToFile::STOP" );
        }
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "Idle." ) );
    };
    CUL::MultiWorkerSystem::getInstance().registerTask( saveTask );

    startWorkers();
    m_searchStarted = true;
}

void CApp::startDBLoad()
{
    ProfilerScope( "CApp::searchBackground::startDBLoad" );
    CUL::TaskCallback* loadDbTask = new CUL::TaskCallback();
    loadDbTask->Callback = [this]( int8_t workerId )
    {
        ProfilerScope( "CApp::searchBackground::startDBLoad::callback" );
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
}

void CApp::startFileSearch()
{
    CUL::TaskCallback* searchTask = new CUL::TaskCallback();
    searchTask->Type = CUL::ITask::EType::DeleteAfterExecute;
    searchTask->Priority = CUL::EPriority::High;
    searchTask->Callback = [this, searchTask]( int8_t workerId )
    {
        ProfilerScope( "CApp::startFileSearch::task" );
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
}

void CApp::setMainStatus( const String& status )
{
    ProfilerScope( "CApp::setMainStatus" );
    std::lock_guard<std::mutex> lock( m_statusMutex );
    m_statusText = status;
}

void CApp::searchAllFiles()
{
    ProfilerScope( "CApp::searchAllFiles" );
    const int8_t currentThreadWorkerId = CUL::MultiWorkerSystem::getInstance().getCurrentThreadWorkerId();

    auto culFF = m_culInterface->getFS();
    for( const String& searchPath : m_duplicateFinderBase.getPaths() )
    {
        const CUL::FS::Path searchPathAsPath = searchPath;

        ProfilerScope( "CApp::searchAllFiles::it" );
        culFF->ListAllFiles(
            searchPath,
            [this, currentThreadWorkerId, searchPathAsPath]( const CUL::FS::Path& path )
            {
                ProfilerScope( "CApp::searchAllFiles::it::task" );
                ++m_foundFiles;

                const String pathAsString = path.getPath();
                const auto iterator = std::find_if( m_skippedDirs.begin(), m_skippedDirs.end(),
                                                    [&pathAsString]( const CUL::FS::Path& inCurrent )
                                                    {
                                                        return inCurrent.isRootOf( pathAsString );
                                                    } );

                if( iterator != m_skippedDirs.end() )
                {
                    ++m_processedFiles;
                    return;
                }

                if( !path.getIsDir() && path != m_outputFile )
                {
                    const String workerTxt = String::createFromPrintf( "Found: %s", path.getPath().getUtfChar() );
                    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( currentThreadWorkerId, workerTxt.getUtfChar() ) );
                    addTask(
                        [this, path]( int8_t workerId )
                        {
                            ProfilerScope( "CApp::searchAllFiles::it::add_file_task" );
                            const String pathAsString = path.getPath();
                            addFile( pathAsString, workerId );
                            ++m_processedFiles;
                        },
                        path );
                }
                else
                {
                    ++m_processedFiles;
                }
            } );
    }
    setMainStatus( "Searching files... done." );
}

void CApp::showList()
{
    ProfilerScope( "CApp::showList" );
    const auto workerId = CUL::MultiWorkerSystem::getInstance().getCurrentThreadWorkerId();
    std::vector<uint64_t> listOfSizes;
    m_fileDb.getListOfSizes( listOfSizes );
    const std::int64_t listOfSizesSize = static_cast<std::int64_t>( listOfSizes.size() );
    std::int64_t md5It = 0;
    std::int64_t maxMd5s = 4;
    bool exitLoop = false;
    String status;
    for( std::int64_t i = listOfSizesSize - 1; i >= 0; --i )
    {
        const std::int32_t size = static_cast<std::int32_t>( listOfSizes[i] );
        if( ( size < m_minFileSizeMB ) || ( size < m_maxFileSizeMB ) )
        {
            continue;
        }

        if( exitLoop )
        {
            exitLoop = false;
            break;
        }

        std::vector<CUL::FS::FileInfo> sameSizeFiles;
        m_fileDb.getFiles( size, sameSizeFiles );
        if( sameSizeFiles.size() > 1 )
        {
            const auto md5s = getListOfMd5s( sameSizeFiles );
            for( const auto& md5 : md5s )
            {
                std::vector<CUL::FS::FileInfo> duplicatesList;
                m_fileDb.getFiles( size, md5.getValue(), duplicatesList );
                if( duplicatesList.size() > 1 )
                {
                    const String current = String::createFromPrintf( "Size: %d, MD5: %s", size, md5.getUtfChar() );
                    if( ImGui::TreeNode( current.getUtfChar() ) )
                    {
                        for( const CUL::FS::FileInfo& fileInfo : duplicatesList )
                        {
                            const std::string pathAsStr = fileInfo.FilePath.getSTDString();
                            ImGui::TextUnformatted( pathAsStr.c_str() );
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

        status =
            String::createFromPrintf( "Saved duplicate: %4.2f%%", ( 100.f * static_cast<float>( i ) ) / ( 1.f * (float)listOfSizesSize ) );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status.getSTDString() ) );
    }
}

void CApp::fetchDuplicates()
{
    ProfilerScope( "CApp::fetchDuplicates" );

    String status;

    std::vector<uint64_t> inOutListOfSizes;

    m_fileDb.getListOfSizes( inOutListOfSizes );
    // std::reverse( inOutListOfSizes.begin(), inOutListOfSizes.end() );

    std::size_t i = 0u;
    std::size_t max = inOutListOfSizes.size();

    constexpr std::size_t bufferLength = 1024u;
    char statusBuffer[bufferLength];

    for( std::uint64_t currentSize : inOutListOfSizes )
    {
        ProfilerScope( "CApp::fetchDuplicates::it" );
        std::vector<CUL::StringWr> md5List = m_fileDb.getListOfMd5( currentSize );
        for( const CUL::StringWr& currentMD5 : md5List )
        {
            SameFilesGroup sfg;
            sfg.MD5 = currentMD5;
            sfg.Size = currentSize;

            std::vector<CUL::FS::FileInfo> inOutFileList;
            m_fileDb.getFiles( currentSize, currentMD5, inOutFileList );
            if( inOutFileList.size() > 1 )
            {
                SameFilesGroup sfg;
                sfg.MD5 = currentMD5;
                sfg.Size = currentSize;
                for( const CUL::FS::FileInfo& fi : inOutFileList )
                {
                    FileEntry fe;
                    fe.Path = fi.FilePath.getValue();
                    fe.ModTime = fi.ModTime;
                    sfg.Files.push_back( fe );
                }

                std::lock_guard<std::mutex> locker( m_fileGroupsMtx );
                m_fileGroups.insert( sfg );
            }
        }
        const float percentage = 100.f * static_cast<float>( i + 1u ) / static_cast<float>( max );

        const std::uint64_t currSizeMB = currentSize / 1048576u;

#if defined( CUL_WINDOWS )
        snprintf( statusBuffer, bufferLength, "CApp::fetchDuplicates: size: %dMB %4.2f%% (%d/%d)", currSizeMB, percentage, i, max );
#else   // #if defined( CUL_WINDOWS )
        snprintf( statusBuffer, bufferLength, "CApp::fetchDuplicates: size: %ldMB %4.2f%% (%ld/%ld)", currSizeMB, percentage, i, max );
#endif  // #if defined( CUL_WINDOWS )

        CUL::ThreadUtil::getInstance().setThreadStatus( statusBuffer );
        ++i;

        CUL::ITimer::sleepMiliSeconds( 80u );
    }
}

void CApp::saveDuplicates()
{
    ProfilerScope( "CApp::saveDuplicates" );
    const auto workerId = CUL::MultiWorkerSystem::getInstance().getCurrentThreadWorkerId();
    String status;

    auto file = m_culInterface->getFF()->createRegularFileRawPtr( CUL::FS::Path( m_outputFile ) );
    file->toggleCache( false );

    String text = String( "Files: " );
    m_culInterface->getLogger()->log( text.getValue() );
    file->addLine( text );

    std::vector<std::uint64_t> listOfSizes;
    m_fileDb.getListOfSizes( listOfSizes );
    listOfSizes.erase( std::remove_if( listOfSizes.begin(), listOfSizes.end(),
                                       [this]( uint64_t valB )
                                       {
                                           const std::int32_t val = valB / 1048576u;
                                           return ( val < m_minFileSizeMB ) || ( val < m_maxFileSizeMB );
                                       } ),
                       listOfSizes.end() );

    const size_t listOfSizesSize = listOfSizes.size();

    bool save = false;
    std::size_t md5It = 0;
    bool exitLoop = false;

    float wholePercentage = 0.f;
    float currentMd5Percentage = 0.f;

    for( std::int64_t i = listOfSizesSize - 1; i >= 0; --i )
    {
        currentMd5Percentage = 0.f;
        const auto size = listOfSizes[i];

        if( exitLoop )
        {
            exitLoop = false;
            break;
        }

        std::vector<CUL::FS::FileInfo> sameSizeFiles;
        m_fileDb.getFiles( size, sameSizeFiles );
        if( sameSizeFiles.size() > 1 )
        {
            const auto md5s = getListOfMd5s( sameSizeFiles );
            const std::size_t md5Count = md5s.size();

            md5It = 0u;
            for( const auto& md5 : md5s )
            {
                std::vector<CUL::FS::FileInfo> duplicatesList;
                m_fileDb.getFiles( size, md5.getValue(), duplicatesList );
                if( duplicatesList.size() > 1 )
                {
                    const float MegaBytes = 1.f * static_cast<float>( size ) / ( 1.f * bytesINMegabyte );
                    text = String::createFromPrintf( "Size: %4.2fMB", MegaBytes );
                    file->addLine( text );

                    text = String( "MD5: " ) + md5;
                    file->addLine( text );

                    for( const auto& fileInfo : duplicatesList )
                    {
                        file->addLine( fileInfo.FilePath.getValue() );
                        save = true;
                    }
                    ++md5It;
                    currentMd5Percentage = 100.f * static_cast<float>( md5It ) / static_cast<float>( md5Count );
                    wholePercentage = 100.f * ( listOfSizesSize - i ) / static_cast<float>( listOfSizesSize );
                    status = String::createFromPrintf( "Saved duplicate: %4.2f%%, md5: %4.2f%%", wholePercentage, currentMd5Percentage );
                    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status.getSTDString() ) );
                }
            }
        }

        if( save )
        {
            file->saveFile();
            save = false;
        }

        wholePercentage = 100.f * ( listOfSizesSize - i ) / static_cast<float>( listOfSizesSize );
        status = String::createFromPrintf( "Saved duplicate: %4.2f%%, md5: %4.2f%%", wholePercentage, currentMd5Percentage );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status.getSTDString() ) );
    }

    delete file;
}

std::set<String> CApp::getListOfMd5s( const std::vector<CUL::FS::FileInfo>& fiList )
{
    ProfilerScope( "CApp::getListOfMd5s" );
    std::set<String> result;

    for( const auto& fi : fiList )
    {
        result.insert( fi.MD5.getValue() );
    }

    return result;
}

void CApp::startWorkers()
{
    m_workersEnabled = true;
    setWorkersCount( m_maxThreadCount );
}

void CApp::setWorkersCount( uint8_t inCount )
{
    ProfilerScope( "CApp::setWorkersCount" );
    uint8_t workersCount = CUL::MultiWorkerSystem::getInstance().getCurrentWorkersCount();
    while( workersCount != inCount )
    {
        if( workersCount < inCount )
        {
            CUL::MultiWorkerSystem::getInstance().addWorker( CUL::EPriority::Medium );
            workersCount = CUL::MultiWorkerSystem::getInstance().getCurrentWorkersCount();
        }
        else if( workersCount > inCount )
        {
            CUL::MultiWorkerSystem::getInstance().removeWorker( CUL::EPriority::Medium );
            workersCount = CUL::MultiWorkerSystem::getInstance().getCurrentWorkersCount();
        }
    }
}

void CApp::addTask( const std::function<void( int8_t )>& task, const CUL::FS::Path& /*filePath*/ ) const
{
    ProfilerScope( "CApp::addTask" );

    CUL::TaskCallback* taskPtr = new CUL::TaskCallback();
    taskPtr->Callback = task;
    taskPtr->Type = CUL::ITask::EType::DeleteAfterExecute;

    CUL::MultiWorkerSystem::getInstance().registerTask( taskPtr );
}

void CApp::addFile( const String& path, int8_t workerId )
{
    ProfilerScope( "CApp::addFile" );
    String status = String::createFromPrintf( "[START] %s", path.getUtfChar() );
    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status.getUtfChar() ) );

    CUL::GUTILS::ScopeExit se(
        [this, workerId]()
        {
            CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, "IDLE" ) );
        } );

    CUL::FS::Path fp = path;
    if( fp.exists() == false )
    {
        return;
    }

    std::unique_ptr<CUL::FS::IFile> file;
    file.reset( m_culInterface->getFF()->createRegularFileRawPtr( path ) );
    String fileSize = file->getSizeBytes();
    const float fileSizeB = fileSize.toFloat();
    const float sizeMBf = fileSizeB / ( 1048576.f );
    const std::int32_t sizeMB = static_cast<std::int32_t>( sizeMBf );
    const bool isLess = ( sizeMB < m_minFileSizeMB );
    const bool isGreater = ( sizeMB > m_maxFileSizeMB );
    if( isLess || isGreater )
    {
        return;
    }

    status = String::createFromPrintf( "[Get last mod time]  %s", path.getUtfChar() );
    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status.getSTDString() ) );
    CUL::Time modTimeFromFS;
    file->getLastModificationTime( modTimeFromFS );

    status = String::createFromPrintf( "[Get DB info]  %s", path.getUtfChar() );
    CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, status.getSTDString() ) );
    std::optional<CUL::FS::FileInfo> info = m_fileDb.getFileInfo( path.getValue() );
    CUL::Time modTimeFromDb;
    String md5;
    if( info )
    {
        modTimeFromDb = info->ModTime;
        md5 = info->MD5.getValue();
    }

    auto almostTheSame = []( const CUL::Time& t1, const CUL::Time& t2 )
    {
        if( t1 == t2 )
        {
            return true;
        }

        return t1.almostTheSame( t2, 3 );
    };

    // if( modTimeFromFS == modTimeFromDb )
    if( almostTheSame( modTimeFromFS, modTimeFromDb ) )
    {
    }
    else
    {
        const std::string pathAsStdString = path.getSTDString();
        CUL::LOG::ILogger::getInstance().logVariable( CUL::LOG::Severity::Info, "%s has was changed in: %s, but in db is: %s",
                                                      pathAsStdString.c_str(), modTimeFromFS.cStr(), modTimeFromDb.cStr() );

        constexpr std::size_t buffSize{ 2048u };
        char buffer[buffSize];
        sprintf( buffer, "[File changed, calcualte md5...] %s", pathAsStdString.c_str() );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, buffer ) );

        md5 = file->getMD5();
        sprintf( buffer, "[File changed, calcualte md5 done.] %s", pathAsStdString.c_str() );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, buffer ) );

        sprintf( buffer, "[File adding to db...] %s", pathAsStdString.c_str() );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, buffer ) );

        const String fileSizeStr = String::createFromPrintf( "%d", sizeMB );
        m_fileDb.addFile( md5.getValue(), path.getValue(), fileSizeStr.getValue(), modTimeFromFS.toString().getValue() );

        sprintf( buffer, "[File adding to db... done.] %s", pathAsStdString.c_str() );
        CUL::ThreadUtil::getInstance().setThreadStatus( WorkerStatus( workerId, buffer ) );
    }
}

void CApp::addFileToList( const String )
{
}

void CApp::addDuplicate( const FileSize, const MD5Value&, const CUL::FS::Path& )
{
}

String CApp::getModTimeFromDb( const String& filePath )
{
    ProfilerScope( "CApp::getModTimeFromDb" );
    std::optional<CUL::FS::FileInfo> info = m_fileDb.getFileInfo( filePath.getValue() );

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

void CApp::addForCheckForDeletionList( const String& inPath )
{
    ProfilerScope( "CApp::addForCheckForDeletionList" );
    const auto it = std::find_if( m_deletionList.begin(), m_deletionList.end(),
                                  [inPath]( const String& path )
                                  {
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
    std::setlocale( LC_ALL, "C" );
    std::locale::global( std::locale::classic() );

    CUL::GUTILS::ConsoleUtilities cu;
    cu.setArgs( argc, args );
    auto width = cu.getFlagValue( "-w" );
    auto height = cu.getFlagValue( "-h" );

    if( width.empty() || height.empty() )
    {
        width = "1920";
        height = "1080";
    }

    const String winName = "Duplicate finder";
    const char* winNameStr = winName.getUtfChar();
    CApp app( false, std::stoul( width.string() ), std::stoul( height.string() ), 256, 127, winNameStr, "Config.txt" );
    app.run();

    return 0;
}

#if defined( DEBUG_THIS_FILE )
#if defined( _MSC_VER )
#pragma optimize( "", on )
#elif defined( __clang__ )
#pragma clang optimize on
#endif
#endif  // #if defined(DEBUG_THIS_FILE)
